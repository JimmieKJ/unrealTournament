// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCheatManager.generated.h"

UCLASS(Within=UTPlayerController)
class UNREALTOURNAMENT_API UUTCheatManager : public UCheatManager
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(exec)
	virtual void Sum();

	UFUNCTION(exec)
	virtual void AllAmmo();

	UFUNCTION(exec)
		virtual void Gibs();

	UFUNCTION(exec)
	virtual void UnlimitedAmmo();

	UFUNCTION(exec)
	virtual void Loaded();

	// Alias to UnlimitedAmmo
	UFUNCTION(exec)
	virtual void ua();

	UFUNCTION(exec)
	virtual void SetChar(const FString& NewChar);

	UFUNCTION(exec)
	virtual void SetHat(const FString& Hat);

	UFUNCTION(exec)
	virtual void ImpartHats(const FString& Hat);

	UFUNCTION(exec)
	virtual void SetEyewear(const FString& Eyewear);

	UFUNCTION(exec)
	virtual void ImpartEyewear(const FString& Eyewear);

	UFUNCTION(exec)
	virtual void Ann(int32 Switch);

	virtual void BugItWorker(FVector TheLocation, FRotator TheRotation) override;

	virtual void God() override;
};
