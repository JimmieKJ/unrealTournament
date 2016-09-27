// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTInGameIntroZone.h"
#include "UTCharacter.h"

//#if WITH_EDITORONLY_DATA
//AUTInGameIntroEditorVisualization::AUTInGameIntroEditorVisualization(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//}
//#endif //WITH_EDITORONLY_DATA

AUTInGameIntroZoneTeamSpawnPointList::AUTInGameIntroZoneTeamSpawnPointList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSnapToFloor = true;

	SceneRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneRoot;
	RootComponent->Mobility = EComponentMobility::Static;
	
	//// Setup camera defaults
	//TeamCamera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("CameraComponent"));
	//TeamCamera->AttachToComponent(RootComponent,FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative,false));
}

AUTInGameIntroZone::AUTInGameIntroZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ZoneType = InGameIntroZoneTypes::FFA_Intro;
}


#if WITH_EDITORONLY_DATA

void AUTInGameIntroZoneTeamSpawnPointList::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AUTInGameIntroZoneTeamSpawnPointList::UpdateMeshVisualizations()
{
	//Delete any spawned mesh components and early out if we have no Player Spawn Locations to map them to
	if (PlayerSpawnLocations.Num() == 0)
	{
		DeleteAllMeshVisualizations();
		return;
	}
	
	//if sizes don't match. Wipe visualizations and start over
	if (PlayerSpawnLocations.Num() != MeshVisualizations.Num())
	{
		InitializeMeshVisualizations();
	}

	for (int index = 0; index < MeshVisualizations.Num(); ++index)
	{
		if (MeshVisualizations[index] != nullptr)
		{
			MeshVisualizations[index]->GetRootComponent()->SetRelativeTransform(PlayerSpawnLocations[index]);
		}
	}
}

void AUTInGameIntroZoneTeamSpawnPointList::DeleteAllMeshVisualizations()
{
	for (int index = 0; index < MeshVisualizations.Num(); ++index)
	{
		//MeshVisualizations[index]->DestroyComponent();
		MeshVisualizations[index]->DetachRootComponentFromParent(true);
		MeshVisualizations[index]->Instigator = nullptr;
		MeshVisualizations[index]->SetOwner(nullptr);
		MeshVisualizations[index]->Destroy(true, true);
	}

	MeshVisualizations.Empty();
}

void AUTInGameIntroZoneTeamSpawnPointList::InitializeMeshVisualizations()
{
	DeleteAllMeshVisualizations();

	//Without any player spawn locations, we should just early return after the empty. Nothing to see here.
	if ((PlayerSpawnLocations.Num() == 0) || (EditorVisualizationCharacter == nullptr) || (GetWorld() == nullptr))
	{
		return;
	}

	for (int index = 0; index < PlayerSpawnLocations.Num(); ++index)
	{
		//UChildActorComponent* ChildActorComp = NewObject<UChildActorComponent>(this);
		//if (ChildActorComp)
		//{
		//	MeshVisualizations.Add(ChildActorComp);
		//	ChildActorComp->SetChildActorClass(EditorVisualizationCharacter);
		//	ChildActorComp->CreateChildActor();

		//	//ChildActorComp->DetachFromParent(true, true);
		//	// ChildActorComp->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false));
		//}

		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTInGameIntroZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTInGameIntroZoneVisualizationCharacter>(EditorVisualizationCharacter, PlayerSpawnLocations[index], Params);
		if (SpawnedActor)
		{
			MeshVisualizations.Add(SpawnedActor);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void AUTInGameIntroZone::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		for (AUTInGameIntroZoneTeamSpawnPointList* PointList : TeamSpawns)
		{
			PointList->UpdateMeshVisualizations();
		}
	}

	Super::PostEditMove(bFinished);
}

void AUTInGameIntroZoneTeamSpawnPointList::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		if (bSnapToFloor)
		{
			SnapToFloor();
		}
		else
		{
			UpdateMeshVisualizations();
		}
	}

	Super::PostEditMove(bFinished);
}

void AUTInGameIntroZoneTeamSpawnPointList::SnapToFloor()
{
	float CapsuleHalfHeight = 92.0f;

	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	for (FTransform& Location : PlayerSpawnLocations)
	{
		FTransform TestLocation = Location * ActorToWorld();

		FVector Start(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z + 500.0f);
		FVector End(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z - 10000.0f);

		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), FCollisionQueryParams(NAME_FreeCam, false, this));
		if (Hit.bBlockingHit)
		{
			FVector NewLocation = Location.GetLocation();
			NewLocation.Z = (Hit.Location - GetActorLocation()).Z + CapsuleHalfHeight;
			Location.SetLocation(NewLocation);
		}
	}

	UpdateMeshVisualizations();
}

void AUTInGameIntroZoneTeamSpawnPointList::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("PlayerSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}
	
	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("bSnapToFloor")))
	{
		if (bSnapToFloor)
		{
			SnapToFloor();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AUTInGameIntroZoneTeamSpawnPointList::UpdateSpawnLocationsWithVisualizationMove()
{
	for (int index = 0; index < MeshVisualizations.Num();++index)
	{
		PlayerSpawnLocations[index] = MeshVisualizations[index]->GetRootComponent()->GetRelativeTransform();
		//PlayerSpawnLocations[index] = GetTransform().GetRelativeTransformReverse(MeshVisualizations[index]->GetTransform());
	}
}

#endif