// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemTestPawn.h"
#include "AbilitySystemTestAttributeSet.h"
#include "AbilitySystemComponent.h"

FName  AAbilitySystemTestPawn::AbilitySystemComponentName(TEXT("AbilitySystemComponent0"));

AAbilitySystemTestPawn::AAbilitySystemTestPawn(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(AAbilitySystemTestPawn::AbilitySystemComponentName);
	AbilitySystemComponent->SetIsReplicated(true);

	//DefaultAbilitySet = NULL;
}

void AAbilitySystemTestPawn::PostInitializeComponents()
{	
	static UProperty *DamageProperty = FindFieldChecked<UProperty>(UAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UAbilitySystemTestAttributeSet, Damage));

	Super::PostInitializeComponents();
	AbilitySystemComponent->InitStats(UAbilitySystemTestAttributeSet::StaticClass(), NULL);

	/*
	if (DefaultAbilitySet != NULL)
	{
		AbilitySystemComponent->InitializeAbilities(DefaultAbilitySet);
	}
	*/
}

UAbilitySystemComponent* AAbilitySystemTestPawn::GetAbilitySystemComponent() const
{
	return FindComponentByClass<UAbilitySystemComponent>();
}

/** Returns AbilitySystemComponent subobject **/
UAbilitySystemComponent* AAbilitySystemTestPawn::GetAbilitySystemComponent() { return AbilitySystemComponent; }
