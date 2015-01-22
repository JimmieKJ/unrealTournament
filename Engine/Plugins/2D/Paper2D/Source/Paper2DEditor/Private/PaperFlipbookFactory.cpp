// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE "Paper2D"

/////////////////////////////////////////////////////
// UPaperFlipbookFactory

UPaperFlipbookFactory::UPaperFlipbookFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPaperFlipbook::StaticClass();
}

UObject* UPaperFlipbookFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPaperFlipbook* NewFlipbook = ConstructObject<UPaperFlipbook>(Class, InParent, Name, Flags | RF_Transactional);
	{
		FScopedFlipbookMutator EditLock(NewFlipbook);
		EditLock.KeyFrames = KeyFrames;
	}
	return NewFlipbook;
}

#undef LOCTEXT_NAMESPACE