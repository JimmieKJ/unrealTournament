// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VoicePrivatePCH.h"
#include "VoiceCaptureWindows.h"

#include "AllowWindowsPlatformTypes.h"

struct FVoiceCaptureWindowsVars
{
	/** Voice capture device */
	LPDIRECTSOUNDCAPTURE8 VoiceCaptureDev;
	/** Voice capture device caps */
	DSCCAPS VoiceCaptureDevCaps;
	/** Voice capture buffer */
	LPDIRECTSOUNDCAPTUREBUFFER8 VoiceCaptureBuffer8;
	/** Wave format of buffer */
	WAVEFORMATEX WavFormat;
	/** Buffer description */
	DSCBUFFERDESC VoiceCaptureBufferDesc;
	/** Buffer caps */
	DSCBCAPS VoiceCaptureBufferCaps8;
	/** Notification events */
	HANDLE Events[NUM_EVENTS];
	/** Current audio position of valid data in capture buffer */
	DWORD NextCaptureOffset;

	FVoiceCaptureWindowsVars() :
		VoiceCaptureDev(NULL),
		VoiceCaptureBuffer8(NULL),
		NextCaptureOffset(0)
	{
		FMemory::Memzero(&VoiceCaptureDevCaps, sizeof(VoiceCaptureDevCaps));

		FMemory::Memzero(&WavFormat, sizeof(WavFormat));
		FMemory::Memzero(&VoiceCaptureBufferDesc, sizeof(VoiceCaptureBufferDesc));
		FMemory::Memzero(&VoiceCaptureBufferCaps8, sizeof(VoiceCaptureBufferCaps8));
		FMemory::Memzero(&Events, ARRAY_COUNT(Events) * sizeof(HANDLE));

		for (int i = 0; i < NUM_EVENTS; i++)
		{
			Events[i] = INVALID_HANDLE_VALUE;
		}
	}
};

/** Calculate silence in an audio buffer by using a RMS threshold */
template<typename T>
bool IsSilence(T* Buffer, int32 BuffSize)
{
	if (BuffSize == 0)
	{
		return true;
	} 

	const int32 IterSize = BuffSize / sizeof(T);

	double Total = 0.0f;
	for (int32 i = 0; i < IterSize; i++) 
	{
		Total += Buffer[i];
	}

	const double Average = Total / IterSize;

	double SumMeanSquare = 0.0f;
	double Diff = 0.0f;
	for (int32 i = 0; i < IterSize; i++) 
	{
		Diff = (Buffer[i] - Average);
		SumMeanSquare += (Diff * Diff);
	}

	double AverageMeanSquare = SumMeanSquare / IterSize;

	static double Threshold = 75.0 * 75.0;
	return AverageMeanSquare < Threshold;
}

FVoiceCaptureWindows::FVoiceCaptureWindows() :
	CV(NULL),
	LastEventTriggered(STOP_EVENT - 1),
	VoiceCaptureState(EVoiceCaptureState::UnInitialized)
{
	CV = new FVoiceCaptureWindowsVars();
	UncompressedAudioBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
}

FVoiceCaptureWindows::~FVoiceCaptureWindows()
{
	Shutdown();

	FVoiceCaptureDeviceWindows* VoiceCaptureDev = FVoiceCaptureDeviceWindows::Get();
 	if (VoiceCaptureDev)	
	{
 		VoiceCaptureDev->FreeVoiceCaptureObject(this);
 	}
	
	if (CV)
	{
		delete CV;
		CV = NULL;
	}	
}

