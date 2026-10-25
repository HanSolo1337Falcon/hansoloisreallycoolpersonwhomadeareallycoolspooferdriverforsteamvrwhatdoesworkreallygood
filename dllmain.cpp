// dllmain.cpp - the single exported entry point vrserver.exe looks for.
#include <openvr_driver.h>

#include <cstring>

#include "ServerDriver.h"

#if defined( _WIN32 )
	#define VRSPOOFER_EXPORT extern "C" __declspec( dllexport )
#else
	#define VRSPOOFER_EXPORT extern "C" __attribute__( ( visibility( "default" ) ) )
#endif

VRSPOOFER_EXPORT void *HmdDriverFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( pInterfaceName &&
	     std::strcmp( pInterfaceName, vr::IServerTrackedDeviceProvider_Version ) == 0 )
	{
		if ( pReturnCode )
			*pReturnCode = vr::VRInitError_None;
		return &vrspoof::ServerDriver::Instance();
	}

	// We deliberately do not provide IVRWatchdogProvider: a watchdog exists to
	// wake SteamVR when a headset powers on, and this driver owns no hardware.
	if ( pReturnCode )
		*pReturnCode = vr::VRInitError_Init_InterfaceNotFound;

	return nullptr;
}
