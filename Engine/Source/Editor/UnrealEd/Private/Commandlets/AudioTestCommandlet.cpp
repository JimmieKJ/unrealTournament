// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "UnrealAudioModule.h"
#include "UnrealAudioTests.h"

DEFINE_LOG_CATEGORY_STATIC(AudioTestCommandlet, Log, All);

#if ENABLE_UNREAL_AUDIO

/** Unreal audio module ptr */
static UAudio::IUnrealAudioModule* UnrealAudioModule = nullptr;

static bool UnrealAudioLoad(const FString* DeviceApi = nullptr)
{
	UnrealAudioModule = FModuleManager::LoadModulePtr<UAudio::IUnrealAudioModule>(FName("UnrealAudio"));
	if (DeviceApi)
	{
		UnrealAudioModule->Initialize(*DeviceApi);
	}
	else
	{
		UnrealAudioModule->Initialize();
	}
	return UnrealAudioModule != nullptr;
}

static bool UnrealAudioUnload()
{
	if (UnrealAudioModule)
	{
		UnrealAudioModule->Shutdown();
		return true;
	}
	return false;
}

/**
* Test static functions which call into module test code
*/

static bool TestAudioDeviceAll()
{
	if (!UAudio::TestDeviceQuery())
	{
		return false;
	}

	if (!UAudio::TestDeviceOutputSimple(10))
	{
		return false;
	}

	if (!UAudio::TestDeviceOutputRandomizedFm(10))
	{
		return false;
	}

	if (!UAudio::TestDeviceOutputNoisePan(10))
	{
		return false;
	}

	return true;
}

static bool TestAudioDeviceQuery()
{
	return UAudio::TestDeviceQuery();
}

static bool TestAudioDeviceOutputSimple()
{
	return UAudio::TestDeviceOutputSimple(-1);
}

static bool TestAudioDeviceOutputFm()
{
	return UAudio::TestDeviceOutputRandomizedFm(-1);
}

static bool TestAudioDeviceOutputPan()
{
	return UAudio::TestDeviceOutputNoisePan(-1);
}

/**
* Setting up commandlet code
*/

typedef bool AudioTestFunction();

struct AudioTestInfo
{
	FString CategoryName;
	FString TestName;
	bool (*TestFunction)();
};

enum EAudioTests
{
	AUDIO_TEST_DEVICE_ALL,
	AUDIO_TEST_DEVICE_QUERY,
	AUDIO_TEST_DEVICE_OUTPUT_SIMPLE,
	AUDIO_TEST_DEVICE_OUTPUT_FM,
	AUDIO_TEST_DEVICE_OUTPUT_PAN,
	AUDIO_TESTS
};

static AudioTestInfo AudioTestInfoList[] =
{
	{ "device", "all",			TestAudioDeviceAll			}, // AUDIO_TEST_DEVICE_ALL
	{ "device", "query",		TestAudioDeviceQuery		}, // AUDIO_TEST_DEVICE_QUERY
	{ "device", "out",			TestAudioDeviceOutputSimple }, // AUDIO_TEST_DEVICE_OUTPUT_SIMPLE
	{ "device", "out_fm",		TestAudioDeviceOutputFm		}, // AUDIO_TEST_DEVICE_OUTPUT_FM
	{ "device", "out_pan",		TestAudioDeviceOutputPan	}, // AUDIO_TEST_DEVICE_OUTPUT_PAN
};
static_assert(ARRAY_COUNT(AudioTestInfoList) == AUDIO_TESTS, "Mismatched info list and test enumeration");

static void PrintUsage()
{
	UE_LOG(AudioTestCommandlet, Display, TEXT("AudioTestCommandlet Usage: {Editor}.exe UnrealEd.AudioTestCommandlet {testcategory} {test}"));
	UE_LOG(AudioTestCommandlet, Display, TEXT("Possible Tests: [testcategory]: [test]"));
	for (uint32 Index = 0; Index < AUDIO_TESTS; ++Index)
	{
		UE_LOG(AudioTestCommandlet, Display, TEXT("    %s: %s"), *AudioTestInfoList[Index].CategoryName, *AudioTestInfoList[Index].TestName);
	}
}

#endif // #if ENABLE_UNREAL_AUDIO

// -- UAudioTestCommandlet Functions -------------------

UAudioTestCommandlet::UAudioTestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UAudioTestCommandlet::Main(const FString& InParams)
{
#if ENABLE_UNREAL_AUDIO
    
    // Mac stupidly is adds "-NSDocumentRevisionsDebugMode YES" to command line args so lets remove that
    FString Params;
#if PLATFORM_MAC
    FString BadString(TEXT("-NSDocumentRevisionsDebugMode YES"));
    int32 DebugModeIndex = InParams.Find(BadString);
    if (DebugModeIndex != INDEX_NONE)
    {
        Params = InParams.LeftChop(BadString.Len());
    }
    else
#endif // #if PLATFORM_MAC
    {
        Params = InParams;
    }
   
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() < 3 || Tokens.Num() > 4)
	{
		PrintUsage();
		return 0;
	}
	int32 CategoryNameIndex = 1;
	int32 TestNameIndex = 2;

	if (Tokens.Num() == 4)
	{
		FString DeviceApiName(Tokens[1]);
		if (!UnrealAudioLoad(&DeviceApiName))
		{
			UE_LOG(AudioTestCommandlet, Display, TEXT("Failed to load unreal audio module. Exiting."));
			return 0;
		}
		CategoryNameIndex++;
		TestNameIndex++;
	}
	else
	{
		if (!UnrealAudioLoad())
		{
			UE_LOG(AudioTestCommandlet, Display, TEXT("Failed to load unreal audio module. Exiting."));
			return 0;
		}
	}

	check(UnrealAudioModule != nullptr);

	bool FoundTest = false;
	for (uint32 Index = 0; Index < AUDIO_TESTS; ++Index)
	{
		if (AudioTestInfoList[Index].CategoryName == Tokens[CategoryNameIndex])
		{
			if (AudioTestInfoList[Index].TestName == Tokens[TestNameIndex])
			{
				FoundTest = true;
				if (AudioTestInfoList[Index].TestFunction())
				{
					UE_LOG(AudioTestCommandlet, Display, TEXT("Test %s succeeded."), *AudioTestInfoList[Index].TestName);
				}
				else
				{
					UE_LOG(AudioTestCommandlet, Display, TEXT("Test %s failed."), *AudioTestInfoList[Index].TestName);
				}
				break;
			}
		}
	}

	if (!FoundTest)
	{
		UE_LOG(AudioTestCommandlet, Display, TEXT("Unknown category or test. Exiting."));
		return 0;
	}

	UnrealAudioUnload();
#else // #if ENABLE_UNREAL_AUDIO
	UE_LOG(AudioTestCommandlet, Display, TEXT("Unreal Audio Module Not Enabled For This Platform"));
#endif // #else // #if ENABLE_UNREAL_AUDIO


	return 0;
}
