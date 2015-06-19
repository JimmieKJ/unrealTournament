// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetsPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "SlateBasics.h"

SAssetDiscoveryIndicator::~SAssetDiscoveryIndicator()
{
	if ( FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")) )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFileLoadProgressUpdated().RemoveAll( this );
	}
}

void SAssetDiscoveryIndicator::Construct( const FArguments& InArgs )
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFileLoadProgressUpdated().AddRaw( this, &SAssetDiscoveryIndicator::OnAssetRegistryFileLoadProgress );

	ScaleMode = InArgs._ScaleMode;

	FadeAnimation = FCurveSequence();
	FadeAnimation.AddCurve(0.f, 4.f); // Add some space at the beginning to delay before fading in
	ScaleCurve = FadeAnimation.AddCurve(4.f, 0.75f);
	FadeCurve = FadeAnimation.AddCurve(4.75f, 0.75f);
	FadeAnimation.AddCurve(5.5f, 1.f); // Add some space at the end to cause a short delay before fading out

	if ( AssetRegistryModule.Get().IsLoadingAssets() )
	{
		// Loading assets, show the progress
		Progress = 0.f;

		if ( InArgs._FadeIn )
		{
			FadeAnimation.Play( this->AsShared() );
		}
		else
		{
			FadeAnimation.JumpToEnd();
		}
	}
	else
	{
		// Already done loading assets, set to complete and don't play the complete animation
		Progress = 1.f;
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(InArgs._Padding)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			.BorderBackgroundColor(this, &SAssetDiscoveryIndicator::GetBorderBackgroundColor)
			.ColorAndOpacity(this, &SAssetDiscoveryIndicator::GetIndicatorColorAndOpacity)
			.DesiredSizeScale(this, &SAssetDiscoveryIndicator::GetIndicatorDesiredSizeScale)
			.Visibility(this, &SAssetDiscoveryIndicator::GetIndicatorVisibility)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Text
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(12, 4)
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("AssetDiscoveryIndicator.DiscovertingAssetsFont") )
					.Text( NSLOCTEXT("AssetDiscoveryIndicator", "DiscoveringAssets", "Discovering Assets") )
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
				]

				// Progress bar
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(12, 4)
				[
					SNew(SProgressBar)
					.Percent( this, &SAssetDiscoveryIndicator::GetProgress )
				]
			]
		]
	];
}

void SAssetDiscoveryIndicator::OnAssetRegistryFileLoadProgress(int32 NumDiscoveredAssets, int32 TotalAssets)
{
	if ( Progress < 1.f )
	{
		Progress = NumDiscoveredAssets / (float)TotalAssets;

		if ( Progress >= 1.f )
		{
			FadeAnimation.PlayReverse(this->AsShared());
		}
	}
}

TOptional<float> SAssetDiscoveryIndicator::GetProgress() const
{
	return Progress;
}

FSlateColor SAssetDiscoveryIndicator::GetBorderBackgroundColor() const
{
	return FSlateColor(FLinearColor(1, 1, 1, 0.8f * FadeCurve.GetLerp()));
}

FLinearColor SAssetDiscoveryIndicator::GetIndicatorColorAndOpacity() const
{
	return FLinearColor(1, 1, 1, FadeCurve.GetLerp());
}

FVector2D SAssetDiscoveryIndicator::GetIndicatorDesiredSizeScale() const
{
	const float Lerp = ScaleCurve.GetLerp();

	switch (ScaleMode)
	{
	case EAssetDiscoveryIndicatorScaleMode::Scale_Horizontal: return FVector2D(Lerp, 1);
	case EAssetDiscoveryIndicatorScaleMode::Scale_Vertical: return FVector2D(1, Lerp);
	case EAssetDiscoveryIndicatorScaleMode::Scale_Both: return FVector2D(Lerp, Lerp);
	default:
		return FVector2D(1, 1);
	}
}

EVisibility SAssetDiscoveryIndicator::GetIndicatorVisibility() const
{
	return FadeAnimation.IsAtStart() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}