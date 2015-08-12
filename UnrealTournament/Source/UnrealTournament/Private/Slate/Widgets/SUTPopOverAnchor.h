// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"


#if !UE_SERVER

class SUTPopOverAnchor;
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FGetPopoverWidget, TSharedPtr<SUTPopOverAnchor>);

class UNREALTOURNAMENT_API SUTPopOverAnchor: public SMenuAnchor
{
	SLATE_BEGIN_ARGS(SUTPopOverAnchor)
		: _Content()
		, _MenuPlacement(MenuPlacement_MenuRight)
		, _AssociatedString(TEXT(""))
		{}

		
		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT( FArguments, Content )
		
		// Events

		// Called when the submenu is shown
		SLATE_EVENT( FGetPopoverWidget, OnGetPopoverWidget)

		/** Where should the menu be placed */
		SLATE_ATTRIBUTE( EMenuPlacement, MenuPlacement )

		SLATE_ARGUMENT(TWeakObjectPtr<AActor>, AssociatedActor)

		SLATE_ARGUMENT(FString, AssociatedString)

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	TWeakObjectPtr<AActor> AssociatedActor;
	FString AssociatedString;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:

	bool bWaitingToPopup;
	float PopupStartTime;

	void Popup();

	FGetPopoverWidget OnGetPopoverWidget;
	TWeakPtr<SWidget> WidgetToFocusPtr;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;


};

#endif