// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "KismetCompiler.h"

class IBlueprintCompilerCppBackend
{
public:
	virtual void GenerateCodeFromClass(UClass* SourceClass, FString NewClassName, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly) = 0;

	virtual const FString& GetBody() const = 0;
	virtual const FString& GetHeader() const = 0;

	virtual ~IBlueprintCompilerCppBackend() {}
};

/**
 * BlueprintCompilerCppBackend module interface
 */
class IBlueprintCompilerCppBackendModuleInterface : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IBlueprintCompilerCppBackendModuleInterface& Get()
	{
		return FModuleManager::LoadModuleChecked< IBlueprintCompilerCppBackendModuleInterface >("BlueprintCompilerCppBackend");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("BlueprintCompilerCppBackend");
	}

	virtual IBlueprintCompilerCppBackend* Create(FKismetCompilerContext& InContext) = 0;
};