// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ComponentInstanceDataCache.h"
#include "Components/ChildActorComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogChildActorComponent, Warning, All);

UChildActorComponent::UChildActorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UChildActorComponent::OnRegister()
{
	Super::OnRegister();

	if (ChildActor)
	{
		if (ChildActor->GetClass() != ChildActorClass)
		{
			DestroyChildActor();
			CreateChildActor();
		}
		else
		{
			ChildActorName = ChildActor->GetFName();
			// attach new actor to this component
			// we can't attach in CreateChildActor since it has intermediate Mobility set up
			// causing spam with inconsistent mobility set up
			// so moving Attach to happen in Register
			ChildActor->AttachRootComponentTo(this, NAME_None, EAttachLocation::SnapToTargetIncludingScale);
		}
	}
	else if (ChildActorClass)
	{
		CreateChildActor();
	}
}

#if WITH_EDITOR
void UChildActorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_ChildActorClass = GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorClass);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == NAME_ChildActorClass)
	{
		ChildActorName = NAME_None;

		// If this was created by construction script, the post edit change super call will destroy it anyways
		if (!IsCreatedByConstructionScript())
		{
			DestroyChildActor();
			CreateChildActor();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UChildActorComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// This hack exists to fix up known cases where the AttachChildren array is broken in very problematic ways.
	// The correct fix will be to use a Transaction Annotation at the SceneComponent level, however, it is too risky
	// to do right now, so this will go away when that is done.
	for (USceneComponent*& Component : AttachChildren)
	{
		if (Component)
		{
			if (Component->IsPendingKill() && Component->GetOwner() == ChildActor)
			{
				Component = ChildActor->GetRootComponent();
			}
		}
	}
	
}
#endif

void UChildActorComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	CreateChildActor();
}

void UChildActorComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();

	DestroyChildActor(GetWorld() && !GetWorld()->IsGameWorld());
}

class FChildActorComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FChildActorComponentInstanceData(const UChildActorComponent* Component)
		: FSceneComponentInstanceData(Component)
		, ChildActorName(Component->ChildActorName)
	{
		if (Component->ChildActor)
		{
			USceneComponent* ChildRootComponent = Component->ChildActor->GetRootComponent();
			if (ChildRootComponent)
			{
				for (USceneComponent* AttachedComponent : ChildRootComponent->AttachChildren)
				{
					if (AttachedComponent)
					{
						AActor* AttachedActor = AttachedComponent->GetOwner();
						if (AttachedActor != Component->ChildActor)
						{
							FAttachedActorInfo Info;
							Info.Actor = AttachedActor;
							Info.SocketName = AttachedComponent->AttachSocketName;
							Info.RelativeTransform = AttachedComponent->GetRelativeTransform();
							AttachedActors.Add(Info);
						}
					}
				}
			}
		}
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UChildActorComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FName ChildActorName;

	struct FAttachedActorInfo
	{
		TWeakObjectPtr<AActor> Actor;
		FName SocketName;
		FTransform RelativeTransform;
	};

	TArray<FAttachedActorInfo> AttachedActors;
};

FActorComponentInstanceData* UChildActorComponent::GetComponentInstanceData() const
{
	FChildActorComponentInstanceData* InstanceData = CachedInstanceData;

	if (CachedInstanceData)
	{
		// We've handed over ownership of the pointer to the instance cache, so drop our reference
		CachedInstanceData = nullptr;
	}
	else
	{
		InstanceData = new FChildActorComponentInstanceData(this);
	}
	
	return InstanceData;
}

void UChildActorComponent::ApplyComponentInstanceData(FChildActorComponentInstanceData* ChildActorInstanceData)
{
	check(ChildActorInstanceData);

	ChildActorName = ChildActorInstanceData->ChildActorName;
	if (ChildActor)
	{
		// Only rename if it is safe to
		if(ChildActorName != NAME_None)
		{
			const FString ChildActorNameString = ChildActorName.ToString();
			if (ChildActor->Rename(*ChildActorNameString, nullptr, REN_Test))
			{
				ChildActor->Rename(*ChildActorNameString, nullptr, REN_DoNotDirty | (IsLoading() ? REN_ForceNoResetLoaders : REN_None));
			}
		}

		USceneComponent* ChildActorRoot = ChildActor->GetRootComponent();
		if (ChildActorRoot)
		{
			for (const auto& AttachInfo : ChildActorInstanceData->AttachedActors)
			{
				AActor* AttachedActor = AttachInfo.Actor.Get();
				if (AttachedActor)
				{
					USceneComponent* AttachedRootComponent = AttachedActor->GetRootComponent();
					if (AttachedRootComponent)
					{
						AttachedActor->DetachRootComponentFromParent();
						AttachedRootComponent->AttachTo(ChildActorRoot, AttachInfo.SocketName, EAttachLocation::KeepWorldPosition);
						AttachedRootComponent->SetRelativeTransform(AttachInfo.RelativeTransform);
						AttachedRootComponent->UpdateComponentToWorld();
					}
				}
			}
		}
	}
}

