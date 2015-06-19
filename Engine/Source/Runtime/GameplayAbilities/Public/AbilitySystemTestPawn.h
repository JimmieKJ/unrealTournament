// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameplayCueInterface.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemTestPawn.generated.h"

UCLASS(Blueprintable, BlueprintType, notplaceable)
class GAMEPLAYABILITIES_API AAbilitySystemTestPawn : public ADefaultPawn, public IGameplayCueInterface, public IAbilitySystemInterface
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
private_subobject:
	/** DefaultPawn collision component */
	DEPRECATED_FORGAME(4.6, "AbilitySystemComponent should not be accessed directly, please use GetAbilitySystemComponent() function instead. AbilitySystemComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = AbilitySystem, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UAbilitySystemComponent* AbilitySystemComponent;
public:

	//UPROPERTY(EditDefaultsOnly, Category=GameplayEffects)
	//UGameplayAbilitySet * DefaultAbilitySet;

	static FName AbilitySystemComponentName;

	/** Returns AbilitySystemComponent subobject **/
	class UAbilitySystemComponent* GetAbilitySystemComponent();
};
