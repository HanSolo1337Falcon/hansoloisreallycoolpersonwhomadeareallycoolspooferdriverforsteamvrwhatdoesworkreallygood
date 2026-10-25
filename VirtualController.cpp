#include "VirtualController.h"
#include "Logger.h"

#include <cstring>

namespace vrspoof
{
	namespace
	{
		void SetStr( vr::PropertyContainerHandle_t c, vr::ETrackedDeviceProperty prop, const std::string &v )
		{
			if ( !v.empty() )
				vr::VRProperties()->SetStringProperty( c, prop, v.c_str() );
		}

		vr::HmdQuaternion_t Identity()
		{
			vr::HmdQuaternion_t q;
			q.w = 1.0; q.x = 0.0; q.y = 0.0; q.z = 0.0;
			return q;
		}
	}

	VirtualController::VirtualController( const DeviceSpoof &spoof, vr::ETrackedControllerRole role )
		: m_spoof( spoof )
		, m_role( role )
	{
		if ( !spoof.serialNumber.empty() )
			m_serial = spoof.serialNumber;
		else
			m_serial = ( role == vr::TrackedControllerRole_LeftHand ) ? "VRSPOOF-CTRL-L" : "VRSPOOF-CTRL-R";
	}

	vr::EVRInitError VirtualController::Activate( uint32_t unObjectId )
	{
		m_objectId  = unObjectId;
		m_container = vr::VRPropertiesRaw()->TrackedDeviceToPropertyContainer( m_objectId );

		if ( m_container == vr::k_ulInvalidPropertyContainer )
		{
			Log::Error( "virtual controller activated but has no property container" );
			return vr::VRInitError_Driver_Failed;
		}

		WriteProperties();
		CreateInputComponents();

		Log::Info( "virtual %s controller active at index %u as '%s %s'",
		           m_role == vr::TrackedControllerRole_LeftHand ? "left" : "right",
		           unObjectId, m_spoof.manufacturer.c_str(), m_spoof.modelNumber.c_str() );
		return vr::VRInitError_None;
	}

	void VirtualController::WriteProperties()
	{
		vr::PropertyContainerHandle_t c = m_container;

		SetStr( c, vr::Prop_ManufacturerName_String,     m_spoof.manufacturer );
		SetStr( c, vr::Prop_ModelNumber_String,          m_spoof.modelNumber );
		SetStr( c, vr::Prop_SerialNumber_String,         m_serial );
		SetStr( c, vr::Prop_RenderModelName_String,      m_spoof.renderModel );
		SetStr( c, vr::Prop_TrackingSystemName_String,   m_spoof.trackingSystem );
		SetStr( c, vr::Prop_RegisteredDeviceType_String, m_spoof.registeredDeviceType );
		SetStr( c, vr::Prop_ControllerType_String,       m_spoof.controllerType );
		SetStr( c, vr::Prop_HardwareRevision_String,     m_spoof.hardwareRevision );
		SetStr( c, vr::Prop_DriverVersion_String,        m_spoof.driverVersion );

		// Without an input profile SteamVR logs binding errors for the device.
		if ( m_spoof.inputProfilePath.empty() )
			vr::VRProperties()->SetStringProperty( c, vr::Prop_InputProfilePath_String,
			                                       "{vrspoofer}/input/vrspoofer_controller_profile.json" );
		else
			SetStr( c, vr::Prop_InputProfilePath_String, m_spoof.inputProfilePath );

		SetStr( c, vr::Prop_NamedIconPathDeviceOff_String,            m_spoof.icons.off );
		SetStr( c, vr::Prop_NamedIconPathDeviceSearching_String,      m_spoof.icons.searching );
		SetStr( c, vr::Prop_NamedIconPathDeviceSearchingAlert_String, m_spoof.icons.searchingAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceReady_String,          m_spoof.icons.ready );
		SetStr( c, vr::Prop_NamedIconPathDeviceReadyAlert_String,     m_spoof.icons.readyAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceNotReady_String,       m_spoof.icons.notReady );
		SetStr( c, vr::Prop_NamedIconPathDeviceStandby_String,        m_spoof.icons.standby );
		SetStr( c, vr::Prop_NamedIconPathDeviceStandbyAlert_String,   m_spoof.icons.standbyAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceAlertLow_String,       m_spoof.icons.alertLow );

		// While stood down we must not claim a hand, or SteamVR keeps routing
		// that hand's input to us instead of the real controller.
		vr::VRProperties()->SetInt32Property( c, vr::Prop_ControllerRoleHint_Int32,
		                                      m_stoodDown ? vr::TrackedControllerRole_Invalid : m_role );
		vr::VRProperties()->SetInt32Property( c, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller );

		for ( const auto &kv : m_spoof.extraStrings )
			vr::VRProperties()->SetStringProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second.c_str() );
		for ( const auto &kv : m_spoof.extraFloats )
			vr::VRProperties()->SetFloatProperty( c, (vr::ETrackedDeviceProperty)kv.first, (float)kv.second );
		for ( const auto &kv : m_spoof.extraInts )
			vr::VRProperties()->SetInt32Property( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
		for ( const auto &kv : m_spoof.extraBools )
			vr::VRProperties()->SetBoolProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
	}

