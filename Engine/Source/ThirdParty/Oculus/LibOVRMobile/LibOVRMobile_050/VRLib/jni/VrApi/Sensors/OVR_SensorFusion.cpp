/************************************************************************************

Filename    :   OVR_SensorFusion.cpp
Content     :   Methods that determine head orientation from sensor data over time
Created     :   October 9, 2012
Authors     :   Michael Antonov, Steve LaValle, Dov Katz, Max Katsev, Dan Gierl

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_SensorFusion.h"

#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_JSON.h"


//#define YAW_LOGGING

namespace OVR {


//-------------------------------------------------------------------------------------
// ***** Sensor Fusion

SensorFusion::SensorFusion(SensorDevice* sensor) :
	ApplyDrift(false),
	FAccelHeadset(1000),
	FAngV(20),
	MotionTrackingEnabled(true),
	EnableGravity(true),
	EnableYawCorrection(true),
	FocusDirection(Vector3f(0, 0, 0)),
	FocusFOV(0.0),
	RecenterTransform()
{
   OVR_DEBUG_LOG(("SensorFusion::SensorFusion"));

   pHandler = new BodyFrameHandler(this);

   if (sensor) {
	   AttachToSensor(sensor);
   }

   Reset();
}

SensorFusion::~SensorFusion()
{
	delete pHandler;
}


bool SensorFusion::AttachToSensor(SensorDevice* sensor)
{
    if (sensor != NULL)
    {
        MessageHandler* pCurrentHandler = sensor->GetMessageHandler();

        if (pCurrentHandler == pHandler)
        {
            Reset();
            return true;
        }

        if (pCurrentHandler != NULL)
        {
            OVR_DEBUG_LOG(
                ("SensorFusion::AttachToSensor failed - sensor %p already has handler", sensor));
            return false;
        }
    }

    if (pHandler->IsHandlerInstalled())
    {
        pHandler->RemoveHandlerFromDevices();
    }

    if (sensor != NULL)
    {
        sensor->SetMessageHandler(pHandler);
    }

    Reset();
    return true;
}

void SensorFusion::Reset()
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

    UpdatedState.SetState(StateForPrediction());
    State = PoseStatef();
    Stage = 0;

    MagRefs.Clear();
    MagRefIdx = -1;
    MagCorrectionIntegralTerm = 0.0f;
	MagLatencyCompBufferIndex = 0;
	MagLatencyCompFillCount = 0;
	YawCorrectionTimer = 0.0f;
	
    FAccelHeadset.Clear();
    FAngV.Clear();
}

float SensorFusion::GetYaw()
{
	// get the current state
	const StateForPrediction state = UpdatedState.GetState();

	// get the yaw in the current state
	float yaw, pitch, roll;
	state.State.Transform.Orientation.GetEulerAngles< Axis_Y, Axis_X, Axis_Z >( &yaw, &pitch, &roll );

	return yaw;
}

void SensorFusion::SetYaw( float newYaw )
{
	// get the current state
	const StateForPrediction state = UpdatedState.GetState();

	// get the yaw in the current state
	float yaw, pitch, roll;
	state.State.Transform.Orientation.GetEulerAngles< Axis_Y, Axis_X, Axis_Z >( &yaw, &pitch, &roll );

	// get the pose that adjusts the yaw
	Posef yawAdjustment( Quatf( Axis_Y, newYaw - yaw ), Vector3f( 0.0f ) );

	// To allow SetYaw() to be called from multiple threads we need a mutex
	// because LocklessUpdater is only safe for single producer cases.
	RecenterMutex.DoLock();
	RecenterTransform.SetState( yawAdjustment );
	RecenterMutex.Unlock();
}

void SensorFusion::RecenterYaw()
{
	// get the current state
	const StateForPrediction state = UpdatedState.GetState();

	// get the yaw in the current state
	float yaw, pitch, roll;
	state.State.Transform.Orientation.GetEulerAngles< Axis_Y, Axis_X, Axis_Z >( &yaw, &pitch, &roll );

	// get the pose that adjusts the yaw
	Posef yawAdjustment( Quatf( Axis_Y, -yaw ), Vector3f( 0.0f ) );

	// To allow RecenterYaw() to be called from multiple threads we need a mutex
	// because LocklessUpdater is only safe for single producer cases.
	RecenterMutex.DoLock();
	RecenterTransform.SetState( yawAdjustment );
	RecenterMutex.Unlock();
}

void SensorFusion::handleMessage(const MessageBodyFrame& msg)
{
    if (msg.Type != Message_BodyFrame || !IsMotionTrackingEnabled())
        return;

    if (msg.Acceleration == Vector3f::ZERO)
    	return;

    // Put the sensor readings into convenient local variables
    Vector3f gyro(msg.RotationRate);
    Vector3f accel(msg.Acceleration);
    Vector3f mag(msg.MagneticField);
    Vector3f magBias(msg.MagneticBias);
    float DeltaT = msg.TimeDelta;

    // Keep track of time
    State.TimeInSeconds = msg.AbsoluteTimeSeconds;
    Stage++;

    // Insert current sensor data into filter history
    FAngV.PushBack(gyro);
    FAccelHeadset.Update(accel, DeltaT, Quatf(gyro, gyro.Length() * DeltaT));

    // Process raw inputs
    State.AngularVelocity = gyro;
    State.LinearAcceleration = State.Transform.Orientation.Rotate(accel) - Vector3f(0, 9.8f, 0);

    // Update headset orientation
    float angle = gyro.Length() * DeltaT;
    if (angle > 0)
        State.Transform.Orientation = State.Transform.Orientation * Quatf(gyro, angle);
    // Tilt correction based on accelerometer
    if (EnableGravity)
        applyTiltCorrection(DeltaT);
    // Yaw correction based on magnetometer
	if (EnableYawCorrection && HasMagCalibration())
		applyMagYawCorrection(mag, magBias, gyro, DeltaT);
/*
	// Focus Correction
	if ((FocusDirection.x != 0.0f || FocusDirection.z != 0.0f) && FocusFOV < Mathf::Pi)
		applyFocusCorrection(DeltaT);
*/
    // The quaternion magnitude may slowly drift due to numerical error,
    // so it is periodically normalized.
    if ((Stage & 0xFF) == 0)
        State.Transform.Orientation.Normalize();

    // Update headset position
    {
    	EnableGravity = true;
    	EnableYawCorrection = true;

    	// TBD apply neck model here
    	State.LinearVelocity.x = State.LinearVelocity.y = State.LinearVelocity.z = 0.0f;
    	State.Transform.Position.x = State.Transform.Position.y = State.Transform.Position.z = 0.0f;
    }

    // Compute the angular acceleration
    State.AngularAcceleration = (FAngV.GetSize() >= 12 && DeltaT > 0) ?
        (FAngV.SavitzkyGolayDerivative12() / DeltaT) : Vector3f();

    // Store the lockless state.
    StateForPrediction state;
    state.State = State;
    state.Temperature = msg.Temperature;
    UpdatedState.SetState(state);
}

