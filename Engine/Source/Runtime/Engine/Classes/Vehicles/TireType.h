// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
 * Represents a type of tire surface used to specify friction values against physical materials.
 */

#pragma once
#include "Engine/DataAsset.h"
#include "TireType.generated.h"

UCLASS()
class ENGINE_API UTireType : public UDataAsset
{
	GENERATED_UCLASS_BODY()

private:

	// Scale the tire friction for this tire type
	UPROPERTY(EditAnywhere, Category=TireType)
	float								FrictionScale;

private:

	// Tire type ID to pass to PhysX
	uint32								TireTypeID;

public:

	// All loaded tire types - used to assign each tire type a unique TireTypeID
	static TArray<TWeakObjectPtr<UTireType>>			AllTireTypes;

	/**
	 * Getter for FrictionScale
	 */
	float GetFrictionScale() { return FrictionScale; }

	/**
	 * Setter for FrictionScale
	 */
	void SetFrictionScale( float NewFrictionScale );

	/**
	 * Getter for TireTypeID
	 */
	int32 GetTireTypeID() { return TireTypeID; }

	/**
	 * Called after the C++ constructor and after the properties have been initialized, but before the config has been loaded, etc.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were intialized.
	 */
	virtual void PostInitProperties() override;
	
	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	virtual void BeginDestroy() override;

#if WITH_EDITOR

	/**
	 * Respond to a property change in editor
	 */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

#endif //WITH_EDITOR

protected:

	/**
	 * Add this tire type to the TireTypes array
	 */
	void NotifyTireFrictionUpdated();
};
