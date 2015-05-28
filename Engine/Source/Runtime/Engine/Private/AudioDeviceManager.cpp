// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Sound/SoundWave.h"

// Private consts for helping with index/generation determination in audio device manager
static const uint32 AUDIO_DEVICE_HANDLE_INDEX_BITS		= 24;
static const uint32 AUDIO_DEVICE_HANDLE_INDEX_MASK		= (1 << AUDIO_DEVICE_HANDLE_INDEX_BITS) - 1;
static const uint32 AUDIO_DEVICE_HANDLE_GENERATION_BITS = 8;
static const uint32 AUDIO_DEVICE_HANDLE_GENERATION_MASK = (1 << AUDIO_DEVICE_HANDLE_GENERATION_BITS) - 1;

static const uint16 AUDIO_DEVICE_MINIMUM_FREE_AUDIO_DEVICE_INDICES = 32;

// Invalid handle used for initializing audio device handles
static const uint32 AUDIO_DEVICE_HANDLE_INVALID			= ~0u;

// The number of multiple audio devices allowed by default
static const uint32 AUDIO_DEVICE_DEFAULT_ALLOWED_DEVICE_COUNT = 2;

// The max number of audio devices allowed
static const uint32 AUDIO_DEVICE_MAX_DEVICE_COUNT = 8;

/*-----------------------------------------------------------------------------
FAudioDeviceManager implementation.
-----------------------------------------------------------------------------*/

FAudioDeviceManager::FAudioDeviceManager()
	: AudioDeviceModule(nullptr)
	, FreeIndicesSize(0)
	, NumActiveAudioDevices(0)
	, NumWorldsUsingMainAudioDevice(0)
	, NextResourceID(1)
	, SoloDeviceHandle(INDEX_NONE)
	, ActiveAudioDeviceHandle(INDEX_NONE)
{
}

FAudioDeviceManager::~FAudioDeviceManager()
{
	// Confirm that we freed all the audio devices
	check(NumActiveAudioDevices == 0);

	// Release any loaded buffers - this calls stop on any sources that need it
	for (int32 Index = Buffers.Num() - 1; Index >= 0; Index--)
	{
		FreeBufferResource(Buffers[Index]);
	}
}

void FAudioDeviceManager::RegisterAudioDeviceModule(IAudioDeviceModule* AudioDeviceModuleInput)
{
	// Currently can't have multiple audio device modules registered
	check(AudioDeviceModule == nullptr);
	AudioDeviceModule = AudioDeviceModuleInput;
}

FAudioDevice* FAudioDeviceManager::CreateAudioDevice(uint32& HandleOut, bool bCreateNewDevice)
{
	// If we don't have an audio device module, then we can't create new audio devices.
	if (AudioDeviceModule == nullptr)
	{
		HandleOut = AUDIO_DEVICE_HANDLE_INVALID;
		return nullptr;
	}

	// If we are running without the editor, we only need one audio device.
	if (!GIsEditor)
	{
		if (NumActiveAudioDevices == 1)
		{
			FAudioDevice* MainAudioDevice = GEngine->GetMainAudioDevice();
			if (MainAudioDevice)
			{
				HandleOut = MainAudioDevice->DeviceHandle;
				return MainAudioDevice;
			}
			return nullptr;
		}
	}

	FAudioDevice* NewAudioDevice = nullptr;

	if (NumActiveAudioDevices < AUDIO_DEVICE_DEFAULT_ALLOWED_DEVICE_COUNT || (bCreateNewDevice && NumActiveAudioDevices < AUDIO_DEVICE_MAX_DEVICE_COUNT))
	{
		// Create the new audio device and make sure it succeeded
		NewAudioDevice = AudioDeviceModule->CreateAudioDevice();
		if (NewAudioDevice == nullptr)
		{
			HandleOut = AUDIO_DEVICE_HANDLE_INVALID;
			return nullptr;
		}

		// Now generation a new audio device handle for the device and store the
		// ptr to the new device in the array of audio devices.

		uint32 AudioDeviceIndex(AUDIO_DEVICE_HANDLE_INVALID);

		// First check to see if we should start recycling audio device indices, if not
		// then we add a new entry to the Generation array and generate a new index
		if (FreeIndicesSize > AUDIO_DEVICE_MINIMUM_FREE_AUDIO_DEVICE_INDICES)
		{
			FreeIndices.Dequeue(AudioDeviceIndex);
			--FreeIndicesSize;
			check(int32(AudioDeviceIndex) < Devices.Num());
			check(Devices[AudioDeviceIndex] == nullptr);
			Devices[AudioDeviceIndex] = NewAudioDevice;
		}
		else
		{
			// Add a zeroth generation entry in the Generation array, get a brand new
			// index and append the created device to the end of the Devices array

			Generations.Add(0);
			AudioDeviceIndex = Generations.Num() - 1;
			check(AudioDeviceIndex < (1 << AUDIO_DEVICE_HANDLE_INDEX_BITS));
			Devices.Add(NewAudioDevice);
		}

		HandleOut = CreateHandle(AudioDeviceIndex, Generations[AudioDeviceIndex]);

		// Store the handle on the audio device itself
		NewAudioDevice->DeviceHandle = HandleOut;
	}
	else
	{
		++NumWorldsUsingMainAudioDevice;
		FAudioDevice* MainAudioDevice = GEngine->GetMainAudioDevice();
		if (MainAudioDevice)
		{
			HandleOut = MainAudioDevice->DeviceHandle;
			NewAudioDevice = MainAudioDevice;
		}
	}

	++NumActiveAudioDevices;

	return NewAudioDevice;
}

