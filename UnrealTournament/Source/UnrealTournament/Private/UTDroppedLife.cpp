// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UTDroppedLife.h"
#include "UnrealNetwork.h"
#include "UTPickupMessage.h"
#include "UTWorldSettings.h"

AUTDroppedLife::AUTDroppedLife(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Collision->SetCapsuleRadius(96);
	Collision->SetCapsuleHalfHeight(64);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkullMesh(TEXT("StaticMesh'/Game/RestrictedAssets/Environments/FacingWorlds/Halloween/SM_Skull_Coin.SM_Skull_Coin'"));
	UStaticMeshComponent* MeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("Skull"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlueMat(TEXT("Material'/Game/RestrictedAssets/Environments/FacingWorlds/Halloween/M_Skull_Coin_Blue.M_Skull_Coin_Blue'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedMat(TEXT("Material'/Game/RestrictedAssets/Environments/FacingWorlds/Halloween/M_Skull_Coin_Red.M_Skull_Coin_Red'"));

	TeamMaterials.Add(RedMat.Object);
	TeamMaterials.Add(BlueMat.Object);

	MeshComp->SetStaticMesh(SkullMesh.Object);
	Mesh = MeshComp;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->AttachParent = RootComponent;
	Value = 0.0f;
	InitialLifeSpan = 60.0f;
}

void AUTDroppedLife::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTDroppedLife, OwnerPlayerState);
	DOREPLIFETIME(AUTDroppedLife, Value);

}

void AUTDroppedLife::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->AddPostRenderedActor(this);
		}
	}
}

void AUTDroppedLife::Init(AUTPlayerState* inOwnerPlayerState, AUTPlayerState* inKillerPlayerState, float inValue)
{
	OwnerPlayerState = inOwnerPlayerState;
	KillerPlayerState = inKillerPlayerState;

	Value = inValue;
	OnReceivedOwnerPlayerState();
}


void AUTDroppedLife::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role == ROLE_Authority && OwnerPlayerState)
	{
		// Someone touched me.. give them points.

		AUTCharacter* TouchingChar = Cast<AUTCharacter>(OtherActor);
		if (TouchingChar)
		{
			AUTPlayerState* TouchingPlayerState = Cast<AUTPlayerState>(TouchingChar->PlayerState);
			if (TouchingPlayerState)
			{
				float MyPoints = float(int(Value * 0.75f));
				float OtherPoints = float(int(Value - MyPoints));
										  
				TouchingPlayerState->AdjustCurrency(MyPoints);

				AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
				if (GameState)
				{
					for (int32 i=0; i < GameState->PlayerArray.Num();i++)
					{
						AUTPlayerState* OtherPlayerState = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
						if (OtherPlayerState)
						{
							if (TouchingPlayerState != OtherPlayerState && GameState->OnSameTeam(TouchingPlayerState, OtherPlayerState))
							{
								OtherPlayerState->AdjustCurrency(OtherPoints);
							}
						}
					}
				}
			}
		}


		Destroy();
	}
}


void AUTDroppedLife::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	BounceZ += DeltaTime;
	FVector MeshTranslation = FVector(0.0f, 0.0f, 0.0f);
	MeshTranslation.Z = 32 * (FMath::Sin(BounceZ));
	AutoRotate.Yaw += 90 * DeltaTime;
	Mesh->SetRelativeLocationAndRotation(MeshTranslation, AutoRotate);
}


void AUTDroppedLife::OnReceivedOwnerPlayerState()
{
	if (OwnerPlayerState)
	{
		uint8 TeamNum = OwnerPlayerState->GetTeamNum();
		if (TeamMaterials.IsValidIndex(TeamNum))
		{
			Mesh->SetMaterial(0, TeamMaterials[TeamNum]);					
		}
	}
}