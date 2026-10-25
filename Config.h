// Config.h - the driver's view of %LOCALAPPDATA%\VRSpoofer\config.json.
//
// The GUI owns this file; the driver only reads it and polls its timestamp so
// that pressing Apply in the GUI takes effect without restarting SteamVR.
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace vrspoof
{
	enum class SpoofMode
	{
		Off,        // leave these devices completely alone
		Override,   // rewrite properties on the real devices SteamVR already has
		Virtual     // publish our own fake devices
	};

	// The eight status icons SteamVR shows for a device, plus the standby-alert
	// variant. Empty strings are left untouched on the target device.
	struct IconSet
	{
		std::string off;
		std::string searching;
		std::string searchingAlert;
		std::string ready;
		std::string readyAlert;
		std::string notReady;
		std::string standby;
		std::string standbyAlert;
		std::string alertLow;

		bool Empty() const
		{
			return off.empty() && searching.empty() && searchingAlert.empty() &&
			       ready.empty() && readyAlert.empty() && notReady.empty() &&
			       standby.empty() && standbyAlert.empty() && alertLow.empty();
		}
	};

	// One device's worth of spoofed identity. Empty strings mean "don't touch".
	struct DeviceSpoof
	{
		bool enabled = false;

		std::string manufacturer;
		std::string modelNumber;
		std::string serialNumber;
		std::string renderModel;
		std::string trackingSystem;
		std::string registeredDeviceType;
		std::string controllerType;
		std::string inputProfilePath;
		std::string hardwareRevision;
		std::string driverVersion;

		// Serial numbers are permanently bound to a device index once SteamVR
		// has seen them, so rewriting one on a live device is opt-in. It is
		// always applied to virtual devices, where we own the serial.
		bool spoofSerial = false;

		// Prop_ControllerType_String and Prop_InputProfilePath_String choose
		// which binding file SteamVR loads for a device. Writing them onto real
		// hardware whose driver publishes differently-shaped input components
		// silently breaks bindings - dead buttons, and no dashboard laser,
		// because the pointer pose comes from the profile. Off by default; only
		// meaningful in Override mode (virtual devices always declare their own).
		bool spoofBindings = false;

		IconSet icons;

		// Escape hatch: raw ETrackedDeviceProperty id -> value, for anything the
		// GUI's structured fields don't cover.
		std::map<int, std::string> extraStrings;
		std::map<int, double>      extraFloats;
		std::map<int, int32_t>     extraInts;
		std::map<int, bool>        extraBools;
	};

	// Display characteristics used only by the virtual HMD.
	struct VirtualDisplay
	{
		int32_t  windowX = 0;
		int32_t  windowY = 0;
		uint32_t windowWidth  = 2064;
		uint32_t windowHeight = 2208;
		uint32_t renderWidth  = 2064;
		uint32_t renderHeight = 2208;
		float    displayFrequency = 90.0f;
		float    secondsFromVsyncToPhotons = 0.011f;
		float    ipd = 0.063f;
		bool     isOnDesktop = false;
	};

	struct Config
	{
		bool   masterEnabled = true;
		bool   verboseLog = false;
		double applyIntervalSeconds = 1.0;

		SpoofMode hmdMode        = SpoofMode::Off;
		SpoofMode controllerMode = SpoofMode::Off;

		DeviceSpoof hmd;
		DeviceSpoof left;
		DeviceSpoof right;

		VirtualDisplay display;
	};

	// Owns the on-disk config and reloads it when it changes underneath us.
	class ConfigStore
	{
	public:
		// Resolves the config path and does a first load. Missing or malformed
		// files are not an error - the driver runs with everything Off.
		bool Load();

		// Cheap timestamp check; returns true when a fresh config was read in.
		bool ReloadIfChanged();

		const Config &Get() const  { return m_config; }
		const std::string &Path() const { return m_path; }

		// Human-readable reason the last load failed, or "" when it succeeded.
		const std::string &LastError() const { return m_lastError; }

	private:
		bool ReadFile( std::string &out ) const;
		bool ParseInto( const std::string &text, Config &out ) const;

		Config      m_config;
		std::string m_path;
		std::string m_lastError;
		uint64_t    m_lastWriteTime = 0;
		uint64_t    m_lastSize = 0;
	};
}