bool FVoiceCaptureWindows::Init(int32 SampleRate, int32 NumChannels)
{
	if (SampleRate < 8000 || SampleRate > 48000)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Voice capture doesn't support %d hz"), SampleRate);
		return false;
	}

	if (NumChannels < 0 || NumChannels > 2)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Voice capture only supports 1 or 2 channels"));
		return false;
	}

	FVoiceCaptureDeviceWindows* VoiceDev = FVoiceCaptureDeviceWindows::Get();
	if (!VoiceDev)
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("No voice capture interface."));
		return false;
	}

	// DSDEVID_DefaultCapture WAVEINCAPS 
	HRESULT hr = DirectSoundCaptureCreate8(&VoiceDev->VoiceCaptureDeviceGuid, &CV->VoiceCaptureDev, NULL);
	if (FAILED(hr))
	{
		//DSERR_ALLOCATED, DSERR_INVALIDPARAM, DSERR_NOAGGREGATION, DSERR_OUTOFMEMORY
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create capture device 0x%08x"), hr);
		return false;
	}

	// Device capabilities
	CV->VoiceCaptureDevCaps.dwSize = sizeof(DSCCAPS);
	hr = CV->VoiceCaptureDev->GetCaps(&CV->VoiceCaptureDevCaps);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get mic device caps 0x%08x"), hr);
		return false;
	}

	// Wave format setup
	CV->WavFormat.wFormatTag = WAVE_FORMAT_PCM;
	CV->WavFormat.nChannels = NumChannels;
	CV->WavFormat.wBitsPerSample = 16;
	CV->WavFormat.nSamplesPerSec = SampleRate;
	CV->WavFormat.nBlockAlign = (CV->WavFormat.nChannels * CV->WavFormat.wBitsPerSample) / 8;
	CV->WavFormat.nAvgBytesPerSec = CV->WavFormat.nBlockAlign * CV->WavFormat.nSamplesPerSec;
	CV->WavFormat.cbSize = 0;

	// Buffer setup
	CV->VoiceCaptureBufferDesc.dwSize = sizeof(DSCBUFFERDESC);
	CV->VoiceCaptureBufferDesc.dwFlags = 0;
	CV->VoiceCaptureBufferDesc.dwBufferBytes = CV->WavFormat.nAvgBytesPerSec / 2; // .5 sec buffer
	CV->VoiceCaptureBufferDesc.dwReserved = 0;
	CV->VoiceCaptureBufferDesc.lpwfxFormat = &CV->WavFormat;
	CV->VoiceCaptureBufferDesc.dwFXCount = 0;
	CV->VoiceCaptureBufferDesc.lpDSCFXDesc = NULL;

	LPDIRECTSOUNDCAPTUREBUFFER VoiceBuffer = NULL;

	hr = CV->VoiceCaptureDev->CreateCaptureBuffer(&CV->VoiceCaptureBufferDesc, &VoiceBuffer, NULL);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice capture buffer 0x%08x"), hr);
		return false;
	}

	hr = VoiceBuffer->QueryInterface(IID_IDirectSoundCaptureBuffer8, (LPVOID*)&CV->VoiceCaptureBuffer8);
	VoiceBuffer->Release(); 
	VoiceBuffer = NULL;
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice capture buffer 0x%08x"), hr);
		return false;
	}

	CV->VoiceCaptureBufferCaps8.dwSize = sizeof(DSCBCAPS);
	hr = CV->VoiceCaptureBuffer8->GetCaps(&CV->VoiceCaptureBufferCaps8);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer caps 0x%08x"), hr);
		return false;
	}

	// TEST ------------------------
	if (0)
	{
		DWORD SizeWritten8 = 0;
		CV->VoiceCaptureBuffer8->GetFormat(NULL, sizeof(WAVEFORMATEX), &SizeWritten8);

		LPWAVEFORMATEX BufferFormat8 = (WAVEFORMATEX*)FMemory::Malloc(SizeWritten8);
		CV->VoiceCaptureBuffer8->GetFormat(BufferFormat8, SizeWritten8, &SizeWritten8);
		FMemory::Free(BufferFormat8);
	}
	// TEST ------------------------

	if (!CreateNotifications(CV->VoiceCaptureBufferCaps8.dwBufferBytes))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice buffer notifications"));
		return false;
	}

	VoiceCaptureState = EVoiceCaptureState::NotCapturing;
	return true;
}

void FVoiceCaptureWindows::Shutdown()
{
	Stop();

	for (int i = 0; i < ARRAY_COUNT(CV->Events); i++)
	{
		if (CV->Events[i] != INVALID_HANDLE_VALUE)
		{
			CloseHandle(CV->Events[i]);
			CV->Events[i] = INVALID_HANDLE_VALUE;
		}
	}

	// Free up DirectSound resources
	if (CV->VoiceCaptureBuffer8)
	{
		CV->VoiceCaptureBuffer8->Release();
		CV->VoiceCaptureBuffer8 = NULL;
	}

	if (CV->VoiceCaptureDev)
	{
		CV->VoiceCaptureDev->Release();
		CV->VoiceCaptureDev = NULL;
	}

	FMemory::Memzero(&CV->VoiceCaptureDevCaps, sizeof(CV->VoiceCaptureDevCaps));

	VoiceCaptureState = EVoiceCaptureState::UnInitialized;
}

bool FVoiceCaptureWindows::Start()
{
	check(VoiceCaptureState != EVoiceCaptureState::UnInitialized);

	HRESULT hr = CV->VoiceCaptureBuffer8->Start(DSCBSTART_LOOPING);
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to start capture 0x%08x"), hr);
		return false;
	}

	VoiceCaptureState = EVoiceCaptureState::NoData;
	return true;
}

void FVoiceCaptureWindows::Stop()
{
	if (CV->VoiceCaptureBuffer8 &&
		VoiceCaptureState != EVoiceCaptureState::Stopping && 
		VoiceCaptureState != EVoiceCaptureState::NotCapturing)
	{
		CV->VoiceCaptureBuffer8->Stop();
		VoiceCaptureState = EVoiceCaptureState::Stopping;
	}
}

