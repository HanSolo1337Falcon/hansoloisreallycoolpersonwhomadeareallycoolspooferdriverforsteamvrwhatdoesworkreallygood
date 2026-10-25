// ServerDriver.h - the IServerTrackedDeviceProvider vrserver.exe loads.
//
// Two jobs:
//   1. In Override mode, rewrite identity properties on the devices your real
//      headset driver already published.
//   2. In Virtual mode, publish our own HMD and controllers.
//
// It also writes %LOCALAPPDATA%\VRSpoofer\status.json every couple of seconds
// so the GUI can show what SteamVR actually has, live.
#pragma once

#include <openvr_driver.h>

#include <memory>
#include <set>
#include <string>

#include "Config.h"
#include "PropertySpoofer.h"
#include "VirtualHmd.h"
#include "VirtualController.h"

namespace vrspoof
{
	class ServerDriver : public vr::IServerTrackedDeviceProvider
	{
	public:
		static ServerDriver &Instance();

		vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext ) override;
		void Cleanup() override;
		const char *const *GetInterfaceVersions() override;
		void RunFrame() override;
		bool ShouldBlockStandbyMode() override;
		void EnterStandby() override;
		void LeaveStandby() override;

	private:
		void CreateVirtualDevices();

		// Real hardware always wins a hand. Stands our virtual controllers down
		// when Virtual mode is switched off, or when a real controller for that
		// hand is present, so they can't hold a hand role hostage.
		void UpdateVirtualControllerStandDown( bool wanted );

		void OnConfigReloaded();
		void WriteStatusFile();
		std::set<uint32_t> OwnedIndices() const;

		ConfigStore     m_configStore;
		PropertySpoofer m_spoofer;

		std::unique_ptr<VirtualHmd>        m_virtualHmd;
		std::unique_ptr<VirtualController> m_virtualLeft;
		std::unique_ptr<VirtualController> m_virtualRight;

		uint64_t m_lastConfigPollMs = 0;
		uint64_t m_lastApplyMs      = 0;
		uint64_t m_lastStatusMs     = 0;
		bool     m_initialised      = false;
	};
}
