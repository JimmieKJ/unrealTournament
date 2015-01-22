// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Factory for flipbooks
 */

#pragma once
#include "PaperFlipbookFactory.generated.h"

UCLASS()
class PAPER2DEDITOR_API UPaperFlipbookFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	TArray<FPaperFlipbookKeyFrame> KeyFrames;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
