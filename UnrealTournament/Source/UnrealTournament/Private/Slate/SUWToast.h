// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWToast : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWToast)
	:_Lifetime(3.0f)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, ToastText)											
	SLATE_ARGUMENT(float, Lifetime)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SWidget> GameViewportWidget;
	float Lifetime;
	float InitialLifetime;

	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const override;
};

#endif