// These two functions need to be moved into Quat class
// Compute a rotation required to Pose "from" into "to".
Quatf vectorAlignmentRotation(const Vector3f &from, const Vector3f &to)
{
    Vector3f axis = from.Cross(to);
    if (axis.LengthSq() == 0)
        // this handles both collinear and zero-length input cases
        return Quatf();
    float angle = from.Angle(to);
    return Quatf(axis, angle);
}

bool SensorFusion::getBufferedOrientation(Quatf* orientation, const Vector3f& gyro, float gyroThreshold, float deltaT)
{
	
	MagLatencyCompBuffer[MagLatencyCompBufferIndex].Orientation = State.Transform.Orientation;
	MagLatencyCompBuffer[MagLatencyCompBufferIndex].GyroMagnitude = gyro.Length();
	MagLatencyCompBufferIndex++;
	if (MagLatencyCompBufferIndex >= MagLatencyBufferSizeMax)
	{
		MagLatencyCompBufferIndex = 0;
	}


	// Determine how far to look back in buffer.
	int backDist = (int) ((float) MagLatencyCompensationMilliseconds / (1000.0f * deltaT));
	
	backDist = Alg::Min(backDist, MagLatencyBufferSizeMax-1);


	if (MagLatencyCompFillCount < MagLatencyBufferSizeMax)
	{
		MagLatencyCompFillCount++;
	}

	if (MagLatencyCompFillCount <= backDist)
	{
		// Haven't buffered enough yet.
#ifdef YAW_LOGGING
		LogText("YAW - Not buffered enough orientation values.\n");
#endif
		return false;
	}

		
	int readIndex = MagLatencyCompBufferIndex - backDist;
	if (readIndex < 0)
	{
		readIndex += MagLatencyBufferSizeMax;
	}

	*orientation = MagLatencyCompBuffer[readIndex].Orientation;

	// Check to see if the angular velocity was too high.
	float gyroMagnitude = MagLatencyCompBuffer[readIndex].GyroMagnitude;
	if (gyroMagnitude > gyroThreshold)
	{
#ifdef YAW_LOGGING
		LogText("YAW - Angular velocity too high.\n");
#endif
		return false;
	}
		
	return true;
}

