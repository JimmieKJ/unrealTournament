// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

// Forward Declarations
class IWebBrowserSingleton;

/**
 * WebBrowserModule interface
 */
class IWebBrowserModule : public IModuleInterface
{
public:
	/**
	 * Get or load the Web Browser Module
	 * 
	 * @return The loaded module
	 */
	static inline IWebBrowserModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IWebBrowserModule >("WebBrowser");
	}

	/**
	 * Get the Web Browser Singleton
	 * 
	 * @return The Web Browser Singleton
	 */
	virtual IWebBrowserSingleton* GetSingleton() = 0;
};