/************************************************************************************

Filename    :   HmdSensors.cpp
Content     :   State associated with a single HMD
Created     :   January 24, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "HmdSensors.h"

#include <unistd.h>					// gettid()
#include "Android/LogUtils.h"

HMDState::HMDState() :
		SensorStarted( 0 ),
		SensorCaps( 0 ),
		SensorChangedCount( 0 ),
		LatencyTesterChangedCount( 0 )
{
}

HMDState::~HMDState()
{
	StopSensor();
	RemoveHandlerFromDevices();
}

bool HMDState::InitDevice()
{
	DeviceManager = *OVR::DeviceManager::Create();
	DeviceManager->SetMessageHandler( this );

	// Just use the first HMDDevice found.
	Device = *DeviceManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
	return ( Device != NULL );
}

bool HMDState::StartSensor( unsigned supportedCaps, unsigned requiredCaps )
{
	if ( SensorStarted )
	{
		StopSensor();
	}

	supportedCaps |= requiredCaps;

	if ( requiredCaps & ovrHmdCap_Position )
	{
		LOG( "HMDState::StartSensor: ovrHmdCap_Position not supported." );
		return false;
	}

	// Assume the first sensor found is associated with this HMDDevice.
	Sensor = *DeviceManager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
	if ( Sensor != NULL )
	{
		Sensor->SetCoordinateFrame( OVR::SensorDevice::Coord_HMD );
		Sensor->SetReportRate( 500 );
		SFusion.AttachToSensor( Sensor );
		SFusion.SetYawCorrectionEnabled( ( supportedCaps & ovrHmdCap_YawCorrection ) != 0 );

		if ( requiredCaps & ovrHmdCap_YawCorrection )
		{
			if ( !SFusion.HasMagCalibration() )
			{
				LOG( "HMDState::StartSensor: ovrHmdCap_YawCorrection not available." );
				SFusion.AttachToSensor( 0 );
				SFusion.Reset();
				Sensor.Clear();
				return false;
			}
		}
		LOG( "HMDState::StartSensor: created sensor." );
	}
	else
	{
		if ( requiredCaps & ovrHmdCap_Orientation )
		{
			LOG( "HMDState::StartSensor: ovrHmdCap_Orientation not available." );
			return false;
		}
		LOG( "HMDState::StartSensor: wait for sensor." );
	}

	SensorStarted = true;
	SensorCaps = supportedCaps;

	return true;
}

void HMDState::StopSensor()
{
	if ( SensorStarted )
	{
		SFusion.AttachToSensor( 0 );
		SFusion.Reset();
		Sensor.Clear();
		SensorCaps = 0;
		SensorStarted = false;
		LastSensorState = OVR::SensorState();
		LOG( "HMDState::StopSensor: stopped sensor.\n" );
	}
}

void HMDState::ResetSensor()
{
	SFusion.Reset();
	LastSensorState = OVR::SensorState();
}

OVR::SensorInfo	HMDState::GetSensorInfo()
{
	OVR::SensorInfo si;

	if ( Sensor != NULL )
	{
		Sensor->GetDeviceInfo( &si );
	}
	else
	{
		memset( &si, 0, sizeof( si ) );
	}
	return si;
}

float HMDState::GetYaw()
{
	return SFusion.GetYaw();
}

void HMDState::SetYaw( float yaw )
{
	SFusion.SetYaw( yaw );
}

void HMDState::RecenterYaw()
{
	SFusion.RecenterYaw();
}

// Any number of threads can fetch the predicted sensor state.
OVR::SensorState HMDState::PredictedSensorState( double absTime )
{
	// Only update when a device was added or removed because enumerating the devices is expensive.
	if ( SensorChangedCount > 0 )
	{
		// Use a mutex to keep multiple threads from trying to create a sensor simultaneously.
		SensorChangedMutex.DoLock();
		for ( int count = SensorChangedCount; count > 0; count = SensorChangedCount )
		{
			SensorChangedCount -= count;

			// Assume the first sensor found is associated with this HMDDevice.
			Sensor = *DeviceManager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
			if ( Sensor != NULL )
			{
				Sensor->SetCoordinateFrame( OVR::SensorDevice::Coord_HMD );
				Sensor->SetReportRate( 500 );
				SFusion.AttachToSensor( Sensor );
				SFusion.SetYawCorrectionEnabled( ( SensorCaps & ovrHmdCap_YawCorrection ) != 0 );

				// restore the yaw from the last sensor state when we re-connect to a sensor
				float yaw, pitch, roll;
				LastSensorState.Predicted.Transform.Orientation.GetEulerAngles< OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z >( &yaw, &pitch, &roll );
				SFusion.SetYaw( yaw );

				LOG( "HMDState::PredictedSensorState: created sensor (tid=%d)", gettid() );
			}
			else
			{
				SFusion.AttachToSensor( 0 );
				LOG( "HMDState::PredictedSensorState: wait for sensor (tid=%d)", gettid() );
			}
		}
		SensorChangedMutex.Unlock();
	}

	if ( Sensor != NULL )
	{
		// GetPredictionForTime() is thread safe.
		LastSensorState = SFusion.GetPredictionForTime( absTime );
	}
	else
	{
		// Always set valid times so frames will get a delta-time and
		// joypad operation works when the sensor isn't connected.
		LastSensorState.Recorded.TimeInSeconds = absTime;
		LastSensorState.Predicted.TimeInSeconds = absTime;
	}

	return LastSensorState;
}

// Only one thread can use the latency tester.
bool HMDState::ProcessLatencyTest( unsigned char rgbColorOut[3] )
{
	// Only update when a device was added or removed because enumerating the devices is expensive.
	if ( LatencyTesterChangedCount > 0 )
	{
		// Use a mutex to keep multiple threads from trying to create a latency tester simultaneously.
		LatencyTesterChangedMutex.DoLock();
		for ( int count = LatencyTesterChangedCount; count > 0; count = LatencyTesterChangedCount )
		{
			LatencyTesterChangedCount -= count;

			// Assume the first latency tester found is associated with this HMDDevice.
			LatencyTester = *DeviceManager->EnumerateDevices<OVR::LatencyTestDevice>().CreateDevice();
			if ( LatencyTester != NULL )
			{
				LatencyUtil.SetDevice( LatencyTester );
				LOG( "HMDState::ProcessLatencyTest: created latency tester (tid=%d)", gettid() );
			}
			else
			{
				LatencyUtil.SetDevice( 0 );
				LOG( "HMDState::ProcessLatencyTest: wait for latency tester (tid=%d)", gettid() );
			}
		}
		LatencyTesterChangedMutex.Unlock();
	}

	bool result = false;

	if ( LatencyTester != NULL )
	{
		OVR::Color colorToDisplay;

		// NOTE: ProcessInputs() is not thread safe.
		LatencyUtil.ProcessInputs();
		result = LatencyUtil.DisplayScreenColor( colorToDisplay );
		rgbColorOut[0] = colorToDisplay.R;
		rgbColorOut[1] = colorToDisplay.G;
		rgbColorOut[2] = colorToDisplay.B;
	}

	return result;
}

// OnMessage() is called from the device manager thread.
void HMDState::OnMessage( const OVR::Message & msg )
{
	if ( msg.pDevice != DeviceManager )
	{
		return;
	}

	const OVR::MessageDeviceStatus & statusMsg = static_cast<const OVR::MessageDeviceStatus &>(msg);

	if ( statusMsg.Handle.GetType() == OVR::Device_Sensor )
	{
		SensorChangedCount++;
		if ( msg.Type == OVR::Message_DeviceAdded )
		{
			LOG( "HMDState::OnMessage: added Device_Sensor (tid=%d, cnt=%d)", gettid(), SensorChangedCount );
		}
		else if ( msg.Type == OVR::Message_DeviceRemoved )
		{
			LOG( "HMDState::OnMessage: removed Device_Sensor (tid=%d, cnt=%d)", gettid(), SensorChangedCount );
		}
	}
	else if ( statusMsg.Handle.GetType() == OVR::Device_LatencyTester )
	{
		LatencyTesterChangedCount++;
		if ( msg.Type == OVR::Message_DeviceAdded )
		{
			LOG( "HMDState::OnMessage: added Device_LatencyTester (tid=%d, cnt=%d)", gettid(), LatencyTesterChangedCount );
		}
		else if ( msg.Type == OVR::Message_DeviceRemoved )
		{
			LOG( "HMDState::OnMessage: removed Device_LatencyTester (tid=%d, cnt=%d)", gettid(), LatencyTesterChangedCount );
		}
	}
}
