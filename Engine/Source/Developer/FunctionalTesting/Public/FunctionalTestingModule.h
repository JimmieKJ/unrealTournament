// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFuncTestManager.h"

class FFunctionalTestingModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the debugger singleton or returns NULL */
	static class IFuncTestManager* Get()
	{
		FFunctionalTestingModule& Module = FModuleManager::Get().LoadModuleChecked<FFunctionalTestingModule>("FunctionalTesting");
		return Module.GetSingleton();
	}

private:
	void OnModulesChanged(FName Module, EModuleChangeReason Reason);

	class IFuncTestManager* GetSingleton() const 
	{ 
		return Manager.Get(); 
	}

private:
	TSharedPtr<class IFuncTestManager> Manager;
};
