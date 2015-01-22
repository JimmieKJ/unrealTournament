// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
   IMeshProxyDialog
-----------------------------------------------------------------------------*/

class IMeshProxyDialog
{
public:
	/** Creates geometry mode controls */
	static TSharedPtr<IMeshProxyDialog> MakeControls();

	/** Assigns a SMeshProxyDialog to the window's content */
	virtual void AssignWindowContent(TSharedPtr<SWindow> Window) = 0;

	/** Parent window accessors */
	virtual TSharedPtr<SWindow> GetParentWindow() = 0;
	virtual void SetParentWindow(TSharedPtr<SWindow> Window) = 0;

	/** Called when the dialog window is closed */
	virtual void OnWindowClosed(const TSharedRef<SWindow>&) = 0;

	/** Marks the dialog as in need of an update */
	virtual void MarkDirty() = 0;
};
