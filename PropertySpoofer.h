// PropertySpoofer.h - rewrites identity properties on devices that already
// exist in SteamVR (the ones your real headset driver published).
//
// SteamVR hands every driver an IVRProperties, and TrackedDeviceToPropertyContainer
// resolves any device index - not just our own - so a driver that publishes no
// devices of its own can still relabel what the status window shows.
#pragma once

#include <openvr_driver.h>

#include <map>
#include <set>
#include <string>

#include "Config.h"

namespace vrspoof
{
	class PropertySpoofer
	{
	public:
		// Walks every live device and applies the matching spoof block. Devices
		// listed in ownedIndices are ones we published ourselves; they are
		// configured at activation time and skipped here.
		void Apply( const Config &cfg, const std::set<uint32_t> &ownedIndices );

		// Forget what we've written so the next Apply rewrites unconditionally.
		// Called when the config file changes.
		void Invalidate();

		int DevicesTouchedLastPass() const { return m_touchedLastPass; }

	private:
		// Writes every non-empty field of spoof onto the container.
		void WriteDevice( vr::PropertyContainerHandle_t container,
		                  const DeviceSpoof &spoof,
		                  bool allowSerialRewrite );

		// True when the device still carries what we last wrote, meaning the
		// owning driver hasn't clobbered it and we can skip the write.
		bool AlreadyCurrent( uint32_t index,
		                     vr::PropertyContainerHandle_t container,
		                     const DeviceSpoof &spoof );

		static std::string Signature( const DeviceSpoof &spoof );

		std::map<uint32_t, std::string> m_signatures;
		int m_touchedLastPass = 0;
	};
}
