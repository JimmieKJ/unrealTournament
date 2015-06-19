// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataAsset.generated.h"


/**
 * Base class for a simple asset containing data.
 */
UCLASS(abstract,MinimalAPI)
class UDataAsset
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// UObject interface

#if WITH_EDITORONLY_DATA
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
#endif
};
