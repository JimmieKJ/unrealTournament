// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICollisionAnalyzer.h"

#include "SlateBasics.h"

class FCollisionAnalyzerModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	/** Gets the debugger singleton or returns NULL */
	static ICollisionAnalyzer* Get()
	{
		static FName CollisionAnalyzerModuleName("CollisionAnalyzer");
		FCollisionAnalyzerModule& DebuggerModule = FModuleManager::Get().LoadModuleChecked<FCollisionAnalyzerModule>(CollisionAnalyzerModuleName);
		return DebuggerModule.GetSingleton();
	}

private:
	virtual ICollisionAnalyzer* GetSingleton() const 
	{ 
		return CollisionAnalyzer; 
	}

	/** Spawns the Collision Analyzer tab in an SDockTab */
	TSharedRef<class SDockTab> SpawnCollisionAnalyzerTab(const FSpawnTabArgs& Args);

	ICollisionAnalyzer* CollisionAnalyzer;
};
