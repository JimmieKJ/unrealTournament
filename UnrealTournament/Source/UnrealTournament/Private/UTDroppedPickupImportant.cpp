// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDroppedPickupImportant.h"
#include "UTRecastNavMesh.h"

AUTDroppedPickupImportant::AUTDroppedPickupImportant(const FObjectInitializer& OI)
	: Super(OI)
{
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InitialLifeSpan = 0.0f;
}

void AUTDroppedPickupImportant::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld()->OverlapAnyTestByChannel(GetActorLocation(), GetActorQuat(), RootComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(10.0f)))
	{
		MoveToSafeLocation(FBox::BuildAABB(GetActorLocation(), FVector(1.0f)));
	}
}

void AUTDroppedPickupImportant::MoveToSafeLocation(const FBox& AvoidZone)
{
	AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
	const FVector MyLoc = GetActorLocation();
	bool bMoved = false;
	if (NavData != NULL)
	{
		NavNodeRef Poly = NavData->UTFindNearestPoly(MyLoc, NavData->GetPOIExtent(this));
		if (Poly == INVALID_NAVNODEREF)
		{
			TArray<FNavPoly> NearbyPolys;
			NavData->GetPolysInBox(FBox::BuildAABB(MyLoc, FVector(10000.0f, 10000.0f, 10000.0f)), NearbyPolys);
			// first pass, try to find nearby visible poly
			// second pass, take closest regardless of walls
			for (int32 i = 0; i < 2 && Poly == INVALID_NAVNODEREF; i++)
			{
				NavNodeRef Best = INVALID_NAVNODEREF;
				float BestDist = FLT_MAX;
				for (FNavPoly TestPoly : NearbyPolys)
				{
					if (!AvoidZone.IsInside(TestPoly.Center))
					{
						const float TestDist = (TestPoly.Center - MyLoc).Size();
						if (TestDist < BestDist && (i > 0 || !GetWorld()->LineTraceTestByChannel(MyLoc + FVector(0.0f, 0.0f, 100.0f), TestPoly.Center, ECC_Visibility, FCollisionQueryParams::DefaultQueryParam, WorldResponseParams)))
						{
							Best = TestPoly.Ref;
							BestDist = TestDist;
						}
					}
				}
			}
		}
		if (Poly != INVALID_NAVNODEREF)
		{
			bMoved = TeleportTo(NavData->GetPolySurfaceCenter(Poly) + FVector(0.0f, 0.0f, Collision->GetUnscaledCapsuleHalfHeight() * 2.0f), GetActorRotation());
		}
	}
	if (!bMoved)
	{
		// try to teleport back to pickup base
		for (TActorIterator<AUTPickupInventory> It(GetWorld()); It; ++It)
		{
			if (It->GetInventoryType() == InventoryType && TeleportTo(It->GetActorLocation(), GetActorRotation()))
			{
				bMoved = true;
				break;
			}
		}
	}
	if (bMoved && Movement != NULL)
	{
		Movement->SetUpdatedComponent(Collision);
	}
}

void AUTDroppedPickupImportant::FellOutOfWorld(const class UDamageType& dmgType)
{
	MoveToSafeLocation(FBox::BuildAABB(GetActorLocation(), FVector(1.0f)));
}