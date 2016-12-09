// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

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
