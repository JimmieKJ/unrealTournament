// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperFlipbook.h"
#include "PaperFlipbookFactory.generated.h"

/**
 * Factory for flipbooks
 */

UCLASS()
class PAPER2DEDITOR_API UPaperFlipbookFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	TArray<FPaperFlipbookKeyFrame> KeyFrames;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
