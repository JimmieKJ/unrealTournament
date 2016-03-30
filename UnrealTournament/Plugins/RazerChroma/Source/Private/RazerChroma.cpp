// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RazerChroma.h"

#include "UnrealTournament.h"
#include "UTPlayerController.h"
#include "UTGameState.h"
#include "UTArmor.h"
#include "UTTimedPowerup.h"

#ifdef _WIN64
#define CHROMASDKDLL        _T("RzChromaSDK64.dll")
#else
#define CHROMASDKDLL        _T("RzChromaSDK.dll")
#endif

using namespace ChromaSDK;
using namespace ChromaSDK::Keyboard;
using namespace ChromaSDK::Keypad;
using namespace ChromaSDK::Mouse;
using namespace ChromaSDK::Mousepad;
using namespace ChromaSDK::Headset;

DEFINE_LOG_CATEGORY_STATIC(LogUTKBLightShow, Log, All);

ARazerChroma::ARazerChroma(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FRazerChroma::FRazerChroma()
{
	FrameTimeMinimum = 0.03f;
	DeltaTimeAccumulator = 0;
	bIsFlashingForEnd = false;
	bInitialized = false;
	bChromaSDKEnabled = false;

	FlashSpeed = 100;
	
	TextScrollerFrame = 0;
	TextScrollerDeltaTimeAccumulator = 0;
	TextScrollerFrameTimeMinimum = 0.10f;

	Init = nullptr;
	CreateEffect = nullptr;
	CreateKeyboardEffect = nullptr;
	CreateMouseEffect = nullptr;
	CreateHeadsetEffect = nullptr;
	CreateMousepadEffect = nullptr;
	CreateKeypadEffect = nullptr;
	SetEffect = nullptr;
	DeleteEffect = nullptr;
	QueryDevice = nullptr;
	UnInit = nullptr;

	bPlayingIdleColors = false;
	bPlayingUDamage = false;
	bPlayingBerserk = false;
	bPlayingShieldBelt = false;

	AtoZToRZKEY[0] = RZKEY_A;
	AtoZToRZKEY[1] = RZKEY_B;
	AtoZToRZKEY[2] = RZKEY_C;
	AtoZToRZKEY[3] = RZKEY_D;
	AtoZToRZKEY[4] = RZKEY_E;
	AtoZToRZKEY[5] = RZKEY_F;
	AtoZToRZKEY[6] = RZKEY_G;
	AtoZToRZKEY[7] = RZKEY_H;
	AtoZToRZKEY[8] = RZKEY_I;
	AtoZToRZKEY[9] = RZKEY_J;
	AtoZToRZKEY[10] = RZKEY_K;
	AtoZToRZKEY[11] = RZKEY_L;
	AtoZToRZKEY[12] = RZKEY_M;
	AtoZToRZKEY[13] = RZKEY_N;
	AtoZToRZKEY[14] = RZKEY_O;
	AtoZToRZKEY[15] = RZKEY_P;
	AtoZToRZKEY[16] = RZKEY_Q;
	AtoZToRZKEY[17] = RZKEY_R;
	AtoZToRZKEY[18] = RZKEY_S;
	AtoZToRZKEY[19] = RZKEY_T;
	AtoZToRZKEY[20] = RZKEY_U;
	AtoZToRZKEY[21] = RZKEY_V;
	AtoZToRZKEY[22] = RZKEY_W;
	AtoZToRZKEY[23] = RZKEY_X;
	AtoZToRZKEY[24] = RZKEY_Y;
	AtoZToRZKEY[25] = RZKEY_Z;
}

IMPLEMENT_MODULE(FRazerChroma, RazerChroma)

void FRazerChroma::StartupModule()
{
	HMODULE ChromaSDKModule = nullptr;

	ChromaSDKModule = LoadLibrary(CHROMASDKDLL);
	if (ChromaSDKModule == NULL)
	{
		return;
	}

	// GetProcAddress will throw 4191 because it's an unsafe type cast, but I almost know what I'm doing here
#pragma warning(disable: 4191)
	RZRESULT Result = RZRESULT_INVALID;
	Init = (INIT)GetProcAddress(ChromaSDKModule, "Init");
	CreateEffect = (CREATEEFFECT)GetProcAddress(ChromaSDKModule, "CreateEffect");
	CreateKeyboardEffect = (CREATEKEYBOARDEFFECT)GetProcAddress(ChromaSDKModule, "CreateKeyboardEffect");
	CreateMouseEffect = (CREATEMOUSEEFFECT)GetProcAddress(ChromaSDKModule, "CreateMouseEffect");
	CreateHeadsetEffect = (CREATEHEADSETEFFECT)GetProcAddress(ChromaSDKModule, "CreateHeadsetEffect");
	CreateMousepadEffect = (CREATEMOUSEPADEFFECT)GetProcAddress(ChromaSDKModule, "CreateMousepadEffect");
	CreateKeypadEffect = (CREATEKEYPADEFFECT)GetProcAddress(ChromaSDKModule, "CreateKeypadEffect");
	SetEffect = (SETEFFECT)GetProcAddress(ChromaSDKModule, "SetEffect");
	DeleteEffect = (DELETEEFFECT)GetProcAddress(ChromaSDKModule, "DeleteEffect");
	QueryDevice = (QUERYDEVICE)GetProcAddress(ChromaSDKModule, "QueryDevice");
	UnInit = (UNINIT)GetProcAddress(ChromaSDKModule, "UnInit");
	if (Init)
	{
		Result = Init();
		if (Result == RZRESULT_SUCCESS)
		{
			if (CreateEffect &&
				CreateKeyboardEffect &&
				CreateMouseEffect &&
				CreateHeadsetEffect &&
				CreateMousepadEffect &&
				CreateKeypadEffect &&
				SetEffect &&
				DeleteEffect &&
				QueryDevice &&
				UnInit)
			{
				bChromaSDKEnabled = true;
			}
		}
	}
#pragma warning(default: 4191)

//	QueryDevice(RZDEVICEID)

	if (bChromaSDKEnabled)
	{
		ChromaSDK::Keyboard::CUSTOM_EFFECT_TYPE Effect = {};

		// U
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Q)][LOBYTE(RZKEY_Q)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_A)][LOBYTE(RZKEY_A)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Z)][LOBYTE(RZKEY_Z)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_X)][LOBYTE(RZKEY_X)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_C)][LOBYTE(RZKEY_C)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_V)][LOBYTE(RZKEY_V)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_F)][LOBYTE(RZKEY_F)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_R)][LOBYTE(RZKEY_R)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = RED;
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = RED;
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = RED;
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = RED;
		Effect.Color[HIBYTE(RZKEY_I)][LOBYTE(RZKEY_I)] = RED;
		Effect.Color[HIBYTE(RZKEY_K)][LOBYTE(RZKEY_K)] = RED;
		Effect.Color[HIBYTE(RZKEY_OEM_9)][LOBYTE(RZKEY_OEM_9)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[0]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_Z)][LOBYTE(RZKEY_Z)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_X)][LOBYTE(RZKEY_X)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_C)][LOBYTE(RZKEY_C)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_D)][LOBYTE(RZKEY_D)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_E)][LOBYTE(RZKEY_E)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = RED;
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = RED;
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = RED;
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = RED;
		Effect.Color[HIBYTE(RZKEY_U)][LOBYTE(RZKEY_U)] = RED;
		Effect.Color[HIBYTE(RZKEY_J)][LOBYTE(RZKEY_J)] = RED;
		Effect.Color[HIBYTE(RZKEY_M)][LOBYTE(RZKEY_M)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[1]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_Z)][LOBYTE(RZKEY_Z)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_X)][LOBYTE(RZKEY_X)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_S)][LOBYTE(RZKEY_S)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_W)][LOBYTE(RZKEY_W)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = BLUE;

		// Second U
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_P)][LOBYTE(RZKEY_P)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_7)][LOBYTE(RZKEY_OEM_7)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_11)][LOBYTE(RZKEY_OEM_11)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = RED;
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = RED;
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = RED;
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = RED;
		Effect.Color[HIBYTE(RZKEY_Y)][LOBYTE(RZKEY_Y)] = RED;
		Effect.Color[HIBYTE(RZKEY_H)][LOBYTE(RZKEY_H)] = RED;
		Effect.Color[HIBYTE(RZKEY_N)][LOBYTE(RZKEY_N)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[2]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Q)][LOBYTE(RZKEY_Q)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_A)][LOBYTE(RZKEY_A)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Z)][LOBYTE(RZKEY_Z)] = BLUE;

		// Second U
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_O)][LOBYTE(RZKEY_O)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_L)][LOBYTE(RZKEY_L)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_10)][LOBYTE(RZKEY_OEM_10)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = RED;
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = RED;
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = RED;
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = RED;
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_T)][LOBYTE(RZKEY_T)] = RED;
		Effect.Color[HIBYTE(RZKEY_G)][LOBYTE(RZKEY_G)] = RED;
		Effect.Color[HIBYTE(RZKEY_B)][LOBYTE(RZKEY_B)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[3]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_I)][LOBYTE(RZKEY_I)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_K)][LOBYTE(RZKEY_K)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_9)][LOBYTE(RZKEY_OEM_9)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_10)][LOBYTE(RZKEY_OEM_10)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_11)][LOBYTE(RZKEY_OEM_11)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = RED;
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = RED;
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = RED;
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = RED;
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = RED;
		Effect.Color[HIBYTE(RZKEY_R)][LOBYTE(RZKEY_R)] = RED;
		Effect.Color[HIBYTE(RZKEY_F)][LOBYTE(RZKEY_F)] = RED;
		Effect.Color[HIBYTE(RZKEY_V)][LOBYTE(RZKEY_V)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[4]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_U)][LOBYTE(RZKEY_U)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_J)][LOBYTE(RZKEY_J)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_M)][LOBYTE(RZKEY_M)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_9)][LOBYTE(RZKEY_OEM_9)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_10)][LOBYTE(RZKEY_OEM_10)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_11)][LOBYTE(RZKEY_OEM_11)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_7)][LOBYTE(RZKEY_OEM_7)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_P)][LOBYTE(RZKEY_P)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = RED;
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = RED;
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = RED;
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = RED;
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = RED;
		Effect.Color[HIBYTE(RZKEY_E)][LOBYTE(RZKEY_E)] = RED;
		Effect.Color[HIBYTE(RZKEY_D)][LOBYTE(RZKEY_D)] = RED;
		Effect.Color[HIBYTE(RZKEY_C)][LOBYTE(RZKEY_C)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[5]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Y)][LOBYTE(RZKEY_Y)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_H)][LOBYTE(RZKEY_H)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_N)][LOBYTE(RZKEY_N)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_M)][LOBYTE(RZKEY_M)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_9)][LOBYTE(RZKEY_OEM_9)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_10)][LOBYTE(RZKEY_OEM_10)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_L)][LOBYTE(RZKEY_L)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_O)][LOBYTE(RZKEY_O)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = RED;
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = RED;
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = RED;
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = RED;
		Effect.Color[HIBYTE(RZKEY_W)][LOBYTE(RZKEY_W)] = RED;
		Effect.Color[HIBYTE(RZKEY_S)][LOBYTE(RZKEY_S)] = RED;
		Effect.Color[HIBYTE(RZKEY_X)][LOBYTE(RZKEY_X)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[6]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_T)][LOBYTE(RZKEY_T)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_G)][LOBYTE(RZKEY_G)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_B)][LOBYTE(RZKEY_B)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_N)][LOBYTE(RZKEY_N)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_M)][LOBYTE(RZKEY_M)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_OEM_9)][LOBYTE(RZKEY_OEM_9)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_K)][LOBYTE(RZKEY_K)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_I)][LOBYTE(RZKEY_I)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = RED;
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = RED;
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = RED;
		Effect.Color[HIBYTE(RZKEY_Q)][LOBYTE(RZKEY_Q)] = RED;
		Effect.Color[HIBYTE(RZKEY_A)][LOBYTE(RZKEY_A)] = RED;
		Effect.Color[HIBYTE(RZKEY_Z)][LOBYTE(RZKEY_Z)] = RED;

		// Second T
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[7]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_4)][LOBYTE(RZKEY_4)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_R)][LOBYTE(RZKEY_R)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_F)][LOBYTE(RZKEY_F)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_V)][LOBYTE(RZKEY_V)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_B)][LOBYTE(RZKEY_B)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_N)][LOBYTE(RZKEY_N)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_M)][LOBYTE(RZKEY_M)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_J)][LOBYTE(RZKEY_J)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_U)][LOBYTE(RZKEY_U)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = RED;
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = RED;

		// Second T
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = RED;
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[8]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_3)][LOBYTE(RZKEY_3)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_E)][LOBYTE(RZKEY_E)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_D)][LOBYTE(RZKEY_D)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_C)][LOBYTE(RZKEY_C)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_V)][LOBYTE(RZKEY_V)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_B)][LOBYTE(RZKEY_B)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_N)][LOBYTE(RZKEY_N)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_Y)][LOBYTE(RZKEY_Y)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_H)][LOBYTE(RZKEY_H)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_6)][LOBYTE(RZKEY_6)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_1)][LOBYTE(RZKEY_1)] = RED;

		// Second T
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = RED;
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = RED;
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = RED;
		Effect.Color[HIBYTE(RZKEY_P)][LOBYTE(RZKEY_P)] = RED;
		Effect.Color[HIBYTE(RZKEY_OEM_7)][LOBYTE(RZKEY_OEM_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_OEM_11)][LOBYTE(RZKEY_OEM_11)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[9]);

		FMemory::Memzero(Effect);

		// U
		Effect.Color[HIBYTE(RZKEY_2)][LOBYTE(RZKEY_2)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_W)][LOBYTE(RZKEY_W)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_S)][LOBYTE(RZKEY_S)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_X)][LOBYTE(RZKEY_X)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_C)][LOBYTE(RZKEY_C)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_V)][LOBYTE(RZKEY_V)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_B)][LOBYTE(RZKEY_B)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_T)][LOBYTE(RZKEY_T)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_G)][LOBYTE(RZKEY_G)] = BLUE;
		Effect.Color[HIBYTE(RZKEY_5)][LOBYTE(RZKEY_5)] = BLUE;

		// T
		Effect.Color[HIBYTE(RZKEY_7)][LOBYTE(RZKEY_7)] = RED;
		Effect.Color[HIBYTE(RZKEY_8)][LOBYTE(RZKEY_8)] = RED;
		Effect.Color[HIBYTE(RZKEY_9)][LOBYTE(RZKEY_9)] = RED;
		Effect.Color[HIBYTE(RZKEY_0)][LOBYTE(RZKEY_0)] = RED;
		Effect.Color[HIBYTE(RZKEY_O)][LOBYTE(RZKEY_O)] = RED;
		Effect.Color[HIBYTE(RZKEY_L)][LOBYTE(RZKEY_L)] = RED;
		Effect.Color[HIBYTE(RZKEY_OEM_10)][LOBYTE(RZKEY_OEM_10)] = RED;

		CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, &UnrealTextScroller[10]);
	}

}