void SensorFusion::applyMagYawCorrection(const Vector3f& magUncalibrated, const Vector3f& magBias, const Vector3f& gyro, float deltaT)
{
    const float minMagLengthSq   = Mathd::Tolerance; // need to use a real value to discard very weak fields
    const float maxAngleRefDist  = 5.0f * Mathf::DegreeToRadFactor;
    const float maxTiltError     = 0.05f;
	const float correctionDegreesPerSecMax = 0.07f;
	const float correctionRadPerSecMax = correctionDegreesPerSecMax * Mathf::DegreeToRadFactor;
	const float proportionalGain = correctionDegreesPerSecMax / 5.0f;	// When we had the integral term we used a proportional gain value of 0.01.
																		// When the integral term was removed and the max correction clipped, the
																		// gain was altered to provided the clipped amount at 5 degrees of error.
	const float tiltStabilizeTimeSeconds = 5.0f;
	const int	maxRefPointScore = 5000;

	// If angular velocity is above this then we don't perform mag yaw correction. This is because error grows 
	// due to our approximate latency compensation and because of latency due to the mag sample rate being 100Hz 
	// rather than 1kHz for other sensors. 100Hz => 10mS latency @ 200 deg/s = 2 degrees error.
	const float gyroThreshold = 200.0f * Mathf::DegreeToRadFactor;
			
	
	if (magBias == Vector3f::ZERO)
	{
		// Assume Android calibration has not yet occurred.
#ifdef YAW_LOGGING
		LogText("YAW - Android calibration not occurred.\n");
#endif
		return;
	}

	// Buffer orientation since mag latency is higher than HMT sensor latency.
	Quatf orientation;
	if (!getBufferedOrientation(&orientation, gyro, gyroThreshold, deltaT))
	{
		return;
	}
	
	// Wait a while for tilt to stabilize.
	YawCorrectionTimer += deltaT;
	if (YawCorrectionTimer < tiltStabilizeTimeSeconds)
	{
#ifdef YAW_LOGGING
		LogText("YAW - Waiting for tilt to stabilize.\n");
#endif
		return;
	}

	
	Vector3f magCalibrated = magUncalibrated - magBias;
    Vector3f magInWorldFrame = orientation.Rotate(magCalibrated);
	
    // Verify that the horizontal component is sufficient.
    if (magInWorldFrame.x * magInWorldFrame.x + magInWorldFrame.z * magInWorldFrame.z < minMagLengthSq)
	{
#ifdef YAW_LOGGING
    	LogText("YAW - Field horizontal component too small.\n");
#endif
        return;
	}
    magInWorldFrame.Normalize();

	
    // Delete a bad point
    if (MagRefIdx >= 0 && MagRefs[MagRefIdx].Score < 0)
    {
#ifdef YAW_LOGGING
    	LogText("YAW - Deleted ref point %d\n", MagRefIdx);
#endif

        MagRefs.RemoveAtUnordered(MagRefIdx);
        MagRefIdx = -1;
    }

    // Update the reference point if needed
    if (MagRefIdx < 0 || orientation.Angle(MagRefs[MagRefIdx].WorldFromImu) > maxAngleRefDist)
    {
        // Find a new one
        MagRefIdx = -1;
        float bestDist = maxAngleRefDist;
        for (UPInt i = 0; i < MagRefs.GetSize(); ++i)
        {
            float dist = orientation.Angle(MagRefs[i].WorldFromImu);
            if (bestDist > dist)
            {
                bestDist = dist;
                MagRefIdx = i;
            }
        }

#ifdef YAW_LOGGING
		if (MagRefIdx != -1)
        {
        	LogText("YAW - Switched to ref point %d\n", MagRefIdx);
        }		
#endif

        // Create one if needed
        if (MagRefIdx < 0 && MagRefs.GetSize() < MagMaxReferences)
		{
            MagRefs.PushBack(MagReferencePoint(magUncalibrated, orientation, 1000));

#ifdef YAW_LOGGING
            LogText("YAW - Created ref point [%d] %f %f %f\n", MagRefs.GetSize()-1, magUncalibrated.x, magUncalibrated.y, magUncalibrated.z);
#endif
		}		
    }

    if (MagRefIdx >= 0)
    {
        Vector3f magRefInWorldFrame = MagRefs[MagRefIdx].WorldFromImu.Rotate(MagRefs[MagRefIdx].MagUncalibratedInImuFrame-magBias);

        // Verify that the horizontal component is sufficient when using current bias.
        if (magRefInWorldFrame.x * magRefInWorldFrame.x + magRefInWorldFrame.z * magRefInWorldFrame.z < minMagLengthSq)
        {
#ifdef YAW_LOGGING
        	LogText("YAW - Calibrated ref point field horizontal component too small.\n");
#endif
            return;
        }
		magRefInWorldFrame.Normalize();


        // If the vertical angle is wrong, decrease the score and do nothing.
        if (Alg::Abs(magRefInWorldFrame.y - magInWorldFrame.y) > maxTiltError)
        {
            MagRefs[MagRefIdx].Score -= 1;
#ifdef YAW_LOGGING
			LogText("YAW - Decrement ref point score %d\n", MagRefs[MagRefIdx].Score);
#endif
            return;
        }

        if (MagRefs[MagRefIdx].Score < maxRefPointScore)
        {
			MagRefs[MagRefIdx].Score += 2;
        }


        // Correction is computed in the horizontal plane
        magInWorldFrame.y = magRefInWorldFrame.y = 0;

        // Don't need to check for zero vectors since we already validated horizontal components above.
        float yawError = magInWorldFrame.Angle(magRefInWorldFrame);
        if (magInWorldFrame.Cross(magRefInWorldFrame).y < 0.0f)
        {
        	yawError *= -1.0f;
        }


        float propCorrectionRadPerSec = yawError * proportionalGain;
		float totalCorrectionRadPerSec = propCorrectionRadPerSec;

        // Limit correction.
		totalCorrectionRadPerSec = Alg::Min(totalCorrectionRadPerSec, correctionRadPerSecMax);
		totalCorrectionRadPerSec = Alg::Max(totalCorrectionRadPerSec, -correctionRadPerSecMax);

        Quatf correction(Vector3f(0.0f, 1.0f, 0.0f), totalCorrectionRadPerSec * deltaT);
        State.Transform.Orientation = correction * State.Transform.Orientation;

#ifdef YAW_LOGGING
		static int logCount = 0;
		static int lastLogCount = 0;
		logCount++;

		if (logCount - lastLogCount > 1000)
		{
			lastLogCount = logCount;

			float yaw, pitch, roll;
			orientation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&yaw, &pitch, &roll);
			yaw *= Mathf::RadToDegreeFactor;
			pitch *= Mathf::RadToDegreeFactor;
			roll *= Mathf::RadToDegreeFactor;

			float pyaw, ppitch, proll;
			MagRefs[MagRefIdx].WorldFromImu.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&pyaw, &ppitch, &proll);
			pyaw *= Mathf::RadToDegreeFactor;
			ppitch *= Mathf::RadToDegreeFactor;
			proll *= Mathf::RadToDegreeFactor;

			LogText("YAW %5.2f::: [%d:%d] ypr=%.1f,%.1f,%.1f ref_ypr=%.1f,%.1f,%.1f magUncalib=%.3f %.3f %.3f magBias=%.3f %.3f %.3f error=%.3f prop=%.6f correction=%.6f\n",
					(float) logCount/1000.0f,
					MagRefIdx,
					MagRefs.GetSize(),
					yaw, pitch, roll,
					pyaw, ppitch, proll,
					magUncalibrated.x, magUncalibrated.y, magUncalibrated.z,
					magBias.x, magBias.y, magBias.z,
					yawError * Mathf::RadToDegreeFactor,
					propCorrectionRadPerSec * Mathf::RadToDegreeFactor,
					totalCorrectionRadPerSec * Mathf::RadToDegreeFactor);
		}
