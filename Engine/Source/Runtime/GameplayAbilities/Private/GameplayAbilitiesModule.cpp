// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

class FGameplayAbilitiesModule : public IGameplayAbilitiesModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	virtual UAbilitySystemGlobals* GetAbilitySystemGlobals() override
	{
		// Defer loading of globals to the first time it is requested
		if (!AbilitySystemGlobals)
		{
			FStringClassReference AbilitySystemClassName = (UAbilitySystemGlobals::StaticClass()->GetDefaultObject<UAbilitySystemGlobals>())->AbilitySystemGlobalsClassName;

			UClass* SingletonClass = LoadClass<UObject>(NULL, *AbilitySystemClassName.ToString(), NULL, LOAD_None, NULL);

			checkf(SingletonClass != NULL, TEXT("Ability config value AbilitySystemGlobalsClassName is not a valid class name."));

			AbilitySystemGlobals = NewObject<UAbilitySystemGlobals>(GetTransientPackage(), SingletonClass, NAME_None, RF_RootSet);
		}

		check(AbilitySystemGlobals);
		return AbilitySystemGlobals;
	}

	UAbilitySystemGlobals *AbilitySystemGlobals;

	void GetActiveAbilitiesDebugDataForActor(AActor* Actor, FString& AbilityString, bool& bIsUsingAbilities);

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

void FGameplayAbilitiesModule::GetActiveAbilitiesDebugDataForActor(AActor* Actor, FString& AbilityString, bool& bIsUsingAbilities)
{
	UAbilitySystemComponent* AbilityComp = Actor->FindComponentByClass<UAbilitySystemComponent>();
	bIsUsingAbilities = (AbilityComp != nullptr);
	  
	int32 NumActive = 0;
	if (AbilityComp)
	{
		AbilityString = TEXT("");
	  
		for (const FGameplayAbilitySpec& AbilitySpec : AbilityComp->GetActivatableAbilities())
		{
	  		if (AbilitySpec.Ability && AbilitySpec.IsActive())
	  		{
	  			if (NumActive)
	  			{
					AbilityString += TEXT(", ");
	  			}
	  
	  			UClass* AbilityClass = AbilitySpec.Ability->GetClass();
	  			FString AbClassName = GetNameSafe(AbilityClass);
	  			AbClassName.RemoveFromEnd(TEXT("_c"));
	  
				AbilityString += AbClassName;
	  			NumActive++;
	  		}
		}
	}
	  
	if (NumActive == 0)
	{
		AbilityString = TEXT("None");
	}
}
