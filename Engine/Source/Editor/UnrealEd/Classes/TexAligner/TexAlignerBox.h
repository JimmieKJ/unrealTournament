// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// TexAlignerBox
// Aligns to the best U and V axis according to the polys normal.
//
//=============================================================================

#pragma once
#include "TexAlignerBox.generated.h"

UCLASS(hidecategories=Object)
class UTexAlignerBox : public UTexAligner
{
	GENERATED_UCLASS_BODY()


	// Begin UObject Interface
	virtual void PostInitProperties() override;
	// End UObject Interface

	// Begin UTexAligner Interface
	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal ) override;
	// End UTexAligner Interface
};

