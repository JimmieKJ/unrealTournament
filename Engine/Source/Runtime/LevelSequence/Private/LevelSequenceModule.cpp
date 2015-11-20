// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "ModuleInterface.h"


/**
 * Implements the LevelSequence module.
 */
class FLevelSequenceModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FLevelSequenceModule, LevelSequence);
