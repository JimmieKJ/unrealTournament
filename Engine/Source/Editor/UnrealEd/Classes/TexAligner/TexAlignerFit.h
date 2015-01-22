// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// TexAlignerFit
// Fits the texture to a face
// 
//=============================================================================

#pragma once
#include "TexAlignerFit.generated.h"

UCLASS(hidecategories=Object)
class UTexAlignerFit : public UTexAligner
{
	GENERATED_UCLASS_BODY()


	// Begin UObject Interface
	virtual void PostInitProperties() override;
	// End UObject Interface

	// Begin UTexAligner Interface
	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal ) override;
	// End UTexAligner Interface
};

