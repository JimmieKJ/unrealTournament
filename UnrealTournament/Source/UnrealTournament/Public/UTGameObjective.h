// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCharacter.h"
#include "UTPathBuilderInterface.h"
#include "UTTeamInterface.h"

#include "UTGameObjective.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTGameObjective : public AActor, public IUTPathBuilderInterface, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	float InitialSpawnDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	TSubclassOf<AUTCarriedObject> CarriedObjectClass;

	/** Best angle to view this objective (for spectator cams). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
		float BestViewYaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Team)
	uint8 TeamNum;

	/**
	 *	Called from UTGameMode - Initializes all of the GameObjectives before PostBeginPlay because
	 *  PostBeginPlay for them would happen outside of the normal expected order
	 **/
	virtual void InitializeObjective();

	/**	This is a server-side function that is called by CarriedObject when it's state is changed.*/
	virtual void ObjectStateWasChanged(FName NewObjectState);

	/**	Will be called via the GameObject when it has been picked up.*/
	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome);

	/**	Will be called via the GameObject when this object has been dropped*/
	virtual void ObjectWasDropped(AUTCharacter* LastHolder);

	/**	Will be called when the GameObject has been returned home*/
	virtual void ObjectReturnedHome(AUTCharacter* Returner);

	/**	@returns the carried object*/
	virtual AUTCarriedObject* GetCarriedObject() const;

	/**	@returns the state of the owned UTCarriedObject*/
	virtual FName GetCarriedObjectState() const;

	/**	@returns the PlayerState of the UTCharacter holding CarriedObject otherwise returns NULL*/
	virtual AUTPlayerState* GetCarriedObjectHolder();

	/** Within this distance is considered last second save. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	float LastSecondSaveDistance;

	/** Return true if Other is within LastSecondSaveDistance. */
	virtual bool ActorIsNearMe(AActor *Other) const;

	virtual uint8 GetTeamNum() const
	{
		return TeamNum;
	}

	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{
		TeamNum = NewTeamNum;
		if (Role == ROLE_Authority)
		{
			if (CarriedObject != NULL)
			{
				CarriedObject->Destroy();
			}
			CreateCarriedObject();
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != NULL)
			{
				Game->GameObjectiveInitialized(this);
			}
		}
	}
protected:

	// Holds the actual object
	UPROPERTY(BlueprintReadOnly, Replicated, Category = GameObject)
	AUTCarriedObject* CarriedObject;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameObject)
	AUTPlayerState* CarriedObjectHolder;
	
	/**	The owned game object replicates it's state via it's base.*/
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnObjectStateChanged, Category = GameObject)
	FName CarriedObjectState;

	virtual void CreateCarriedObject();

	/**	Called when CarriedObject's state changes and is replicated to the client*/
	UFUNCTION()
	virtual void OnObjectStateChanged();
};