void FRazerChroma::ShutdownModule()
{
	if (bChromaSDKEnabled)
	{
		for (int i = 0; i < UNREALTEXTSCROLLERFRAMES; i++)
		{
			DeleteEffect(UnrealTextScroller[i]);
		}

		UnInit();
	}
}

void FRazerChroma::Tick(float DeltaTime)
{
	if (GIsEditor)
	{
		return;
	}

	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (UserSettings && !UserSettings->IsKeyboardLightingEnabled())
	{
		if (bChromaSDKEnabled && UnInit)
		{
			UnInit();
			bChromaSDKEnabled = false;
		}

		return;
	}

	if (!bChromaSDKEnabled)
	{
		if (Init && Init() == RZRESULT_SUCCESS)
		{
			bChromaSDKEnabled = true;
		}
	}

	if (!bChromaSDKEnabled)
	{
		return;
	}

	// Avoid double ticking
	if (LastFrameCounter > 0 && LastFrameCounter == GFrameCounter)
	{
		return;
	}

	LastFrameCounter = GFrameCounter;

	// We may be going 120hz, don't spam the device
	DeltaTimeAccumulator += DeltaTime;
	if (DeltaTimeAccumulator < FrameTimeMinimum)
	{
		return;
	}
	DeltaTimeAccumulator = 0;

	AUTPlayerController* UTPC = nullptr;
	AUTGameState* GS = nullptr;
	const TIndirectArray<FWorldContext>& AllWorlds = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : AllWorlds)
	{
		UWorld* World = Context.World();
		if (World && World->WorldType == EWorldType::Game)
		{
			UTPC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(World));
			if (UTPC)
			{
				UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(UTPC->GetLocalPlayer());
				if (UTLP == nullptr || UTLP->IsMenuGame())
				{
					UTPC = nullptr;
					continue;
				}

				GS = World->GetGameState<AUTGameState>();
				break;
			}
		}
	}

	if (!UTPC || !GS)
	{
		UpdateIdleColors(DeltaTime);
		return;
	}

	RZRESULT Result = RZRESULT_INVALID;

	if (GS->HasMatchEnded())
	{
		if (GS->WinningTeam != nullptr)
		{
			if (GS->WinningTeam->GetTeamNum() == UTPC->GetTeamNum())
			{
			}
			else
			{
			}
		}
		else if (GS->WinnerPlayerState != nullptr)
		{
			if (GS->WinnerPlayerState == UTPC->PlayerState)
			{
			}
			else
			{
			}
		}

		UpdateIdleColors(DeltaTime);
	}
	else if (GS->HasMatchStarted())
	{
		bPlayingIdleColors = false;

		// Spectators and DM players are all on 255
		int32 TeamNum = UTPC->GetTeamNum();

		uint32 TeamColor = ORANGE;
		if (TeamNum == 0)
		{
			TeamColor = RED;
		}
		else if (TeamNum == 1)
		{
			TeamColor = BLUE;
		}

		ChromaSDK::Keyboard::CUSTOM_EFFECT_TYPE Effect;
		for (int i = 0; i < ChromaSDK::Keyboard::MAX_ROW; i++)
			for (int j = 0; j < ChromaSDK::Keyboard::MAX_COLUMN; j++)
		{
			Effect.Color[i][j] = TeamColor;
		}

		bool bHasShieldBelt = false;
		bool bHasUDamage = false;
		bool bHasBerserk = false;

		if (UTPC->GetUTCharacter() && !UTPC->GetUTCharacter()->IsDead())
		{
			for (TInventoryIterator<> It(UTPC->GetUTCharacter()); It; ++It)
			{
				AUTArmor* Armor = Cast<AUTArmor>(*It);
				if (Armor)
				{
					if (Armor->ArmorType == ArmorTypeName::ShieldBelt)
					{
						bHasShieldBelt = true;
					}
				}
				AUTTimedPowerup* TimedPowerUp = Cast<AUTTimedPowerup>(*It);
				if (TimedPowerUp)
				{
					// There's gotta be a cleaner way than this
					static FName UDamageName = TEXT("UDamageCount");
					static FName BerserkName = TEXT("BerserkCount");
					if (TimedPowerUp->StatsNameCount == UDamageName)
					{
						bHasUDamage = true;
					}
					if (TimedPowerUp->StatsNameCount == BerserkName)
					{
						bHasBerserk = true;
					}
				}
			}

			float HealthPct = (float)UTPC->GetUTCharacter()->Health / 100.0f;
			HealthPct = FMath::Clamp(HealthPct, 0.0f, 1.0f);
			int32 KeysToFill = FMath::Clamp((int)(HealthPct * 12), 0, 12);
			
			uint32 HealthColor = GREEN;
			if (HealthPct <= 0.33f)
			{
				HealthColor = RED;
			}
			else if (HealthPct <= 0.66f)
			{
				HealthColor = YELLOW;
			}

			for (int32 i = 0; i < 12; i++)
			{
				if (i < KeysToFill)
				{
					Effect.Color[HIBYTE(RZKEY_F1 + i)][LOBYTE(RZKEY_F1 + i)] = HealthColor;
				}
				else
				{
					Effect.Color[HIBYTE(RZKEY_F1 + i)][LOBYTE(RZKEY_F1 + i)] = BLACK;
				}
			}

			// Find WASD and other variants like ESDF or IJKL
			TArray<FKey> Keys;
			UTPC->ResolveKeybindToFKey(TEXT("MoveForward"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					if (KeyName.GetCharArray()[0] >= 'A' && KeyName.GetCharArray()[0] <= 'Z')
					{
						int32 Offset = KeyName.GetCharArray()[0] - 'A';
						Effect.Color[HIBYTE(AtoZToRZKEY[Offset])][LOBYTE(AtoZToRZKEY[Offset])] = GREEN;
					}
				}
				if (KeyName == TEXT("Up"))
				{
					Effect.Color[HIBYTE(RZKEY_UP)][LOBYTE(RZKEY_UP)] = GREEN;
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveBackward"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					if (KeyName.GetCharArray()[0] >= 'A' && KeyName.GetCharArray()[0] <= 'Z')
					{
						int32 Offset = KeyName.GetCharArray()[0] - 'A';
						Effect.Color[HIBYTE(AtoZToRZKEY[Offset])][LOBYTE(AtoZToRZKEY[Offset])] = GREEN;
					}
				}
				if (KeyName == TEXT("Down"))
				{
					Effect.Color[HIBYTE(RZKEY_DOWN)][LOBYTE(RZKEY_DOWN)] = GREEN;
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveLeft"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					if (KeyName.GetCharArray()[0] >= 'A' && KeyName.GetCharArray()[0] <= 'Z')
					{
						int32 Offset = KeyName.GetCharArray()[0] - 'A';
						Effect.Color[HIBYTE(AtoZToRZKEY[Offset])][LOBYTE(AtoZToRZKEY[Offset])] = GREEN;
					}
				}
				if (KeyName == TEXT("Left"))
				{
					Effect.Color[HIBYTE(RZKEY_LEFT)][LOBYTE(RZKEY_LEFT)] = GREEN;
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveRight"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					if (KeyName.GetCharArray()[0] >= 'A' && KeyName.GetCharArray()[0] <= 'Z')
					{
						int32 Offset = KeyName.GetCharArray()[0] - 'A';
						Effect.Color[HIBYTE(AtoZToRZKEY[Offset])][LOBYTE(AtoZToRZKEY[Offset])] = GREEN;
					}
				}
				if (KeyName == TEXT("Right"))
				{
					Effect.Color[HIBYTE(RZKEY_RIGHT)][LOBYTE(RZKEY_RIGHT)] = GREEN;
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("TurnLeft"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName == TEXT("Left"))
				{
					Effect.Color[HIBYTE(RZKEY_LEFT)][LOBYTE(RZKEY_LEFT)] = GREEN;
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("TurnRight"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName == TEXT("Right"))
				{
					Effect.Color[HIBYTE(RZKEY_LEFT)][LOBYTE(RZKEY_LEFT)] = GREEN;
				}
			}

			bool bFoundWeapon = false;
			for (int32 i = 0; i < 10; i++)
			{
				bFoundWeapon = false;

				int32 CurrentWeaponGroup = -1;
				if (UTPC->GetUTCharacter()->GetWeapon())
				{
					CurrentWeaponGroup = UTPC->GetUTCharacter()->GetWeapon()->DefaultGroup;
				}

				for (TInventoryIterator<AUTWeapon> It(UTPC->GetUTCharacter()); It; ++It)
				{
					AUTWeapon* Weap = *It;
					if (Weap->HasAnyAmmo())
					{
						if (Weap->DefaultGroup == (i + 1))
						{
							bFoundWeapon = true;
							break;
						}
					}
				}

				
				if (bFoundWeapon)
				{
					uint32 WeaponColor;
					switch (i)
					{
					case 0:
					default:
						WeaponColor = GREY;
						break;
					case 1:
						WeaponColor = WHITE;
						break;
					case 2:
						WeaponColor = GREEN;
						break;
					case 3:
						WeaponColor = PURPLE;
						break;
					case 4:
						WeaponColor = CYAN;
						break;
					case 5:
						WeaponColor = BLUE;
						break;
					case 6:
						WeaponColor = YELLOW;
						break;
					case 7:
						WeaponColor = RED;
						break;
					case 8:
						WeaponColor = WHITE;
						break;
					}
					Effect.Color[HIBYTE(RZKEY_1 + i)][LOBYTE(RZKEY_1 + i)] = WeaponColor;
				}
				else
				{
					Effect.Color[HIBYTE(RZKEY_1 + i)][LOBYTE(RZKEY_1 + i)] = BLACK;
				}
			}
		}

		Result = CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_CUSTOM, &Effect, NULL);
		
		if (bHasUDamage)
		{
			if (!bPlayingUDamage)
			{
				ChromaSDK::Headset::BREATHING_EFFECT_TYPE HeadsetEffect = {};
				HeadsetEffect.Color = PURPLE;
				Result = CreateHeadsetEffect(ChromaSDK::Headset::CHROMA_BREATHING, &HeadsetEffect, NULL);

				ChromaSDK::Mouse::BREATHING_EFFECT_TYPE MouseEffect = {};
				MouseEffect.Type = ChromaSDK::Mouse::BREATHING_EFFECT_TYPE::Type::ONE_COLOR;
				MouseEffect.Color1 = PURPLE;
				MouseEffect.LEDId = RZLED_ALL;
				Result = CreateMouseEffect(ChromaSDK::Mouse::CHROMA_BREATHING, &MouseEffect, NULL);

				ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE MousepadEffect = {};
				MousepadEffect.Type = ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
				MousepadEffect.Color1 = PURPLE;
				MousepadEffect.Color2 = WHITE;
				Result = CreateMousepadEffect(ChromaSDK::Mousepad::CHROMA_BREATHING, &MousepadEffect, NULL);

				bPlayingUDamage = true;
			}
			bPlayingBerserk = false;
			bPlayingShieldBelt = false;
		}
		else if (bHasShieldBelt)
		{
			if (!bPlayingShieldBelt)
			{
				ChromaSDK::Headset::BREATHING_EFFECT_TYPE HeadsetEffect = {};
				HeadsetEffect.Color = YELLOW;
				Result = CreateHeadsetEffect(ChromaSDK::Headset::CHROMA_BREATHING, &HeadsetEffect, NULL);

				ChromaSDK::Mouse::BREATHING_EFFECT_TYPE MouseEffect = {};
				MouseEffect.Type = ChromaSDK::Mouse::BREATHING_EFFECT_TYPE::Type::ONE_COLOR;
				MouseEffect.Color1 = YELLOW;
				MouseEffect.LEDId = RZLED_ALL;
				Result = CreateMouseEffect(ChromaSDK::Mouse::CHROMA_BREATHING, &MouseEffect, NULL);

				ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE MousepadEffect = {};
				MousepadEffect.Type = ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
				MousepadEffect.Color1 = YELLOW;
				MousepadEffect.Color2 = WHITE;
				Result = CreateMousepadEffect(ChromaSDK::Mousepad::CHROMA_BREATHING, &MousepadEffect, NULL);

				bPlayingShieldBelt = true;
			}
			bPlayingBerserk = false;
			bPlayingUDamage = false;
		}
		else if (bHasBerserk)
		{
			if (!bPlayingBerserk)
			{
				ChromaSDK::Headset::BREATHING_EFFECT_TYPE HeadsetEffect = {};
				HeadsetEffect.Color = RED;
				Result = CreateHeadsetEffect(ChromaSDK::Headset::CHROMA_BREATHING, &HeadsetEffect, NULL);

				ChromaSDK::Mouse::BREATHING_EFFECT_TYPE MouseEffect = {};
				MouseEffect.Type = ChromaSDK::Mouse::BREATHING_EFFECT_TYPE::Type::ONE_COLOR;
				MouseEffect.Color1 = RED;
				MouseEffect.LEDId = RZLED_ALL;
				Result = CreateMouseEffect(ChromaSDK::Mouse::CHROMA_BREATHING, &MouseEffect, NULL);

				ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE MousepadEffect = {};
				MousepadEffect.Type = ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
				MousepadEffect.Color1 = RED;
				MousepadEffect.Color2 = WHITE;
				Result = CreateMousepadEffect(ChromaSDK::Mousepad::CHROMA_BREATHING, &MousepadEffect, NULL);

				bPlayingBerserk = true;
			}
			bPlayingShieldBelt = false;
			bPlayingUDamage = false;
		}
		else
		{
			bPlayingShieldBelt = false;
			bPlayingUDamage = false;
			bPlayingBerserk = false;

			ChromaSDK::Headset::STATIC_EFFECT_TYPE HeadsetEffect = {};
			HeadsetEffect.Color = TeamColor;
			Result = CreateHeadsetEffect(ChromaSDK::Headset::CHROMA_STATIC, &HeadsetEffect, NULL);

			ChromaSDK::Mouse::STATIC_EFFECT_TYPE MouseEffect = {};
			MouseEffect.Color = TeamColor;
			MouseEffect.LEDId = RZLED_ALL;
			Result = CreateMouseEffect(ChromaSDK::Mouse::CHROMA_STATIC, &MouseEffect, NULL);

			ChromaSDK::Mousepad::STATIC_EFFECT_TYPE MousepadEffect = {};
			MousepadEffect.Color = TeamColor;
			Result = CreateMousepadEffect(ChromaSDK::Mousepad::CHROMA_STATIC, &MousepadEffect, NULL);
		}
	}
	else
	{
		UpdateIdleColors(DeltaTime);
	}
}

