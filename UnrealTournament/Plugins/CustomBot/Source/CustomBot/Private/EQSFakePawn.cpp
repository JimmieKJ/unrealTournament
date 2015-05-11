// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"
#include "EQSFakeBot.h"
#include "EQSFakePawn.h"
#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

AEQSFakePawn::AEQSFakePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AEQSFakeBot::StaticClass();
}

void AEQSFakePawn::PostLoad()
{
	Super::PostLoad();
	
#if WITH_EDITOR
	// a little hack to update other props once every thing's done loading
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(TimerHandle, this, &AEQSFakePawn::UpdateEnemy, 1, /*inbLoop=*/false);
#endif // WITH_EDITOR
}

void AEQSFakePawn::UpdateEnemy()
{
	if (GetController() == nullptr)
	{
		SpawnDefaultController();
	}
	check(GetController());

	AUTBot* Bot = Cast<AUTBot>(GetController());
	if (Bot)
	{
		Bot->SetEnemy(Enemy);
	}
}

#if WITH_EDITOR
void AEQSFakePawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_Enemy = GET_MEMBER_NAME_CHECKED(AEQSFakePawn, Enemy);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();
		if (PropName == NAME_Enemy)
		{
			UpdateEnemy();
		}
	}
}
#endif // WITH_EDITOR
