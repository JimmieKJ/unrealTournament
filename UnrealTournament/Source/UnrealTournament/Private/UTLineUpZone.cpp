// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLineUpZone.h"
#include "UTCharacter.h"


AUTLineUpZone::AUTLineUpZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSnapToFloor = true;
	SnapFloorOffset = 95.f;

	SceneRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneRoot;
	RootComponent->Mobility = EComponentMobility::Static;

	ZoneType = LineUpTypes::Invalid;

	bIsTeamSpawnList = true;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (SpriteComponent)
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteObject(TEXT("/Engine/EditorResources/S_Note"));
		SpriteComponent->Sprite = SpriteObject.Get();
			
		SpriteComponent->SpriteInfo.Category = FName(TEXT("Notes"));
		SpriteComponent->SpriteInfo.DisplayName = FText(NSLOCTEXT("SpriteCategory", "Notes", "Notes"));
	
		SpriteComponent->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepRelativeTransform);
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->Mobility = EComponentMobility::Static;
	}
#endif //WITH_EDITORONLY_DATA
}


#if WITH_EDITOR

void AUTLineUpZone::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		InitializeMeshVisualizations();
	}
}

void AUTLineUpZone::PostEditMove(bool bFinished)
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

void AUTLineUpZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("RedAndWinningTeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("BlueAndLosingTeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("FFATeamSpawnLocations")))
	{
		UpdateMeshVisualizations();
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("ZoneType")))
	{
		if (bIsTeamSpawnList)
		{
			if (ZoneType == LineUpTypes::Intro)
			{
				DefaultCreateForTeamIntro();
			}
			else if (ZoneType == LineUpTypes::Intermission)
			{
				DefaultCreateForTeamIntermission();
			}
			else if (ZoneType == LineUpTypes::PostMatch)
			{
				DefaultCreateForTeamEndMatch();
			}
		}
		else
		{
			if (ZoneType == LineUpTypes::Intro)
			{
				DefaultCreateForFFAIntro();
			}
			else if (ZoneType == LineUpTypes::Intermission)
			{
				DefaultCreateForFFAIntermission();
			}
			else if (ZoneType == LineUpTypes::PostMatch)
			{
				DefaultCreateForFFAEndMatch();
			}
		}
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("bSnapToFloor")))
	{
		if (bSnapToFloor)
		{
			SnapToFloor();
		}
	}

	if (PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("bIsTeamSpawnList")))
	{
		if (bIsTeamSpawnList)
		{
			if (ZoneType == LineUpTypes::Intro)
			{
				DefaultCreateForTeamIntro();
			}
			else if (ZoneType == LineUpTypes::Intermission)
			{
				DefaultCreateForTeamIntermission();
			}
			else if (ZoneType == LineUpTypes::PostMatch)
			{
				DefaultCreateForTeamEndMatch();
			}
		}
		else
		{
			if (ZoneType == LineUpTypes::Intro)
			{
				DefaultCreateForFFAIntro();
			}
			else if (ZoneType == LineUpTypes::Intermission)
			{
				DefaultCreateForFFAIntermission();
			}
			else if (ZoneType == LineUpTypes::PostMatch)
			{
				DefaultCreateForFFAEndMatch();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void AUTLineUpZone::UpdateMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	//If visualization counts don't match up wipe them and start over
	if ((RedAndWinningTeamSpawnLocations.Num() + BlueAndLosingTeamSpawnLocations.Num() + FFATeamSpawnLocations.Num()) != MeshVisualizations.Num())
	{
		DeleteAllMeshVisualizations();
		InitializeMeshVisualizations();
		return;
	}

	int MeshIndex = 0;
	for (int RedTeamIndex = 0; ((RedTeamIndex < RedAndWinningTeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)) ; ++RedTeamIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(RedAndWinningTeamSpawnLocations[RedTeamIndex]);
		++MeshIndex;
	}

	for (int BlueTeamIndex = 0; ((BlueTeamIndex < BlueAndLosingTeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)); ++BlueTeamIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(BlueAndLosingTeamSpawnLocations[BlueTeamIndex]);
		++MeshIndex;
	}

	for (int FFAIndex = 0; ((FFAIndex < FFATeamSpawnLocations.Num()) && (MeshVisualizations.Num() > MeshIndex)); ++FFAIndex)
	{
		MeshVisualizations[MeshIndex]->GetRootComponent()->SetRelativeTransform(FFATeamSpawnLocations[FFAIndex]);
		++MeshIndex;
	}
#endif
}

void AUTLineUpZone::DeleteAllMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	for (int index = 0; index < MeshVisualizations.Num(); ++index)
	{
		if (MeshVisualizations[index])
		{
			//MeshVisualizations[index]->DestroyComponent();
			MeshVisualizations[index]->DetachRootComponentFromParent(true);
			MeshVisualizations[index]->Instigator = nullptr;
			MeshVisualizations[index]->SetOwner(nullptr);
			MeshVisualizations[index]->Destroy(true, true);
		}
	}

	MeshVisualizations.Empty();
#endif
}

void AUTLineUpZone::InitializeMeshVisualizations()
{
#if WITH_EDITORONLY_DATA
	DeleteAllMeshVisualizations();

	//Without any player spawn locations, we should just early return after the empty. Nothing to see here.
	if ((EditorVisualizationCharacter == nullptr) || (GetWorld() == nullptr))
	{
		return;
	}

	for (int RedIndex = 0; RedIndex < RedAndWinningTeamSpawnLocations.Num(); ++RedIndex)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTLineUpZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTLineUpZoneVisualizationCharacter>(EditorVisualizationCharacter, RedAndWinningTeamSpawnLocations[RedIndex], Params);
		if (SpawnedActor)
		{
			MeshVisualizations.Add(SpawnedActor);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
			SpawnedActor->TeamNum = 0;
			SpawnedActor->OnChangeTeamNum();
		}
	}

	for (int BlueIndex = 0; BlueIndex < BlueAndLosingTeamSpawnLocations.Num(); ++BlueIndex)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;

		AUTLineUpZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTLineUpZoneVisualizationCharacter>(EditorVisualizationCharacter, BlueAndLosingTeamSpawnLocations[BlueIndex], Params);
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

		AUTLineUpZoneVisualizationCharacter* SpawnedActor = GetWorld()->SpawnActor<AUTLineUpZoneVisualizationCharacter>(EditorVisualizationCharacter, FFATeamSpawnLocations[FFAIndex], Params);
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

void AUTLineUpZone::SnapToFloor()
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));

	//Move base LineUpZone actor to snapped floor position
	{
		FTransform TestLocation = ActorToWorld();

		FVector Start(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z + 500.f);
		FVector End(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z - 10000.f);

		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), FCollisionQueryParams(NAME_FreeCam, false, this));
		if (Hit.bBlockingHit)
		{
			FVector NewLocation = Hit.Location;
			NewLocation.Z += SnapFloorOffset;
			SetActorLocation(NewLocation);
		}
	}

	//Move all Red/Winning team spawns
	for (FTransform& Location : RedAndWinningTeamSpawnLocations)
	{
		FTransform TestLocation = Location * ActorToWorld();

		FVector Start(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z + 500.0f);
		FVector End(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z - 10000.0f);

		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), FCollisionQueryParams(NAME_FreeCam, false, this));
		if (Hit.bBlockingHit)
		{
			FVector NewLocation = Location.GetLocation();
			NewLocation.Z = (Hit.Location - GetActorLocation()).Z + SnapFloorOffset;
			Location.SetLocation(NewLocation);
		}
	}

	//Move all Blue/Losing team spawns
	for (FTransform& Location : BlueAndLosingTeamSpawnLocations)
	{
		FTransform TestLocation = Location * ActorToWorld();

		FVector Start(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z + 500.0f);
		FVector End(TestLocation.GetTranslation().X, TestLocation.GetTranslation().Y, TestLocation.GetTranslation().Z - 10000.0f);

		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), FCollisionQueryParams(NAME_FreeCam, false, this));
		if (Hit.bBlockingHit)
		{
			FVector NewLocation = Location.GetLocation();
			NewLocation.Z = (Hit.Location - GetActorLocation()).Z + SnapFloorOffset;
			Location.SetLocation(NewLocation);
		}
	}

	//Move all FFA spawns
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
			NewLocation.Z = (Hit.Location - GetActorLocation()).Z + SnapFloorOffset;
			Location.SetLocation(NewLocation);
		}
	}

	UpdateMeshVisualizations();
}

