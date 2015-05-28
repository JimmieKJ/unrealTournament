// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TextureEditorPrivatePCH.h"


UTextureEditorSettings::UTextureEditorSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, Background(TextureEditorBackground_Checkered)
	, BackgroundColor(FColor::Black)
	, CheckerColorOne(FColor(128, 128, 128))
	, CheckerColorTwo(FColor(64, 64, 64))
	, CheckerSize(32)
	, FitToViewport(true)
	, TextureBorderColor(FColor::White)
	, TextureBorderEnabled(true)
{ }