bool FVoiceCaptureWindows::IsCapturing()
{		
	if (CV->VoiceCaptureBuffer8)
	{
		DWORD Status = 0;

		HRESULT hr = CV->VoiceCaptureBuffer8->GetStatus(&Status);
		if (FAILED(hr))
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer status 0x%08x"), hr);
		}

		// Status & DSCBSTATUS_LOOPING
		return Status & DSCBSTATUS_CAPTURING ? true : false;
	}

	return false;
}

EVoiceCaptureState::Type FVoiceCaptureWindows::GetCaptureState(uint32& OutAvailableVoiceData) const
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized &&
		VoiceCaptureState != EVoiceCaptureState::Error)
	{
		OutAvailableVoiceData = UncompressedAudioBuffer.Num();
	}
	else
	{
		OutAvailableVoiceData = 0;
	}

	return VoiceCaptureState;
}

void FVoiceCaptureWindows::ProcessData()
{
	DWORD CurrentCapturePos = 0;
	DWORD CurrentReadPos = 0;

	HRESULT hr = CV->VoiceCaptureBuffer8 ? CV->VoiceCaptureBuffer8->GetCurrentPosition(&CurrentCapturePos, &CurrentReadPos) : E_FAIL;
	if (FAILED(hr))
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to get voice buffer cursor position 0x%08x"), hr);
		VoiceCaptureState = EVoiceCaptureState::Error;
		return;
	}

	DWORD LockSize = ((CurrentReadPos - CV->NextCaptureOffset) + CV->VoiceCaptureBufferCaps8.dwBufferBytes) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;
	if(LockSize != 0) 
	{ 
		DWORD CaptureFlags = 0;
		DWORD CaptureLength = 0;
		void* CaptureData = NULL;
		DWORD CaptureLength2 = 0;
		void* CaptureData2 = NULL;
		hr = CV->VoiceCaptureBuffer8->Lock(CV->NextCaptureOffset, LockSize,
			&CaptureData, &CaptureLength, 
			&CaptureData2, &CaptureLength2, CaptureFlags);
		if (SUCCEEDED(hr))
		{
			CaptureLength = FMath::Min(CaptureLength, (DWORD)MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
			CaptureLength2 = FMath::Min(CaptureLength2, (DWORD)MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE - CaptureLength);

			UncompressedAudioBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
			UncompressedAudioBuffer.AddUninitialized(CaptureLength + CaptureLength2);

			uint8* AudioBuffer = UncompressedAudioBuffer.GetData();
			FMemory::Memcpy(AudioBuffer, CaptureData, CaptureLength);

			if (CaptureData2 && CaptureLength2 > 0)
			{
				FMemory::Memcpy(AudioBuffer + CaptureLength, CaptureData2, CaptureLength2);
			}

			CA_SUPPRESS(6385);
			CV->VoiceCaptureBuffer8->Unlock(CaptureData, CaptureLength, CaptureData2, CaptureLength2);

			// Move the capture offset forward.
			CV->NextCaptureOffset = (CV->NextCaptureOffset + CaptureLength) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;
			CV->NextCaptureOffset = (CV->NextCaptureOffset + CaptureLength2) % CV->VoiceCaptureBufferCaps8.dwBufferBytes;

			if (IsSilence((int16*)AudioBuffer, UncompressedAudioBuffer.Num()))
			{
				VoiceCaptureState = EVoiceCaptureState::NoData;
			}
			else
			{
				VoiceCaptureState = EVoiceCaptureState::Ok;
			}

#if !UE_BUILD_SHIPPING
			static double LastCapture = 0.0;
			double NewTime = FPlatformTime::Seconds();
			UE_LOG(LogVoiceCapture, VeryVerbose, TEXT("LastCapture: %f %s"), (NewTime - LastCapture) * 1000.0, EVoiceCaptureState::ToString(VoiceCaptureState));
			LastCapture = NewTime;
#endif
		}
		else
		{
			UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to lock voice buffer 0x%08x"), hr);
			VoiceCaptureState = EVoiceCaptureState::Error;
		}
	}
}

EVoiceCaptureState::Type FVoiceCaptureWindows::GetVoiceData(uint8* OutVoiceBuffer, const uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData)
{
	EVoiceCaptureState::Type NewMicState = VoiceCaptureState;
	OutAvailableVoiceData = 0;

	if (VoiceCaptureState == EVoiceCaptureState::Ok ||
		VoiceCaptureState == EVoiceCaptureState::Stopping)
	{
		OutAvailableVoiceData = UncompressedAudioBuffer.Num();
		if (InVoiceBufferSize >= OutAvailableVoiceData)
		{
			FMemory::Memcpy(OutVoiceBuffer, UncompressedAudioBuffer.GetData(), OutAvailableVoiceData);
			VoiceCaptureState = EVoiceCaptureState::NoData;
		}
		else
		{
			NewMicState = EVoiceCaptureState::BufferTooSmall;
		}
	}

	return NewMicState;
}

