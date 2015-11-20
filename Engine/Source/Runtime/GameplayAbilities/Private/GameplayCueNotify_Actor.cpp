// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayTagsModule.h"
#include "GameplayCueManager.h"
#include "AbilitySystemComponent.h"

AGameplayCueNotify_Actor::AGameplayCueNotify_Actor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	IsOverride = true;
	PrimaryActorTick.bCanEverTick = true;
	bAutoDestroyOnRemove = false;
	AutoDestroyDelay = 0.f;
	bUniqueInstancePerSourceObject = false;
	bUniqueInstancePerInstigator = false;
}

#if WITH_EDITOR
void AGameplayCueNotify_Actor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(GetClass());

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("GameplayCueTag")))
	{
		DeriveGameplayCueTagFromAssetName();
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleAssetDeleted(Blueprint);
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleAssetAdded(Blueprint);
	}
}
#endif

void AGameplayCueNotify_Actor::DeriveGameplayCueTagFromAssetName()
{
	UAbilitySystemGlobals::DeriveGameplayCueTagFromAssetName(GetName(), GameplayCueTag, GameplayCueName);
}

void AGameplayCueNotify_Actor::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		DeriveGameplayCueTagFromAssetName();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		DeriveGameplayCueTagFromAssetName();
	}
}

void AGameplayCueNotify_Actor::BeginPlay()
{
	Super::BeginPlay();

	// Most likely case for this is the target actor is no longer net relevant to us and has been destroyed, so this should be destroyed too
	if (GetOwner())
	{
		GetOwner()->OnDestroyed.AddDynamic(this, &AGameplayCueNotify_Actor::OnOwnerDestroyed);
	}
}

void AGameplayCueNotify_Actor::PostInitProperties()
{
	Super::PostInitProperties();
	DeriveGameplayCueTagFromAssetName();
}

bool AGameplayCueNotify_Actor::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return true;
}

void AGameplayCueNotify_Actor::HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleGameplayCueNotifyActor);

	if (MyTarget && !MyTarget->IsPendingKill())
	{
		K2_HandleGameplayCue(MyTarget, EventType, Parameters);

		// Clear any pending auto-destroy that may have occurred from a previous OnRemove
		SetLifeSpan(0.f);

		switch (EventType)
		{
		case EGameplayCueEvent::OnActive:
			OnActive(MyTarget, Parameters);
			break;

		case EGameplayCueEvent::WhileActive:
			WhileActive(MyTarget, Parameters);
			break;

		case EGameplayCueEvent::Executed:
			OnExecute(MyTarget, Parameters);
			break;

		case EGameplayCueEvent::Removed:
			OnRemove(MyTarget, Parameters);

			if (bAutoDestroyOnRemove)
			{
				if (AutoDestroyDelay > 0.f)
				{
					SetLifeSpan(AutoDestroyDelay);
				}
				else
				{
					Destroy();
				}
			}
			break;
		};
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("Null Target"));
	}
}

void AGameplayCueNotify_Actor::OnOwnerDestroyed()
{
	// May need to do extra cleanup in child classes
	Destroy();
}

bool AGameplayCueNotify_Actor::OnExecute_Implementation(AActor* MyTarget, FGameplayCueParameters Parameters)
{
	return false;
}

bool AGameplayCueNotify_Actor::OnActive_Implementation(AActor* MyTarget, FGameplayCueParameters Parameters)
{
	return false;
}

bool AGameplayCueNotify_Actor::WhileActive_Implementation(AActor* MyTarget, FGameplayCueParameters Parameters)
{
	return false;
}

bool AGameplayCueNotify_Actor::OnRemove_Implementation(AActor* MyTarget, FGameplayCueParameters Parameters)
{
	return false;
}
