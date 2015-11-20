// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#include "IWebBrowserPopupFeatures.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_base.h"
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

class FWebBrowserPopupFeatures
	: public IWebBrowserPopupFeatures
{
public:
	FWebBrowserPopupFeatures();
	FWebBrowserPopupFeatures(const CefPopupFeatures& PopupFeatures);
	virtual ~FWebBrowserPopupFeatures();

	// IWebBrowserPopupFeatures Interface

	virtual int GetX() const override;
	virtual bool IsXSet() const override;
	virtual int GetY() const override;
	virtual bool IsYSet() const override;
	virtual int GetWidth() const override;
	virtual bool IsWidthSet() const override;
	virtual int GetHeight() const override;
	virtual bool IsHeightSet() const override;
	virtual bool IsMenuBarVisible() const override;
	virtual bool IsStatusBarVisible() const override;
	virtual bool IsToolBarVisible() const override;
	virtual bool IsLocationBarVisible() const override;
	virtual bool IsScrollbarsVisible() const override;
	virtual bool IsResizable() const override;
	virtual bool IsFullscreen() const override;
	virtual bool IsDialog() const override;
	virtual TArray<FString> GetAdditionalFeatures() const override;

private:

	int X;
	bool bXSet;
	int Y;
	bool bYSet;
	int Width;
	bool bWidthSet;
	int Height;
	bool bHeightSet;

	bool bMenuBarVisible;
	bool bStatusBarVisible;
	bool bToolBarVisible;
	bool bLocationBarVisible;
	bool bScrollbarsVisible;
	bool bResizable;

	bool bIsFullscreen;
	bool bIsDialog;
	TArray<FString> AdditionalFeatures;
};

#endif