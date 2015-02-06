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
		ChildActorName = ChildActor->GetFName();
		// attach new actor to this component
		// we can't attach in CreateChildActor since it has intermediate Mobility set up
		// causing spam with inconsistent mobility set up
		// so moving Attach to happen in Register
		ChildActor->AttachRootComponentTo(this, NAME_None, EAttachLocation::SnapToTarget);
	}
}

void UChildActorComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	CreateChildActor();
}

void UChildActorComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();

	DestroyChildActor();
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

	virtual void ApplyToComponent(UActorComponent* Component) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component);
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

FName UChildActorComponent::GetComponentInstanceDataType() const
{
	static const FName ChildActorComponentInstanceDataName(TEXT("ChildActorInstanceData"));
	return ChildActorComponentInstanceDataName;
}

FComponentInstanceDataBase* UChildActorComponent::GetComponentInstanceData() const
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
				ChildActor->Rename(*ChildActorNameString, nullptr, REN_DoNotDirty);
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
			AActor* Actor = GetOwner();
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
				Params.bNoCollisionFail = true;
				Params.bDeferConstruction = true; // We defer construction so that we set ParentComponentActor prior to component registration so they appear selected
				Params.bAllowDuringConstructionScript = true;
				Params.OverrideLevel = GetOwner()->GetLevel();
				Params.Name = ChildActorName;
				Params.ObjectFlags &= ~RF_Transactional;

				// Spawn actor of desired class
				FVector Location = GetComponentLocation();
				FRotator Rotation = GetComponentRotation();
				ChildActor = World->SpawnActor(ChildActorClass, &Location, &Rotation, Params);

				// If spawn was successful, 
				if(ChildActor != nullptr)
				{
					ChildActorName = ChildActor->GetFName();

					// Remember which actor spawned it (for selection in editor etc)
					ChildActor->ParentComponentActor = GetOwner();

					// Parts that we deferred from SpawnActor
					ChildActor->FinishSpawning(ComponentToWorld);
				}
			}
		}
	}
}

void UChildActorComponent::DestroyChildActor()
{
	// If we own an Actor, kill it now
	if(ChildActor != nullptr)
	{
		// if still alive, destroy, otherwise just clear the pointer
		if(!ChildActor->IsPendingKill())
		{
			check(!CachedInstanceData);
			CachedInstanceData = new FChildActorComponentInstanceData(this);

			UWorld* World = ChildActor->GetWorld();
			// World may be nullptr during shutdown
			if(World != nullptr)
			{
				UClass* ChildClass = ChildActor->GetClass();

				// We would like to make certain that our name is not going to accidentally get taken from us while we're destroyed
				// so we increment ClassUnique beyond our index to be certain of it.  This is ... a bit hacky.
				ChildClass->ClassUnique = FMath::Max(ChildClass->ClassUnique, ChildActor->GetFName().GetNumber());

				const FString ObjectBaseName = FString::Printf(TEXT("DESTROYED_%s_CHILDACTOR"), *ChildClass->GetName());
				ChildActor->Rename(*MakeUniqueObjectName(ChildActor->GetOuter(), ChildClass, *ObjectBaseName).ToString(), nullptr, REN_DoNotDirty);
				World->DestroyActor(ChildActor);
			}
		}

		ChildActor = nullptr;
	}
}
