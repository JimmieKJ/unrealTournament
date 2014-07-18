// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameObjective.generated.h"

UCLASS()
class AUTGameObjective : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	float InitialSpawnDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	TSubclassOf<AUTCarriedObject> CarriedObjectClass;

	

	/**
	 *	Called from UTGameMode - Initializes all of the GameObjectives before PostBeginPlay because
	 *  PostBeginPlay for them would happen outside of the normal expected order
	 **/
	virtual void InitializeObjective();


	/**
	 *	Will be called via the GameObject when it has been picked up.
	 **/
	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder);

	/**
	 *	Will be called when the GameObject has been returned home
	 **/
	virtual void ObjectReturnedHome(AUTCharacter* Returner);

	/**
	 *	@returns the state of the owned UTCarriedObject
	 **/
	virtual FName GetObjectState();

	virtual AUTCarriedObject* GetCarriedObject() 
	{
		return MyCarriedObject;
	}

protected:

	// Holds the actual object
	UPROPERTY(BlueprintReadOnly, Replicated, Category = GameObject)
	AUTCarriedObject* MyCarriedObject;

	virtual void CreateCarriedObject();

};