void UChildActorComponent::SetChildActorClass(TSubclassOf<AActor> Class)
{
	ChildActorClass = Class;
	if (IsRegistered())
	{
		DestroyChildActor();
		CreateChildActor();
	}
}

void UChildActorComponent::CreateChildActor()
{
	// Kill spawned actor if we have one
	DestroyChildActor();

	// This is no longer needed
	if (CachedInstanceData)
	{
		delete CachedInstanceData;
		CachedInstanceData = nullptr;
	}

	// If we have a class to spawn.
	if(ChildActorClass != nullptr)
	{
		UWorld* World = GetWorld();
		if(World != nullptr)
		{
			// Before we spawn let's try and prevent cyclic disaster
			bool bSpawn = true;
			AActor* MyOwner = GetOwner();
			AActor* Actor = MyOwner;
			while (Actor && bSpawn)
			{
				if (Actor->GetClass() == ChildActorClass)
				{
					bSpawn = false;
					UE_LOG(LogChildActorComponent, Error, TEXT("Found cycle in child actor component '%s'.  Not spawning Actor of class '%s' to break."), *GetPathName(), *ChildActorClass->GetName());
				}
				Actor = Actor->ParentComponentActor.Get();
			}

			if (bSpawn)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.bDeferConstruction = true; // We defer construction so that we set ParentComponentActor prior to component registration so they appear selected
				Params.bAllowDuringConstructionScript = true;
				Params.OverrideLevel = (MyOwner ? MyOwner->GetLevel() : nullptr);
				Params.Name = ChildActorName;
				if (!HasAllFlags(RF_Transactional))
				{
					Params.ObjectFlags &= ~RF_Transactional;
				}

				// Spawn actor of desired class
				FVector Location = GetComponentLocation();
				FRotator Rotation = GetComponentRotation();
				ChildActor = World->SpawnActor(ChildActorClass, &Location, &Rotation, Params);

				// If spawn was successful, 
				if(ChildActor != nullptr)
				{
					ChildActorName = ChildActor->GetFName();

					// Remember which actor spawned it (for selection in editor etc)
					ChildActor->ParentComponentActor = MyOwner;

					// Parts that we deferred from SpawnActor
					ChildActor->FinishSpawning(ComponentToWorld);

					ChildActor->AttachRootComponentTo(this, NAME_None, EAttachLocation::SnapToTargetIncludingScale);
				}
			}
		}
	}
}

void UChildActorComponent::DestroyChildActor(const bool bRequiresRename)
{
	// If we own an Actor, kill it now
	if(ChildActor != nullptr && !GExitPurge)
	{
		// if still alive, destroy, otherwise just clear the pointer
		if(!ChildActor->IsPendingKill())
		{
#if WITH_EDITOR
			if (CachedInstanceData)
			{
				delete CachedInstanceData;
			}
#else
			check(!CachedInstanceData);
#endif
			CachedInstanceData = new FChildActorComponentInstanceData(this);

			UWorld* World = ChildActor->GetWorld();
			// World may be nullptr during shutdown
			if(World != nullptr)
			{
				UClass* ChildClass = ChildActor->GetClass();

				// We would like to make certain that our name is not going to accidentally get taken from us while we're destroyed
				// so we increment ClassUnique beyond our index to be certain of it.  This is ... a bit hacky.
				ChildClass->ClassUnique = FMath::Max(ChildClass->ClassUnique, ChildActor->GetFName().GetNumber());

				if (bRequiresRename)
				{
					const FString ObjectBaseName = FString::Printf(TEXT("DESTROYED_%s_CHILDACTOR"), *ChildClass->GetName());
					const ERenameFlags RenameFlags = ((GetWorld()->IsGameWorld() || IsLoading()) ? REN_DoNotDirty | REN_ForceNoResetLoaders : REN_DoNotDirty);
					ChildActor->Rename(*MakeUniqueObjectName(ChildActor->GetOuter(), ChildClass, *ObjectBaseName).ToString(), nullptr, RenameFlags);
				}
				World->DestroyActor(ChildActor);
			}
		}

		ChildActor = nullptr;
	}
}
