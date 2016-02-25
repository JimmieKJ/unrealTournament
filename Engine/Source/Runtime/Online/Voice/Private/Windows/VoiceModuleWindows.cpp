// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VoicePrivatePCH.h"

#include "VoiceCaptureWindows.h"
#include "VoiceCodecOpus.h"
#include "Voice.h"

FVoiceCaptureDeviceWindows* FVoiceCaptureDeviceWindows::Singleton = NULL;

/** Helper for printing MS guids */
FString PrintMSGUID(LPGUID Guid)
{
	FString Result;
	if (Guid)
	{
		Result = FString::Printf(TEXT("{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}"),
			Guid->Data1, Guid->Data2, Guid->Data3,
			Guid->Data4[0], Guid->Data4[1], Guid->Data4[2], Guid->Data4[3],
			Guid->Data4[4], Guid->Data4[5], Guid->Data4[6], Guid->Data4[7]);
	}

	return Result;
}

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
	UE_LOG(LogVoiceCapture, Display, TEXT("Device: %s Desc: %s GUID: %s Context:0x%08x"), lpcstrDescription, lpcstrModule, *PrintMSGUID(lpGuid), lpContext);
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

class FAudioDuckingWindows
{
private:
	
	FAudioDuckingWindows() {}
	FAudioDuckingWindows(const FAudioDuckingWindows& Copy) {}
	~FAudioDuckingWindows() {}
	void operator=(const FAudioDuckingWindows&) {}

public:

	static HRESULT EnableDuckingOptOut(IMMDevice* pEndpoint, bool bDuckingOptOutChecked)
	{
		HRESULT hr = S_OK;

		// Activate session manager.
		IAudioSessionManager2* pSessionManager2 = NULL;
		if (SUCCEEDED(hr))
		{
			hr = pEndpoint->Activate(__uuidof(IAudioSessionManager2),
				CLSCTX_INPROC_SERVER,
				NULL,
				reinterpret_cast<void **>(&pSessionManager2));
		}

		IAudioSessionControl* pSessionControl = NULL;
		if (SUCCEEDED(hr))
		{
			hr = pSessionManager2->GetAudioSessionControl(NULL, 0, &pSessionControl);

			pSessionManager2->Release();
			pSessionManager2 = NULL;
		}

		IAudioSessionControl2* pSessionControl2 = NULL;
		if (SUCCEEDED(hr))
		{
			hr = pSessionControl->QueryInterface(
				__uuidof(IAudioSessionControl2),
				(void**)&pSessionControl2);

			pSessionControl->Release();
			pSessionControl = NULL;
		}

		//  Sync the ducking state with the specified preference.
		if (SUCCEEDED(hr))
		{
			hr = pSessionControl2->SetDuckingPreference((::BOOL)bDuckingOptOutChecked);
			if (FAILED(hr))
			{
				UE_LOG(LogVoiceCapture, Display, TEXT("Failed to duck audio endpoint. Error: %0x08x"), hr);
			}

			pSessionControl2->Release();
			pSessionControl2 = NULL;
		}
		return hr;
	}

	static bool UpdateAudioDucking(bool bDuckingOptOutChecked)
	{
		HRESULT hr = S_OK;

		//  Start with the default endpoint.
		IMMDeviceEnumerator* pDeviceEnumerator = NULL;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pDeviceEnumerator));

		if (SUCCEEDED(hr))
		{
			{
				IMMDevice* pEndpoint = NULL;
				hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
				if (SUCCEEDED(hr))
				{
					LPWSTR Desc;
					pEndpoint->GetId(&Desc);
					UE_LOG(LogVoiceCapture, Display, TEXT("%s ducking on audio device. Desc: %s"), bDuckingOptOutChecked ? TEXT("Disabling") : TEXT("Enabling"), Desc);
					CoTaskMemFree(Desc);

					FAudioDuckingWindows::EnableDuckingOptOut(pEndpoint, bDuckingOptOutChecked);
					pEndpoint->Release();
					pEndpoint = NULL;
				}
			}

			if (0) // reference for enumerating all endpoints in case its necessary
			{
				IMMDeviceCollection* pDeviceCollection = NULL;
				IMMDevice* pCollEndpoint = NULL;
				hr = pDeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDeviceCollection);
				if (SUCCEEDED(hr))
				{
					IPropertyStore *pProps = NULL;

					::UINT DeviceCount = 0;
					pDeviceCollection->GetCount(&DeviceCount);
					for (::UINT i = 0; i < DeviceCount; ++i)
					{
						hr = pDeviceCollection->Item(i, &pCollEndpoint);
						if (SUCCEEDED(hr) && pCollEndpoint)
						{
							LPWSTR Desc;
							pCollEndpoint->GetId(&Desc);

							hr = pCollEndpoint->OpenPropertyStore(STGM_READ, &pProps);
							if (SUCCEEDED(hr))
							{
								PROPVARIANT varName;
								// Initialize container for property value.
								PropVariantInit(&varName);

								// Get the endpoint's friendly-name property.
								hr = pProps->GetValue(
									PKEY_Device_FriendlyName, &varName);
								if (SUCCEEDED(hr))
								{
									// Print endpoint friendly name and endpoint ID.
									UE_LOG(LogVoiceCapture, Display, TEXT("%s ducking on audio device [%d]: \"%s\" (%s)"),
										bDuckingOptOutChecked ? TEXT("Disabling") : TEXT("Enabling"),
										i, varName.pwszVal, Desc);

									CoTaskMemFree(Desc);

									Desc = NULL;
									PropVariantClear(&varName);

									pProps->Release();
									pProps = NULL;
								}
							}

							FAudioDuckingWindows::EnableDuckingOptOut(pCollEndpoint, bDuckingOptOutChecked);

							pCollEndpoint->Release();
							pCollEndpoint = NULL;
						}
					}

					pDeviceCollection->Release();
					pDeviceCollection = NULL;
				}
			}

			pDeviceEnumerator->Release();
			pDeviceEnumerator = NULL;
		}

		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to duck audio endpoint. Error: %0x08x"), hr);
		}

		return SUCCEEDED(hr);
	}
};

bool FVoiceCaptureDeviceWindows::Init()
{
	HRESULT hr = DirectSoundCreate8(NULL, &DirectSound, NULL);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to init DirectSound %d"), hr);
		return false;
	}
	
	if (0)
	{
		hr = DirectSoundEnumerate((LPDSENUMCALLBACK)CaptureDeviceCallback, this);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to enumerate render devices %d"), hr);
			return false;
		}
	}

	hr = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK)CaptureDeviceCallback, this);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to enumerate capture devices %d"), hr);
		return false;
	}

	bool bDuckingOptOut = false;
	if (GConfig)
	{
		if (!GConfig->GetBool(TEXT("Voice"), TEXT("bDuckingOptOut"), bDuckingOptOut, GEngineIni))
		{
			bDuckingOptOut = false;
		}
	}
	FAudioDuckingWindows::UpdateAudioDucking(bDuckingOptOut);

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
