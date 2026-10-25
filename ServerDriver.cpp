#include "ServerDriver.h"
#include "Logger.h"

#include <windows.h>

#include <cstdio>
#include <ctime>

namespace vrspoof
{
	namespace
	{
		const uint64_t kConfigPollMs = 750;
		const uint64_t kStatusWriteMs = 2000;

		std::string JsonEscape( const std::string &s )
		{
			std::string out;
			out.reserve( s.size() + 8 );
			for ( char c : s )
			{
				switch ( c )
				{
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n";  break;
				case '\r': out += "\\r";  break;
				case '\t': out += "\\t";  break;
				default:
					if ( (unsigned char)c < 0x20 )
					{
						char buf[ 8 ];
						snprintf( buf, sizeof( buf ), "\\u%04x", (unsigned char)c );
						out += buf;
					}
					else
					{
						out.push_back( c );
					}
				}
			}
			return out;
		}

		const char *ModeName( SpoofMode m )
		{
			switch ( m )
			{
			case SpoofMode::Override: return "override";
			case SpoofMode::Virtual:  return "virtual";
			default:                  return "off";
			}
		}

		// Writes to a temp file and moves it into place so the GUI never reads a
		// half-written status file.
		bool WriteFileAtomic( const std::string &path, const std::string &contents )
		{
			std::string tmp = path + ".tmp";

			HANDLE h = CreateFileA( tmp.c_str(), GENERIC_WRITE, 0, nullptr,
			                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
			if ( h == INVALID_HANDLE_VALUE )
				return false;

			DWORD written = 0;
			BOOL ok = WriteFile( h, contents.data(), (DWORD)contents.size(), &written, nullptr );
			CloseHandle( h );

			if ( !ok || written != contents.size() )
			{
				DeleteFileA( tmp.c_str() );
				return false;
			}
			return MoveFileExA( tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING ) != FALSE;
		}
	}

	ServerDriver &ServerDriver::Instance()
	{
		static ServerDriver s_instance;
		return s_instance;
	}

	vr::EVRInitError ServerDriver::Init( vr::IVRDriverContext *pDriverContext )
	{
		VR_INIT_SERVER_DRIVER_CONTEXT( pDriverContext );

		std::string appData = GetAppDataDir();
		if ( !appData.empty() )
			Log::Init( appData + "\\driver.log" );

		Log::Info( "VRSpoofer driver starting (build " __DATE__ " " __TIME__ ")" );

		if ( !m_configStore.Load() )
			Log::Warn( "no usable config yet - idle until the GUI writes one" );

		CreateVirtualDevices();

		m_initialised = true;
		m_lastConfigPollMs = GetTickCount64();
		m_lastApplyMs      = 0;
		m_lastStatusMs     = 0;

		return vr::VRInitError_None;
	}

	void ServerDriver::CreateVirtualDevices()
	{
		const Config &cfg = m_configStore.Get();
		if ( !cfg.masterEnabled )
			return;

		vr::IVRServerDriverHost *host = vr::VRServerDriverHost();
		if ( !host )
		{
			Log::Error( "no server driver host - cannot publish virtual devices" );
			return;
		}

		// Devices can only be added while the driver is initialising, so mode
		// changes between Virtual and Override need a SteamVR restart. The GUI
		// says so, and prompts.
		if ( cfg.hmdMode == SpoofMode::Virtual && cfg.hmd.enabled )
		{
			m_virtualHmd = std::make_unique<VirtualHmd>( cfg.hmd, cfg.display );
			if ( !host->TrackedDeviceAdded( m_virtualHmd->Serial().c_str(),
			                                vr::TrackedDeviceClass_HMD, m_virtualHmd.get() ) )
			{
				Log::Error( "SteamVR refused the virtual HMD (a real HMD is probably already active)" );
				m_virtualHmd.reset();
			}
		}

		if ( cfg.controllerMode == SpoofMode::Virtual )
		{
			if ( cfg.left.enabled )
			{
				m_virtualLeft = std::make_unique<VirtualController>( cfg.left, vr::TrackedControllerRole_LeftHand );
				if ( !host->TrackedDeviceAdded( m_virtualLeft->Serial().c_str(),
				                                vr::TrackedDeviceClass_Controller, m_virtualLeft.get() ) )
				{
					Log::Error( "SteamVR refused the virtual left controller" );
					m_virtualLeft.reset();
				}
			}

			if ( cfg.right.enabled )
			{
				m_virtualRight = std::make_unique<VirtualController>( cfg.right, vr::TrackedControllerRole_RightHand );
				if ( !host->TrackedDeviceAdded( m_virtualRight->Serial().c_str(),
				                                vr::TrackedDeviceClass_Controller, m_virtualRight.get() ) )
				{
					Log::Error( "SteamVR refused the virtual right controller" );
					m_virtualRight.reset();
				}
			}
		}
	}

