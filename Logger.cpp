#include "Logger.h"

#include <openvr_driver.h>

#include <windows.h>
#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <share.h>

#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "ole32.lib" )

namespace vrspoof
{
	namespace
	{
		std::mutex  g_mutex;
		FILE       *g_file = nullptr;
		bool        g_verbose = false;

		// Rolls the log over rather than growing without bound across sessions.
		const long kMaxLogBytes = 1 * 1024 * 1024;

		void WriteLine( const char *level, const char *fmt, va_list args )
		{
			char body[ 2048 ];
			vsnprintf( body, sizeof( body ), fmt, args );
			body[ sizeof( body ) - 1 ] = '\0';

			char line[ 2176 ];
			snprintf( line, sizeof( line ), "[VRSpoofer] %s: %s\n", level, body );

			// SteamVR's log. VRDriverLog() is null before the driver context is
			// initialised, which is exactly when early failures happen, so the
			// file sink below still has to work on its own.
			if ( vr::VRDriverLog() )
				vr::VRDriverLog()->Log( line );

			std::lock_guard<std::mutex> lock( g_mutex );
			if ( !g_file )
				return;

			SYSTEMTIME st;
			GetLocalTime( &st );
			fprintf( g_file, "%02d:%02d:%02d.%03d %-5s %s\n",
			         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level, body );
			fflush( g_file );
		}
	}

	std::string GetAppDataDir()
	{
		PWSTR wide = nullptr;
		if ( FAILED( SHGetKnownFolderPath( FOLDERID_LocalAppData, 0, nullptr, &wide ) ) )
			return "";

		char narrow[ MAX_PATH ] = { 0 };
		int written = WideCharToMultiByte( CP_UTF8, 0, wide, -1, narrow, MAX_PATH, nullptr, nullptr );
		CoTaskMemFree( wide );
		if ( written <= 0 )
			return "";

		std::string dir = std::string( narrow ) + "\\VRSpoofer";
		CreateDirectoryA( dir.c_str(), nullptr );

		DWORD attrs = GetFileAttributesA( dir.c_str() );
		if ( attrs == INVALID_FILE_ATTRIBUTES || !( attrs & FILE_ATTRIBUTE_DIRECTORY ) )
			return "";

		return dir;
	}

	void Log::Init( const std::string &logFilePath )
	{
		std::lock_guard<std::mutex> lock( g_mutex );
		if ( g_file || logFilePath.empty() )
			return;

		// Truncate instead of appending once the log gets large.
		WIN32_FILE_ATTRIBUTE_DATA data;
		const char *mode = "a";
		if ( GetFileAttributesExA( logFilePath.c_str(), GetFileExInfoStandard, &data ) &&
		     data.nFileSizeHigh == 0 && data.nFileSizeLow > (DWORD)kMaxLogBytes )
		{
			mode = "w";
		}

		// _fsopen with _SH_DENYNO, not fopen: the GUI tails this file while
		// SteamVR is running, and fopen takes an exclusive lock that would make
		// the log unreadable exactly when it matters most.
		g_file = _fsopen( logFilePath.c_str(), mode, _SH_DENYNO );
		if ( !g_file )
			return;

		SYSTEMTIME st;
		GetLocalTime( &st );
		fprintf( g_file, "\n===== VRSpoofer driver session %04d-%02d-%02d %02d:%02d:%02d =====\n",
		         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
		fflush( g_file );
	}

	void Log::Shutdown()
	{
		std::lock_guard<std::mutex> lock( g_mutex );
		if ( g_file )
		{
			fprintf( g_file, "===== session ended =====\n" );
			fclose( g_file );
			g_file = nullptr;
		}
	}

	void Log::SetVerbose( bool verbose ) { g_verbose = verbose; }
	bool Log::IsVerbose()                { return g_verbose; }

	void Log::Info( const char *fmt, ... )
	{
		va_list args; va_start( args, fmt );
		WriteLine( "INFO", fmt, args );
		va_end( args );
	}

	void Log::Warn( const char *fmt, ... )
	{
		va_list args; va_start( args, fmt );
		WriteLine( "WARN", fmt, args );
		va_end( args );
	}

	void Log::Error( const char *fmt, ... )
	{
		va_list args; va_start( args, fmt );
		WriteLine( "ERROR", fmt, args );
		va_end( args );
	}

	void Log::Debug( const char *fmt, ... )
	{
		if ( !g_verbose )
			return;
		va_list args; va_start( args, fmt );
		WriteLine( "DEBUG", fmt, args );
		va_end( args );
	}
}
