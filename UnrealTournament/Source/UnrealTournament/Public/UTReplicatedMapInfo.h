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
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
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

	// What rounds are this weapon available in
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_VoteCount)
	int32 VoteCount;

	bool bNeedsUpdate;

#if !UE_SERVER
	FSlateDynamicImageBrush* MapBrush;
#endif

	UPROPERTY()
	UTexture2D* MapScreenshot;

public:
	void RegisterVoter(AUTPlayerState* Voter);
	void UnregisterVoter(AUTPlayerState* Voter);

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

protected:
	TArray<AUTPlayerState*> VoterRegistry;

	UFUNCTION()
	virtual void OnRep_VoteCount();

	UFUNCTION()
	virtual void OnRep_MapScreenshotReference();

	virtual void PreLoadScreenshot();
	void MapTextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
};



