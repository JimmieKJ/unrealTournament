// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Visual.generated.h"

/** The base class for elements in UMG: slots and widgets. */
UCLASS()
class UMG_API UVisual : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren);

	// Begin UObject interface
	virtual void BeginDestroy() override;
	// End UObject interface
};
