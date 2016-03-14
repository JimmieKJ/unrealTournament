// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCheatManager.generated.h"

#if WITH_PROFILE

#include "McpQueryResult.h"

#endif

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
	virtual void PlayTaunt(const FString& TauntClass);

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
	virtual void ImpartWeaponSkin(const FString& Skin);

	UFUNCTION(exec)
	virtual void Ann(int32 Switch);

	UFUNCTION(exec)
	virtual void Teleport();

	virtual void BugItWorker(FVector TheLocation, FRotator TheRotation) override;

	virtual void God() override;

	UFUNCTION(exec)
	void McpGrantItem(const FString& ItemId);

	UFUNCTION(exec)
	void McpDestroyItem(const FString& ItemId);

	UFUNCTION(exec)
	void McpCheat();

	UFUNCTION(exec)
	void McpRefreshProfile();

	/** Adjust spread on all weapons (multiply by scaling). */
	UFUNCTION(exec)
	void Spread(float Scaling);

	UFUNCTION(exec)
	void MatchmakeMyParty(int32 PlaylistId);

#if WITH_PROFILE
	void LogWebResponse(const FMcpQueryResult& Response);
#endif

	UFUNCTION(exec)
	void TestPaths(bool bHighJumps, bool bWallDodges, bool bLifts, bool bLiftJumps);
};
