// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CharacterAIPrivatePCH.h"
#include "CharacterAIModule.h"
#include "ModuleManager.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterAIModule

class FCharacterAIModule : public ICharacterAIModuleInterface
{
public:
	virtual void StartupModule() override
	{
		check(GConfig);		
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FCharacterAIModule, CharacterAI);
DEFINE_LOG_CATEGORY(LogCharacterAI);