#endif
    }
}

void SensorFusion::applyTiltCorrection(float deltaT)
{
    const float gain = 0.25;
    const float snapThreshold = 0.1;
    const Vector3f up(0, 1, 0);

	Vector3f accelLocalFiltered(FAccelHeadset.GetFilteredValue());
    Vector3f accelW = State.Transform.Orientation.Rotate(accelLocalFiltered);
    Quatf error = vectorAlignmentRotation(accelW, up);

    Quatf correction;
    if (FAccelHeadset.GetSize() == 1 ||
        ((Alg::Abs(error.w) < cos(snapThreshold / 2) && FAccelHeadset.Confidence() > 0.75)))
	{
        // full correction for start-up
        // or large error with high confidence
        correction = error;
	}
    else if (FAccelHeadset.Confidence() > 0.5)
	{
        correction = error.Nlerp(Quatf(), gain * deltaT);
	}
    else
	{
        // accelerometer is unreliable due to movement
        return;
	}

    State.Transform.Orientation = correction * State.Transform.Orientation;
}

void SensorFusion::applyFocusCorrection(float deltaT)
{
	Vector3f up = Vector3f(0, 1, 0);
	float gain = 0.01;
	Vector3f currentDir = State.Transform.Orientation.Rotate(Vector3f(0, 0, 1));

	Vector3f focusYawComponent = FocusDirection.ProjectToPlane(up);
	Vector3f currentYawComponent = currentDir.ProjectToPlane(up);

	float angle = focusYawComponent.Angle(currentYawComponent);

	if( angle > FocusFOV )
	{
		Quatf yawError;
		if ( FocusFOV != 0.0f)
		{
			Vector3f lFocus = Quatf(up, -FocusFOV).Rotate(focusYawComponent);
			Vector3f rFocus = Quatf(up, FocusFOV).Rotate(focusYawComponent);
			float lAngle = lFocus.Angle(currentYawComponent);
			float rAngle = rFocus.Angle(currentYawComponent);
			if(lAngle < rAngle)
			{
				yawError = vectorAlignmentRotation(currentDir, lFocus);
			} else
			{
				yawError = vectorAlignmentRotation(currentDir, rFocus);
			}
		} else
		{
			yawError = vectorAlignmentRotation(currentYawComponent, focusYawComponent);
			Vector3f axis;
			float angle;
			yawError.GetAxisAngle(&axis, &angle);
		}

		Quatf correction = yawError.Nlerp(Quatf(), gain * deltaT);
        State.Transform.Orientation = correction * State.Transform.Orientation;

	}
}

