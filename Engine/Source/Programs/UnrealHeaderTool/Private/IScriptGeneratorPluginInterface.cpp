// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "IScriptGeneratorPluginInterface.h"
#include "Algo/FindSortedStringCaseInsensitive.h"

EBuildModuleType::Type EBuildModuleType::Parse(const TCHAR* Value)
{
	static const TCHAR* AlphabetizedTypes[] = {
		TEXT("EngineDeveloper"),
		TEXT("EngineEditor"),
		TEXT("EngineRuntime"),
		TEXT("EngineThirdParty"),
		TEXT("GameDeveloper"),
		TEXT("GameEditor"),
		TEXT("GameRuntime"),
		TEXT("GameThirdParty"),
		TEXT("Program")
	};

	int32 TypeIndex = Algo::FindSortedStringCaseInsensitive(Value, AlphabetizedTypes);
	if (TypeIndex < 0)
	{
		FError::Throwf(TEXT("Unrecognized EBuildModuleType name: %s"), Value);
	}

	static EBuildModuleType::Type AlphabetizedValues[] = {
		EngineDeveloper,
		EngineEditor,
		EngineRuntime,
		EngineThirdParty,
		GameDeveloper,
		GameEditor,
		GameRuntime,
		GameThirdParty,
		Program
	};

	return AlphabetizedValues[TypeIndex];
}
