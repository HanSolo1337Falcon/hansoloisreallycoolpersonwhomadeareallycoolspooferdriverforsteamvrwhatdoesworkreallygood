// Logger.h - dual-sink logging for the VRSpoofer driver.
//
// Everything goes to SteamVR's own driver log (vrserver.txt) and, in parallel,
// to %LOCALAPPDATA%\VRSpoofer\driver.log so the GUI can show the driver's view
// of the world without the user digging through Steam's log directory.
#pragma once

#include <string>

namespace vrspoof
{
	class Log
	{
	public:
		static void Init( const std::string &logFilePath );
		static void Shutdown();

		static void SetVerbose( bool verbose );
		static bool IsVerbose();

		static void Info( const char *fmt, ... );
		static void Warn( const char *fmt, ... );
		static void Error( const char *fmt, ... );
		static void Debug( const char *fmt, ... );   // suppressed unless verbose
	};

	// Absolute path of %LOCALAPPDATA%\VRSpoofer, creating it if needed.
	// Returns an empty string if the folder could not be resolved or created.
	std::string GetAppDataDir();
}
