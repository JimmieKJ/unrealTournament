// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogEditorPCH.h"

DEFINE_LOG_CATEGORY(LogGameSubCatalogEditor);

/**
 * Game Sub-Catalog Module
 */
class FGameSubCatalogEditorModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }
};

IMPLEMENT_MODULE(FGameSubCatalogEditorModule, GameSubCatalogEditor);

