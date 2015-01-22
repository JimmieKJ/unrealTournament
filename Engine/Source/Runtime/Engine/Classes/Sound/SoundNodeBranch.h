// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeBranch.generated.h"

/** 
 * Selects a child node based on the value of a boolean parameter
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Branch" ))
class USoundNodeBranch : public USoundNode
{
	GENERATED_UCLASS_BODY()

	/** The name of the boolean parameter to use to determine which branch we should take */
	UPROPERTY(EditAnywhere, Category=Branch)
	FName BoolParameterName;

private:

	enum BranchPurpose
	{
		ParameterTrue,
		ParameterFalse,
		ParameterUnset,
		MAX
	};

public:
	// Begin USoundNode interface.
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual int32 GetMaxChildNodes( void ) const override 
	{ 
		return BranchPurpose::MAX; 
	}
	virtual int32 GetMinChildNodes() const override
	{ 
		return BranchPurpose::MAX;
	}
	virtual void CreateStartingConnectors( void ) override;

	virtual void RemoveChildNode( int32 Index ) override
	{
		// Do nothing - we do not want to allow deleting of children nodes
	}

#if WITH_EDITOR
	virtual FString GetInputPinName(int32 PinIndex) const override;
	virtual FString GetTitle() const override;
#endif //WITH_EDITOR
	// End USoundNode interface.
};



