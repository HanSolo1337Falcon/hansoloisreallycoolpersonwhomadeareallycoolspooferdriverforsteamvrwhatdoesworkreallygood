#include "Config.h"
#include "Logger.h"
#include "MiniJson.h"

#include <windows.h>
#include <cstdio>

namespace vrspoof
{
	namespace
	{
		SpoofMode ParseMode( const std::string &s )
		{
			if ( s == "override" ) return SpoofMode::Override;
			if ( s == "virtual" )  return SpoofMode::Virtual;
			return SpoofMode::Off;
		}

		void ReadIcons( const mj::Value &v, IconSet &out )
		{
			if ( v.IsNull() )
				return;
			out.off            = v[ "off" ].AsString( out.off );
			out.searching      = v[ "searching" ].AsString( out.searching );
			out.searchingAlert = v[ "searchingAlert" ].AsString( out.searchingAlert );
			out.ready          = v[ "ready" ].AsString( out.ready );
			out.readyAlert     = v[ "readyAlert" ].AsString( out.readyAlert );
			out.notReady       = v[ "notReady" ].AsString( out.notReady );
			out.standby        = v[ "standby" ].AsString( out.standby );
			out.standbyAlert   = v[ "standbyAlert" ].AsString( out.standbyAlert );
			out.alertLow       = v[ "alertLow" ].AsString( out.alertLow );
		}

		// "extraStrings": { "1005": "Valve" } - keys are property ids as strings
		// because JSON object keys always are.
		template <typename MapT, typename ConvertFn>
		void ReadExtras( const mj::Value &v, MapT &out, ConvertFn convert )
		{
			if ( v.type != mj::Type::Object )
				return;
			for ( const auto &kv : v.object )
			{
				char *end = nullptr;
				long id = strtol( kv.first.c_str(), &end, 10 );
				if ( end == kv.first.c_str() || *end != '\0' || id <= 0 )
				{
					Log::Warn( "ignoring non-numeric property id '%s' in config", kv.first.c_str() );
					continue;
				}
				out[ (int)id ] = convert( kv.second );
			}
		}

		void ReadDevice( const mj::Value &v, DeviceSpoof &out )
		{
			if ( v.IsNull() )
				return;

			out.enabled              = v[ "enabled" ].AsBool( out.enabled );
			out.manufacturer         = v[ "manufacturer" ].AsString( out.manufacturer );
			out.modelNumber          = v[ "modelNumber" ].AsString( out.modelNumber );
			out.serialNumber         = v[ "serialNumber" ].AsString( out.serialNumber );
			out.renderModel          = v[ "renderModel" ].AsString( out.renderModel );
			out.trackingSystem       = v[ "trackingSystem" ].AsString( out.trackingSystem );
			out.registeredDeviceType = v[ "registeredDeviceType" ].AsString( out.registeredDeviceType );
			out.controllerType       = v[ "controllerType" ].AsString( out.controllerType );
			out.inputProfilePath     = v[ "inputProfilePath" ].AsString( out.inputProfilePath );
			out.hardwareRevision     = v[ "hardwareRevision" ].AsString( out.hardwareRevision );
			out.driverVersion        = v[ "driverVersion" ].AsString( out.driverVersion );
			out.spoofSerial          = v[ "spoofSerial" ].AsBool( out.spoofSerial );
			out.spoofBindings        = v[ "spoofBindings" ].AsBool( out.spoofBindings );

			ReadIcons( v[ "icons" ], out.icons );

			ReadExtras( v[ "extraStrings" ], out.extraStrings,
			            []( const mj::Value &x ) { return x.AsString(); } );
			ReadExtras( v[ "extraFloats" ], out.extraFloats,
			            []( const mj::Value &x ) { return x.AsNumber(); } );
			ReadExtras( v[ "extraInts" ], out.extraInts,
			            []( const mj::Value &x ) { return (int32_t)x.AsInt(); } );
			ReadExtras( v[ "extraBools" ], out.extraBools,
			            []( const mj::Value &x ) { return x.AsBool(); } );
		}

		void ReadDisplay( const mj::Value &v, VirtualDisplay &out )
		{
			if ( v.IsNull() )
				return;
			out.windowX      = v[ "windowX" ].AsInt( out.windowX );
			out.windowY      = v[ "windowY" ].AsInt( out.windowY );
			out.windowWidth  = (uint32_t)v[ "windowWidth" ].AsInt( (int)out.windowWidth );
			out.windowHeight = (uint32_t)v[ "windowHeight" ].AsInt( (int)out.windowHeight );
			out.renderWidth  = (uint32_t)v[ "renderWidth" ].AsInt( (int)out.renderWidth );
			out.renderHeight = (uint32_t)v[ "renderHeight" ].AsInt( (int)out.renderHeight );
			out.displayFrequency          = v[ "displayFrequency" ].AsFloat( out.displayFrequency );
			out.secondsFromVsyncToPhotons = v[ "secondsFromVsyncToPhotons" ].AsFloat( out.secondsFromVsyncToPhotons );
			out.ipd         = v[ "ipd" ].AsFloat( out.ipd );
			out.isOnDesktop = v[ "isOnDesktop" ].AsBool( out.isOnDesktop );

			// A zero-sized window makes vrserver very unhappy.
			if ( out.windowWidth  == 0 ) out.windowWidth  = 2064;
			if ( out.windowHeight == 0 ) out.windowHeight = 2208;
			if ( out.renderWidth  == 0 ) out.renderWidth  = out.windowWidth;
			if ( out.renderHeight == 0 ) out.renderHeight = out.windowHeight;
			if ( out.displayFrequency < 1.0f ) out.displayFrequency = 90.0f;
		}
	}

