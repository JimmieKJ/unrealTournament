// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWDialog)
	: _DialogSize(FVector2D(400,200))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _IsScrollable(true)
	, _ButtonMask(UTDIALOG_BUTTON_OK)
	, _bShadow(true)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(FText, DialogTitle)
	SLATE_ARGUMENT(FVector2D, DialogSize)
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
	SLATE_ARGUMENT(FVector2D, DialogPosition)
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
	SLATE_ARGUMENT(FVector2D, ContentPadding)
	SLATE_ARGUMENT(bool, IsScrollable)
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_ARGUMENT(bool, bShadow)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	virtual void OnDialogOpened();
	virtual void OnDialogClosed();

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}
	
	/** utility to generate a simple text widget for list and combo boxes given a string value */
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);

protected:

	/** Holds a reference to the SCanvas widget that makes up the dialog */
	TSharedPtr<SCanvas> Canvas;

	/** Holds a reference to the SOverlay that defines the content for this dialog */
	TSharedPtr<SOverlay> DialogContent;

	/** Holds a reference to the SUniformGridPanel that is the button bar */
	TSharedPtr<SUniformGridPanel> ButtonBar;

	/** Holds left justifed buttons */
	TSharedPtr<SUniformGridPanel> CustomButtonBar;

	// The actual position of this dialog  
	FVector2D ActualPosition;

	// The actual size of this dialog
	FVector2D ActualSize;

	/** Our forward facing dialog result delegate.  OnButtonClick will make this call if it's defined. */
	FDialogResultDelegate OnDialogResult;

	/**
	*	Builds the TitleBar. Override to build a custom Title bar
	**/
	virtual TSharedRef<class SWidget> BuildTitleBar(FText InDialogTitle);

	/**
	 *	Called when the dialog wants to build the button bar.  It will iterate over the ButtonMask and add any buttons needed.
	 **/
	TSharedRef<class SWidget> BuildButtonBar(uint16 ButtonMask);

	/**
	 *	Allow for children to add custom buttons
	 **/

	virtual TSharedRef<class SWidget> BuildCustomButtonBar();


	/**
	 *	Adds a button to the button bar
	 **/
	void BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount);

	// Our internal OnClick handler.  
	virtual FReply OnButtonClick(uint16 ButtonID);	

	// The current tab index.  Used to determine when tab and shift tab should work :(
	int32 TabStop;
	// Stores a list of widgets that are tab'able
	TArray<TSharedPtr<SWidget>> TabTable;

	// Used to access buttons.
	TMap<uint16, TSharedPtr<SButton>> ButtonMap;

protected:
	TSharedPtr<STextBlock> DialogTitle;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent) override;
	virtual FReply OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent) override;

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SWidget> GameViewportWidget;

	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent) override;

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

public:
	virtual void EnableButton(uint16 ButtonID);
	virtual void DisableButton(uint16 ButtonID);
};

#endif