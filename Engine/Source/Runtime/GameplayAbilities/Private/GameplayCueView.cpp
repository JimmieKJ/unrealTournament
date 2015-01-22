// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffect.h"
#include "GameplayCueView.h"
#include "ParticleDefinitions.h"
#include "SoundDefinitions.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayCueInterface.h"
#include "GameplayTagsModule.h"
#include "GameplayCueActor.h"
#include "AbilitySystemComponent.h"

UGameplayCueView::UGameplayCueView(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

FGameplayCueViewInfo * FGameplayCueHandler::GetBestMatchingView(EGameplayCueEvent::Type Type, const FGameplayTag BaseTag, bool InstigatorLocal, bool TargetLocal)
{
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
	FGameplayTagContainer TagAndParentsContainer = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTagParents(BaseTag);

	for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
	{
		FGameplayTag Tag = *InnerTagIt;

		for (UGameplayCueView * Def : Definitions)
		{
			for (FGameplayCueViewInfo & View : Def->Views)
			{
				if (View.CueType == Type
					&& (!View.InstigatorLocalOnly || InstigatorLocal)
					&& (!View.TargetLocalOnly || TargetLocal) 
					&& View.Tags.HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
				{
					return &View;
				}
			}
		}
	}

	return NULL;
}

void FGameplayCueHandler::GameplayCueActivated(const FGameplayTagContainer& GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectContextHandle& EffectContext)
{
	check(Owner);
	bool InstigatorLocal = EffectContext.IsLocallyControlled();
	bool TargetLocal = OwnerIsLocallyControlled();
	
	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		if (FGameplayCueViewInfo* View = GetBestMatchingView(EGameplayCueEvent::OnActive, *TagIt, InstigatorLocal, TargetLocal))
		{
			View->SpawnViewEffects(Owner, NULL, EffectContext);
		}

		FName MatchedTag;
		UFunction *Func = UAbilitySystemGlobals::Get().GetGameplayCueFunction(*TagIt, Owner->GetClass(), MatchedTag);
		if (Func)
		{
			FGameplayCueParameters Params;
			Params.NormalizedMagnitude = NormalizedMagnitude;
			Params.EffectContext = EffectContext;

			IGameplayCueInterface::DispatchBlueprintCustomHandler(Owner, Func, EGameplayCueEvent::OnActive, Params);
		}
	}
}

void FGameplayCueHandler::GameplayCueExecuted(const FGameplayTagContainer& GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectContextHandle& EffectContext)
{
	check(Owner);
	bool InstigatorLocal = EffectContext.IsLocallyControlled();
	bool TargetLocal = OwnerIsLocallyControlled();

	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		if (FGameplayCueViewInfo* View = GetBestMatchingView(EGameplayCueEvent::Executed, *TagIt, InstigatorLocal, TargetLocal))
		{
			View->SpawnViewEffects(Owner, NULL, EffectContext);
		}
	}
}

void FGameplayCueHandler::GameplayCueAdded(const FGameplayTagContainer& GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectContextHandle& EffectContext)
{
	check(Owner);
	bool InstigatorLocal = EffectContext.IsLocallyControlled();
	bool TargetLocal = OwnerIsLocallyControlled();

	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		TArray<TSharedPtr<FGameplayCueViewEffects> > &Effects = SpawnedViewEffects.FindOrAdd(*TagIt);

		// Clear old effects if they existed? This will vary case to case. Removing old effects is the easiest approach
		ClearEffects(Effects);
		check(Effects.Num() == 0);

		if (FGameplayCueViewInfo * View = GetBestMatchingView(EGameplayCueEvent::WhileActive, *TagIt, InstigatorLocal, TargetLocal))
		{
			TSharedPtr<FGameplayCueViewEffects> SpawnedEffects = View->SpawnViewEffects(Owner, &SpawnedObjects, EffectContext);
			Effects.Add(SpawnedEffects);
		}
	}
}

void FGameplayCueHandler::GameplayCueRemoved(const FGameplayTagContainer& GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectContextHandle& EffectContext)
{
	check(Owner);
	bool InstigatorLocal = EffectContext.IsLocallyControlled();
	bool TargetLocal = OwnerIsLocallyControlled();

	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		TArray<TSharedPtr<FGameplayCueViewEffects> > *Effects = SpawnedViewEffects.Find(*TagIt);
		if (Effects)
		{
			ClearEffects(*Effects);
			SpawnedViewEffects.Remove(*TagIt);
		}	
		
		// Remove old effects
		
		if (FGameplayCueViewInfo * View = GetBestMatchingView(EGameplayCueEvent::Removed, *TagIt, InstigatorLocal, TargetLocal))
		{
			View->SpawnViewEffects(Owner, NULL, EffectContext);
		}

		FName MatchedTag;
		UFunction *Func = UAbilitySystemGlobals::Get().GetGameplayCueFunction(*TagIt, Owner->GetClass(), MatchedTag);
		if (Func)
		{
			FGameplayCueParameters Params;
			Params.NormalizedMagnitude = NormalizedMagnitude;
			Params.EffectContext = EffectContext;

			IGameplayCueInterface::DispatchBlueprintCustomHandler(Owner, Func, EGameplayCueEvent::Removed, Params);
		}
	}
}

