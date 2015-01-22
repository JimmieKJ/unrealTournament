// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SerializationPrivatePCH.h"
#include "ModuleInterface.h"


//DEFINE_LOG_CATEGORY(LogSerialization);


/**
 * Implements the Serialization module.
 */
class FSerializationModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FSerializationModule, Serialization);
