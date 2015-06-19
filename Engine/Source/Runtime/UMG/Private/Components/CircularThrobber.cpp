// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "Slate/SlateBrushAsset.h"
#include "SThrobber.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UCircularThrobber

UCircularThrobber::UCircularThrobber(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SCircularThrobber::FArguments DefaultArgs;
	Image = *DefaultArgs._PieceImage;

	NumberOfPieces = DefaultArgs._NumPieces;
	Period = DefaultArgs._Period;
	Radius = DefaultArgs._Radius;
}

void UCircularThrobber::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyCircularThrobber.Reset();
}

TSharedRef<SWidget> UCircularThrobber::RebuildWidget()
{
	SCircularThrobber::FArguments DefaultArgs;

	MyCircularThrobber = SNew(SCircularThrobber)
		.PieceImage(&Image)
		.NumPieces(FMath::Clamp(NumberOfPieces, 1, 25))
		.Period(Period)
		.Radius(Radius);

	return MyCircularThrobber.ToSharedRef();
}

void UCircularThrobber::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyCircularThrobber->SetNumPieces(FMath::Clamp(NumberOfPieces, 1, 25));
	MyCircularThrobber->SetPeriod(Period);
	MyCircularThrobber->SetRadius(Radius);
}

void UCircularThrobber::SetNumberOfPieces(int32 InNumberOfPieces)
{
	NumberOfPieces = InNumberOfPieces;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetNumPieces(FMath::Clamp(NumberOfPieces, 1, 25));
	}
}

void UCircularThrobber::SetPeriod(float InPeriod)
{
	Period = InPeriod;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetPeriod(InPeriod);
	}
}

void UCircularThrobber::SetRadius(float InRadius)
{
	Radius = InRadius;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetRadius(InRadius);
	}
}

void UCircularThrobber::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( PieceImage_DEPRECATED != nullptr )
		{
			Image = PieceImage_DEPRECATED->Brush;
			PieceImage_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UCircularThrobber::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.CircularThrobber");
}

const FText UCircularThrobber::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
