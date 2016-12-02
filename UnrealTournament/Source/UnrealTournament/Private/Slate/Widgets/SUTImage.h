// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "../SUTStyle.h"

#if !UE_SERVER

DECLARE_DELEGATE_RetVal_OneParam( FReply, FUTButtonClick, int32 );
DECLARE_DELEGATE_OneParam( FUTMouseOver, int32 );

class UNREALTOURNAMENT_API SUTImage : public SImage
{
	SLATE_BEGIN_ARGS(SUTImage)
		: _Image( FCoreStyle::Get().GetDefaultBrush() )
		, _ColorAndOpacity( FLinearColor::White )
		, _OnMouseButtonDown()
		, _WidthOverride(-1.0f)
		, _HeightOverride(-1.0f)
		{}

		/** Image resource */
		SLATE_ATTRIBUTE( const FSlateBrush*, Image )

		/** Color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Invoked when the mouse is pressed in the widget. */
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )

		SLATE_ARGUMENT( float, WidthOverride)
		SLATE_ARGUMENT( float, HeightOverride)

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override;

protected:
	float WidthOverride;
	float HeightOverride;

};

#endif