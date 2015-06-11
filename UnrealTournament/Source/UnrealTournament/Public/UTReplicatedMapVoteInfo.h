// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTPlayerState.h"
#include "UTReplicatedMapVoteInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTReplicatedMapVoteInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	// The weapon
	UPROPERTY()
	FString MapPackage;

	UPROPERTY(Replicated)
	FString MapTitle;

	UPROPERTY(Replicated)
	FString MapScreenshotReference;

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

protected:
	TArray<AUTPlayerState*> VoterRegistry;

	UFUNCTION()
	virtual void OnRep_VoteCount();


};



