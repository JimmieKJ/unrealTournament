// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTInGameIntroZone.h"
#include "UTCharacter.h"


AUTInGameIntroZone::AUTInGameIntroZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSnapToFloor = true;

	SceneRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneRoot;
	RootComponent->Mobility = EComponentMobility::Static;

	ZoneType = InGameIntroZoneTypes::Invalid;
}


#if WITH_EDITOR

void AUTInGameIntroZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}


void AUTInGameIntroZone::PostEditMove(bool bFinished)
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

void AUTInGameIntroZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("RedTeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("BlueTeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("FFATeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("ZoneType")))
	{
		if ((ZoneType == InGameIntroZoneTypes::Intermission) || (ZoneType == InGameIntroZoneTypes::PostMatch))
		{
			DefaultCreateForFFAGame();
		}
		else
		{
			DefaultCreateForTeamGame();
		}
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

#endif

void AUTInGameIntroZone::UpdateMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	//If visualization counts don't match up wipe them and start over
	if ((RedTeamSpawnLocations.Num() + BlueTeamSpawnLocations.Num() + FFATeamSpawnLocations.Num()) != MeshVisualizations.Num())
	{
		DeleteAllMeshVisualizations();
		InitializeMeshVisualizations();
		return;
	}

	int MeshIndex = 0;
	for (int RedTeamIndex = 0; ((RedTeamIndex < RedTeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)) ; ++RedTeamIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(RedTeamSpawnLocations[RedTeamIndex]);
		++MeshIndex;
	}

	for (int BlueTeamIndex = 0; ((BlueTeamIndex < BlueTeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)); ++BlueTeamIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(BlueTeamSpawnLocations[BlueTeamIndex]);
		++MeshIndex;
	}

	for (int FFAIndex = 0; ((FFAIndex < FFATeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)); ++FFAIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(FFATeamSpawnLocations[FFAIndex]);
		++MeshIndex;
	}
#endif
}

void AUTInGameIntroZone::DeleteAllMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	for (int index = 0; index < MeshVisualizations.Num(); ++index)
	{
		//MeshVisualizations[index]->DestroyComponent();
		MeshVisualizations[index]->DetachRootComponentFromParent(true);
		MeshVisualizations[index]->Instigator = nullptr;
		MeshVisualizations[index]->SetOwner(nullptr);
		MeshVisualizations[index]->Destroy(true, true);
	}

	MeshVisualizations.Empty();
#endif
}

void AUTInGameIntroZone::InitializeMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	DeleteAllMeshVisualizations();

	//Without any player spawn locations, we should just early return after the empty. Nothing to see here.
	if ((EditorVisualizationCharacter == nullptr) || (GetWorld() == nullptr))
	{
		return;
	}

	for (int RedIndex = 0; RedIndex < RedTeamSpawnLocations.Num(); ++RedIndex)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTInGameIntroZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTInGameIntroZoneVisualizationCharacter>(EditorVisualizationCharacter, RedTeamSpawnLocations[RedIndex], Params);
		if (SpawnedActor)
		{
			MeshVisualizations.Add(SpawnedActor);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
			SpawnedActor->TeamNum = 0;
			SpawnedActor->OnChangeTeamNum();
		}
	}

	for (int BlueIndex = 0; BlueIndex < BlueTeamSpawnLocations.Num(); ++BlueIndex)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTInGameIntroZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTInGameIntroZoneVisualizationCharacter>(EditorVisualizationCharacter, BlueTeamSpawnLocations[BlueIndex], Params);
		if (SpawnedActor)
		{
			MeshVisualizations.Add(SpawnedActor);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
			SpawnedActor->TeamNum = 1;
			SpawnedActor->OnChangeTeamNum();
		}
	}

	for (int FFAIndex = 0; FFAIndex < FFATeamSpawnLocations.Num(); ++FFAIndex)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTInGameIntroZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTInGameIntroZoneVisualizationCharacter>(EditorVisualizationCharacter, FFATeamSpawnLocations[FFAIndex], Params);
		if (SpawnedActor)
		{
			MeshVisualizations.Add(SpawnedActor);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
			SpawnedActor->TeamNum = 255;
			SpawnedActor->OnChangeTeamNum();
		}
	}
#endif
}

