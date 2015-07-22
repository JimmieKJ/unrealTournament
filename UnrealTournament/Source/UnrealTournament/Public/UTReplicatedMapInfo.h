// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTPlayerState.h"
#include "UTReplicatedMapInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTReplicatedMapInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	// The weapon
	UPROPERTY(Replicated)
	FString MapPackageName;

	UPROPERTY(Replicated)
	FString MapAssetName;

	UPROPERTY(Replicated)
	FString Title;

	UPROPERTY(Replicated)
	FString Author;

	UPROPERTY(Replicated)
	FString Description;

	UPROPERTY(Replicated)
	int32 OptimalPlayerCount;

	UPROPERTY(Replicated)
	int32 OptimalTeamPlayerCount;

	UPROPERTY(Replicated)
	FString MapScreenshotReference;

	UPROPERTY(Replicated)
	FPackageRedirectReference Redirect;

	// What rounds are this weapon available in
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_VoteCount)
	int32 VoteCount;

	bool bNeedsUpdate;

#if !UE_SERVER
	FSlateDynamicImageBrush* MapBrush;
#endif

public:
	void RegisterVoter(AUTPlayerState* Voter);
	void UnregisterVoter(AUTPlayerState* Voter);

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

protected:
	TArray<AUTPlayerState*> VoterRegistry;

	UFUNCTION()
	virtual void OnRep_VoteCount();


};



