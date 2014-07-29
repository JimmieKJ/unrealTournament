// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCharacter.h"

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
	 *	This is a server-side function that is called by CarriedObject when it's state is changed.
	 **/
	virtual void ObjectStateWasChanged(FName NewObjectState);

	/**
	 *	Will be called via the GameObject when it has been picked up.
	 **/
	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder);

	/**
	 *	Will be called via the GameObject when this object has been dropped
	 **/
	virtual void ObjectWasDropped(AUTCharacter* LastHolder);

	/**
	 *	Will be called when the GameObject has been returned home
	 **/
	virtual void ObjectReturnedHome(AUTCharacter* Returner);

	/**
	 *	@returns the carried object
	 **/
	virtual AUTCarriedObject* GetCarriedObject();

	/**
	 *	@returns the state of the owned UTCarriedObject
	 **/
	virtual FName GetCarriedObjectState();

	/**
	 *	@returns the PlayerState of the UTCharacter holding CarriedObject otherwise returns NULL
	 **/
	virtual AUTPlayerState* GetCarriedObjectHolder();

protected:

	// Holds the actual object
	UPROPERTY(BlueprintReadOnly, Replicated, Category = GameObject)
	AUTCarriedObject* CarriedObject;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameObject)
	AUTPlayerState* CarriedObjectHolder;
	
	/**
	 *	The owned game object replicates it's state via it's base.
	 **/
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnObjectStateChanged, Category = GameObject)
	FName CarriedObjectState;

	virtual void CreateCarriedObject();

	/**
	 *	Called when CarriedObject's state changes and is replicated to the client
	 **/
	UFUNCTION()
	virtual void OnObjectStateChanged();

};