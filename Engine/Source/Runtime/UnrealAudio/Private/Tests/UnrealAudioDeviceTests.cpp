// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioPrivate.h"
#include "UnrealAudioModule.h"
#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioTests.h"
#include "UnrealAudioTestGenerators.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	static FUnrealAudioModule* UnrealAudioModule = nullptr;
	static bool bTestInitialized = false;

	void FUnrealAudioModule::InitializeTests(FUnrealAudioModule* Module)
	{
		UnrealAudioModule = Module;
		bTestInitialized = true;
	}

	bool TestDeviceQuery()
	{
		if (!bTestInitialized)
		{
			return false;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Running audio device query test..."));

		IUnrealAudioDeviceModule* UnrealAudioDevice = UnrealAudioModule->GetDeviceModule();

		if (!UnrealAudioDevice)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed: No Audio Device Object."));
			return false;
		}

		
		UE_LOG(LogUnrealAudio, Display, TEXT("Querying which audio device API we're using..."));
		EDeviceApi::Type DeviceApi;
		if (!UnrealAudioDevice->GetDevicePlatformApi(DeviceApi))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed."));
			return false;
		}

		const TCHAR* ApiName = EDeviceApi::ToString(DeviceApi);
		UE_LOG(LogUnrealAudio, Display, TEXT("Success: Using %s"), ApiName);

		if (DeviceApi == EDeviceApi::DUMMY)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("This is a dummy API. Platform not implemented yet."));
			return true;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Querying the number of output devices for current system..."));
		uint32 NumOutputDevices = 0;
		if (!UnrealAudioDevice->GetNumOutputDevices(NumOutputDevices))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed."));
			return false;
		}
		UE_LOG(LogUnrealAudio, Display, TEXT("Success: %d Output Devices Found"), NumOutputDevices);

		UE_LOG(LogUnrealAudio, Display, TEXT("Retrieving output device info for each output device..."));
		for (uint32 DeviceIndex = 0; DeviceIndex < NumOutputDevices; ++DeviceIndex)
		{
			FDeviceInfo DeviceInfo;
			if (!UnrealAudioDevice->GetOutputDeviceInfo(DeviceIndex, DeviceInfo))
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("Failed for index %d."), DeviceIndex);
				return false;
			}

			UE_LOG(LogUnrealAudio, Display, TEXT("Device Index: %d"), DeviceIndex);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Name: %s"), *DeviceInfo.FriendlyName);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device FrameRate: %d"), DeviceInfo.FrameRate);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device NumChannels: %d"), DeviceInfo.NumChannels);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Default?: %s"), DeviceInfo.bIsSystemDefault ? TEXT("yes") : TEXT("no"));
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Native Format: %s"), EStreamFormat::ToString(DeviceInfo.StreamFormat));

			UE_LOG(LogUnrealAudio, Display, TEXT("Speaker Array:"));

			for (int32 Channel = 0; Channel < DeviceInfo.Speakers.Num(); ++Channel)
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("    %s"), ESpeaker::ToString(DeviceInfo.Speakers[Channel]));
			}
		}

		return true;
	}

	/**
	* TestCallback
	* Callback function used for all device test functions. 
	* Function takes input params and passes to the ITestGenerator object to generate output buffers.
	*/
	static bool TestCallback(FCallbackInfo& CallbackInfo)	
	{
		static int32 CurrentSecond = -1;
		int32 StreamSecond = (int32)CallbackInfo.StreamTime;
		if (StreamSecond != CurrentSecond)
		{
			if (CurrentSecond == -1)
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("Stream Time (seconds):"));
			}
			CurrentSecond = StreamSecond;
			UE_LOG(LogUnrealAudio, Display, TEXT("%d"), CurrentSecond);
		}

		// Sets any data used by lots of different generators in static data struct
		Test::UpdateCallbackData(CallbackInfo);

		// Cast the user data object to our own FSimpleFM object (since we know we used that when we created the stream)
		Test::IGenerator* Generator = (Test::IGenerator*)CallbackInfo.UserData;

		// Get the next buffer of output data
		Generator->GetNextBuffer(CallbackInfo);
		return true;
	}

	static bool DoOutputTest(const FString& TestName, double LifeTime, Test::IGenerator* Generator)
	{
		check(Generator);

		if (!bTestInitialized)
		{
			return false;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Running audio test: \"%s\"..."), *TestName);

		if (LifeTime > 0.0)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Time of test: %d (seconds)"), LifeTime);
		}
		else
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Time of test: (indefinite)"));
		}

		IUnrealAudioDeviceModule* UnrealAudioDevice = UnrealAudioModule->GetDeviceModule();

		EDeviceApi::Type DeviceApi;
		if (!UnrealAudioDevice->GetDevicePlatformApi(DeviceApi))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed."));
			return false;
		}
		if (DeviceApi == EDeviceApi::DUMMY)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("This is a dummy API. Platform not implemented yet."));
			return true;
		}

		uint32 DefaultDeviceIndex = 0;
		if (!UnrealAudioDevice->GetDefaultOutputDeviceIndex(DefaultDeviceIndex))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed to get default device index."));
			return false;
		}

		FDeviceInfo DeviceInfo;
		if (!UnrealAudioDevice->GetOutputDeviceInfo(DefaultDeviceIndex, DeviceInfo))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed to get default device info."));
			return false;
		}

		FCreateStreamParams CreateStreamParams;
		CreateStreamParams.OutputDeviceIndex = DefaultDeviceIndex;
		CreateStreamParams.CallbackFunction = &TestCallback;
		CreateStreamParams.UserData = (void*)Generator;
		CreateStreamParams.CallbackBlockSize = 1024;

		UE_LOG(LogUnrealAudio, Display, TEXT("Creating the stream..."));
		if (!UnrealAudioDevice->CreateStream(CreateStreamParams))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed to create a stream."));
			return false;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Starting the stream."));
		UnrealAudioDevice->StartStream();

		// Block until the synthesizer is done
		while (!Generator->IsDone())
		{
			FPlatformProcess::Sleep(1);
		}

		// Stop the output stream (which blocks until its actually freed)
		UE_LOG(LogUnrealAudio, Display, TEXT("Shutting down the stream."));
		if (!UnrealAudioDevice->StopStream())
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed to stop the device stream."));
			return false;
		}

		if (!UnrealAudioDevice->ShutdownStream())
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed to shut down the device stream."));
			return false;
		}

		// At this point audio device I/O is done
		UE_LOG(LogUnrealAudio, Display, TEXT("Success!"));
		return true;
	}

	bool TestDeviceOutputSimple(double LifeTime)
	{
		Test::FSimpleOutput SimpleOutput(LifeTime);
		return DoOutputTest("output simple test", LifeTime, &SimpleOutput);
	}

	bool TestDeviceOutputRandomizedFm(double LifeTime)
	{
		Test::FPhaseModulator RandomizedFmGenerator(LifeTime);
		return DoOutputTest("output randomized FM synthesis", LifeTime, &RandomizedFmGenerator);
	}

	bool TestDeviceOutputNoisePan(double LifeTime)
	{
		Test::FNoisePan SimpleWhiteNoisePan(LifeTime);
		return DoOutputTest("output white noise panner", LifeTime, &SimpleWhiteNoisePan);
	}

}

#endif // #if ENABLE_UNREAL_AUDIO
