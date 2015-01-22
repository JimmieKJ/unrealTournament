// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SphereComponent.h"
#include "DrawSphereComponent.generated.h"

/** 
 * A sphere generally used for simple collision. Bounds are rendered as lines in the editor.
 */
UCLASS(collapsecategories, hidecategories=Object, editinlinenew, MinimalAPI)
class UDrawSphereComponent : public USphereComponent
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
};