	bool ConfigStore::Load()
	{
		std::string dir = GetAppDataDir();
		if ( dir.empty() )
		{
			m_lastError = "could not resolve %LOCALAPPDATA%\\VRSpoofer";
			Log::Error( "%s", m_lastError.c_str() );
			return false;
		}

		m_path = dir + "\\config.json";
		m_lastWriteTime = 0;
		m_lastSize = 0;

		// Force a read on the next poll rather than duplicating the logic here.
		return ReloadIfChanged();
	}

	bool ConfigStore::ReloadIfChanged()
	{
		if ( m_path.empty() )
			return false;

		WIN32_FILE_ATTRIBUTE_DATA data;
		if ( !GetFileAttributesExA( m_path.c_str(), GetFileExInfoStandard, &data ) )
		{
			// No config yet - the GUI hasn't written one. Stay idle, and make
			// sure we notice when it appears.
			if ( m_lastWriteTime != 0 || m_lastSize != 0 )
			{
				Log::Warn( "config file disappeared, disabling all spoofing" );
				m_config = Config();
				m_lastWriteTime = 0;
				m_lastSize = 0;
				return true;
			}
			return false;
		}

		uint64_t writeTime = ( (uint64_t)data.ftLastWriteTime.dwHighDateTime << 32 ) |
		                     data.ftLastWriteTime.dwLowDateTime;
		uint64_t size = ( (uint64_t)data.nFileSizeHigh << 32 ) | data.nFileSizeLow;

		if ( writeTime == m_lastWriteTime && size == m_lastSize )
			return false;

		std::string text;
		if ( !ReadFile( text ) )
		{
			// Don't spin retrying a file we just failed to open - most likely the
			// GUI is mid-write and we'll pick it up on the next distinct stamp.
			m_lastWriteTime = writeTime;
			m_lastSize = size;
			return false;
		}

		Config parsed;
		if ( !ParseInto( text, parsed ) )
		{
			m_lastWriteTime = writeTime;
			m_lastSize = size;
			Log::Error( "config.json is not valid JSON - keeping previous settings" );
			return false;
		}

		m_config = parsed;
		m_lastWriteTime = writeTime;
		m_lastSize = size;
		m_lastError.clear();

		Log::SetVerbose( m_config.verboseLog );
		Log::Info( "config loaded (enabled=%d hmdMode=%d controllerMode=%d)",
		           m_config.masterEnabled ? 1 : 0,
		           (int)m_config.hmdMode, (int)m_config.controllerMode );
		return true;
	}

	bool ConfigStore::ReadFile( std::string &out ) const
	{
		// Share-deny-none so a GUI write in flight doesn't hard-fail the read.
		HANDLE h = CreateFileA( m_path.c_str(), GENERIC_READ,
		                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
		if ( h == INVALID_HANDLE_VALUE )
			return false;

		LARGE_INTEGER size;
		if ( !GetFileSizeEx( h, &size ) || size.QuadPart <= 0 || size.QuadPart > 8 * 1024 * 1024 )
		{
			CloseHandle( h );
			return false;
		}

		out.resize( (size_t)size.QuadPart );
		DWORD read = 0;
		BOOL ok = ::ReadFile( h, &out[ 0 ], (DWORD)size.QuadPart, &read, nullptr );
		CloseHandle( h );

		if ( !ok || read != (DWORD)size.QuadPart )
			return false;

		// Tolerate a UTF-8 BOM if something else rewrote the file.
		if ( out.size() >= 3 && (unsigned char)out[ 0 ] == 0xEF &&
		     (unsigned char)out[ 1 ] == 0xBB && (unsigned char)out[ 2 ] == 0xBF )
		{
			out.erase( 0, 3 );
		}
		return true;
	}

	bool ConfigStore::ParseInto( const std::string &text, Config &out ) const
	{
		mj::Value root;
		if ( !mj::Parser::Parse( text, root ) || root.type != mj::Type::Object )
			return false;

		out.masterEnabled        = root[ "enabled" ].AsBool( true );
		out.verboseLog           = root[ "verboseLog" ].AsBool( false );
		out.applyIntervalSeconds = root[ "applyIntervalSeconds" ].AsNumber( 1.0 );
		if ( out.applyIntervalSeconds < 0.1 )  out.applyIntervalSeconds = 0.1;
		if ( out.applyIntervalSeconds > 30.0 ) out.applyIntervalSeconds = 30.0;

		out.hmdMode        = ParseMode( root[ "hmdMode" ].AsString( "off" ) );
		out.controllerMode = ParseMode( root[ "controllerMode" ].AsString( "off" ) );

		ReadDevice( root[ "hmd" ], out.hmd );
		ReadDevice( root[ "leftController" ], out.left );
		ReadDevice( root[ "rightController" ], out.right );
		ReadDisplay( root[ "display" ], out.display );

		return true;
	}
}
