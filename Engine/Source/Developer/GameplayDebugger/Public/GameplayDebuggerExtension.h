// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// GAMEPLAY DEBUGGER EXTENSION
// 
// Extensions allows creating additional key bindings for gameplay debugger for custom functionality.
// For example, you can use them to add another way of selecting actor to Debug.
//
// It should be compiled and used only when module is included, so every extension class
// needs be placed in #if WITH_GAMEPLAY_DEBUGGER block.
// 
// Extensions needs to be manually registered and unregistered with GameplayDebugger.
// It's best to do it in owning module's Startup / Shutdown, similar to detail view customizations.

#pragma once

#include "GameplayDebuggerAddonBase.h"

class GAMEPLAYDEBUGGER_API FGameplayDebuggerExtension : public FGameplayDebuggerAddonBase
{
public:

	virtual ~FGameplayDebuggerExtension() {}

	/** called when extension is added to debugger or debugger is enabled */
	virtual void OnActivated();

	/** called when extension is removed from debugger or debugger is disabled */
	virtual void OnDeactivated();

	/** description for gameplay debugger's header row, newline character is ignored */
	virtual FString GetDescription() const;

protected:

	/** get player controller owning gameplay debugger tool */
	APlayerController* GetPlayerController() const;
};
