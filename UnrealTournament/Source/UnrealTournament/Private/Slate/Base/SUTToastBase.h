// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *	This is the base class for our toast windows.  Toasts are static UI elements that require no input
 **/


#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTToastBase : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTToastBase)
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

	virtual TSharedRef<SWidget> BuildToast(const FArguments& InArgs);

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SWidget> GameViewportWidget;
	float Lifetime;
	float InitialLifetime;

	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const override;

protected:
	const FSlateBrush* GetFocusBrush() const
	{
		return FCoreStyle::Get().GetBrush("NoBrush");
	}


};

#endif