	std::set<uint32_t> ServerDriver::OwnedIndices() const
	{
		std::set<uint32_t> owned;
		if ( m_virtualHmd && m_virtualHmd->ObjectId() != vr::k_unTrackedDeviceIndexInvalid )
			owned.insert( m_virtualHmd->ObjectId() );
		if ( m_virtualLeft && m_virtualLeft->ObjectId() != vr::k_unTrackedDeviceIndexInvalid )
			owned.insert( m_virtualLeft->ObjectId() );
		if ( m_virtualRight && m_virtualRight->ObjectId() != vr::k_unTrackedDeviceIndexInvalid )
			owned.insert( m_virtualRight->ObjectId() );
		return owned;
	}

	void ServerDriver::UpdateVirtualControllerStandDown( bool wanted )
	{
		if ( !m_virtualLeft && !m_virtualRight )
			return;

		// Turned off in the GUI: yield both hands immediately.
		if ( !wanted )
		{
			if ( m_virtualLeft )  m_virtualLeft->SetStoodDown( true );
			if ( m_virtualRight ) m_virtualRight->SetStoodDown( true );
			return;
		}

		const std::set<uint32_t> owned = OwnedIndices();
		bool realLeft = false;
		bool realRight = false;

		for ( uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++ )
		{
			if ( owned.count( i ) )
				continue;

			vr::PropertyContainerHandle_t c =
				vr::VRPropertiesRaw()->TrackedDeviceToPropertyContainer( i );
			if ( c == vr::k_ulInvalidPropertyContainer )
				continue;

			vr::ETrackedPropertyError err = vr::TrackedProp_Success;
			int32_t deviceClass = vr::VRProperties()->GetInt32Property( c, vr::Prop_DeviceClass_Int32, &err );
			if ( err != vr::TrackedProp_Success || deviceClass != vr::TrackedDeviceClass_Controller )
				continue;

			switch ( vr::VRProperties()->GetInt32Property( c, vr::Prop_ControllerRoleHint_Int32 ) )
			{
			case vr::TrackedControllerRole_LeftHand:
				realLeft = true;
				break;
			case vr::TrackedControllerRole_RightHand:
				realRight = true;
				break;
			default:
				// A real controller that hasn't been assigned a hand yet could
				// end up as either, so yield both rather than race it.
				realLeft = true;
				realRight = true;
				break;
			}
		}

		if ( m_virtualLeft )
			m_virtualLeft->SetStoodDown( realLeft );
		if ( m_virtualRight )
			m_virtualRight->SetStoodDown( realRight );
	}

	void ServerDriver::OnConfigReloaded()
	{
		const Config &cfg = m_configStore.Get();

		// Force the override engine to rewrite everything with the new values.
		m_spoofer.Invalidate();

		// Virtual devices already exist, so relabel them in place rather than
		// asking for a restart just to change a name.
		if ( m_virtualHmd && cfg.hmd.enabled )
			m_virtualHmd->ReapplyIdentity( cfg.hmd );
		if ( m_virtualLeft && cfg.left.enabled )
			m_virtualLeft->ReapplyIdentity( cfg.left );
		if ( m_virtualRight && cfg.right.enabled )
			m_virtualRight->ReapplyIdentity( cfg.right );
	}

	void ServerDriver::RunFrame()
	{
		if ( !m_initialised )
			return;

		const uint64_t now = GetTickCount64();

		if ( now - m_lastConfigPollMs >= kConfigPollMs )
		{
			m_lastConfigPollMs = now;
			if ( m_configStore.ReloadIfChanged() )
				OnConfigReloaded();
		}

		const Config &cfg = m_configStore.Get();

		const uint64_t applyIntervalMs = (uint64_t)( cfg.applyIntervalSeconds * 1000.0 );
		if ( now - m_lastApplyMs >= applyIntervalMs )
		{
			m_lastApplyMs = now;

			// Runs even when spoofing is disabled. Devices published earlier in
			// this session outlive the setting that created them, so switching
			// Virtual mode off has to release the hands here - otherwise the
			// only way to get a real controller back is a SteamVR restart.
			const bool wantVirtualControllers =
				cfg.masterEnabled && cfg.controllerMode == SpoofMode::Virtual;
			UpdateVirtualControllerStandDown( wantVirtualControllers );

			if ( cfg.masterEnabled )
				m_spoofer.Apply( cfg, OwnedIndices() );
		}

		if ( m_virtualHmd )   m_virtualHmd->RunFrame();
		if ( m_virtualLeft )  m_virtualLeft->RunFrame();
		if ( m_virtualRight ) m_virtualRight->RunFrame();

		if ( now - m_lastStatusMs >= kStatusWriteMs )
		{
			m_lastStatusMs = now;
			WriteStatusFile();
		}

		// Drain the event queue so vrserver doesn't back up on our account.
		vr::VREvent_t event;
		while ( vr::VRServerDriverHost() &&
		        vr::VRServerDriverHost()->PollNextEvent( &event, sizeof( event ) ) )
		{
			// Nothing to handle yet, but a fresh device deserves an immediate
			// re-apply rather than waiting for the next interval.
			if ( event.eventType == vr::VREvent_TrackedDeviceActivated ||
			     event.eventType == vr::VREvent_TrackedDeviceRoleChanged )
			{
				m_spoofer.Invalidate();
				m_lastApplyMs = 0;
			}
		}
	}