bool FAudioDeviceManager::IsValidAudioDeviceHandle(uint32 Handle) const
{
	if (AudioDeviceModule == nullptr || Handle == AUDIO_DEVICE_HANDLE_INVALID)
	{
		return false;
	}

	uint32 Index = GetIndex(Handle);
	if (int32(Index) >= Generations.Num())
	{
		return false;
	}

	uint8 Generation = GetGeneration(Handle);
	return Generations[Index] == Generation;
}

bool FAudioDeviceManager::ShutdownAudioDevice(uint32 Handle)
{
	if (!IsValidAudioDeviceHandle(Handle))
	{
		return false;
	}

	check(NumActiveAudioDevices > 0);
	--NumActiveAudioDevices;

	// If there are more than 1 device active, check to see if this handle is the main audio device handle
	if (NumActiveAudioDevices >= 1)
	{
		uint32 MainDeviceHandle = GEngine->GetAudioDeviceHandle();

		if (NumActiveAudioDevices == 1)
		{
			// If we only have one audio device left, then set the active
			// audio device to be the main audio device
			SetActiveDevice(MainDeviceHandle);
		}

		// If this is the main device handle and there's more than one reference to the main device, 
		// don't shut it down until it's the very last handle to get shut down
		// this is because it's possible for some PIE sessions to be using the main audio device as a fallback to 
		// preserve CPU performance on low-performance machines
		if (NumWorldsUsingMainAudioDevice > 0 && MainDeviceHandle == Handle)
		{
			--NumWorldsUsingMainAudioDevice;

			return true;
		}
	}

	uint32 Index = GetIndex(Handle);
	uint8 Generation = GetGeneration(Handle);

	check(int32(Index) < Generations.Num());

	// Bump up the generation at the given index. This will invalidate
	// the handle without needing to broadcast to everybody who might be using the handle
	Generations[Index] = ++Generation;

	// Make sure we have a non-null device ptr in the index slot, then delete it
	FAudioDevice* AudioDevice = Devices[Index];
	check(AudioDevice != nullptr);

    // Tear down the audio device
	AudioDevice->Teardown();

	delete AudioDevice;

	// Nullify the audio device slot for future audio device creations
	Devices[Index] = nullptr;

	// Add this index to the list of free indices
	++FreeIndicesSize;
	FreeIndices.Enqueue(Index);

	return true;
}

bool FAudioDeviceManager::ShutdownAllAudioDevices()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			ShutdownAudioDevice(AudioDevice->DeviceHandle);
		}
	}

	check(NumActiveAudioDevices == 0);
	check(NumWorldsUsingMainAudioDevice == 0);

	return true;
}

class FAudioDevice* FAudioDeviceManager::GetAudioDevice(uint32 Handle)
{
	if (!IsValidAudioDeviceHandle(Handle))
	{
		return nullptr;
	}

	uint32 Index = GetIndex(Handle);
	check(int32(Index) < Devices.Num());
	FAudioDevice* AudioDevice = Devices[Index];
	check(AudioDevice != nullptr);
	return AudioDevice;
}

class FAudioDevice* FAudioDeviceManager::GetActiveAudioDevice()
{
	if (ActiveAudioDeviceHandle != INDEX_NONE)
	{
		return GetAudioDevice(ActiveAudioDeviceHandle);
	}
	return GEngine->GetMainAudioDevice();
}

void FAudioDeviceManager::UpdateActiveAudioDevices(bool bGameTicking)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->Update(bGameTicking);
		}
	}
}

void FAudioDeviceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->AddReferencedObjects(Collector);
		}
	}
}

void FAudioDeviceManager::StopSoundsUsingWave(class USoundWave* InSoundWave)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			TArray<UAudioComponent*> StoppedComponents;
			AudioDevice->StopSoundsUsingResource(InSoundWave, StoppedComponents);
		}
	}
}

void FAudioDeviceManager::StopSoundsUsingResource(class USoundWave* InSoundWave, TArray<UAudioComponent*>& StoppedComponents)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->StopSoundsUsingResource(InSoundWave, StoppedComponents);
		}
	}
}