bool FVoiceCaptureWindows::CreateNotifications(uint32 BufferSize)
{
	bool bSuccess = true;

	LPDIRECTSOUNDNOTIFY8 NotifyInt = NULL;

	HRESULT hr = CV->VoiceCaptureBuffer8->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&NotifyInt);
	if (SUCCEEDED(hr))
	{
		DSBPOSITIONNOTIFY NotifyEvents[NUM_EVENTS];

		// Create events.
		for (int i = 0; i < NUM_EVENTS; i++)
		{
			CV->Events[i] = CreateEvent(NULL, true, false, NULL);
			if (CV->Events[i] == INVALID_HANDLE_VALUE)
			{
				UE_LOG(LogVoiceCapture, Warning, TEXT("Error creating sync event"));
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			// Evenly distribute notifications throughout the buffer
			uint32 NumSegments = NUM_EVENTS - 1; 
			uint32 BufferSegSize = BufferSize / NumSegments;
			for (uint32 i = 0; i < NumSegments; i++)
			{
				NotifyEvents[i].dwOffset = ((i + 1) * BufferSegSize) - 1;
				NotifyEvents[i].hEventNotify = CV->Events[i];
			}

			// when buffer stops
			NotifyEvents[STOP_EVENT].dwOffset = DSBPN_OFFSETSTOP;
			NotifyEvents[STOP_EVENT].hEventNotify = CV->Events[STOP_EVENT];

			hr = NotifyInt->SetNotificationPositions(NUM_EVENTS, NotifyEvents);
			if (FAILED(hr))
			{
				UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to set notifications 0x%08x"), hr);
				bSuccess = false;
			}

			float BufferSegMs = BufferSegSize / ((CV->WavFormat.nSamplesPerSec / 1000.0) * (CV->WavFormat.wBitsPerSample / 8.0));
			UE_LOG(LogVoiceCapture, Display, TEXT("%d notifications, every %d bytes [%f ms]"), NumSegments, BufferSegSize, BufferSegMs);
		}

		NotifyInt->Release();
	}
	else
	{
		UE_LOG(LogVoiceCapture, Warning, TEXT("Failed to create voice notification interface 0x%08x"), hr);
	}

	if (!bSuccess)
	{
		for (int i = 0; i < NUM_EVENTS; i++)
		{
			if (CV->Events[i] != INVALID_HANDLE_VALUE)
			{
				CloseHandle(CV->Events[i]);
				CV->Events[i] = INVALID_HANDLE_VALUE;
			}
		}
	}

	return bSuccess;
}

bool FVoiceCaptureWindows::Tick(float DeltaTime)
{
	if (VoiceCaptureState != EVoiceCaptureState::UnInitialized &&
		VoiceCaptureState != EVoiceCaptureState::NotCapturing)
	{
#if 1
		uint32 NextEvent = (LastEventTriggered + 1) % (NUM_EVENTS - 1);
		if (WaitForSingleObject(CV->Events[NextEvent], 0) == WAIT_OBJECT_0)
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogVoiceCapture, VeryVerbose, TEXT("Event %d Signalled"), NextEvent);
#endif
			ProcessData();

			LastEventTriggered = NextEvent;
			ResetEvent(CV->Events[NextEvent]);
		}
#else
		// All "non stop" events
		for (int32 EventIdx = 0; EventIdx < STOP_EVENT; EventIdx++)
		{
			if (WaitForSingleObject(Events[EventIdx], 0) == WAIT_OBJECT_0)
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogVoiceCapture, VeryVerbose, TEXT("Event %d Signalled"), EventIdx);
#endif
				if (LastEventTriggered != EventIdx)
				{
					ProcessData();
				}

				LastEventTriggered = EventIdx;
				ResetEvent(CV->Events[EventIdx]);
			}
		}
#endif

		if (CV->Events[STOP_EVENT] != INVALID_HANDLE_VALUE && WaitForSingleObject(CV->Events[STOP_EVENT], 0) == WAIT_OBJECT_0)
		{
			UE_LOG(LogVoiceCapture, Verbose, TEXT("Voice capture stopped"));
			ResetEvent(CV->Events[STOP_EVENT]);
			VoiceCaptureState = EVoiceCaptureState::NotCapturing;
		}
	}

	return true;
}

#include "HideWindowsPlatformTypes.h"