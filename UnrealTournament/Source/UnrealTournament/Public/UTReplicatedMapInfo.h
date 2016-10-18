// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTPlayerState.h"
#include "UTReplicatedMapInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTReplicatedMapInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	virtual void PreInitializeComponents() override;

	// The weapon
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset, ReplicatedUsing = OnRep_MapPackageName)
	FString MapPackageName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString MapAssetName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Title;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Author;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Description;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	int32 OptimalPlayerCount;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	int32 OptimalTeamPlayerCount;

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_MapScreenshotReference, BlueprintReadOnly, Category = Ruleset)
	FString MapScreenshotReference;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FPackageRedirectReference Redirect;

	// Will be true if this map is a fully meshed map
	UPROPERTY()
	bool bIsMeshedMap;

	// Will be true if this is an Epic official map
	UPROPERTY()
	bool bIsEpicMap;

	// What rounds are this weapon available in
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_VoteCount)
	int32 VoteCount;

	bool bNeedsUpdate;

#if !UE_SERVER
	FSlateDynamicImageBrush* MapBrush;
#endif

	UPROPERTY()
	UTexture2D* MapScreenshot;

	UPROPERTY()
	bool bHasRights;

public:
	void RegisterVoter(AUTPlayerState* Voter);
	void UnregisterVoter(AUTPlayerState* Voter);

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

#if !UE_SERVER
	virtual TSharedRef<SWidget> BuildMapOverlay(FVector2D Size, bool bIgnoreLock = false);
#endif

protected:
	TArray<AUTPlayerState*> VoterRegistry;

	UFUNCTION()
	virtual void OnRep_VoteCount();

	UFUNCTION()
	virtual void OnRep_MapScreenshotReference();

	UFUNCTION()
	virtual void OnRep_MapPackageName();

	virtual void PreLoadScreenshot();
	void MapTextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
};



