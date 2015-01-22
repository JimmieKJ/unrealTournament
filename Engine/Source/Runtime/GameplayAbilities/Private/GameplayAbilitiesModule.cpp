// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemGlobals.h"


class FGameplayAbilitiesModule : public IGameplayAbilitiesModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	virtual UAbilitySystemGlobals* GetAbilitySystemGlobals()
	{
		// Defer loading of globals to the first time it is requested
		if (!AbilitySystemGlobals)
		{
			FStringClassReference AbilitySystemClassName = (UAbilitySystemGlobals::StaticClass()->GetDefaultObject<UAbilitySystemGlobals>())->AbilitySystemGlobalsClassName;

			UClass* SingletonClass = LoadClass<UObject>(NULL, *AbilitySystemClassName.ToString(), NULL, LOAD_None, NULL);

			checkf(SingletonClass != NULL, TEXT("Ability config value AbilitySystemGlobalsClassName is not a valid class name."));

			AbilitySystemGlobals = ConstructObject<UAbilitySystemGlobals>(SingletonClass, GetTransientPackage(), NAME_None, RF_RootSet);
		}

		check(AbilitySystemGlobals);
		return AbilitySystemGlobals;
	}

	UAbilitySystemGlobals *AbilitySystemGlobals;

private:
	
};

IMPLEMENT_MODULE(FGameplayAbilitiesModule, GameplayAbilities)

void FGameplayAbilitiesModule::StartupModule()
{	
	// This is loaded upon first request
	AbilitySystemGlobals = NULL;
}

void FGameplayAbilitiesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	AbilitySystemGlobals = NULL;
}
