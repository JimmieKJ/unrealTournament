// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "UTGameInstance.generated.h"

UCLASS(MinimalAPI)
class UUTGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

	virtual void Init() override;
	virtual void StartGameInstance() override;

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;
};

