// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTPanelBase.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

#include "UserWidget.h"


class SUTBorder;

class UNREALTOURNAMENT_API SUTUMGPanel : public SUTPanelBase
{
	SLATE_BEGIN_ARGS(SUTUMGPanel)

	:_UMGClass(TEXT(""))
	, _bShowBackButton(true)
	{}

	// The class of the UMG widget to display in this panel.  
	SLATE_ARGUMENT(FString, UMGClass)
	SLATE_ARGUMENT(bool, bShowBackButton)
	SLATE_END_ARGS()

	~SUTUMGPanel();
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> PlayerOwner);

	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:
	bool bShowBackButton;
	FString UMGClass;
	TSharedPtr<SOverlay> SlateContainer;
	TWeakObjectPtr<class UUserWidget> UMGWidget;

	virtual void OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow);
	virtual void OnHidePanel();

	TSharedPtr<SUTBorder> AnimWidget;
	virtual void AnimEnd();

public:

	virtual bool ShouldShowBackButton()
	{
		return bShowBackButton;
	}

};

#endif