void SensorFusion::SetFocusDirection()
{
	SetFocusDirection(Vector3f(State.Transform.Orientation.Rotate(Vector3f(0.0, 0.0, 1.0))));
}

void SensorFusion::SetFocusDirection(Vector3f direction)
{
	FocusDirection = direction;
}

void SensorFusion::SetFocusFOV(float fov)
{
	OVR_ASSERT(fov >= 0.0);
	FocusFOV = fov;
}

void SensorFusion::ClearFocus()
{
	FocusDirection = Vector3f(0.0, 0.0, 0.0);
	FocusFOV = 0.0f;
}

// This is a "perceptually tuned predictive filter", which means that it is optimized
// for improvements in the VR experience, rather than pure error.  In particular,
// jitter is more perceptible at lower speeds whereas latency is more perceptable
// after a high-speed motion.  Therefore, the prediction interval is dynamically
// adjusted based on speed.  Significant more research is needed to further improve
// this family of filters.
Posef calcPredictedPose(const PoseStatef& poseState, float predictionDt )
{
    Posef pose              = poseState.Transform;
	const float linearCoef  = 1.0;
	Vector3f angularVelocity = poseState.AngularVelocity;
	float angularSpeed      = angularVelocity.Length();

	// This could be tuned so that linear and angular are combined with different coefficients
	float speed             = angularSpeed + linearCoef * poseState.LinearVelocity.Length();

	const float slope       = 0.2; // The rate at which the dynamic prediction interval varies
	float candidateDt       = slope * speed; // TODO: Replace with smoothstep function

	float dynamicDt         = predictionDt;

	// Choose the candidate if it is shorter, to improve stability
	if (candidateDt < predictionDt)
		dynamicDt = candidateDt;

    if (angularSpeed > 0.001)
        pose.Orientation = pose.Orientation * Quatf(angularVelocity, angularSpeed * dynamicDt);

    pose.Position += poseState.LinearVelocity * dynamicDt;

    return pose;
}

