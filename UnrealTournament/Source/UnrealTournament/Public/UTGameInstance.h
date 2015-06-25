// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "UTGameInstance.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

	virtual void StartGameInstance() override;

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;

	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString ) override;

	virtual void ReplayErrorConfirm( TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID );
};