void AUTInGameIntroZone::SnapToFloor()
{
	float CapsuleHalfHeight = 100.f;

	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	for (FTransform& Location : RedTeamSpawnLocations)
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

	for (FTransform& Location : BlueTeamSpawnLocations)
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

	for (FTransform& Location : FFATeamSpawnLocations)
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

void AUTInGameIntroZone::UpdateSpawnLocationsWithVisualizationMove()
{
#if WITH_EDITORONLY_DATA
	
	int RedIndex = 0;
	int BlueIndex = 0;
	int FFAIndex = 0;

	for (int MeshIndex = 0; MeshIndex < MeshVisualizations.Num(); ++MeshIndex)
	{
		AUTInGameIntroZoneVisualizationCharacter* Mesh = Cast<AUTInGameIntroZoneVisualizationCharacter>(MeshVisualizations[MeshIndex]);
		if (Mesh)
		{
			if ((Mesh->TeamNum == 0) && (RedTeamSpawnLocations.Num() > RedIndex))
			{
				RedTeamSpawnLocations[RedIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++RedIndex;
			}
			else if ((Mesh->TeamNum == 1) && (BlueTeamSpawnLocations.Num() > BlueIndex))
			{
				BlueTeamSpawnLocations[BlueIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++BlueIndex;
			}
			else if (FFATeamSpawnLocations.Num() > FFAIndex)
			{
				FFATeamSpawnLocations[FFAIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++FFAIndex;
			}
		}
	}
#endif
}

#if WITH_EDITORONLY_DATA
void AUTInGameIntroZone::DefaultCreateForTeamGame()
{
	RedTeamSpawnLocations.Empty();
	BlueTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	RedTeamSpawnLocations.SetNum(5);
	BlueTeamSpawnLocations.SetNum(5);

	FVector RedStartLocation(200, 200, 0);
	FVector RedDifference(100, 100, 0);

	FVector BlueStartLocation(200, -200, 0);
	FVector BlueDifference(100, -100, 0);
	
	FRotator WarpRotation;
	WarpRotation.Pitch = 0.f;
	WarpRotation.Roll = 0.f;
	WarpRotation.Yaw = -180.f;

	for (int RedIndex = 0; RedIndex < 5; ++RedIndex)
	{
		RedTeamSpawnLocations[RedIndex].SetTranslation(RedStartLocation + (RedDifference * RedIndex));
		RedTeamSpawnLocations[RedIndex].SetRotation(WarpRotation.Quaternion());
	}

	for (int BlueIndex = 0; BlueIndex < 5; ++BlueIndex)
	{
		BlueTeamSpawnLocations[BlueIndex].SetTranslation(BlueStartLocation + (BlueDifference * BlueIndex));
		BlueTeamSpawnLocations[BlueIndex].SetRotation(WarpRotation.Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!TeamCamera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			TeamCamera = SpawnedActor;
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void AUTInGameIntroZone::DefaultCreateForFFAGame()
{
	RedTeamSpawnLocations.Empty();
	BlueTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	FFATeamSpawnLocations.SetNum(10);

	FVector LeftSideStartLocation(200, 200, 0);
	FVector LeftSideDifference(100, 100, 0);

	FVector RightSideStartLocation(200, -200, 0);
	FVector RightSideDifference(100, -100, 0);

	FRotator WarpRotation;
	WarpRotation.Pitch = 0.f;
	WarpRotation.Roll = 0.f;
	WarpRotation.Yaw = -180.f;

	for (int LeftIndex = 0; LeftIndex < 5; ++LeftIndex)
	{
		FFATeamSpawnLocations[LeftIndex].SetTranslation(LeftSideStartLocation + (LeftSideDifference * LeftIndex));
		FFATeamSpawnLocations[LeftIndex].SetRotation(WarpRotation.Quaternion());
	}

	for (int RightIndex = 5; RightIndex < 10; ++RightIndex)
	{
		FFATeamSpawnLocations[RightIndex].SetTranslation(RightSideStartLocation + (RightSideDifference * (RightIndex - 5)));
		FFATeamSpawnLocations[RightIndex].SetRotation(WarpRotation.Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!TeamCamera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			TeamCamera = SpawnedActor;
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}
#endif //WITH_EDITORONLY_DATA