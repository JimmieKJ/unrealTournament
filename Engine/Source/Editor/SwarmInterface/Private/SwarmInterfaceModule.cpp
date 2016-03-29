// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObject.h"
#include "ModuleManager.h"
#include "SwarmMessages.h"


class FSwarmInterfaceModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FSwarmInterfaceModule, SwarmInterface);