void FRazerChroma::UpdateIdleColors(float DeltaTime)
{
	/*
	SetEffect(UnrealTextScroller[TextScrollerFrame]);

	TextScrollerDeltaTimeAccumulator += DeltaTime;
	if (TextScrollerDeltaTimeAccumulator > TextScrollerFrameTimeMinimum)
	{
		TextScrollerDeltaTimeAccumulator = 0;
		TextScrollerFrame++;
		TextScrollerFrame %= UNREALTEXTSCROLLERFRAMES;
	}
	*/

	if (!bPlayingIdleColors)
	{
		// Might need to check that this isn't already set
		RZRESULT Result = RZRESULT_INVALID;

		ChromaSDK::Keyboard::BREATHING_EFFECT_TYPE KeyboardEffect = {};
		KeyboardEffect.Type = ChromaSDK::Keyboard::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
		KeyboardEffect.Color1 = GREEN;
		KeyboardEffect.Color2 = BLACK;
		Result = CreateKeyboardEffect(ChromaSDK::Keyboard::CHROMA_BREATHING, &KeyboardEffect, NULL);

		ChromaSDK::Headset::BREATHING_EFFECT_TYPE HeadsetEffect = {};
		HeadsetEffect.Color = GREEN;
		Result = CreateHeadsetEffect(ChromaSDK::Headset::CHROMA_BREATHING, &HeadsetEffect, NULL);

		ChromaSDK::Mouse::BREATHING_EFFECT_TYPE MouseEffect = {};
		MouseEffect.Type = ChromaSDK::Mouse::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
		MouseEffect.Color1 = GREEN;
		MouseEffect.Color2 = BLACK;
		MouseEffect.LEDId = RZLED_ALL;
		Result = CreateMouseEffect(ChromaSDK::Mouse::CHROMA_BREATHING, &MouseEffect, NULL);

		ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE MousepadEffect = {};
		MousepadEffect.Type = ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE::Type::TWO_COLORS;
		MousepadEffect.Color1 = GREEN;
		MousepadEffect.Color2 = BLACK;
		Result = CreateMousepadEffect(ChromaSDK::Mousepad::CHROMA_BREATHING, &MousepadEffect, NULL);

		bPlayingIdleColors = true;
		bPlayingShieldBelt = false;
		bPlayingUDamage = false;
		bPlayingBerserk = false;
	}
}