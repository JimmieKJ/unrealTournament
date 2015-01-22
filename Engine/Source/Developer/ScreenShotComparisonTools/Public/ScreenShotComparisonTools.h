// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotComparisonTools.h: ScreenShotComparisonTools module public header file.
=============================================================================*/

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"
#include "IFilter.h"
#include "TextFilter.h"
#include "FilterCollection.h"

/**
* The screen shot filter collection - used for updating the screen shot list
*/
typedef TFilterCollection< const TSharedPtr< class IScreenShotData >& > ScreenShotFilterCollection;

/* Interfaces
 *****************************************************************************/
#include "ScreenShotDataItem.h"
#include "IScreenShotData.h"
#include "IScreenShotManager.h"
#include "IScreenShotToolsModule.h"