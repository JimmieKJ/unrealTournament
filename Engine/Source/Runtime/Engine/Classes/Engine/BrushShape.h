// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// BrushShape: A brush that acts as a template for geometry mode modifiers like "Lathe"
//=============================================================================

#pragma once
#include "BrushShape.generated.h"

UCLASS(MinimalAPI)
class ABrushShape : public ABrush
{
	GENERATED_UCLASS_BODY()


	virtual bool IsStaticBrush() const override {return false; }

	virtual bool IsBrushShape() const override {return true; }
};



