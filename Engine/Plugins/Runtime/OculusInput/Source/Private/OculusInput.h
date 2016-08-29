// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OculusInputPrivatePCH.h"
#include "OculusInputState.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack (push,8)
#endif

#define OVR_PARTNER_CODE //? @TEMP

#include <OVR_Version.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_Keys.h>
#include <Extras/OVR_CAPI_Util.h>
#include <Extras/OVR_Math.h>
#include <OVR_ErrorCode.h>


#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack (pop)
#endif

/**
 * A pair of wireless motion controllers, one for either hand
 */
struct FOculusTouchControllerPair
{
	/** The Unreal controller index assigned to this pair */
	int32 UnrealControllerIndex;

	/** Current device state for either hand */
	FOculusTouchControllerState ControllerStates[ 2 ];


	/** Default constructor that sets up sensible defaults */
	FOculusTouchControllerPair()
		: UnrealControllerIndex( INDEX_NONE ),
		  ControllerStates()
	{
		ControllerStates[ (int32)EControllerHand::Left ] = FOculusTouchControllerState( EControllerHand::Left );
		ControllerStates[ (int32)EControllerHand::Right ] = FOculusTouchControllerState( EControllerHand::Right );
	}
};

/**
 * Unreal Engine support for Oculus motion controller devices
 */
class FOculusInput : public IInputDevice, public IMotionController, public IHapticDevice
{

public:

	/** Constructor that takes an initial message handler that will receive motion controller events */
	FOculusInput( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Clean everything up */
	virtual ~FOculusInput();

	static void PreInit();

	/** Loads any settings from the config folder that we need */
	static void LoadConfig();

	// IInputDevice overrides
	virtual void Tick( float DeltaTime ) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void SetChannelValue( int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value ) override;
	virtual void SetChannelValues( int32 ControllerId, const FForceFeedbackValues& Values ) override;

	// IMotionController overrides
	virtual bool GetControllerOrientationAndPosition( const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition ) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;

	// IHapticDevice overrides
	IHapticDevice* GetHapticDevice() override { return (IHapticDevice*)this; }
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;

	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const override;
	virtual float GetHapticAmplitudeScale() const override;

private:

	/** Applies force feedback settings to the controller */
	void UpdateForceFeedback( const FOculusTouchControllerPair& ControllerPair, const EControllerHand Hand );

private:

	/** The recipient of motion controller input events */
	TSharedPtr< FGenericApplicationMessageHandler > MessageHandler;

	/** List of the connected pairs of controllers, with state for each controller device */
	TArray< FOculusTouchControllerPair > ControllerPairs;

	FOculusRemoteControllerState Remote;

	/** Threshold for treating trigger pulls as button presses, from 0.0 to 1.0 */
	static float TriggerThreshold;

	/** Are Remote keys mapped to gamepad or not. */
	static bool bRemoteKeysMappedToGamepad;

	ovrTouchHapticsDesc HapticsDesc;
};

DEFINE_LOG_CATEGORY_STATIC(LogOcInput, Log, All);

#endif //OCULUS_INPUT_SUPPORTED_PLATFORMS
