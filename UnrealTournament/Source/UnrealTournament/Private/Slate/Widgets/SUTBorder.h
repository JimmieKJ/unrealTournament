// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "../SUTStyle.h"

#if !UE_SERVER

DECLARE_DELEGATE(FUTBorderAnimEndDelegate);

class UNREALTOURNAMENT_API SUTBorder : public SBorder
{
	SLATE_BEGIN_ARGS(SUTBorder)
		: _Content()
		, _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _Padding( FMargin(2.0f) )
		, _OnMouseButtonDown()
		, _OnMouseButtonUp()
		, _OnMouseMove()
		, _OnMouseDoubleClick()
		, _BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
		, _ContentScale( FVector2D(1,1) )
		, _DesiredSizeScale( FVector2D(1,1) )
		, _ColorAndOpacity( FLinearColor(1,1,1,1) )
		, _BorderBackgroundColor( FLinearColor::White )
		, _ForegroundColor( FSlateColor::UseForeground() )
		, _ShowEffectWhenDisabled( true )
		{}

		SLATE_DEFAULT_SLOT( FArguments, Content )

		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )
		SLATE_ATTRIBUTE( FMargin, Padding )
		
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonUp )
		SLATE_EVENT( FPointerEventHandler, OnMouseMove )
		SLATE_EVENT( FPointerEventHandler, OnMouseDoubleClick )

		SLATE_ATTRIBUTE( const FSlateBrush*, BorderImage )

		SLATE_ATTRIBUTE( FVector2D, ContentScale )

		SLATE_ATTRIBUTE( FVector2D, DesiredSizeScale )

		/** ColorAndOpacity is the color and opacity of content in the border */
		SLATE_ATTRIBUTE( FLinearColor, ColorAndOpacity )
		/** BorderBackgroundColor refers to the actual color and opacity of the supplied border image.*/
		SLATE_ATTRIBUTE( FSlateColor, BorderBackgroundColor )
		/** The foreground color of text and some glyphs that appear as the border's content. */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		/** Whether or not to show the disabled effect when this border is disabled */
		SLATE_ATTRIBUTE( bool, ShowEffectWhenDisabled )

		SLATE_EVENT(FUTBorderAnimEndDelegate, OnAnimEnd)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	void Animate(FVector2D StartTransform, FVector2D EndTransform, float StartOpacity, float EndOpacity, float Duration);

protected:
	FUTBorderAnimEndDelegate OnAnimEnd;
	void UpdateAnim(float DeltaTime);

	bool bIsAnimating;
	TArray<FVector2D> AnimTransforms;
	TArray<float> AnimOpacity;
	float AnimTimer;
	float AnimDuration;
	

};

#endif