// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Engine/TriggerBase.h"
#include "TriggerSphere.generated.h"

/** A sphere shaped trigger, used to generate overlap events in the level */
UCLASS(MinimalAPI)
class ATriggerSphere : public ATriggerBase
{
	GENERATED_UCLASS_BODY()


#if WITH_EDITOR
	// Begin AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	// End AActor interface.
#endif
};



