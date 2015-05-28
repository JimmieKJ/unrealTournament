// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/LevelBounds.h"
#include "Components/BoxComponent.h"

// Default size of the box (scale)
static const FVector DefaultLevelSize = FVector(1000.f);

ALevelBounds::ALevelBounds(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UBoxComponent* BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent0"));
	RootComponent = BoxComponent;
	RootComponent->Mobility = EComponentMobility::Movable;
	RootComponent->RelativeScale3D = DefaultLevelSize;

	bAutoUpdateBounds = true;

	BoxComponent->bDrawOnlyIfSelected = true;
	BoxComponent->bUseAttachParentBound = false;
	BoxComponent->bUseEditorCompositing = true;
	BoxComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BoxComponent->InitBoxExtent(FVector(0.5f, 0.5f, 0.5f));
	
	bCanBeDamaged = false;
	
#if WITH_EDITOR
	bLevelBoundsDirty = true;
	bUsingDefaultBounds = false;
#endif
}

void ALevelBounds::PostLoad()
{
	Super::PostLoad();
	GetLevel()->LevelBoundsActor = this;
}

FBox ALevelBounds::GetComponentsBoundingBox(bool bNonColliding) const
{
	FVector BoundsCenter = RootComponent->GetComponentLocation();
	FVector BoundsExtent = RootComponent->ComponentToWorld.GetScale3D() * 0.5f;
	return FBox(BoundsCenter - BoundsExtent, 
				BoundsCenter + BoundsExtent);
}

FBox ALevelBounds::CalculateLevelBounds(ULevel* InLevel)
{
	FBox LevelBounds = FBox(0);
	
	if (InLevel)
	{
		// Iterate over all level actors
		for (int32 ActorIndex = 0; ActorIndex < InLevel->Actors.Num() ; ++ActorIndex)
		{
			AActor* Actor = InLevel->Actors[ActorIndex];
			if (Actor && Actor->IsLevelBoundsRelevant())
			{
				// Sum up components bounding boxes
				FBox ActorBox = Actor->GetComponentsBoundingBox(true);
				if (ActorBox.IsValid)
				{
					LevelBounds+= ActorBox;
				}
			}
		}
	}

	return LevelBounds;
}

#if WITH_EDITOR
void ALevelBounds::PostEditUndo()
{
	Super::PostEditUndo();
	
	MarkLevelBoundsDirty();
}

void ALevelBounds::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	MarkLevelBoundsDirty();
}

void ALevelBounds::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty( PropertyChangedEvent );
	
	MarkLevelBoundsDirty();
}

void ALevelBounds::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	
	if (!IsTemplate())
	{
		GetLevel()->LevelBoundsActor = this;
		SubscribeToUpdateEvents();
	}
}

void ALevelBounds::PostUnregisterAllComponents()
{
	if (!IsTemplate())
	{
		UnsubscribeFromUpdateEvents();
	}
	
	Super::PostUnregisterAllComponents();
}

void ALevelBounds::Tick(float DeltaTime)
{
	if (bLevelBoundsDirty)
	{
		UpdateLevelBounds();
		bLevelBoundsDirty = false;
	}
}

TStatId ALevelBounds::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ALevelBounds, STATGROUP_Tickables);
}

bool ALevelBounds::IsTickable() const
{
	if (GIsEditor && bAutoUpdateBounds && !IsTemplate())
	{
		UWorld* World = GetWorld();
		return (World && World->WorldType == EWorldType::Editor);
	}

	return false;
}

bool ALevelBounds::IsTickableInEditor() const
{
	return IsTickable();
}

void ALevelBounds::UpdateLevelBounds()
{
	FBox LevelBounds = CalculateLevelBounds(GetLevel());
	if (LevelBounds.IsValid)
	{
		FVector LevelCenter = LevelBounds.GetCenter();
		FVector LevelSize = LevelBounds.GetSize();
		
		SetActorTransform(FTransform(FQuat::Identity, LevelCenter, LevelSize));
		bUsingDefaultBounds = false;
	}
	else
	{
		SetActorTransform(FTransform(FQuat::Identity, FVector::ZeroVector, DefaultLevelSize));
		bUsingDefaultBounds = true;
	}
	
	BroadcastLevelBoundsUpdated();
}

void ALevelBounds::MarkLevelBoundsDirty()
{
	bLevelBoundsDirty = true;
}

bool ALevelBounds::IsUsingDefaultBounds() const
{
	return bUsingDefaultBounds;
}

void ALevelBounds::UpdateLevelBoundsImmediately()
{
	// This is used to get accurate bounds right when spawned.
	// This can't be done in PostActorCreated because the SpawnLocation interferes with the root component transform
	UpdateLevelBounds();
}

void ALevelBounds::OnLevelActorMoved(AActor* InActor)
{
	if (InActor->GetOuter() == GetOuter())
	{
		if (InActor == this)
		{
			BroadcastLevelBoundsUpdated();
		}
		else
		{
			MarkLevelBoundsDirty();
		}
	}
}
	
void ALevelBounds::OnLevelActorAddedRemoved(AActor* InActor)
{
	if (InActor->GetOuter() == GetOuter())
	{
		MarkLevelBoundsDirty();
	}
}

void ALevelBounds::BroadcastLevelBoundsUpdated()
{
	ULevel* Level = GetLevel();
	if (Level && 
		Level->LevelBoundsActor.Get() == this)
	{
		Level->BroadcastLevelBoundsActorUpdated();
	}
}

void ALevelBounds::SubscribeToUpdateEvents()
{
	// Subscribe only in editor worlds
	if (!GetWorld()->IsGameWorld())
	{
		UnsubscribeFromUpdateEvents();

		OnLevelActorMovedDelegateHandle   = GEngine->OnActorMoved       ().AddUObject(this, &ALevelBounds::OnLevelActorMoved);
		OnLevelActorDeletedDelegateHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &ALevelBounds::OnLevelActorAddedRemoved);
		OnLevelActorAddedDelegateHandle   = GEngine->OnLevelActorAdded  ().AddUObject(this, &ALevelBounds::OnLevelActorAddedRemoved);
	}
}

void ALevelBounds::UnsubscribeFromUpdateEvents()
{
	GEngine->OnActorMoved       ().Remove(OnLevelActorMovedDelegateHandle);
	GEngine->OnLevelActorDeleted().Remove(OnLevelActorDeletedDelegateHandle);
	GEngine->OnLevelActorAdded  ().Remove(OnLevelActorAddedDelegateHandle);
}


#endif // WITH_EDITOR


