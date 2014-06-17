// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameState.generated.h"

UCLASS(minimalapi)
class AUTGameState : public AGameState
{
	GENERATED_UCLASS_BODY()

	/** If TRUE, then we weapon pick ups to stay on their base */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bWeaponStay:1;

	/** If TRUE, then the player has to signal ready before they can start */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bPlayerMustBeReady:1;

	/** If a single player's (or team's) score hits this limited, the game is over */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = GameState)
	uint32 GoalScore;

	/** The maximum amount of time the game will be */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 TimeLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> MultiKillMessageClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> SpreeMessageClass;
	/** amount of time between kills to qualify as a multikill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	float MultiKillDelay;

	// If true, we will stop counting down time
	UPROPERTY(Replicated, BlueprintReadWrite, Category = GameState)
	uint32 bStopCountdown:1;

	// Used to sync the time on clients to the server. -- See DefaultTimer()
	UPROPERTY(Replicated)
	uint32 RemainingMinute;

	/** How much time is remaining in this match. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	uint32 RemainingTime;

	// How long must a player wait before respawning
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 RespawnWaitTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	AUTPlayerState* WinnerPlayerState;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetTimeLimit(float NewTimeLimit);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetGoalScore(uint32 NewGoalScore);

	/** Called once per second (or so depending on TimeDilation) after RemainingTime() has been replicated */
	virtual void DefaultTimer();

	/** Determines if a player is on the same team */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool OnSameTeam(class APlayerState* Player1, class APlayerState* Player2);

	/** Determines if 2 PlayerStates are in score order */
	virtual bool InOrder( class AUTPlayerState* P1, class AUTPlayerState* P2 );

	/** Sorts the Player State Array */
	virtual void SortPRIArray();

	/** Returns true if the match state is InProgress or later */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasMatchStarted() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInProgress() const;
	virtual bool IsMatchInOvertime() const;

	virtual void BeginPlay() OVERRIDE;

	/** add an overlay to the OverlayMaterials list */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void AddOverlayMaterial(UMaterialInterface* NewOverlay);
	/** find an overlay in the OverlayMaterials list, return its index */
	int32 FindOverlayMaterial(UMaterialInterface* TestOverlay)
	{
		for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
		{
			if (OverlayMaterials[i] == TestOverlay)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
	/** get overlay material from index */
	UMaterialInterface* GetOverlayMaterial(int32 Index)
	{
		if (Index >= 0 && Index < ARRAY_COUNT(OverlayMaterials))
		{
			return OverlayMaterials[Index];
		}
	}
	/** returns first active overlay material given the passed in flags */
	UMaterialInterface* GetFirstOverlay(uint16 Flags)
	{
		// early out
		if (Flags == 0)
		{
			return NULL;
		}
		else
		{
			for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
			{
				if (Flags & (1 << i))
				{
					return OverlayMaterials[i];
				}
			}
			return NULL;
		}
	}

protected:
	/** overlay materials, mapped to bits in UTCharacter's OverlayFlags/WeaponOverlayFlags and used to efficiently handle character/weapon overlays
	 * only replicated at startup so set any used materials via BeginPlay()
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Effects)
	UMaterialInterface* OverlayMaterials[16];
};



