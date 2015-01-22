// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE "Paper2D"

/////////////////////////////////////////////////////
// UPaperSpriteFactory

UPaperSpriteFactory::UPaperSpriteFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseSourceRegion(false)
	, InitialSourceUV(0, 0)
	, InitialSourceDimension(0, 0)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPaperSprite::StaticClass();
}

bool UPaperSpriteFactory::ConfigureProperties()
{
	//@TODO: Maybe create a texture picker here?
	return true;
}

UObject* UPaperSpriteFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPaperSprite* NewSprite = ConstructObject<UPaperSprite>(Class, InParent, Name, Flags | RF_Transactional);

	FSpriteAssetInitParameters SpriteInitParams;
	SpriteInitParams.bNewlyCreated = true;
	if (bUseSourceRegion)
	{
		SpriteInitParams.Texture = InitialTexture;
		SpriteInitParams.Offset = InitialSourceUV;
		SpriteInitParams.Dimension = InitialSourceDimension;
	}
	else
	{
		SpriteInitParams.SetTextureAndFill(InitialTexture);
	}
	NewSprite->InitializeSprite(SpriteInitParams);

	return NewSprite;
}

#undef LOCTEXT_NAMESPACE