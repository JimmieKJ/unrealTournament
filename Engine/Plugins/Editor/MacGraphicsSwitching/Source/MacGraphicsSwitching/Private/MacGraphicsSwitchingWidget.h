// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMacGraphicsSwitchingModule.h"
#include "PropertyHandle.h"
#include "SlateBasics.h"
#include "SlateStyle.h"

struct FRendererItem
{
	FRendererItem(const FText& InText, int32 const InRendererID)
		: Text(InText)
		, RendererID(InRendererID)
	{
	}

	/** Text to display */
	FText Text;

	/** ID of the renderer */
	int32 RendererID;
};

class SMacGraphicsSwitchingWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMacGraphicsSwitchingWidget)
		: _bLiveSwitching(false) {}
	
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PreferredRendererPropertyHandle)
		SLATE_ARGUMENT(bool, bLiveSwitching)
	SLATE_END_ARGS();
	
	SMacGraphicsSwitchingWidget();
	virtual ~SMacGraphicsSwitchingWidget();
	
	void Construct(FArguments const& InArgs);
	
private:
	/** Generate a row widget for display in the list view */
	TSharedRef<SWidget> OnGenerateWidget( TSharedPtr<FRendererItem> InItem );
	
	/** Set the renderer when we change selection */
	void OnSelectionChanged(TSharedPtr<FRendererItem> InItem, ESelectInfo::Type InSeletionInfo, TSharedPtr<IPropertyHandle> PreferredProviderPropertyHandle);
	
	/** Get the text to display on the renderer drop-down */
	FText GetRendererText() const;
	
private:
	/** Accessor names to display in a drop-down list */
	TArray<TSharedPtr<FRendererItem>> Renderers;
	/** Whether we are modifying the current or default */
	bool bLiveSwitching;
};