	void ServerDriver::WriteStatusFile()
	{
		std::string appData = GetAppDataDir();
		if ( appData.empty() )
			return;

		const Config &cfg = m_configStore.Get();
		std::set<uint32_t> owned = OwnedIndices();

		std::string json;
		json.reserve( 4096 );
		json += "{\n";
		json += "  \"driverRunning\": true,\n";
		json += "  \"pid\": " + std::to_string( (unsigned long)GetCurrentProcessId() ) + ",\n";
		json += "  \"timestamp\": " + std::to_string( (long long)time( nullptr ) ) + ",\n";
		json += "  \"enabled\": " + std::string( cfg.masterEnabled ? "true" : "false" ) + ",\n";
		json += "  \"hmdMode\": \"" + std::string( ModeName( cfg.hmdMode ) ) + "\",\n";
		json += "  \"controllerMode\": \"" + std::string( ModeName( cfg.controllerMode ) ) + "\",\n";
		json += "  \"devicesTouched\": " + std::to_string( m_spoofer.DevicesTouchedLastPass() ) + ",\n";
		json += "  \"devices\": [\n";

		bool first = true;
		for ( uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++ )
		{
			vr::PropertyContainerHandle_t c = vr::VRPropertiesRaw()->TrackedDeviceToPropertyContainer( i );
			if ( c == vr::k_ulInvalidPropertyContainer )
				continue;

			vr::ETrackedPropertyError err = vr::TrackedProp_Success;
			int32_t deviceClass = vr::VRProperties()->GetInt32Property( c, vr::Prop_DeviceClass_Int32, &err );
			if ( err != vr::TrackedProp_Success || deviceClass == vr::TrackedDeviceClass_Invalid )
				continue;

			if ( !first )
				json += ",\n";
			first = false;

			json += "    {";
			json += "\"index\": " + std::to_string( i );
			json += ", \"class\": " + std::to_string( deviceClass );
			json += ", \"role\": " + std::to_string( vr::VRProperties()->GetInt32Property( c, vr::Prop_ControllerRoleHint_Int32 ) );
			json += ", \"manufacturer\": \"" + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_ManufacturerName_String ) ) + "\"";
			json += ", \"model\": \""        + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_ModelNumber_String ) ) + "\"";
			json += ", \"serial\": \""       + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_SerialNumber_String ) ) + "\"";
			json += ", \"renderModel\": \""  + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_RenderModelName_String ) ) + "\"";
			json += ", \"trackingSystem\": \"" + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_TrackingSystemName_String ) ) + "\"";
			// Read the icon back so a "my icon didn't change" report can be told
			// apart from "the property was never written".
			json += ", \"iconReady\": \"" + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_NamedIconPathDeviceReady_String ) ) + "\"";
			json += ", \"iconOff\": \""   + JsonEscape( vr::VRProperties()->GetStringProperty( c, vr::Prop_NamedIconPathDeviceOff_String ) ) + "\"";
			json += ", \"ours\": " + std::string( owned.count( i ) ? "true" : "false" );
			json += "}";
		}

		json += "\n  ]\n}\n";

		WriteFileAtomic( appData + "\\status.json", json );
	}

	void ServerDriver::Cleanup()
	{
		Log::Info( "VRSpoofer driver shutting down" );

		// Leave a status file that says we're gone, so the GUI doesn't keep
		// reporting a connected driver after SteamVR exits.
		std::string appData = GetAppDataDir();
		if ( !appData.empty() )
		{
			std::string json = "{\n  \"driverRunning\": false,\n  \"timestamp\": " +
			                   std::to_string( (long long)time( nullptr ) ) +
			                   ",\n  \"devices\": []\n}\n";
			WriteFileAtomic( appData + "\\status.json", json );
		}

		m_virtualHmd.reset();
		m_virtualLeft.reset();
		m_virtualRight.reset();
		m_initialised = false;

		Log::Shutdown();
		VR_CLEANUP_SERVER_DRIVER_CONTEXT();
	}

	const char *const *ServerDriver::GetInterfaceVersions()
	{
		return vr::k_InterfaceVersions;
	}

	bool ServerDriver::ShouldBlockStandbyMode() { return false; }
	void ServerDriver::EnterStandby()           {}
	void ServerDriver::LeaveStandby()           {}
}
