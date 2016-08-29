// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "EngineGlobals.h"
#include "GameplayTagsManager.h"


class UGameplayTagsManager;

/**
 * The public interface to this module
 */
class IGameplayTagsModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGameplayTagsModule& Get()
	{
		static const FName GameplayTagModuleName(TEXT("GameplayTags"));
		return FModuleManager::LoadModuleChecked< IGameplayTagsModule >(GameplayTagModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName GameplayTagModuleName(TEXT("GameplayTags"));
		return FModuleManager::Get().IsModuleLoaded(GameplayTagModuleName);
	}

	/**
	 * Helper function to request a gameplay tag by name
	 * 
	 * @param InTagName	Tag name to request
	 * 
	 * @return Gameplay tag associated with the specified name. Will be marked invalid if tag not found
	 */
	FORCEINLINE_DEBUGGABLE static FGameplayTag RequestGameplayTag(FName InTagName, bool ErrorIfNotFound=true)
	{
		IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
		return GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(InTagName, ErrorIfNotFound);
	}


	// Gets the UGameplayTagsManager manager
	FORCEINLINE_DEBUGGABLE static UGameplayTagsManager& GetGameplayTagsManager()
	{
		if (GGameplayTagsManager == nullptr)
		{
			IGameplayTagsModule::Get();
		}
		
		return *GGameplayTagsManager;
	}

};

