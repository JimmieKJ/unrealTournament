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
	Collision->OnComponentEndOverlap.AddDynamic(this, &AUTDroppedLife::OnOverlapEnd);
	Collision->SetCapsuleRadius(96);
	Collision->SetCapsuleHalfHeight(64);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkullMesh(TEXT("StaticMesh'/Game/RestrictedAssets/Environments/FacingWorlds/Halloween/SM_Skull_Coin.SM_Skull_Coin'"));
	UStaticMeshComponent* MeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("Skull"));
	MeshComp->SetStaticMesh(SkullMesh.Object);
	Mesh = MeshComp;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->AttachParent = RootComponent;
	StealAmount = 0;
	InitialLifeSpan = 60.0f;
}

void AUTDroppedLife::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTDroppedLife, OwnerPlayerState);
	DOREPLIFETIME(AUTDroppedLife, StealAmount);
	DOREPLIFETIME(AUTDroppedLife, CaptureCount);
	
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

void AUTDroppedLife::Destroyed()
{
	if (OwnerPlayerState != nullptr && OwnerPlayerState->CriticalObject == this) OwnerPlayerState->CriticalObject = nullptr;

	Super::Destroyed();
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}
}

void AUTDroppedLife::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	if (OwnerPlayerState != nullptr && GetWorld()->GetTimeSeconds() - GetLastRenderTime() < 0.5f && StealAmount != 0.0f)
	{
		float XL, YL;
		float Scale = Canvas->ClipX / 1920;

		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HugeFont;
		FString Text = FString::Printf(TEXT("%i"), int32(StealAmount));
		Canvas->TextSize(TinyFont, *Text, XL, YL, Scale, Scale);

		FVector ScreenPosition = Canvas->Project(GetActorLocation() + FVector(0, 0, 32.f));
		float XPos = ScreenPosition.X - (XL * 0.5);
		if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
		{
			FCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + ScreenPosition.Y - YL)), FText::FromString(Text), TinyFont, FLinearColor::White);
			TextItem.Scale = FVector2D(Scale, Scale);
			TextItem.BlendMode = SE_BLEND_Translucent;
			TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
			Canvas->DrawItem(TextItem);
		}
	}
}

void AUTDroppedLife::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role == ROLE_Authority && OwnerPlayerState)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(OtherActor);
		if (UTCharacter && !UTCharacter->IsDead())
		{
			TouchingCharacters.Add(UTCharacter);
		}

		CaptureCount = TouchingCharacters.Num();
	}
}

void AUTDroppedLife::OnOverlapEnd(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Role == ROLE_Authority && OwnerPlayerState)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(OtherActor);
		if (UTCharacter && !UTCharacter->IsDead() && TouchingCharacters.Contains(UTCharacter))
		{
			TouchingCharacters.Remove(UTCharacter);
		}

		CaptureCount  = TouchingCharacters.Num();
	}


}


void AUTDroppedLife::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (OwnerPlayerState != nullptr)
	{
		int32 Multiplier = 0;
		for (int32 i=0; i < TouchingCharacters.Num(); i++)
		{
			if (TouchingCharacters[i] != nullptr)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(TouchingCharacters[i]->PlayerState);
				if (PS != nullptr)
				{
					Multiplier += PS->GetTeamNum() == OwnerPlayerState->GetTeamNum() ? 1 : -1;
				}
			}
		}

		StealAmount += DeltaTime * float(Multiplier);
		if ( FMath::Abs<float>(StealAmount) >= DROPPED_LIFE_CAPTURE_TIME)
		{
			// steal or return the life.

			if (StealAmount > 0)
			{
				// Return the life
				OwnerPlayerState->RemainingLives++;
			}

			Destroy();
		}
	}

	BounceZ += CaptureCount > 0 ? DeltaTime : 0.0f;
	FVector MeshTranslation = FVector(0.0f, 0.0f, 0.0f);
	MeshTranslation.Z = 32 * (FMath::Sin(BounceZ));
	AutoRotate.Yaw += 90 * DeltaTime;
	Mesh->SetRelativeLocationAndRotation(MeshTranslation, AutoRotate);
}
