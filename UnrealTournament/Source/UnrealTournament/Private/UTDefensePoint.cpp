// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTBot.h"
#include "UTGameObjective.h"
#include "UTDefensePoint.h"
#include "UTRecastNavMesh.h"

AUTDefensePoint::AUTDefensePoint(const FObjectInitializer& OI)
	: Super(OI)
{
	Icon = OI.CreateOptionalDefaultSubobject<UBillboardComponent>(this, FName(TEXT("Icon")));
	if (Icon != NULL)
	{
		Icon->bHiddenInGame = true;
		RootComponent = Icon;
	}
#if WITH_EDITORONLY_DATA
	EditorArrow = OI.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("EditorArrow"));
	if (EditorArrow != NULL)
	{
		EditorArrow->SetupAttachment(RootComponent);
		EditorArrow->ArrowSize = 0.5f;
		EditorArrow->ArrowColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f).ToFColor(false);
	}
#endif

	BasePriority = 3;
}

void AUTDefensePoint::BeginPlay()
{
	Super::BeginPlay();

	if (Objective != NULL)
	{
		Objective->DefensePoints.AddUnique(this);
	}

	AUTRecastNavMesh* NavMesh = GetUTNavData(GetWorld());
	if (NavMesh != NULL)
	{
		MyNode = NavMesh->FindNearestNode(GetActorLocation(), NavMesh->GetPOIExtent(this));
		if (MyNode == NULL)
		{
			// try a bit lower
			MyNode = NavMesh->FindNearestNode(GetActorLocation() - FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight()), NavMesh->GetPOIExtent(this));
		}
	}
}
void AUTDefensePoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (Objective != NULL)
	{
		Objective->DefensePoints.Remove(this);
	}
}

int32 AUTDefensePoint::GetPriorityFor(AUTBot* B) const
{
	int32 Priority = BasePriority * 100;
	if (MyNode != NULL && (B->Personality.MapAwareness > 0.0f || B->Skill + B->Personality.MapAwareness >= 5.0f))
	{
		Priority += 33 * FMath::Clamp<float>((float(MyNode->NearbyKills) * 0.33f / float(MyNode->NearbyDeaths)), 0.0f, 1.0f);
	}
	if (B->Personality.Accuracy - B->Personality.Aggressiveness >= 1.0f || (B->GetUTChar() != NULL && B->GetUTChar()->GetWeapon() != NULL && B->GetUTChar()->GetWeapon()->bSniping))
	{
		Priority += 33;
	}
	return Priority;
}