// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWallDodgePoint.h"
#include "UTRecastNavMesh.h"
#include "UTPathNode.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "UTReachSpec_WallDodge.h"

AUTWallDodgePoint::AUTWallDodgePoint(const FObjectInitializer& OI)
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

	CheckRange = 1500.0f;
}

void AUTWallDodgePoint::AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
{
	FMessageLog MapCheckLog(TEXT("MapCheck"));

	AUTCharacter* UTScout = (NavData->ScoutClass != NULL) ? Cast<AUTCharacter>(NavData->ScoutClass.GetDefaultObject()) : NULL;
	if (UTScout == NULL)
	{
		MapCheckLog.Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(NSLOCTEXT("WallDodgePoint", "NoUTScout", "Cannot use WallDodgePoint because navigation scout in UTRecastNavMesh isn't a UTCharacter")));
	}
	else
	{
		// find wall
		FHitResult WallHit;
		if (!GetWorld()->LineTraceSingleByChannel(WallHit, GetActorLocation(), GetActorLocation() + GetActorRotation().Vector() * 256.0f, ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
		{
			MapCheckLog.Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(NSLOCTEXT("WallDodgePoint", "NoWall", "Failed to find wall for WallDodgePoint")));
		}
		else
		{
			FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(UTScout->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), UTScout->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());

			// note that the jump sim doesn't slide along walls so we need to be generous here and adjust at the end
			const FVector WallPoint = WallHit.Location + WallHit.Normal * (ScoutShape.GetCapsuleRadius() + 0.1f);

			const float GravityZ = GetLocationGravityZ(GetWorld(), WallPoint, ScoutShape);

			TArray<NavNodeRef> PolyList;
			{
				const TArray<const UUTPathNode*>& NodeList = NavData->GetAllNodes();
				for (const UUTPathNode* SrcNode : NodeList)
				{
					for (NavNodeRef SrcPoly : SrcNode->Polys)
					{
						const FVector PolyCenter = NavData->GetPolyCenter(SrcPoly);
						if ((PolyCenter - WallPoint).Size() < CheckRange && NavData->IsValidJumpPoint(PolyCenter))
						{
							PolyList.Add(SrcPoly);
						}
					}
				}
			}

			// find all polys that a Pawn can jump from and reach the wall dodge point
			struct FDodgeStartPoint
			{
				NavNodeRef Poly;
				FVector JumpStart;
				float DodgeTimeZSpeed;

				FDodgeStartPoint(NavNodeRef InPoly, const FVector& InJumpStart, float InZSpeed)
					: Poly(InPoly), JumpStart(InJumpStart), DodgeTimeZSpeed(InZSpeed)
				{}
			};
			TArray<FDodgeStartPoint> StartPoints;

			for (NavNodeRef SrcPoly : PolyList)
			{
				const FVector PolyCenter = NavData->GetPolyCenter(SrcPoly);
				// test wall centers and corners
				TArray<FVector> TestPoints;
				TArray<FLine> Walls = NavData->GetPolyWalls(SrcPoly);
				for (const FLine& Wall : Walls)
				{
					TestPoints.Add(Wall.GetCenter());
				}
				for (int32 i = 0; i < Walls.Num(); i++)
				{
					for (int32 j = i + 1; j < Walls.Num(); j++)
					{
						FVector Corner(FVector::ZeroVector);
						if ((Walls[i].A - Walls[j].B).IsNearlyZero())
						{
							Corner = Walls[i].A;
						}
						else if ((Walls[i].B - Walls[j].A).IsNearlyZero())
						{
							Corner = Walls[i].B;
						}
						if (!Corner.IsZero())
						{
							TestPoints.Add(Corner);
						}
					}
				}
				// sort by distance
				TestPoints.Sort([&](const FVector& A, const FVector& B){ return (A - WallPoint).SizeSquared() < (B - WallPoint).SizeSquared(); });
				// jump test each
				for (FVector TestPt : TestPoints)
				{
					TestPt.Z += ScoutShape.GetCapsuleHalfHeight();
					// try to push forward from poly center -> test point to get closer to the edge of the walkable space
					// this is mostly to get around small outcroppings that can block the jump test
					const FVector ForwardTestPt = TestPt + (TestPt - PolyCenter).GetSafeNormal2D() * (ScoutShape.GetCapsuleRadius() * 0.5f);
					FHitResult Hit;
					if (GetWorld()->SweepSingleByChannel(Hit, TestPt, ForwardTestPt, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
					{
						TestPt = Hit.Location;
					}
					else
					{
						TestPt = ForwardTestPt;
					}
					float MaxFallSpeed = 0.0f;
					// OnlyJumpReachable() can fail for some reasons we don't want, such as reaching another poly, but we use it first for its push-to-edge logic
					// if it fails, try just straight jumping from our start pos
					if ( NavData->OnlyJumpReachable(UTScout, TestPt, WallPoint, SrcPoly, INVALID_NAVNODEREF, UTScout->GetCharacterMovement()->JumpZVelocity, NULL, &MaxFallSpeed) ||
						NavData->JumpTraceTest(TestPt, WallPoint, SrcPoly, INVALID_NAVNODEREF, ScoutShape, UTScout->UTCharacterMovement->MaxWalkSpeed, GravityZ, UTScout->GetCharacterMovement()->JumpZVelocity, UTScout->GetCharacterMovement()->JumpZVelocity, NULL, &MaxFallSpeed) )
					{
						if (MaxFallSpeed == 0.0f)
						{
							// this happens if the walk part of OnlyJumpReachable() makes it to the target; i.e. the wall to dodge off of is the wall of a walkable surface
							MaxFallSpeed = UTScout->GetCharacterMovement()->JumpZVelocity * 0.9f;
						}
						new(StartPoints)FDodgeStartPoint(SrcPoly, TestPt, MaxFallSpeed);
						break;
					}
				}
			}
			if (StartPoints.Num() == 0)
			{
				MapCheckLog.Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(NSLOCTEXT("WallDodgePoint", "NoStartPoints", "Failed to find any nearby spots that can jump to this point")));
			}
			else
			{
				// sort source points by falling speed, worst first
				StartPoints.Sort([&](const FDodgeStartPoint& A, const FDodgeStartPoint& B) { return A.DodgeTimeZSpeed < B.DodgeTimeZSpeed; });
				// now look for potential destinations
				const float DodgeSpeed = UTScout->UTCharacterMovement->WallDodgeImpulseHorizontal;
				const float DodgeZ = UTScout->UTCharacterMovement->WallDodgeImpulseVertical;
				for (NavNodeRef DestPoly : PolyList)
				{
					const FVector PolyCenter = NavData->GetPolyCenter(DestPoly);

					for (int32 i = 0; i < StartPoints.Num(); i++)
					{
						const FDodgeStartPoint& DodgePt = StartPoints[i];
						const FVector EndPt = PolyCenter + FVector(0.0f, 0.0f, ScoutShape.GetCapsuleHalfHeight());
						if (NavData->JumpTraceTest(WallPoint, EndPt, INVALID_NAVNODEREF, DestPoly, ScoutShape, DodgeSpeed, GravityZ, DodgePt.DodgeTimeZSpeed + DodgeZ, DodgePt.DodgeTimeZSpeed + DodgeZ) &&
							// make sure can't make via normal jump
							!NavData->JumpTraceTest(WallPoint, EndPt, INVALID_NAVNODEREF, DestPoly, ScoutShape, UTScout->UTCharacterMovement->MaxWalkSpeed, GravityZ, FMath::Max<float>(0.0f, DodgePt.DodgeTimeZSpeed), FMath::Max<float>(0.0f, DodgePt.DodgeTimeZSpeed)))
						{
							// create ReachSpecs for this point and all easier jumps
							for (int32 j = i; j < StartPoints.Num(); j++)
							{
								UUTPathNode* StartNode = NavData->GetNodeFromPoly(StartPoints[j].Poly);
								UUTPathNode* EndNode = NavData->GetNodeFromPoly(DestPoly);
								checkSlow(StartNode != NULL && EndNode != NULL);
								if (StartNode != EndNode)
								{
									bool bFoundExisting = false;
									for (FUTPathLink& Link : StartNode->Paths)
									{
										if (Link.End == EndNode && Link.StartEdgePoly == StartPoints[j].Poly && Cast<UUTReachSpec_WallDodge>(Link.Spec.Get()) != NULL)
										{
											// add as additional dest
											Link.AdditionalEndPolys.Add(DestPoly);
											bFoundExisting = true;
											break;
										}
									}
									if (!bFoundExisting)
									{
										// add new path link
										UUTReachSpec_WallDodge* DodgeSpec = NewObject<UUTReachSpec_WallDodge>(StartNode);
										DodgeSpec->WallPoint = WallHit.Location;
										DodgeSpec->WallNormal = WallHit.Normal;
										DodgeSpec->JumpStart = StartPoints[j].JumpStart;
										FUTPathLink* NewLink = new(StartNode->Paths) FUTPathLink(StartNode, StartPoints[j].Poly, EndNode, DestPoly, DodgeSpec, FMath::TruncToInt(ScoutShape.GetCapsuleRadius()), FMath::TruncToInt(ScoutShape.GetCapsuleHalfHeight()), R_JUMP);
										for (NavNodeRef StartPoly : StartNode->Polys)
										{
											NewLink->Distances.Add(NavData->CalcPolyDistance(StartPoly, StartPoints[j].Poly) + FMath::TruncToInt((WallPoint - StartPoints[j].JumpStart).Size() + (EndPt - WallPoint).Size()));
										}
									}
								}
							}
							break;
						}
					}
				}
			}
		}
	}
}