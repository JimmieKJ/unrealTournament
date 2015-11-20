// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserPopupFeatures.h"

#if WITH_CEF3

FWebBrowserPopupFeatures::FWebBrowserPopupFeatures()
	: X(0)
	, bXSet(false)
	, Y(0)
	, bYSet(false)
	, Width(0)
	, bWidthSet(false)
	, Height(0)
	, bHeightSet(false)
	, bMenuBarVisible(true)
	, bStatusBarVisible(false)
	, bToolBarVisible(true)
	, bLocationBarVisible(true)
	, bScrollbarsVisible(true)
	, bResizable(true)
	, bIsFullscreen(false)
	, bIsDialog(false)
{
}


FWebBrowserPopupFeatures::FWebBrowserPopupFeatures( const CefPopupFeatures& PopupFeatures )
{
	X = PopupFeatures.x;
	bXSet = PopupFeatures.xSet ? true : false;
	Y = PopupFeatures.y;
	bYSet = PopupFeatures.ySet ? true : false;
	Width = PopupFeatures.width;
	bWidthSet = PopupFeatures.widthSet ? true : false;
	Height = PopupFeatures.height;
	bHeightSet = PopupFeatures.heightSet ? true : false;
	bMenuBarVisible = PopupFeatures.menuBarVisible ? true : false;
	bStatusBarVisible = PopupFeatures.statusBarVisible ? true : false;
	bToolBarVisible = PopupFeatures.toolBarVisible ? true : false;
	bLocationBarVisible = PopupFeatures.locationBarVisible ? true : false;
	bScrollbarsVisible = PopupFeatures.scrollbarsVisible ? true : false;
	bResizable = PopupFeatures.resizable ? true : false;
	bIsFullscreen = PopupFeatures.fullscreen ? true : false;
	bIsDialog = PopupFeatures.dialog ? true : false;

	int Count = PopupFeatures.additionalFeatures ? cef_string_list_size(PopupFeatures.additionalFeatures) : 0;
	CefString ListValue;

	for(int ListIdx = 0; ListIdx < Count; ListIdx++) 
	{
		cef_string_list_value(PopupFeatures.additionalFeatures, ListIdx, ListValue.GetWritableStruct());
		AdditionalFeatures.Add(ListValue.ToWString().c_str());
	}

}

FWebBrowserPopupFeatures::~FWebBrowserPopupFeatures()
{
}

int FWebBrowserPopupFeatures::GetX() const 
{
	return X;
}

bool FWebBrowserPopupFeatures::IsXSet() const 
{
	return bXSet;
}

int FWebBrowserPopupFeatures::GetY() const 
{
	return Y;
}

bool FWebBrowserPopupFeatures::IsYSet() const 
{
	return bYSet;
}

int FWebBrowserPopupFeatures::GetWidth() const 
{
	return Width;
}

bool FWebBrowserPopupFeatures::IsWidthSet() const 
{
	return bWidthSet;
}

int FWebBrowserPopupFeatures::GetHeight() const 
{
	return Height;
}

bool FWebBrowserPopupFeatures::IsHeightSet() const 
{
	return bHeightSet;
}

bool FWebBrowserPopupFeatures::IsMenuBarVisible() const 
{
	return bMenuBarVisible;
}

bool FWebBrowserPopupFeatures::IsStatusBarVisible() const 
{
	return bStatusBarVisible;
}

bool FWebBrowserPopupFeatures::IsToolBarVisible() const 
{
	return bToolBarVisible;
}

bool FWebBrowserPopupFeatures::IsLocationBarVisible() const 
{
	return bLocationBarVisible;
}

bool FWebBrowserPopupFeatures::IsScrollbarsVisible() const 
{
	return bScrollbarsVisible;
}

bool FWebBrowserPopupFeatures::IsResizable() const 
{
	return bResizable;
}

bool FWebBrowserPopupFeatures::IsFullscreen() const 
{
	return bIsFullscreen;
}

bool FWebBrowserPopupFeatures::IsDialog() const 
{
	return bIsDialog;
}

TArray<FString> FWebBrowserPopupFeatures::GetAdditionalFeatures() const 
{
	return AdditionalFeatures;
}

#endif