void FAudioDeviceManager::RegisterSoundClass(USoundClass* SoundClass)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->RegisterSoundClass(SoundClass);
		}
	}
}

void FAudioDeviceManager::UnregisterSoundClass(USoundClass* SoundClass)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->UnregisterSoundClass(SoundClass);
		}
	}
}

void FAudioDeviceManager::InitSoundClasses()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->InitSoundClasses();
		}
	}
}

void FAudioDeviceManager::SetActiveDevice(uint32 InAudioDeviceHandle)
{
	// Only change the active device if there are no solo'd audio devices
	if (SoloDeviceHandle == INDEX_NONE)
	{
		for (FAudioDevice* AudioDevice : Devices)
		{
			if (AudioDevice)
			{
				if (AudioDevice->DeviceHandle == InAudioDeviceHandle)
				{
					ActiveAudioDeviceHandle = InAudioDeviceHandle;
					AudioDevice->bIsDeviceMuted = false;
				}
				else
				{
					AudioDevice->bIsDeviceMuted = true;
				}
			}
		}
	}
}

void FAudioDeviceManager::SetSoloDevice(uint32 InAudioDeviceHandle)
{
	SoloDeviceHandle = InAudioDeviceHandle;
	if (SoloDeviceHandle != INDEX_NONE)
	{
		for (FAudioDevice* AudioDevice : Devices)
		{
			if (AudioDevice)
			{
				// Un-mute the active audio device and mute non-active device, as long as its not the main audio device (which is used to play UI sounds)
				if (AudioDevice->DeviceHandle == InAudioDeviceHandle)
				{
					ActiveAudioDeviceHandle = InAudioDeviceHandle;
					AudioDevice->bIsDeviceMuted = false;
				}
				else
				{
					AudioDevice->bIsDeviceMuted = true;
				}
			}
		}
	}
}


uint8 FAudioDeviceManager::GetNumActiveAudioDevices() const
{
	return NumActiveAudioDevices;
}

uint8 FAudioDeviceManager::GetNumMainAudioDeviceWorlds() const
{
	return NumWorldsUsingMainAudioDevice;
}

uint32 FAudioDeviceManager::GetIndex(uint32 Handle) const
{
	return Handle & AUDIO_DEVICE_HANDLE_INDEX_MASK;
}

uint32 FAudioDeviceManager::GetGeneration(uint32 Handle) const
{
	return (Handle >> AUDIO_DEVICE_HANDLE_INDEX_BITS) & AUDIO_DEVICE_HANDLE_GENERATION_MASK;
}

uint32 FAudioDeviceManager::CreateHandle(uint32 DeviceIndex, uint8 Generation)
{
	return (DeviceIndex | (Generation << AUDIO_DEVICE_HANDLE_INDEX_BITS));
}

void FAudioDeviceManager::StopSourcesUsingBuffer(FSoundBuffer* SoundBuffer)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->StopSourcesUsingBuffer(SoundBuffer);
		}
	}
}

void FAudioDeviceManager::TrackResource(USoundWave* SoundWave, FSoundBuffer* Buffer)
{
	// Allocate new resource ID and assign to USoundWave. A value of 0 (default) means not yet registered.
	int32 ResourceID = NextResourceID++;
	Buffer->ResourceID = ResourceID;
	SoundWave->ResourceID = ResourceID;

	Buffers.Add(Buffer);
	WaveBufferMap.Add(ResourceID, Buffer);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Keep track of associated resource name.
	Buffer->ResourceName = SoundWave->GetPathName();
#endif
}

void FAudioDeviceManager::FreeResource(USoundWave* SoundWave)
{
	if (SoundWave->ResourceID)
	{
		FSoundBuffer* SoundBuffer = WaveBufferMap.FindRef(SoundWave->ResourceID);
		FreeBufferResource(SoundBuffer);

		SoundWave->ResourceID = 0;
	}
}

void FAudioDeviceManager::FreeBufferResource(FSoundBuffer* SoundBuffer)
{
	if (SoundBuffer)
	{
		Buffers.Remove(SoundBuffer);

		// Stop any sound sources on any audio device currently using this buffer before deleting
		StopSourcesUsingBuffer(SoundBuffer);

		delete SoundBuffer;
		SoundBuffer = nullptr;
	}
}

FSoundBuffer* FAudioDeviceManager::GetSoundBufferForResourceID(uint32 ResourceID)
{
	return WaveBufferMap.FindRef(ResourceID);
}

void FAudioDeviceManager::RemoveSoundBufferForResourceID(uint32 ResourceID)
{
	WaveBufferMap.Remove(ResourceID);
}

void FAudioDeviceManager::RemoveSoundMix(USoundMix* SoundMix)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->RemoveSoundMix(SoundMix);
		}
	}
}
