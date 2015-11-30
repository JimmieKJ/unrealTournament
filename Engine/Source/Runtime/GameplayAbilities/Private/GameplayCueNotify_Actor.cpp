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
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAutoDestroyOnRemove = false;
	AutoDestroyDelay = 0.f;
	bUniqueInstancePerSourceObject = false;
	bUniqueInstancePerInstigator = false;

	NumPreallocatedInstances = 0;

	StackCount = 0;
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

int32 GameplayCueNotifyActorStacking = 0;
static FAutoConsoleVariableRef CVarGameplayCueNotifyActorStacking(TEXT("AbilitySystem.GameplayCueNotifyActorStacking"), GameplayCueNotifyActorStacking, TEXT("Enable simple stacking rules for gameplaycue actors"), ECVF_Default );

void AGameplayCueNotify_Actor::HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleGameplayCueNotifyActor);

	if (MyTarget && !MyTarget->IsPendingKill())
	{
		if (GameplayCueNotifyActorStacking)
		{
			if (EventType == EGameplayCueEvent::WhileActive)
			{
				StackCount++;
				if (StackCount > 1)
				{
					return;
				}
			}
			else if (EventType == EGameplayCueEvent::Removed)
			{
				StackCount--;
				if (StackCount > 0)
				{
					return;
				}
				if (!ensureMsgf(StackCount == 0, TEXT("GameplayCue %s has negative StackCount."), *GetName()))
				{
					StackCount = 0;
				}
			}
		}


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
					GetWorld()->GetTimerManager().SetTimer(FinishTimerHandle, this, &AGameplayCueNotify_Actor::GameplayCueFinishedCallback, AutoDestroyDelay);
				}
				else
				{
					GameplayCueFinishedCallback();
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
	GameplayCueFinishedCallback();
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

void AGameplayCueNotify_Actor::GameplayCueFinishedCallback()
{
	if (FinishTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(FinishTimerHandle);
	}
	
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->NotifyGameplayCueActorFinished(this);
}

bool AGameplayCueNotify_Actor::GameplayCuePendingRemove()
{
	return GetLifeSpan() > 0.f || FinishTimerHandle.IsValid() || IsPendingKill();
}

bool AGameplayCueNotify_Actor::Recycle()
{
	StackCount = 0;
	return false;
}