// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AbilitySystemPrivatePCH.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "DebugRenderSceneProxy.h"
#include "GameplayTasksComponent.h"
#include "AbilitiesGameplayDebuggerObject.h"

UAbilitiesGameplayDebuggerObject::UAbilitiesGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAbilitiesGameplayDebuggerObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DOREPLIFETIME(UAbilitiesGameplayDebuggerObject, AbilityInfo);
	DOREPLIFETIME(UAbilitiesGameplayDebuggerObject, bIsUsingAbilities);
#endif
}

void UAbilitiesGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APawn* MyPawn = Cast<APawn>(SelectedActor);
	if (MyPawn)
	{
		bool bUsingAbilities;
		IGameplayAbilitiesModule& AbilitiesModule = IGameplayAbilitiesModule::Get();
		AbilitiesModule.GetActiveAbilitiesDebugDataForActor(MyPawn, AbilityInfo, bUsingAbilities);
		bIsUsingAbilities = bUsingAbilities;
	}
	else
	{
		bIsUsingAbilities = false;
		AbilityInfo = TEXT("None");
	}
#endif
}

void UAbilitiesGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor* SelectedActor)
{
	if (bIsUsingAbilities)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Ability: {yellow}%s\n"), *AbilityInfo));
	}
}
