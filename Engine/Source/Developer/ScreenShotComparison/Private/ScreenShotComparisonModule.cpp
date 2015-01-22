// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotComparisonModule.cpp: Implements the FScreenShotComparisonModule class.
=============================================================================*/

#include "ScreenShotComparisonPrivatePCH.h"


static const FName ScreenShotComparisonName("ScreenShotComparison");


/**
 * Implements the ScreenShotComparison module.
 */
class FScreenShotComparisonModule
	: public IScreenShotComparisonModule
{
public:
	virtual TSharedRef<class SWidget> CreateScreenShotComparison( const IScreenShotManagerRef& ScreenShotManager ) override
	{
		return SNew(SScreenShotBrowser, ScreenShotManager);
	}

};


IMPLEMENT_MODULE(FScreenShotComparisonModule, ScreenShotComparison);