void AUTLineUpZone::UpdateSpawnLocationsWithVisualizationMove()
{
#if WITH_EDITORONLY_DATA
	
	int RedIndex = 0;
	int BlueIndex = 0;
	int FFAIndex = 0;

	for (int MeshIndex = 0; MeshIndex < MeshVisualizations.Num(); ++MeshIndex)
	{
		AUTLineUpZoneVisualizationCharacter* Mesh = Cast<AUTLineUpZoneVisualizationCharacter>(MeshVisualizations[MeshIndex]);
		if (Mesh)
		{
			if ((Mesh->TeamNum == 0) && (RedAndWinningTeamSpawnLocations.Num() > RedIndex))
			{
				RedAndWinningTeamSpawnLocations[RedIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++RedIndex;
			}
			else if ((Mesh->TeamNum == 1) && (BlueAndLosingTeamSpawnLocations.Num() > BlueIndex))
			{
				BlueAndLosingTeamSpawnLocations[BlueIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++BlueIndex;
			}
			else if (FFATeamSpawnLocations.Num() > FFAIndex)
			{
				FFATeamSpawnLocations[FFAIndex] = MeshVisualizations[MeshIndex]->GetRootComponent()->GetRelativeTransform();
				++FFAIndex;
			}
		}
	}

	if (bSnapToFloor)
	{
		SnapToFloor();
	}
#endif
}

#if WITH_EDITORONLY_DATA
void AUTLineUpZone::DefaultCreateForTeamIntro()
{
	TArray<FVector> RedStartLocations;
	TArray<FVector> BlueStartLocations;

	RedStartLocations.SetNum(5);
	BlueStartLocations.SetNum(5);

	BlueStartLocations[0] = FVector(-150.f, 275.f, 113.f);
	BlueStartLocations[1] = FVector(-50.f, 250.f, 113.f);
	BlueStartLocations[2] = FVector(50.f, 200.f, 113.f);
	BlueStartLocations[3] = FVector(150.f, 150.f, 113.f);
	BlueStartLocations[4] = FVector(250.f, 100.f, 113.f);
	
	RedStartLocations[0] = FVector(-150.f, -275.f, 113.f);
	RedStartLocations[1] = FVector(-50.f, -250.f, 113.f);
	RedStartLocations[2] = FVector(50.f, -200.f, 113.f);
	RedStartLocations[3] = FVector(150.f, -150.f, 113.f);
	RedStartLocations[4] = FVector(250.f, -100.f, 113.f);


	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	RedAndWinningTeamSpawnLocations.SetNum(5);
	BlueAndLosingTeamSpawnLocations.SetNum(5);

	FRotator BlueWarpRotation;
	BlueWarpRotation.Pitch = 0.f;
	BlueWarpRotation.Roll = 0.f;
	BlueWarpRotation.Yaw = -135.f;

	FRotator RedWarpRotation;
	RedWarpRotation.Pitch = 0.f;
	RedWarpRotation.Roll = 0.f;
	RedWarpRotation.Yaw = 135.f;

	for (int RedIndex = 0; RedIndex < 5; ++RedIndex)
	{
		RedAndWinningTeamSpawnLocations[RedIndex].SetTranslation(RedStartLocations[RedIndex]);
		RedAndWinningTeamSpawnLocations[RedIndex].SetRotation(RedWarpRotation.Quaternion());
	}

	for (int BlueIndex = 0; BlueIndex < 5; ++BlueIndex)
	{
		BlueAndLosingTeamSpawnLocations[BlueIndex].SetTranslation(BlueStartLocations[BlueIndex]);
		BlueAndLosingTeamSpawnLocations[BlueIndex].SetRotation(BlueWarpRotation.Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;
	
	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}

void AUTLineUpZone::DefaultCreateForFFAIntro()
{
	TArray<FVector> FFAStartLocations;

	FFAStartLocations.SetNum(10);

	FFAStartLocations[0] = FVector(-100,275 , 113.f);
	FFAStartLocations[1] = FVector(-175.f,150.f, 113.f);
	FFAStartLocations[2] = FVector(-50.f, 75.f, 113.f);
	FFAStartLocations[3] = FVector(-100.f,-75.f, 113.f);
	FFAStartLocations[4] = FVector(-25.f,-250.f, 113.f);

	FFAStartLocations[5] = FVector(-150.f,-275.f, 113.f);
	FFAStartLocations[6] = FVector(375.f,200.f, 113.f);
	FFAStartLocations[7] = FVector(250.f,0.f, 113.f);
	FFAStartLocations[8] = FVector(375.f,-200.f, 113.f);
	FFAStartLocations[9] = FVector(450.f,-300.f, 113.f);


	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	FFATeamSpawnLocations.SetNum(10);

	FRotator Rotation;
	Rotation.Pitch = 0.f;
	Rotation.Roll = 0.f;
	Rotation.Yaw = -180.f;

	for (int FFAIndex = 0; FFAIndex < 10; ++FFAIndex)
	{
		FFATeamSpawnLocations[FFAIndex].SetTranslation(FFAStartLocations[FFAIndex]);
		FFATeamSpawnLocations[FFAIndex].SetRotation(Rotation.Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;

	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}

void AUTLineUpZone::DefaultCreateForTeamIntermission()
{
	TArray<FVector> WinningTeamStartLocations;
	TArray<FRotator> WinningTeamRotations;

	WinningTeamStartLocations.SetNum(5);
	WinningTeamRotations.SetNum(5);

	WinningTeamStartLocations[0] = FVector(0.f, 0.f, 113.f);
	WinningTeamRotations[0].Pitch = 0.f;
	WinningTeamRotations[0].Roll = 0.f;
	WinningTeamRotations[0].Yaw = -179.99f;

	WinningTeamStartLocations[1] = FVector(-50.f, 150.f, 113.f);
	WinningTeamRotations[1].Pitch = 0.f;
	WinningTeamRotations[1].Roll = 0.f;
	WinningTeamRotations[1].Yaw = -168.75f;

	WinningTeamStartLocations[2] = FVector(-50.f, -150.f, 113.f);
	WinningTeamRotations[2].Pitch = 0.f;
	WinningTeamRotations[2].Roll = 0.f;
	WinningTeamRotations[2].Yaw = 168.75f;

	WinningTeamStartLocations[3] = FVector(-150.f, 250.f, 113.f);
	WinningTeamRotations[3].Pitch = 0.f;
	WinningTeamRotations[3].Roll = 0.f;
	WinningTeamRotations[3].Yaw = -157.49f;

	WinningTeamStartLocations[4] = FVector(-150.f, -250.f, 113.f);
	WinningTeamRotations[4].Pitch = 0.f;
	WinningTeamRotations[4].Roll = 0.f;
	WinningTeamRotations[4].Yaw = 157.49f;

	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	RedAndWinningTeamSpawnLocations.SetNum(5);

	for (int WinIndex = 0; WinIndex < 5; ++WinIndex)
	{
		RedAndWinningTeamSpawnLocations[WinIndex].SetTranslation(WinningTeamStartLocations[WinIndex]);
		RedAndWinningTeamSpawnLocations[WinIndex].SetRotation(WinningTeamRotations[WinIndex].Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;

	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}

void AUTLineUpZone::DefaultCreateForFFAIntermission()
{
	TArray<FVector> FFAStartLocations;
	TArray<FRotator> FFARotations;

	FFAStartLocations.SetNum(5);
	FFARotations.SetNum(5);

	FFAStartLocations[0] = FVector(0.f, 0.f, 113.f);
	FFARotations[0].Pitch = 0.f;
	FFARotations[0].Roll = 0.f;
	FFARotations[0].Yaw = -179.99f;

	FFAStartLocations[1] = FVector(-50.f, 150.f, 113.f);
	FFARotations[1].Pitch = 0.f;
	FFARotations[1].Roll = 0.f;
	FFARotations[1].Yaw = -168.75f;

	FFAStartLocations[2] = FVector(-50.f, -150.f, 113.f);
	FFARotations[2].Pitch = 0.f;
	FFARotations[2].Roll = 0.f;
	FFARotations[2].Yaw = 168.75f;

	FFAStartLocations[3] = FVector(-150.f, 250.f, 113.f);
	FFARotations[3].Pitch = 0.f;
	FFARotations[3].Roll = 0.f;
	FFARotations[3].Yaw = -157.49f;

	FFAStartLocations[4] = FVector(-150.f, -250.f, 113.f);
	FFARotations[4].Pitch = 0.f;
	FFARotations[4].Roll = 0.f;
	FFARotations[4].Yaw = 157.49f;

	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	FFATeamSpawnLocations.SetNum(5);

	for (int WinIndex = 0; WinIndex < 5; ++WinIndex)
	{
		FFATeamSpawnLocations[WinIndex].SetTranslation(FFAStartLocations[WinIndex]);
		FFATeamSpawnLocations[WinIndex].SetRotation(FFARotations[WinIndex].Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;

	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}

void AUTLineUpZone::DefaultCreateForTeamEndMatch()
{
	TArray<FVector> WinningStartLocations;
	TArray<FRotator> WinningRotations;

	WinningStartLocations.SetNum(5);
	WinningRotations.SetNum(5);

	WinningStartLocations[0] = FVector(-150.f, 0.f, 113.f);
	WinningRotations[0].Pitch = 0.f;
	WinningRotations[0].Roll = 0.f;
	WinningRotations[0].Yaw = -179.99f;

	WinningStartLocations[1] = FVector(-50.f, 150.f, 113.f);
	WinningRotations[1].Pitch = 0.f;
	WinningRotations[1].Roll = 0.f;
	WinningRotations[1].Yaw = 179.99f;

	WinningStartLocations[2] = FVector(-50.f, -150.f, 113.f);
	WinningRotations[2].Pitch = 0.f;
	WinningRotations[2].Roll = 0.f;
	WinningRotations[2].Yaw = -179.99f;

	WinningStartLocations[3] = FVector(50.f, 300.f, 113.f);
	WinningRotations[3].Pitch = 0.f;
	WinningRotations[3].Roll = 0.f;
	WinningRotations[3].Yaw = -179.99f;

	WinningStartLocations[4] = FVector(50.f, -300.f, 113.f);
	WinningRotations[4].Pitch = 0.f;
	WinningRotations[4].Roll = 0.f;
	WinningRotations[4].Yaw = -179.99f;

	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	RedAndWinningTeamSpawnLocations.SetNum(5);

	for (int WinIndex = 0; WinIndex < 5; ++WinIndex)
	{
		RedAndWinningTeamSpawnLocations[WinIndex].SetTranslation(WinningStartLocations[WinIndex]);
		RedAndWinningTeamSpawnLocations[WinIndex].SetRotation(WinningRotations[WinIndex].Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;

	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}

void AUTLineUpZone::DefaultCreateForFFAEndMatch()
{
	TArray<FVector> FFAStartLocations;
	TArray<FRotator> FFARotations;

	FFAStartLocations.SetNum(5);
	FFARotations.SetNum(5);

	FFAStartLocations[0] = FVector(-150.f, 0.f, 113.f);
	FFARotations[0].Pitch = 0.f;
	FFARotations[0].Roll = 0.f;
	FFARotations[0].Yaw = -179.99f;

	FFAStartLocations[1] = FVector(-50.f, 150.f, 113.f);
	FFARotations[1].Pitch = 0.f;
	FFARotations[1].Roll = 0.f;
	FFARotations[1].Yaw = 179.99f;

	FFAStartLocations[2] = FVector(-50.f, -150.f, 113.f);
	FFARotations[2].Pitch = 0.f;
	FFARotations[2].Roll = 0.f;
	FFARotations[2].Yaw = -179.99f;

	FFAStartLocations[3] = FVector(50.f, 300.f, 113.f);
	FFARotations[3].Pitch = 0.f;
	FFARotations[3].Roll = 0.f;
	FFARotations[3].Yaw = -179.99f;

	FFAStartLocations[4] = FVector(50.f, -300.f, 113.f);
	FFARotations[4].Pitch = 0.f;
	FFARotations[4].Roll = 0.f;
	FFARotations[4].Yaw = -179.99f;

	RedAndWinningTeamSpawnLocations.Empty();
	BlueAndLosingTeamSpawnLocations.Empty();
	FFATeamSpawnLocations.Empty();

	FFATeamSpawnLocations.SetNum(5);

	for (int WinIndex = 0; WinIndex < 5; ++WinIndex)
	{
		FFATeamSpawnLocations[WinIndex].SetTranslation(FFAStartLocations[WinIndex]);
		FFATeamSpawnLocations[WinIndex].SetRotation(FFARotations[WinIndex].Quaternion());
	}

	SnapToFloor();
	DeleteAllMeshVisualizations();
	InitializeMeshVisualizations();

	if (!Camera)
	{
		FTransform CameraTransform;
		CameraTransform.SetTranslation(FVector(-200.f, 0.f, 30.f));

		FActorSpawnParameters Params;
		Params.Owner = this;

		ACameraActor* SpawnedActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraTransform, Params);
		if (SpawnedActor)
		{
			Camera = SpawnedActor;
			Camera->GetCameraComponent()->SetFieldOfView(60.f);
			SpawnedActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	FRotator DefaultCameraRotation;
	DefaultCameraRotation.Roll = 0.f;
	DefaultCameraRotation.Pitch = 0.f;
	DefaultCameraRotation.Yaw = 0.f;

	FTransform DefaultCameraTransform;
	DefaultCameraTransform.SetTranslation(FVector(-750.f, 0.f, 47.5f));
	DefaultCameraTransform.SetRotation(DefaultCameraRotation.Quaternion());

	Camera->SetActorRelativeTransform(DefaultCameraTransform);
}
#endif //WITH_EDITORONLY_DATA