// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

class FSuperSearchModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** Generates aSuperSearch box widget.  Remember, this widget will become invalid if the
	   SuperSearch DLL is unloaded on the fly. */
	virtual TSharedRef< SWidget > MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const TOptional<const FSearchBoxStyle*> InStyle = TOptional<const FSearchBoxStyle*>()) const;
};
