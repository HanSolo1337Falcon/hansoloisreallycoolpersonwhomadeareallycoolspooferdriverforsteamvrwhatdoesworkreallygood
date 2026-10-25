// VirtualController.h - a controller device published by VRSpoofer itself.
//
// It registers a conventional set of input components so SteamVR's input system
// has something to bind against, but never reports any activity - the point is
// the device entry, not the tracking.
#pragma once

#include <openvr_driver.h>
#include <string>

#include "Config.h"

namespace vrspoof
{
	class VirtualController : public vr::ITrackedDeviceServerDriver
	{
	public:
		VirtualController( const DeviceSpoof &spoof, vr::ETrackedControllerRole role );

		vr::EVRInitError Activate( uint32_t unObjectId ) override;
		void Deactivate() override;
		void EnterStandby() override;
		void *GetComponent( const char *pchComponentNameAndVersion ) override;
		void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize ) override;
		vr::DriverPose_t GetPose() override;

		void RunFrame();
		void ReapplyIdentity( const DeviceSpoof &spoof );

		// Yields this hand to a real controller that has just connected.
		//
		// A virtual controller activates at driver init, long before real
		// hardware is awake, so it wins the hand role by default and SteamVR
		// then binds the whole input system to our controller type. That leaves
		// the real controller inert. Standing down drops the role claim and
		// reports the device as disconnected, so SteamVR reassigns the hand.
		void SetStoodDown( bool stoodDown );
		bool IsStoodDown() const { return m_stoodDown; }

		vr::ETrackedControllerRole Role() const { return m_role; }
		uint32_t ObjectId() const         { return m_objectId; }
		const std::string &Serial() const { return m_serial; }

	private:
		void WriteProperties();
		void CreateInputComponents();

		DeviceSpoof m_spoof;
		vr::ETrackedControllerRole m_role;
		std::string m_serial;
		bool m_stoodDown = false;

		uint32_t m_objectId = vr::k_unTrackedDeviceIndexInvalid;
		vr::PropertyContainerHandle_t m_container = vr::k_ulInvalidPropertyContainer;

		vr::VRInputComponentHandle_t m_systemClick   = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_aClick        = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_bClick        = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_triggerClick  = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_triggerValue  = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_gripValue     = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_gripClick     = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_joystickX     = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_joystickY     = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_joystickClick = vr::k_ulInvalidInputComponentHandle;
		vr::VRInputComponentHandle_t m_haptic        = vr::k_ulInvalidInputComponentHandle;
	};
}
