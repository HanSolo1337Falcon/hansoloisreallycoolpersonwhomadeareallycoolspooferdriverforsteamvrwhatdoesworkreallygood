#include "PropertySpoofer.h"
#include "Logger.h"

namespace vrspoof
{
	namespace
	{
		void SetStr( vr::PropertyContainerHandle_t c, vr::ETrackedDeviceProperty prop, const std::string &v )
		{
			if ( v.empty() )
				return;   // empty means "leave whatever the real driver set"
			vr::ETrackedPropertyError err = vr::VRProperties()->SetStringProperty( c, prop, v.c_str() );
			if ( err != vr::TrackedProp_Success )
			{
				Log::Debug( "SetStringProperty(%d) failed: %s", (int)prop,
				            vr::VRPropertiesRaw()->GetPropErrorNameFromEnum( err ) );
			}
		}

		void ApplyIcons( vr::PropertyContainerHandle_t c, const IconSet &icons )
		{
			SetStr( c, vr::Prop_NamedIconPathDeviceOff_String,            icons.off );
			SetStr( c, vr::Prop_NamedIconPathDeviceSearching_String,      icons.searching );
			SetStr( c, vr::Prop_NamedIconPathDeviceSearchingAlert_String, icons.searchingAlert );
			SetStr( c, vr::Prop_NamedIconPathDeviceReady_String,          icons.ready );
			SetStr( c, vr::Prop_NamedIconPathDeviceReadyAlert_String,     icons.readyAlert );
			SetStr( c, vr::Prop_NamedIconPathDeviceNotReady_String,       icons.notReady );
			SetStr( c, vr::Prop_NamedIconPathDeviceStandby_String,        icons.standby );
			SetStr( c, vr::Prop_NamedIconPathDeviceStandbyAlert_String,   icons.standbyAlert );
			SetStr( c, vr::Prop_NamedIconPathDeviceAlertLow_String,       icons.alertLow );
		}

		void ApplyExtras( vr::PropertyContainerHandle_t c, const DeviceSpoof &spoof )
		{
			for ( const auto &kv : spoof.extraStrings )
				vr::VRProperties()->SetStringProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second.c_str() );
			for ( const auto &kv : spoof.extraFloats )
				vr::VRProperties()->SetFloatProperty( c, (vr::ETrackedDeviceProperty)kv.first, (float)kv.second );
			for ( const auto &kv : spoof.extraInts )
				vr::VRProperties()->SetInt32Property( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
			for ( const auto &kv : spoof.extraBools )
				vr::VRProperties()->SetBoolProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
		}
	}

	void PropertySpoofer::Invalidate()
	{
		m_signatures.clear();
	}

	std::string PropertySpoofer::Signature( const DeviceSpoof &spoof )
	{
		// Cheap change-detector: any edit in the GUI produces a different string.
		std::string s;
		s.reserve( 256 );
		s += spoof.manufacturer;         s += '\x1f';
		s += spoof.modelNumber;          s += '\x1f';
		s += spoof.serialNumber;         s += '\x1f';
		s += spoof.renderModel;          s += '\x1f';
		s += spoof.trackingSystem;       s += '\x1f';
		s += spoof.registeredDeviceType; s += '\x1f';
		s += spoof.controllerType;       s += '\x1f';
		s += spoof.inputProfilePath;     s += '\x1f';
		s += spoof.hardwareRevision;     s += '\x1f';
		s += spoof.driverVersion;        s += '\x1f';
		s += spoof.icons.off;            s += '\x1f';
		s += spoof.icons.ready;          s += '\x1f';
		s += spoof.icons.searching;      s += '\x1f';
		s += spoof.spoofSerial ? "1" : "0";
		s += spoof.spoofBindings ? "1" : "0";
		s += '\x1f';
		s += std::to_string( spoof.extraStrings.size() + spoof.extraFloats.size() +
		                     spoof.extraInts.size() + spoof.extraBools.size() );
		return s;
	}

	bool PropertySpoofer::AlreadyCurrent( uint32_t index,
	                                      vr::PropertyContainerHandle_t container,
	                                      const DeviceSpoof &spoof )
	{
		auto it = m_signatures.find( index );
		if ( it == m_signatures.end() || it->second != Signature( spoof ) )
			return false;

		// The owning driver can rewrite its own properties at any time (a
		// reconnect, a firmware read, a role change), so verify rather than
		// trusting our bookkeeping alone.
		if ( !spoof.modelNumber.empty() )
		{
			std::string live = vr::VRProperties()->GetStringProperty( container, vr::Prop_ModelNumber_String );
			if ( live != spoof.modelNumber )
				return false;
		}
		if ( !spoof.manufacturer.empty() )
		{
			std::string live = vr::VRProperties()->GetStringProperty( container, vr::Prop_ManufacturerName_String );
			if ( live != spoof.manufacturer )
				return false;
		}
		return true;
	}

