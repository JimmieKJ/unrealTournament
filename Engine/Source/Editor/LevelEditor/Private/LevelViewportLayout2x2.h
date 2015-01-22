// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportLayout2x2_h__
#define __LevelViewportLayout2x2_h__

#pragma once

#include "LevelViewportLayout.h"

class FLevelViewportLayout2x2 : public FLevelViewportLayout
{
public:
	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const override;

	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::FourPanes2x2; }
protected:
	/**
	 * Creates the viewports and splitter for the 2x2 layout                   
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) override;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) override;


private:
	/** The splitter widget */
	TSharedPtr< class SSplitter2x2 > SplitterWidget;
};

#endif
