#include "VirtualHmd.h"
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

	VirtualHmd::VirtualHmd( const DeviceSpoof &spoof, const VirtualDisplay &display )
		: m_spoof( spoof )
		, m_display( display )
	{
		m_serial = spoof.serialNumber.empty() ? std::string( "VRSPOOF-HMD-0001" ) : spoof.serialNumber;
	}

	vr::EVRInitError VirtualHmd::Activate( uint32_t unObjectId )
	{
		m_objectId  = unObjectId;
		m_container = vr::VRPropertiesRaw()->TrackedDeviceToPropertyContainer( m_objectId );

		if ( m_container == vr::k_ulInvalidPropertyContainer )
		{
			Log::Error( "virtual HMD activated but has no property container" );
			return vr::VRInitError_Driver_Failed;
		}

		WriteProperties();
		Log::Info( "virtual HMD active at index %u as '%s %s'", unObjectId,
		           m_spoof.manufacturer.c_str(), m_spoof.modelNumber.c_str() );
		return vr::VRInitError_None;
	}

	void VirtualHmd::WriteProperties()
	{
		vr::PropertyContainerHandle_t c = m_container;

		SetStr( c, vr::Prop_ManufacturerName_String,     m_spoof.manufacturer );
		SetStr( c, vr::Prop_ModelNumber_String,          m_spoof.modelNumber );
		SetStr( c, vr::Prop_SerialNumber_String,         m_serial );
		SetStr( c, vr::Prop_RenderModelName_String,      m_spoof.renderModel );
		SetStr( c, vr::Prop_TrackingSystemName_String,   m_spoof.trackingSystem );
		SetStr( c, vr::Prop_RegisteredDeviceType_String, m_spoof.registeredDeviceType );
		SetStr( c, vr::Prop_HardwareRevision_String,     m_spoof.hardwareRevision );
		SetStr( c, vr::Prop_DriverVersion_String,        m_spoof.driverVersion );

		SetStr( c, vr::Prop_NamedIconPathDeviceOff_String,            m_spoof.icons.off );
		SetStr( c, vr::Prop_NamedIconPathDeviceSearching_String,      m_spoof.icons.searching );
		SetStr( c, vr::Prop_NamedIconPathDeviceSearchingAlert_String, m_spoof.icons.searchingAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceReady_String,          m_spoof.icons.ready );
		SetStr( c, vr::Prop_NamedIconPathDeviceReadyAlert_String,     m_spoof.icons.readyAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceNotReady_String,       m_spoof.icons.notReady );
		SetStr( c, vr::Prop_NamedIconPathDeviceStandby_String,        m_spoof.icons.standby );
		SetStr( c, vr::Prop_NamedIconPathDeviceStandbyAlert_String,   m_spoof.icons.standbyAlert );
		SetStr( c, vr::Prop_NamedIconPathDeviceAlertLow_String,       m_spoof.icons.alertLow );

		vr::VRProperties()->SetFloatProperty( c, vr::Prop_UserIpdMeters_Float, m_display.ipd );
		vr::VRProperties()->SetFloatProperty( c, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.0f );
		vr::VRProperties()->SetFloatProperty( c, vr::Prop_DisplayFrequency_Float, m_display.displayFrequency );
		vr::VRProperties()->SetFloatProperty( c, vr::Prop_SecondsFromVsyncToPhotons_Float, m_display.secondsFromVsyncToPhotons );
		vr::VRProperties()->SetBoolProperty( c, vr::Prop_IsOnDesktop_Bool, m_display.isOnDesktop );
		vr::VRProperties()->SetBoolProperty( c, vr::Prop_HasDisplayComponent_Bool, true );

		// Any non-zero universe works; it just has to be stable across runs so
		// SteamVR keeps the same room setup for this device.
		vr::VRProperties()->SetUint64Property( c, vr::Prop_CurrentUniverseId_Uint64, 2 );

		for ( const auto &kv : m_spoof.extraStrings )
			vr::VRProperties()->SetStringProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second.c_str() );
		for ( const auto &kv : m_spoof.extraFloats )
			vr::VRProperties()->SetFloatProperty( c, (vr::ETrackedDeviceProperty)kv.first, (float)kv.second );
		for ( const auto &kv : m_spoof.extraInts )
			vr::VRProperties()->SetInt32Property( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
		for ( const auto &kv : m_spoof.extraBools )
			vr::VRProperties()->SetBoolProperty( c, (vr::ETrackedDeviceProperty)kv.first, kv.second );
	}

	void VirtualHmd::ReapplyIdentity( const DeviceSpoof &spoof )
	{
		if ( m_container == vr::k_ulInvalidPropertyContainer )
			return;

		// The serial is bound to the device index for the life of the session.
		const std::string keepSerial = m_serial;
		m_spoof = spoof;
		m_serial = keepSerial;

		WriteProperties();
		Log::Info( "virtual HMD relabelled to '%s %s'",
		           m_spoof.manufacturer.c_str(), m_spoof.modelNumber.c_str() );
	}

	void VirtualHmd::Deactivate()
	{
		Log::Info( "virtual HMD deactivated" );
		m_objectId  = vr::k_unTrackedDeviceIndexInvalid;
		m_container = vr::k_ulInvalidPropertyContainer;
	}

	void VirtualHmd::EnterStandby() {}

	void *VirtualHmd::GetComponent( const char *pchComponentNameAndVersion )
	{
		if ( pchComponentNameAndVersion &&
		     strcmp( pchComponentNameAndVersion, vr::IVRDisplayComponent_Version ) == 0 )
		{
			return static_cast<vr::IVRDisplayComponent *>( this );
		}
		return nullptr;
	}

	void VirtualHmd::DebugRequest( const char * /*pchRequest*/, char *pchResponseBuffer, uint32_t unResponseBufferSize )
	{
		if ( pchResponseBuffer && unResponseBufferSize > 0 )
			pchResponseBuffer[ 0 ] = '\0';
	}

	vr::DriverPose_t VirtualHmd::GetPose()
	{
		vr::DriverPose_t pose = { 0 };
		pose.poseIsValid       = true;
		pose.result            = vr::TrackingResult_Running_OK;
		pose.deviceIsConnected = true;
		pose.qWorldFromDriverRotation = Identity();
		pose.qDriverFromHeadRotation  = Identity();
		pose.qRotation                = Identity();
		return pose;
	}

	void VirtualHmd::RunFrame()
	{
		if ( m_objectId == vr::k_unTrackedDeviceIndexInvalid )
			return;

		vr::DriverPose_t pose = GetPose();
		vr::VRServerDriverHost()->TrackedDevicePoseUpdated( m_objectId, pose, sizeof( vr::DriverPose_t ) );
	}

	// ---- IVRDisplayComponent -------------------------------------------------

	void VirtualHmd::GetWindowBounds( int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight )
	{
		*pnX      = m_display.windowX;
		*pnY      = m_display.windowY;
		*pnWidth  = m_display.windowWidth;
		*pnHeight = m_display.windowHeight;
	}

	bool VirtualHmd::IsDisplayOnDesktop()    { return m_display.isOnDesktop; }
	bool VirtualHmd::IsDisplayRealDisplay()  { return false; }

	void VirtualHmd::GetRecommendedRenderTargetSize( uint32_t *pnWidth, uint32_t *pnHeight )
	{
		*pnWidth  = m_display.renderWidth;
		*pnHeight = m_display.renderHeight;
	}

	void VirtualHmd::GetEyeOutputViewport( vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight )
	{
		*pnY      = 0;
		*pnWidth  = m_display.windowWidth / 2;
		*pnHeight = m_display.windowHeight;
		*pnX      = ( eEye == vr::Eye_Left ) ? 0 : m_display.windowWidth / 2;
	}

	void VirtualHmd::GetProjectionRaw( vr::EVREye /*eEye*/, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom )
	{
		// Tangents of the half-angles; a symmetric ~90 degree frustum.
		*pfLeft   = -1.0f;
		*pfRight  =  1.0f;
		*pfTop    = -1.0f;
		*pfBottom =  1.0f;
	}

	vr::DistortionCoordinates_t VirtualHmd::ComputeDistortion( vr::EVREye /*eEye*/, float fU, float fV )
	{
		vr::DistortionCoordinates_t coords;
		coords.rfRed[ 0 ]   = fU; coords.rfRed[ 1 ]   = fV;
		coords.rfGreen[ 0 ] = fU; coords.rfGreen[ 1 ] = fV;
		coords.rfBlue[ 0 ]  = fU; coords.rfBlue[ 1 ]  = fV;
		return coords;
	}

	bool VirtualHmd::ComputeInverseDistortion( vr::HmdVector2_t *pResult, vr::EVREye /*eEye*/, uint32_t /*unChannel*/, float fU, float fV )
	{
		if ( !pResult )
			return false;
		pResult->v[ 0 ] = fU;   // distortion is identity, so its inverse is too
		pResult->v[ 1 ] = fV;
		return true;
	}
}