	void VirtualController::CreateInputComponents()
	{
		vr::IVRDriverInput *input = vr::VRDriverInput();
		if ( !input )
			return;

		input->CreateBooleanComponent( m_container, "/input/system/click",    &m_systemClick );
		input->CreateBooleanComponent( m_container, "/input/a/click",         &m_aClick );
		input->CreateBooleanComponent( m_container, "/input/b/click",         &m_bClick );
		input->CreateBooleanComponent( m_container, "/input/trigger/click",   &m_triggerClick );
		input->CreateBooleanComponent( m_container, "/input/grip/click",      &m_gripClick );
		input->CreateBooleanComponent( m_container, "/input/joystick/click",  &m_joystickClick );

		input->CreateScalarComponent( m_container, "/input/trigger/value", &m_triggerValue,
		                              vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided );
		input->CreateScalarComponent( m_container, "/input/grip/value", &m_gripValue,
		                              vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided );
		input->CreateScalarComponent( m_container, "/input/joystick/x", &m_joystickX,
		                              vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided );
		input->CreateScalarComponent( m_container, "/input/joystick/y", &m_joystickY,
		                              vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided );

		input->CreateHapticComponent( m_container, "/output/haptic", &m_haptic );
	}

	void VirtualController::ReapplyIdentity( const DeviceSpoof &spoof )
	{
		if ( m_container == vr::k_ulInvalidPropertyContainer )
			return;

		const std::string keepSerial = m_serial;
		m_spoof = spoof;
		m_serial = keepSerial;

		WriteProperties();
		Log::Info( "virtual %s controller relabelled to '%s %s'",
		           m_role == vr::TrackedControllerRole_LeftHand ? "left" : "right",
		           m_spoof.manufacturer.c_str(), m_spoof.modelNumber.c_str() );
	}

	void VirtualController::Deactivate()
	{
		m_objectId  = vr::k_unTrackedDeviceIndexInvalid;
		m_container = vr::k_ulInvalidPropertyContainer;
	}

	void VirtualController::EnterStandby() {}

	void *VirtualController::GetComponent( const char * /*pchComponentNameAndVersion*/ )
	{
		return nullptr;   // no display or direct-mode components on a controller
	}

	void VirtualController::DebugRequest( const char * /*pchRequest*/, char *pchResponseBuffer, uint32_t unResponseBufferSize )
	{
		if ( pchResponseBuffer && unResponseBufferSize > 0 )
			pchResponseBuffer[ 0 ] = '\0';
	}

	void VirtualController::SetStoodDown( bool stoodDown )
	{
		if ( m_stoodDown == stoodDown )
			return;

		m_stoodDown = stoodDown;

		if ( m_container != vr::k_ulInvalidPropertyContainer )
			WriteProperties();

		Log::Info( "virtual %s controller %s (real hardware %s)",
		           m_role == vr::TrackedControllerRole_LeftHand ? "left" : "right",
		           stoodDown ? "stood down" : "resumed",
		           stoodDown ? "connected" : "gone" );
	}

	vr::DriverPose_t VirtualController::GetPose()
	{
		vr::DriverPose_t pose = { 0 };

		// Reporting disconnected is what actually makes SteamVR stop treating
		// this as the hand's controller; clearing the role hint alone is not
		// enough once a binding has been established.
		pose.poseIsValid       = !m_stoodDown;
		pose.result            = m_stoodDown ? vr::TrackingResult_Uninitialized
		                                     : vr::TrackingResult_Running_OK;
		pose.deviceIsConnected = !m_stoodDown;
		pose.qWorldFromDriverRotation = Identity();
		pose.qDriverFromHeadRotation  = Identity();
		pose.qRotation                = Identity();

		// Park the controllers to either side of the origin so they are not all
		// stacked on top of each other in the room view.
		pose.vecPosition[ 0 ] = ( m_role == vr::TrackedControllerRole_LeftHand ) ? -0.2 : 0.2;
		pose.vecPosition[ 1 ] = -0.2;
		pose.vecPosition[ 2 ] = -0.3;

		return pose;
	}

	void VirtualController::RunFrame()
	{
		if ( m_objectId == vr::k_unTrackedDeviceIndexInvalid )
			return;

		vr::DriverPose_t pose = GetPose();
		vr::VRServerDriverHost()->TrackedDevicePoseUpdated( m_objectId, pose, sizeof( vr::DriverPose_t ) );
	}
}
