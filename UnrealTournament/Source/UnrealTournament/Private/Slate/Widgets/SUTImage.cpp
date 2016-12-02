// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTImage.h"

#if !UE_SERVER

void SUTImage::Construct( const FArguments& InArgs )
{
	WidthOverride = InArgs._WidthOverride;
	HeightOverride = InArgs._HeightOverride;
	SImage::Construct( SImage::FArguments()
		.Image(InArgs._Image)
		.ColorAndOpacity(InArgs._ColorAndOpacity)
		.OnMouseButtonDown(InArgs._OnMouseButtonDown));
}


FVector2D SUTImage::ComputeDesiredSize( float ) const
{
	FVector2D FinalSize = FVector2D::ZeroVector;
	const FSlateBrush* ImageBrush = Image.Get();
	if (ImageBrush != nullptr)
	{
		FinalSize = ImageBrush->ImageSize;
	}

	if (WidthOverride >= 0.0f) FinalSize.X = WidthOverride;
	if (HeightOverride >= 0.0f) FinalSize.Y = HeightOverride;
	return FinalSize;
}



#endif