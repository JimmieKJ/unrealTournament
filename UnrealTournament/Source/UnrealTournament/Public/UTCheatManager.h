// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCheatManager.generated.h"

UCLASS(Within=UTPlayerController)
class UUTCheatManager : public UCheatManager
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(exec)
	virtual void AllAmmo();

	UFUNCTION(exec)
	virtual void UnlimitedAmmo();

	UFUNCTION(exec)
	virtual void Loaded();

	// Alias to UnlimitedAmmo
	UFUNCTION(exec)
	virtual void ua();

	UFUNCTION(exec)
	virtual void SetHat(const FString& Hat);

	UFUNCTION(exec)
	virtual void ImpartHats(const FString& Hat);

	UFUNCTION(exec)
	virtual void Ann(int32 Switch);
};
