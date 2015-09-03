// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "UTGameInstance.generated.h"

enum EUTNetControlMessage
{
	UNMT_Redirect = 0, // required download from redirect
};

UCLASS()
class UNREALTOURNAMENT_API UUTGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

	virtual void StartGameInstance() override;

	virtual void HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr) override;

	bool IsAutoDownloadingContent();

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;

	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString ) override;

	virtual void RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	TArray<TWeakPtr<class SUWRedirectDialog>> ActiveRedirectDialogs;
};

