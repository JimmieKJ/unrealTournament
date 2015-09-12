// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlagBase.h"
#include "Net/UnrealNetwork.h"

AUTCTFFlagBase::AUTCTFFlagBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlagBaseMesh (TEXT("StaticMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_Pickups_Base_Flag.S_Pickups_Base_Flag'"));

	Mesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("FlagBase"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetStaticMesh(FlagBaseMesh.Object);
	Mesh->AttachParent = RootComponent;
}

void AUTCTFFlagBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTCTFFlagBase, MyFlag);
}

void AUTCTFFlagBase::CreateCarriedObject()
{
	if (TeamFlagTypes.IsValidIndex(TeamNum) && TeamFlagTypes[TeamNum] != NULL)
	{
		CarriedObjectClass = TeamFlagTypes[TeamNum];
	}
	Super::CreateCarriedObject();
	MyFlag = Cast<AUTCTFFlag>(CarriedObject);
	if (MyFlag && MyFlag->GetMesh())
	{
		MyFlag->GetMesh()->ClothBlendWeight = 0.f;
	}
}

FName AUTCTFFlagBase::GetFlagState()
{
	if (MyFlag != NULL)
	{
		return MyFlag->ObjectState;
	}

	return NAME_None;
}

void AUTCTFFlagBase::RecallFlag()
{
	if (MyFlag != NULL && MyFlag->ObjectState != FName(TEXT("Home")) )
	{
		MyFlag->SendHome();
	}
}

void AUTCTFFlagBase::ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome)
{
	Super::ObjectWasPickedUp(NewHolder, bWasHome);

	if (bWasHome)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), FlagTakenSound, this);
	}
}

void AUTCTFFlagBase::ObjectReturnedHome(AUTCharacter* Returner)
{
	Super::ObjectReturnedHome(Returner);

	UUTGameplayStatics::UTPlaySound(GetWorld(), FlagReturnedSound, this);
}
