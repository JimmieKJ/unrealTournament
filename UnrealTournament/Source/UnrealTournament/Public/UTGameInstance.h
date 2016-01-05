// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "../../Engine/Source/Runtime/PerfCounters/Private/PerfCounters.h"
#include "UTGameInstance.generated.h"

enum EUTNetControlMessage
{
	UNMT_Redirect = 0, // required download from redirect
};

UCLASS()
class UNREALTOURNAMENT_API UUTGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()
	
	virtual void Init();
	virtual void StartGameInstance() override;

	virtual void StartRecordingReplay(const FString& Name, const FString& FriendlyName, const TArray<FString>& AdditionalOptions = TArray<FString>()) override;
	virtual void PlayReplay(const FString& Name, UWorld* WorldOverride = nullptr, const TArray<FString>& AdditionalOptions = TArray<FString>()) override;

	virtual void HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr) override;

	bool IsAutoDownloadingContent();

	virtual bool DelayPendingNetGameTravel() override
	{
		return IsAutoDownloadingContent();
	}

	/** starts download for pak file from redirect if needed
	 * @return whether download was required */
	virtual bool StartRedirectDownload(const FString& PakName, const FString& URL, const FString& Checksum);

	inline void SetLastTriedDemo(const FString& NewName)
	{
		if (NewName != LastTriedDemo)
		{
			LastTriedDemo = NewName;
			bRetriedDemoAfterRedirects = false;
		}
	}
	inline FString GetLastTriedDemo() const
	{
		return LastTriedDemo;
	}


inline void InitPerfCounters()
{
#if !UE_SERVER
	
	IPerfCountersModule& PerfCountersModule = FModuleManager::LoadModuleChecked<IPerfCountersModule>("PerfCounters");
	IPerfCounters* PerfCounters = PerfCountersModule.CreatePerformanceCounters();
	
	if (PerfCounters != nullptr)
	{
		// Not exactly full version string, but the build number
		UE_LOG(UT,Log,TEXT("GEngineNetVersion %i"),GEngineNetVersion);
		PerfCounters->Set(TEXT("BuildVersion"), GEngineNetVersion);
	}
	else
	{
		UE_LOG(LogInit, Warning, TEXT("Could not initialize performance counters."));
	}
#endif
}

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;

	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString ) override;

	virtual void RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	TArray<TWeakPtr<class SUTRedirectDialog>> ActiveRedirectDialogs;

	// in order to handle demo redirects, we have to cancel the demo, download, then retry
	// this tracks the demo we need to retry
	FString LastTriedDemo;
	bool bRetriedDemoAfterRedirects;
};