void FGameplayCueHandler::ClearEffects(TArray< TSharedPtr<FGameplayCueViewEffects> > &Effects)
{
	bool RemovedSomething = false;
	for (TSharedPtr<FGameplayCueViewEffects> &EffectPtr : Effects)
	{
		if (!EffectPtr.IsValid())
		{
			continue;
		}

		FGameplayCueViewEffects &Effect = *EffectPtr.Get();

		if (Effect.SpawnedActor.IsValid())
		{
			Effect.SpawnedActor->Destroy();
			RemovedSomething = true;
		}

		if (Effect.AudioComponent.IsValid())
		{
			Effect.AudioComponent->Stop();
			RemovedSomething = true;
		}

		if (Effect.ParticleSystemComponent.IsValid())
		{
			Effect.ParticleSystemComponent->DestroyComponent();
			RemovedSomething = true;
		}
	}

	Effects.Empty();
	
	// Cleanup flat array
	if (RemovedSomething)
	{
		for (int32 idx=0; idx < SpawnedObjects.Num(); ++idx)
		{
			UObject *Obj = SpawnedObjects[idx];
			if (!Obj || Obj->IsPendingKill())
			{
				SpawnedObjects.RemoveAtSwap(idx);
				idx--;
			}
		}
	}
}

TSharedPtr<FGameplayCueViewEffects> FGameplayCueViewInfo::SpawnViewEffects(AActor *Owner, TArray<UObject*> *SpawnedObjects, const FGameplayEffectContextHandle& EffectContext) const
{
	check(Owner);

	TSharedPtr<FGameplayCueViewEffects> SpawnedEffects = TSharedPtr<FGameplayCueViewEffects>(new FGameplayCueViewEffects());

	if (Sound)
	{
		SpawnedEffects->AudioComponent = UGameplayStatics::PlaySoundAttached(Sound, Owner->GetRootComponent());
		if (SpawnedObjects)
		{
			SpawnedObjects->Add(SpawnedEffects->AudioComponent.Get());
		}
	}
	if (ParticleSystem)
	{
		if (EffectContext.GetHitResult())
		{
			const FHitResult &HitResult = *EffectContext.GetHitResult();

			if (AttachParticleSystem)
			{
				SpawnedEffects->ParticleSystemComponent = UGameplayStatics::SpawnEmitterAttached(ParticleSystem, Owner->GetRootComponent(), NAME_None, HitResult.Location,
					HitResult.Normal.Rotation(), EAttachLocation::KeepWorldPosition);
			}
			else
			{
				bool AutoDestroy = SpawnedObjects == nullptr;
				SpawnedEffects->ParticleSystemComponent = UGameplayStatics::SpawnEmitterAtLocation(Owner->GetWorld(), ParticleSystem, HitResult.Location, HitResult.Normal.Rotation(), AutoDestroy);
			}
		}
		else
		{
			if (AttachParticleSystem)
			{
				SpawnedEffects->ParticleSystemComponent = UGameplayStatics::SpawnEmitterAttached(ParticleSystem, Owner->GetRootComponent());
			}
			else
			{
				bool AutoDestroy = SpawnedObjects == nullptr;
				SpawnedEffects->ParticleSystemComponent = UGameplayStatics::SpawnEmitterAtLocation(Owner->GetWorld(), ParticleSystem, Owner->GetActorLocation(), Owner->GetActorRotation(), AutoDestroy);
			}
		}		

		if (SpawnedObjects)
		{
			SpawnedObjects->Add(SpawnedEffects->ParticleSystemComponent.Get());
		}
		else if (SpawnedEffects->ParticleSystemComponent.IsValid())
		{
			for (int32 EmitterIndx = 0; EmitterIndx < SpawnedEffects->ParticleSystemComponent->EmitterInstances.Num(); EmitterIndx++)
			{
				if (SpawnedEffects->ParticleSystemComponent->EmitterInstances[EmitterIndx] &&
					SpawnedEffects->ParticleSystemComponent->EmitterInstances[EmitterIndx]->CurrentLODLevel &&
					SpawnedEffects->ParticleSystemComponent->EmitterInstances[EmitterIndx]->CurrentLODLevel->RequiredModule &&
					SpawnedEffects->ParticleSystemComponent->EmitterInstances[EmitterIndx]->CurrentLODLevel->RequiredModule->EmitterLoops == 0)
				{
					ABILITY_LOG(Warning, TEXT("%s - particle system has a looping emitter. This should not be used in a executed GameplayCue!"), *SpawnedEffects->ParticleSystemComponent->GetName());
					break;
				}
			}
		}
	}
	if (ActorClass)
	{
		FVector Location = Owner->GetActorLocation();
		FRotator Rotation = Owner->GetActorRotation();
		SpawnedEffects->SpawnedActor = Cast<AGameplayCueActor>(Owner->GetWorld()->SpawnActor(ActorClass, &Location, &Rotation));
		
		if (SpawnedObjects)
		{
			SpawnedObjects->Add(SpawnedEffects->SpawnedActor.Get());
		}
	}

	return SpawnedEffects;
}

bool FGameplayCueHandler::OwnerIsLocallyControlled() const
{
	APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}

	return false;
}

// ---------------------------------------------------

