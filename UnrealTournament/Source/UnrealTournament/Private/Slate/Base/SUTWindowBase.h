// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *	this is the base class of our window system.  UI windows are defined as a free form ui element that can be anywhere on the screen.  It has no special management 
 *  associated with it.  NOTE: It's very similar to the Dialog but doesn't have all of the needed button functionality that goes along with it.  I'd love for SUTDialogBase to
 *  extend from SUTWindowBase but slate really doesn't want to work that way so duplicate code alert.
 **/

#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTWindowBase : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTWindowBase)
	: _Size(FVector2D(1.0f,1.0f))
	, _bSizeIsRelative(true)
	, _Position(FVector2D(0.5f,0.5f))
	, _bPositionIsRelative(true)
	, _AnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _bShadow(true)
	, _ShadowAlpha(1.0f)
	{}

	// The size of this window.  Note all of UT's UI is designed for 1080p and then sized accordingly
	SLATE_ARGUMENT(FVector2D, Size)

	// If true, then consider the size values to be relative to 1080p
	SLATE_ARGUMENT(bool, bSizeIsRelative)

	// The position (within 1920x1080)
	SLATE_ARGUMENT(FVector2D, Position)

	// If true, then the position will be relative to the designed resolution.
	SLATE_ARGUMENT(bool, bPositionIsRelative)

	// The anchor point for this window.  By default we use center (0.5,0.5) so that the window is centered around the position
	SLATE_ARGUMENT(FVector2D, AnchorPoint)

	// How much padding to add to the window from the edges.  
	SLATE_ARGUMENT(FVector2D, ContentPadding)

	// If true, then a slightly transparent grey shadow is displayed full screen behind the window.
	SLATE_ARGUMENT(bool, bShadow)

	SLATE_ARGUMENT(float, ShadowAlpha)

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner);

	virtual void BuildWindow();

	virtual void Open();
	virtual void OnOpened();
	// returns false if the window decides you can't close it
	virtual bool Close();
	virtual void OnClosed();

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	/** utility to generate a simple text widget for list and combo boxes given a string value */
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);

	/** Sets the state of the window */
	virtual void SetWindowState(EUIWindowState::Type NewWindowState);

	/** Returns the current state of this window */
	virtual EUIWindowState::Type GetWindowState();

protected:

	/** Holds a reference to the SCanvas widget that makes up the window*/
	TSharedPtr<SCanvas> Canvas;

	/** Holds a reference to the SOverlay that defines the content for this content */
	TSharedPtr<SOverlay> Content;

	// The actual position of this dialog  
	FVector2D ActualPosition;

	// The actual size of this dialog
	FVector2D ActualSize;

	// The current tab index.  Used to determine when tab and shift tab should work :(
	int32 TabStop;
	
	// Stores a list of widgets that are tab'able
	TArray<TSharedPtr<SWidget>> TabTable;

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	EUIWindowState::Type WindowState;

	const FSlateBrush* GetFocusBrush() const
	{
		return FCoreStyle::Get().GetBrush("NoBrush");
	}

	TSharedPtr<class SWidget> GameViewportWidget;

private:
	bool bClosing;

public:
	bool bSkipWorldRender;

};

#endif