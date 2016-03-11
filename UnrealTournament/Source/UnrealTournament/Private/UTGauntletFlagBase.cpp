// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlagBase.h"
#include "Net/UnrealNetwork.h"

AUTGauntletFlagBase::AUTGauntletFlagBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GMesh(TEXT("SkeletalMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_Gauntlet_Flag_Mesh.S_Gauntlet_Flag_Mesh'"));

	GhostMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("GhostMesh"));
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetSkeletalMesh(GMesh.Object);
	GhostMesh->AttachParent = RootComponent;
	bGhostMeshVisibile = false;
}

void AUTGauntletFlagBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	OnRep_bGhostMeshVisibile();	
}

void AUTGauntletFlagBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTGauntletFlagBase, bGhostMeshVisibile);		
}

void AUTGauntletFlagBase::OnRep_bGhostMeshVisibile()
{
	GhostMesh->SetVisibility(bGhostMeshVisibile);

	APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
	if (GetNetMode() != NM_DedicatedServer && PC != NULL && PC->MyHUD != NULL)
	{
		if (bGhostMeshVisibile)
		{
			PC->MyHUD->AddPostRenderedActor(this);
		}
		else
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}
}


void AUTGauntletFlagBase::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	if (PC->GetPawn())
	{
		float Scale = Canvas->ClipY / 1080.0f;
		FVector2D Size = FVector2D(14,47) * Scale;

		AUTHUD* HUD = Cast<AUTHUD>(PC->MyHUD);
		FVector FlagLoc = GetActorLocation() + FVector(0.0f,0.0f,128.0f);
		FVector ScreenPosition = Canvas->Project(FlagLoc);

		FVector LookVec;
		FRotator LookDir;
		PC->GetPawn()->GetActorEyesViewPoint(LookVec,LookDir);

		if (HUD && FVector::DotProduct(LookDir.Vector().GetSafeNormal(), (FlagLoc - LookVec)) > 0.0f && 
			ScreenPosition.X > 0 && ScreenPosition.X < Canvas->ClipX && 
			ScreenPosition.Y > 0 && ScreenPosition.Y < Canvas->ClipY)
		{
			Canvas->SetDrawColor(255,255,255,255);
			Canvas->DrawTile(HUD->HUDAtlas, ScreenPosition.X - (Size.X * 0.5f), ScreenPosition.Y - Size.Y, Size.X, Size.Y,1009,0,14,47);
		}
	}
}

void AUTGauntletFlagBase::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bScoreBase)
	{
		AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
		if (Character != NULL)
		{
			AUTGauntletFlag* Flag = Cast<AUTGauntletFlag>(Character->GetCarriedObject());
			if ( Flag != NULL && Flag->GetTeamNum() == GetTeamNum() )
			{
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS == NULL || (GS->IsMatchInProgress() && !GS->IsMatchIntermission()))
				{
					Flag->Score(FName(TEXT("FlagCapture")), Flag->HoldingPawn, Flag->Holder);
					Flag->PlayCaptureEffect();

					for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
					{
						AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
						if (PC)
						{
							if (GetTeamNum() == PC->GetTeamNum() && FlagScoreRewardSound)
							{
								PC->ClientPlaySound(FlagScoreRewardSound, 2.f);
							}
						}
					}
				}
			}
		}
	}
}

void AUTGauntletFlagBase::CreateCarriedObject()
{
	return;
}

void AUTGauntletFlagBase::Activate()
{
	CreateGauntletObject();
}

void AUTGauntletFlagBase::Deactivate()
{
	bGhostMeshVisibile = false;
	if (Role == ROLE_Authority) OnRep_bGhostMeshVisibile();

	MyFlag = nullptr;
	if (CarriedObject != NULL)
	{
		AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (GameState && GameState->Flag == Cast<AUTGauntletFlag>(CarriedObject))
		{
			GameState->Flag = nullptr;
		}
		CarriedObject->Destroy();
	}
}

void AUTGauntletFlagBase::Reset()
{
	RoundReset();
}

void AUTGauntletFlagBase::RoundReset()
{
	Deactivate();
}


void AUTGauntletFlagBase::CreateGauntletObject()
{
	FActorSpawnParameters Params;
	Params.Owner = this;

	if (bScoreBase)
	{
		bGhostMeshVisibile = true;
		if (Role == ROLE_Authority) OnRep_bGhostMeshVisibile();
	}
	else
	{
		CarriedObject = GetWorld()->SpawnActor<AUTCarriedObject>(CarriedObjectClass, GetActorLocation() + FVector(0, 0, 96), GetActorRotation(), Params);
		AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (GameState && Cast<AUTGauntletFlag>(CarriedObject))
		{
			GameState->Flag = Cast<AUTGauntletFlag>(CarriedObject);
		}
	}

	if (CarriedObject != NULL)
	{
		CarriedObject->Init(this);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("%s Could not create an object of type %s"), *GetNameSafe(this), *GetNameSafe(CarriedObjectClass));
	}

	MyFlag = Cast<AUTCTFFlag>(CarriedObject);
	if (MyFlag && MyFlag->GetMesh())
	{
		MyFlag->GetMesh()->ClothBlendWeight = MyFlag->ClothBlendHome;
	}
}