//  A predictive filter based on extrapolating the smoothed, current angular velocity
SensorState SensorFusion::GetPredictionForTime( const double absoluteTimeSeconds ) const
{		
    SensorState	sstate;
    sstate.Status = Status_OrientationTracked | Status_HmdConnected;
        	
    // lockless state fetch
    const StateForPrediction state = UpdatedState.GetState();

    // Delta time from the last processed message
    const float pdt                = absoluteTimeSeconds - state.State.TimeInSeconds;

    sstate.Recorded                = state.State;
    sstate.Temperature             = state.Temperature;

    const Posef recenter = RecenterTransform.GetState();

    // Do prediction logic
    sstate.Predicted               = sstate.Recorded;
    sstate.Predicted.TimeInSeconds = absoluteTimeSeconds;
    sstate.Predicted.Transform     = recenter * calcPredictedPose(state.State, pdt);

    return sstate;
}

SensorFusion::BodyFrameHandler::~BodyFrameHandler()
{
    RemoveHandlerFromDevices();
}

void SensorFusion::BodyFrameHandler::OnMessage(const Message& msg)
{    
    if (msg.Type == Message_BodyFrame)
    {
        pFusion->handleMessage(static_cast<const MessageBodyFrame&>(msg));
    }
}

bool SensorFusion::BodyFrameHandler::SupportsMessageType(MessageType type) const
{
    return (type == Message_BodyFrame);
}

} // namespace OVR
