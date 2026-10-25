// VirtualHmd.h - a headset device published by VRSpoofer itself.
//
// Used when there is no real HMD to relabel (or when you want a clean device
// entry in the status window). It reports a display but never presents to one,
// so SteamVR treats it much like the stock null driver.
#pragma once

#include <openvr_driver.h>
#include <string>

#include "Config.h"

namespace vrspoof
{
	class VirtualHmd : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent
	{
	public:
		VirtualHmd( const DeviceSpoof &spoof, const VirtualDisplay &display );

		// ITrackedDeviceServerDriver
		vr::EVRInitError Activate( uint32_t unObjectId ) override;
		void Deactivate() override;
		void EnterStandby() override;
		void *GetComponent( const char *pchComponentNameAndVersion ) override;
		void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize ) override;
		vr::DriverPose_t GetPose() override;

		// IVRDisplayComponent
		void GetWindowBounds( int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight ) override;
		bool IsDisplayOnDesktop() override;
		bool IsDisplayRealDisplay() override;
		void GetRecommendedRenderTargetSize( uint32_t *pnWidth, uint32_t *pnHeight ) override;
		void GetEyeOutputViewport( vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight ) override;
		void GetProjectionRaw( vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom ) override;
		vr::DistortionCoordinates_t ComputeDistortion( vr::EVREye eEye, float fU, float fV ) override;
		bool ComputeInverseDistortion( vr::HmdVector2_t *pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV ) override;

		// Called from the server driver's RunFrame.
		void RunFrame();

		// Re-applies identity properties after the config file changes. Display
		// geometry is deliberately not re-applied live - SteamVR caches it at
		// activation, so those edits need a SteamVR restart.
		void ReapplyIdentity( const DeviceSpoof &spoof );

		uint32_t ObjectId() const           { return m_objectId; }
		const std::string &Serial() const   { return m_serial; }

	private:
		void WriteProperties();

		DeviceSpoof    m_spoof;
		VirtualDisplay m_display;
		std::string    m_serial;

		uint32_t m_objectId = vr::k_unTrackedDeviceIndexInvalid;
		vr::PropertyContainerHandle_t m_container = vr::k_ulInvalidPropertyContainer;
	};
}
