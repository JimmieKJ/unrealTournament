// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogPCH.h"

DEFINE_LOG_CATEGORY(LogGameSubCatalog);

/**
 * Game Sub-Catalog Module
 */
class FGameSubCatalogModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }
};

IMPLEMENT_MODULE(FGameSubCatalogModule, GameSubCatalog);