	void PropertySpoofer::WriteDevice( vr::PropertyContainerHandle_t container,
	                                   const DeviceSpoof &spoof,
	                                   bool allowSerialRewrite )
	{
		SetStr( container, vr::Prop_ManufacturerName_String,     spoof.manufacturer );
		SetStr( container, vr::Prop_ModelNumber_String,          spoof.modelNumber );
		SetStr( container, vr::Prop_RenderModelName_String,      spoof.renderModel );
		SetStr( container, vr::Prop_TrackingSystemName_String,   spoof.trackingSystem );
		SetStr( container, vr::Prop_RegisteredDeviceType_String, spoof.registeredDeviceType );

		// Opt-in only: see the note on DeviceSpoof::spoofBindings. Rewriting
		// these on real hardware is what kills bindings and the dashboard laser.
		if ( spoof.spoofBindings )
		{
			SetStr( container, vr::Prop_ControllerType_String,   spoof.controllerType );
			SetStr( container, vr::Prop_InputProfilePath_String, spoof.inputProfilePath );
		}

		SetStr( container, vr::Prop_HardwareRevision_String,     spoof.hardwareRevision );
		SetStr( container, vr::Prop_DriverVersion_String,        spoof.driverVersion );

		if ( allowSerialRewrite && spoof.spoofSerial )
			SetStr( container, vr::Prop_SerialNumber_String, spoof.serialNumber );

		ApplyIcons( container, spoof.icons );
		ApplyExtras( container, spoof );
	}

	void PropertySpoofer::Apply( const Config &cfg, const std::set<uint32_t> &ownedIndices )
	{
		m_touchedLastPass = 0;

		const bool doHmd         = cfg.hmdMode == SpoofMode::Override && cfg.hmd.enabled;
		const bool doControllers = cfg.controllerMode == SpoofMode::Override &&
		                           ( cfg.left.enabled || cfg.right.enabled );
		if ( !doHmd && !doControllers )
			return;

		// Controllers whose role hint isn't assigned yet fall back to discovery
		// order, which matches how SteamVR itself lists them.
		int unroledControllerSeen = 0;

		for ( uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++ )
		{
			if ( ownedIndices.count( i ) )
				continue;

			vr::PropertyContainerHandle_t container =
				vr::VRPropertiesRaw()->TrackedDeviceToPropertyContainer( i );
			if ( container == vr::k_ulInvalidPropertyContainer )
				continue;

			vr::ETrackedPropertyError err = vr::TrackedProp_Success;
			int32_t deviceClass = vr::VRProperties()->GetInt32Property( container, vr::Prop_DeviceClass_Int32, &err );
			if ( err != vr::TrackedProp_Success )
				continue;   // slot not really occupied

			const DeviceSpoof *spoof = nullptr;

			if ( deviceClass == vr::TrackedDeviceClass_HMD )
			{
				if ( doHmd )
					spoof = &cfg.hmd;
			}
			else if ( deviceClass == vr::TrackedDeviceClass_Controller )
			{
				if ( doControllers )
				{
					int32_t role = vr::VRProperties()->GetInt32Property( container, vr::Prop_ControllerRoleHint_Int32 );
					if ( role == vr::TrackedControllerRole_LeftHand )
						spoof = &cfg.left;
					else if ( role == vr::TrackedControllerRole_RightHand )
						spoof = &cfg.right;
					else
						spoof = ( unroledControllerSeen++ == 0 ) ? &cfg.left : &cfg.right;
				}
			}

			if ( !spoof || !spoof->enabled )
				continue;

			if ( AlreadyCurrent( i, container, *spoof ) )
				continue;

			WriteDevice( container, *spoof, /*allowSerialRewrite=*/true );
			m_signatures[ i ] = Signature( *spoof );
			m_touchedLastPass++;

			Log::Info( "spoofed device %u (class %d) as '%s %s'", i, deviceClass,
			           spoof->manufacturer.c_str(), spoof->modelNumber.c_str() );
		}
	}
}
