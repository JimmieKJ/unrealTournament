// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "STaskBrowser.h"

//////////////////////////////////////////////////////////////////////////
// STaskColumn
class STaskColumn : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STaskColumn)
	{
	}
		SLATE_ARGUMENT( TWeakPtr<STaskBrowser>, TaskBrowser )
		SLATE_ARGUMENT( EField, Field )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

private:

	/** Helper func to get the field from the localized name */
	FString GetFieldNameLoc( const EField InField );

	/** Handle when the column in the table has been clicked */
	FReply OnTaskColumnClicked();

	/** The parent browser */
	TWeakPtr<STaskBrowser> TaskBrowser;

	/** The field the column represents */
	EField Field;
};