// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TreeMapModule.h"
#include "TreeMapStyle.h"

/**
 * Implements the TreeMap module.
 */
class FTreeMapModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override
	{
		FTreeMapStyle::Initialize();
	}

	virtual void ShutdownModule() override
	{
		FTreeMapStyle::Shutdown();
	}
};


IMPLEMENT_MODULE(FTreeMapModule, TreeMap);


#undef LOCTEXT_NAMESPACE
