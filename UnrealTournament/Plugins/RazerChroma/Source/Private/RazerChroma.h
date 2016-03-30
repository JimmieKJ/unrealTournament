// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UnrealTournament.h"

#include "AllowWindowsPlatformTypes.h"
#include "RzChromaSDKDefines.h"
#include "RzChromaSDKTypes.h"
#include "RzErrors.h"
#include "HideWindowsPlatformTypes.h"

#include "RazerChroma.generated.h"

UCLASS(Blueprintable, Meta = (ChildCanTick))
class ARazerChroma : public AActor
{
	GENERATED_UCLASS_BODY()
	
};

struct FRazerChroma : FTickableGameObject, IModuleInterface
{
#define UTRGB(r,g,b)          ((uint32)(((uint8)(r)|((uint16)((uint8)(g))<<8))|(((uint32)(uint8)(b))<<16)))

	const uint32 BLACK = UTRGB(0, 0, 0);
	const uint32 WHITE = UTRGB(255, 255, 255);
	const uint32 RED = UTRGB(255, 0, 0);
	const uint32 GREEN = UTRGB(0, 255, 0);
	const uint32 BLUE = UTRGB(0, 0, 255);
	const uint32 YELLOW = UTRGB(255, 255, 0);
	const uint32 PURPLE = UTRGB(128, 0, 128);
	const uint32 CYAN = UTRGB(00, 255, 255);
	const uint32 ORANGE = UTRGB(255, 165, 00);
	const uint32 PINK = UTRGB(255, 192, 203);
	const uint32 GREY = UTRGB(125, 125, 125);

	FRazerChroma();
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const { return true; }
	virtual bool IsTickableInEditor() const { return true; }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	// Put a real stat id here
	virtual TStatId GetStatId() const
	{
		return TStatId();
	}

	float DeltaTimeAccumulator;
	float FrameTimeMinimum;
	int FlashSpeed;

	uint64 LastFrameCounter;

	bool bIsFlashingForEnd;
	bool bInitialized;
	bool bChromaSDKEnabled;

	typedef RZRESULT(*INIT)(void);
	typedef RZRESULT(*UNINIT)(void);
	typedef RZRESULT(*CREATEEFFECT)(RZDEVICEID DeviceId, ChromaSDK::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*CREATEKEYBOARDEFFECT)(ChromaSDK::Keyboard::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*CREATEHEADSETEFFECT)(ChromaSDK::Headset::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*CREATEMOUSEPADEFFECT)(ChromaSDK::Mousepad::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*CREATEMOUSEEFFECT)(ChromaSDK::Mouse::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*CREATEKEYPADEFFECT)(ChromaSDK::Keypad::EFFECT_TYPE Effect, PRZPARAM pParam, RZEFFECTID *pEffectId);
	typedef RZRESULT(*SETEFFECT)(RZEFFECTID EffectId);
	typedef RZRESULT(*DELETEEFFECT)(RZEFFECTID EffectId);
	typedef RZRESULT(*REGISTEREVENTNOTIFICATION)(HWND hWnd);
	typedef RZRESULT(*UNREGISTEREVENTNOTIFICATION)(void);
	typedef RZRESULT(*QUERYDEVICE)(RZDEVICEID DeviceId, ChromaSDK::DEVICE_INFO_TYPE &DeviceInfo);

	INIT Init;
	UNINIT UnInit;
	CREATEEFFECT CreateEffect;
	CREATEKEYBOARDEFFECT CreateKeyboardEffect;
	CREATEMOUSEEFFECT CreateMouseEffect;
	CREATEHEADSETEFFECT CreateHeadsetEffect;
	CREATEMOUSEPADEFFECT CreateMousepadEffect;
	CREATEKEYPADEFFECT CreateKeypadEffect;
	SETEFFECT SetEffect;
	DELETEEFFECT DeleteEffect;
	QUERYDEVICE QueryDevice;

	bool bPlayingIdleColors;
	bool bPlayingUDamage;
	bool bPlayingBerserk;
	bool bPlayingShieldBelt;

#define UNREALTEXTSCROLLERFRAMES 11
	RZEFFECTID UnrealTextScroller[UNREALTEXTSCROLLERFRAMES];
	int32 TextScrollerFrame;
	float TextScrollerDeltaTimeAccumulator;
	float TextScrollerFrameTimeMinimum;

	void UpdateIdleColors(float DeltaTime);
};