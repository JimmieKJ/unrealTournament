// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VoicePrivatePCH.h"
#include "VoiceCaptureWindows.h"
#include "VoiceCodecOpus.h"
#include "Voice.h"

FVoiceCaptureDeviceWindows* FVoiceCaptureDeviceWindows::Singleton = NULL;

/** Callback to access all the voice capture devices on the platform */
BOOL CALLBACK CaptureDeviceCallback(
	LPGUID lpGuid,
	LPCSTR lpcstrDescription,
	LPCSTR lpcstrModule,
	LPVOID lpContext
	)
{
	// @todo identify the proper device
	FVoiceCaptureWindows* VCPtr = (FVoiceCaptureWindows*)(lpContext);
	UE_LOG(LogVoiceCapture, Display, TEXT("Device: %s Desc: %s"), lpcstrDescription, lpcstrModule);
	return true;
}

FVoiceCaptureDeviceWindows::FVoiceCaptureDeviceWindows() :
	bInitialized(false),
	DirectSound(NULL)
{
}

FVoiceCaptureDeviceWindows::~FVoiceCaptureDeviceWindows()
{
	Shutdown();
}

FVoiceCaptureDeviceWindows* FVoiceCaptureDeviceWindows::Create()
{
	if (Singleton == NULL)
	{
		Singleton = new FVoiceCaptureDeviceWindows();
		if (!Singleton->Init())
		{
			Singleton->Destroy();
		}
	}

	return Singleton;
}

void FVoiceCaptureDeviceWindows::Destroy()
{
	if (Singleton)
	{
		// calls Shutdown() above
		delete Singleton;
		Singleton = NULL;
	}
}

FVoiceCaptureDeviceWindows* FVoiceCaptureDeviceWindows::Get()
{
	return Singleton;
}

bool FVoiceCaptureDeviceWindows::Init()
{
	HRESULT hr = DirectSoundCreate8(NULL, &DirectSound, NULL);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to init DirectSound %d"), hr);
		return false;
	}

	hr = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK)CaptureDeviceCallback, this);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to enumerate devices %d"), hr);
		return false;
	}

	bInitialized = true;
	return true;
}

void FVoiceCaptureDeviceWindows::Shutdown()
{
	// Close any active captures
	for (int32 CaptureIdx = 0; CaptureIdx < ActiveVoiceCaptures.Num(); CaptureIdx++)
	{
		IVoiceCapture* VoiceCapture = ActiveVoiceCaptures[CaptureIdx];
		VoiceCapture->Shutdown();
	}

	ActiveVoiceCaptures.Empty();

	// Free up DirectSound
	if (DirectSound)
	{
		DirectSound->Release();
		DirectSound = NULL;
	}

	bInitialized = false;
}

FVoiceCaptureWindows* FVoiceCaptureDeviceWindows::CreateVoiceCaptureObject()
{
	FVoiceCaptureWindows* NewVoiceCapture = NULL;

	if (bInitialized)
	{
		NewVoiceCapture = new FVoiceCaptureWindows();
		if (NewVoiceCapture->Init(VOICE_SAMPLE_RATE, NUM_VOICE_CHANNELS))
		{
			ActiveVoiceCaptures.Add(NewVoiceCapture);
		}
		else
		{
			NewVoiceCapture = NULL;
		}
	}

	return NewVoiceCapture;
}

void FVoiceCaptureDeviceWindows::FreeVoiceCaptureObject(IVoiceCapture* VoiceCaptureObj)
{
	if (VoiceCaptureObj != NULL)
	{
		int32 RemoveIdx = ActiveVoiceCaptures.Find(VoiceCaptureObj);
		if (RemoveIdx != INDEX_NONE)
		{
			ActiveVoiceCaptures.RemoveAtSwap(RemoveIdx);
		}
		else
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Trying to free unknown voice object"));
		}
	}
}

bool InitVoiceCapture()
{
	return FVoiceCaptureDeviceWindows::Create() != NULL;
}

void ShutdownVoiceCapture()
{
	FVoiceCaptureDeviceWindows::Destroy();
}

IVoiceCapture* CreateVoiceCaptureObject()
{
	FVoiceCaptureDeviceWindows* VoiceCaptureDev = FVoiceCaptureDeviceWindows::Get();
	return VoiceCaptureDev ? VoiceCaptureDev->CreateVoiceCaptureObject() : NULL;
}

IVoiceEncoder* CreateVoiceEncoderObject()
{
	FVoiceEncoderOpus* NewEncoder = new FVoiceEncoderOpus();
	if (!NewEncoder->Init(VOICE_SAMPLE_RATE, NUM_VOICE_CHANNELS))
	{
		delete NewEncoder;
		NewEncoder = NULL;
	}

	return NewEncoder; 
}

IVoiceDecoder* CreateVoiceDecoderObject()
{
	FVoiceDecoderOpus* NewDecoder = new FVoiceDecoderOpus();
	if (!NewDecoder->Init(VOICE_SAMPLE_RATE, NUM_VOICE_CHANNELS))
	{
		delete NewDecoder;
		NewDecoder = NULL;
	}

	return NewDecoder; 
}