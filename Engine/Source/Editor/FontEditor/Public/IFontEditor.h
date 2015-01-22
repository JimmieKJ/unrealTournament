// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"


/*-----------------------------------------------------------------------------
   IFontEditor
-----------------------------------------------------------------------------*/

class IFontEditor : public FAssetEditorToolkit
{
public:
	/** Returns the font asset being inspected by the font editor */
	virtual UFont* GetFont() const = 0;

	/** Assigns a font texture object to the page properties control when a new page is selected */
	virtual void SetSelectedPage(int32 PageIdx) = 0;

	/** Refresh the preview viewport */
	virtual void RefreshPreview() = 0;
};


