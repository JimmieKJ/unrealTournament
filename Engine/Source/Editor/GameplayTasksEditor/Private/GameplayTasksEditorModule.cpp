// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksEditorPrivatePCH.h"
#include "GameplayTasksEditorModule.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterAIModule

class FGameplayTasksEditorModule : public IGameplayTasksEditorModule
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

IMPLEMENT_MODULE(FGameplayTasksEditorModule, GameplayTasksEditor);
