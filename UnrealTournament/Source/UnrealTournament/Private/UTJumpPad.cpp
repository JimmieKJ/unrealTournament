// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTJumpPad.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"
#include "UTReachSpec_JumpPad.h"
#include "UTGameEngine.h"

AUTJumpPad::AUTJumpPad(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneRoot;
	RootComponent->bShouldUpdatePhysicsVolume = true;

	// Setup the mesh
	Mesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("JumpPadMesh"));
	Mesh->AttachParent = RootComponent;

	TriggerBox = ObjectInitializer.CreateDefaultSubobject<UBoxComponent>(this, TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->AttachParent = RootComponent;
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTJumpPad::TriggerBeginOverlap);

	JumpSound = nullptr;
	JumpTarget = FVector(100.0f, 0.0f, 0.0f);
	JumpTime = 1.0f;
	bMaintainVelocity = false;

	AreaClass = UNavArea_Default::StaticClass();
#if WITH_EDITORONLY_DATA
	JumpPadComp = ObjectInitializer.CreateDefaultSubobject<UUTJumpPadRenderingComponent>(this, TEXT("JumpPadComp"));
	JumpPadComp->PostPhysicsComponentTick.bCanEverTick = false;
	JumpPadComp->AttachParent = RootComponent;
#endif // WITH_EDITORONLY_DATA
}

void AUTJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Launch the pending actors
	if (PendingJumpActors.Num() > 0)
	{
		for (AActor* Actor : PendingJumpActors)
		{
			Launch(Actor);
		}
		PendingJumpActors.Reset();
	}
}

bool AUTJumpPad::CanLaunch_Implementation(AActor* TestActor)
{
	return (Cast<ACharacter>(TestActor) != NULL && TestActor->Role >= ROLE_AutonomousProxy);
}

void AUTJumpPad::Launch_Implementation(AActor* Actor)
{
	// For now just filter for ACharacter. Maybe certain projectiles/vehicles/ragdolls/etc should bounce in the future
	ACharacter* Char = Cast<ACharacter>(Actor);
	if (Char != NULL)
	{
		//Launch the character to the target
		Char->LaunchCharacter(CalculateJumpVelocity(Char), !bMaintainVelocity, true);

		if (RestrictedJumpTime > 0.0f)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(Char);
			if (UTChar != NULL)
			{
				UTChar->UTCharacterMovement->RestrictJump(RestrictedJumpTime);
			}
		}

		// Play Jump sound if we have one
		UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, Char, SRT_AllButOwner, false);

		// if it's a bot, refocus it to its desired endpoint for any air control adjustments
		AUTBot* B = Cast<AUTBot>(Char->Controller);
		AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
		if (B != NULL && NavData != NULL)
		{
			bool bRepathOnLand = false;
			bool bExpectedJumpPad = B->GetMoveTarget().Actor == this || (B->GetMoveTarget().Node != NULL && B->GetMoveTarget().Node->POIs.Contains(this));
			if (!bExpectedJumpPad)
			{
				UUTReachSpec_JumpPad* JumpPadPath = Cast<UUTReachSpec_JumpPad>(B->GetCurrentPath().Spec.Get());
				bExpectedJumpPad = ((JumpPadPath != NULL && JumpPadPath->JumpPad == this) || B->GetMoveTarget().TargetPoly == GetUTNavData(GetWorld())->FindNearestPoly(GetActorLocation(), GetSimpleCollisionCylinderExtent()));
			}
			if (bExpectedJumpPad)
			{
				bRepathOnLand = true;
				for (int32 i = 0; i < B->RouteCache.Num() - 1; i++)
				{
					if (B->RouteCache[i].Actor == this)
					{
						TArray<FComponentBasedPosition> MovePoints;
						new(MovePoints) FComponentBasedPosition(ActorToWorld().TransformPosition(JumpTarget));
						B->SetMoveTarget(B->RouteCache[i + 1], MovePoints);
						B->MoveTimer = FMath::Max<float>(B->MoveTimer, JumpTime);
						bRepathOnLand = false;
						break;
					}
				}
			}
			else if (!B->GetMoveTarget().IsValid() || B->MoveTimer <= 0.0f)
			{
				// bot got knocked onto jump pad, is stuck on it, or otherwise is surprised to be here
				bRepathOnLand = true;
			}
			if (bRepathOnLand)
			{
				// make sure bot aborts move when it lands
				B->MoveTimer = FMath::Min<float>(B->MoveTimer, JumpTime - 0.1f);
				// if bot might be stuck or the jump pad just goes straight up (such that it will never land without air control), we need to force something to happen
				if (B->MoveTimer <= 0.0f || JumpTarget.Size2D() < 1.0f)
				{
					UUTPathNode* MyNode = NavData->FindNearestNode(GetActorLocation(), NavData->GetPOIExtent(this));
					if (MyNode != NULL)
					{
						for (const FUTPathLink& Path : MyNode->Paths)
						{
							UUTReachSpec_JumpPad* JumpPadPath = Cast<UUTReachSpec_JumpPad>(Path.Spec.Get());
							if (JumpPadPath != NULL && JumpPadPath->JumpPad == this)
							{
								B->SetMoveTargetDirect(FRouteCacheItem(Path.End.Get(), NavData->GetPolySurfaceCenter(Path.EndPoly), Path.EndPoly));
								break;
							}
						}
					}
				}
			}
		}
	}
}

void AUTJumpPad::TriggerBeginOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Add the actor to be launched if it hasn't already
	if (!PendingJumpActors.Contains(OtherActor) && CanLaunch(OtherActor))
	{
		PendingJumpActors.Add(OtherActor);
	}
}

FVector AUTJumpPad::CalculateJumpVelocity(AActor* JumpActor)
{
	FVector Target = ActorToWorld().TransformPosition(JumpTarget) - JumpActor->GetActorLocation();

	float SizeZ = Target.Z / JumpTime + 0.5f * -GetWorld()->GetGravityZ() * JumpTime;
	float SizeXY = Target.Size2D() / JumpTime;

	FVector Velocity = Target.GetSafeNormal2D() * SizeXY + FVector(0.0f, 0.0f, SizeZ);

	// Scale the velocity if Character has gravity scaled
	ACharacter* Char = Cast<ACharacter>(JumpActor);
	if (Char != NULL && Char->GetCharacterMovement() != NULL && Char->GetCharacterMovement()->GravityScale != 1.0f)
	{
		Velocity *= FMath::Sqrt(Char->GetCharacterMovement()->GravityScale);
	}
	return Velocity;
}

void AUTJumpPad::AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
{
	FVector MyLoc = GetActorLocation();
	NavNodeRef MyPoly = NavData->FindNearestPoly(MyLoc, GetSimpleCollisionCylinderExtent());
	if (MyPoly != INVALID_NAVNODEREF)
	{
		const FVector JumpTargetWorld = ActorToWorld().TransformPosition(JumpTarget);
		const float GravityZ = GetLocationGravityZ(GetWorld(), GetActorLocation(), TriggerBox->GetCollisionShape());
		const FCapsuleSize HumanSize = NavData->GetHumanPathSize();
		FVector HumanExtent = FVector(HumanSize.Radius, HumanSize.Radius, HumanSize.Height);
		{
			NavNodeRef TargetPoly = NavData->FindNearestPoly(JumpTargetWorld, HumanExtent);
			if (TargetPoly == INVALID_NAVNODEREF)
			{
				// jump target may be in air
				// extrapolate downward a bit to try to find poly
				FVector JumpVel = CalculateJumpVelocity(this);
				if (JumpVel.Size2D() > 1.0f)
				{
					JumpVel.Z += GravityZ * JumpTime;
					FVector CurrentLoc = JumpTargetWorld;
					const float TimeSlice = 0.1f;
					for (int32 i = 0; i < 5; i++)
					{
						FVector NextLoc = CurrentLoc + JumpVel * TimeSlice;
						JumpVel.Z += GravityZ * TimeSlice;
						FHitResult Hit;
						if (GetWorld()->SweepSingleByChannel(Hit, CurrentLoc, NextLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(HumanExtent), FCollisionQueryParams(), WorldResponseParams))
						{
							TargetPoly = NavData->FindNearestPoly(Hit.Location, HumanExtent);
							break; // hit ground so have to stop here
						}
						else
						{
							TargetPoly = NavData->FindNearestPoly(NextLoc, HumanExtent);
							if (TargetPoly != INVALID_NAVNODEREF)
							{
								break;
							}
						}
						CurrentLoc = NextLoc;
					}
				}
			}
			UUTPathNode* TargetNode = NavData->GetNodeFromPoly(TargetPoly);
			if (TargetPoly != INVALID_NAVNODEREF && TargetNode != NULL)
			{
				UUTReachSpec_JumpPad* JumpSpec = NewObject<UUTReachSpec_JumpPad>(MyNode);
				JumpSpec->JumpPad = this;
				FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, TargetNode, TargetPoly, JumpSpec, HumanSize.Radius, HumanSize.Height, R_JUMP);
				for (NavNodeRef SrcPoly : MyNode->Polys)
				{
					NewLink->Distances.Add(NavData->CalcPolyDistance(SrcPoly, MyPoly) + FMath::TruncToInt(1000.0f * JumpTime));
				}
			}
		}

		// if we support air control, look for additional jump targets that could be reached by air controlling against the jump pad's standard velocity
		if (RestrictedJumpTime < JumpTime && NavData->ScoutClass != NULL && NavData->ScoutClass.GetDefaultObject()->GetCharacterMovement()->AirControl > 0.0f)
		{
			// intentionally place start loc high up to avoid clipping the edges of any irrelevant geometry
			MyLoc.Z += NavData->ScoutClass.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 3.0f;

			const float AirControlPct = NavData->ScoutClass.GetDefaultObject()->GetCharacterMovement()->AirControl;
			const float AirAccelRate = AirControlPct * NavData->ScoutClass.GetDefaultObject()->GetCharacterMovement()->MaxAcceleration;
			const FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(NavData->ScoutClass.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), NavData->ScoutClass.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			const FVector JumpVel = CalculateJumpVelocity(this);
			const float XYSpeed = FMath::Max<float>(JumpVel.Size2D(), NavData->ScoutClass.GetDefaultObject()->GetCharacterMovement()->MaxWalkSpeed); // TODO: clamp contribution of MaxWalkSpeed based on size of jump / time available to apply air control?
			const float JumpZ = JumpVel.Z;
			// if the jump pad primarily executes a "super-jump" (velocity almost entirely on Z axis, allows full air control) then a normal character jump test will give us good results
			const bool bIsZOnlyJump = JumpTarget.Size2D() < 0.1f * JumpTarget.Size();
			const float JumpTargetDist = JumpTarget.Size();
			const FVector JumpDir2D = (JumpTargetWorld - MyLoc).GetSafeNormal2D();
			const FVector HeightAdjust(0.0f, 0.0f, NavData->AgentHeight * 0.5f);
			const TArray<const UUTPathNode*>& NodeList = NavData->GetAllNodes();
			for (const UUTPathNode* TargetNode : NodeList)
			{
				if (TargetNode != MyNode)
				{
					for (NavNodeRef TargetPoly : TargetNode->Polys)
					{
						FVector TargetLoc = NavData->GetPolyCenter(TargetPoly) + HeightAdjust;
						const float Dist = (TargetLoc - MyLoc).Size();
						bool bPossible = bIsZOnlyJump;
						if (!bPossible)
						{
							// time we have to air control
							float AirControlTime = JumpTime * (Dist / JumpTargetDist) - RestrictedJumpTime;
							// extra distance acquirable via air control
							float AirControlDist2D = FMath::Min<float>(XYSpeed * AirControlTime, 0.5f * AirAccelRate * FMath::Square<float>(AirControlTime));
							// apply air control dist towards target, but remove any in jump direction as the jump pad generally exceeds the character's normal max speed (so air control in that dir would have no effect)
							FVector AirControlAdjustment = (TargetLoc - JumpTargetWorld).GetSafeNormal2D() * AirControlDist2D;
							AirControlAdjustment -= JumpDir2D * (JumpDir2D | AirControlAdjustment);
							bPossible = (TargetLoc - JumpTargetWorld).Size() < AirControlAdjustment.Size();
						}
						if (bPossible && NavData->JumpTraceTest(MyLoc, TargetLoc, MyPoly, TargetPoly, ScoutShape, XYSpeed, GravityZ, JumpZ, JumpZ, NULL, NULL))
						{
							// TODO: account for MaxFallSpeed
							bool bFound = false;
							for (FUTPathLink& ExistingLink : MyNode->Paths)
							{
								if (ExistingLink.End == TargetNode && ExistingLink.StartEdgePoly == TargetPoly)
								{
									ExistingLink.AdditionalEndPolys.Add(TargetPoly);
									bFound = true;
									break;
								}
							}

							if (!bFound)
							{
								UUTReachSpec_JumpPad* JumpSpec = NewObject<UUTReachSpec_JumpPad>(MyNode);
								JumpSpec->JumpPad = this;
								FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, TargetNode, TargetPoly, JumpSpec, HumanSize.Radius, HumanSize.Height, R_JUMP);
								for (NavNodeRef SrcPoly : MyNode->Polys)
								{
									NewLink->Distances.Add(NavData->CalcPolyDistance(SrcPoly, MyPoly) + FMath::TruncToInt(1000.0f * JumpTime)); // TODO: maybe Z adjust cost if this target is higher/lower and the jump will end slightly faster/slower?
								}
							}
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void AUTJumpPad::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
void AUTJumpPad::CheckForErrors()
{
	Super::CheckForErrors();

	FVector JumpVelocity = CalculateJumpVelocity(this);
	// figure out default game mode from which we will derive the default character
	TSubclassOf<AGameMode> GameClass = GetWorld()->GetWorldSettings()->DefaultGameMode;
	if (GameClass == NULL)
	{
		// horrible config hack around unexported function UGameMapsSettings::GetGlobalDefaultGameMode()
		FString GameClassPath;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), GameClassPath, GEngineIni);
		GameClass = LoadClass<AGameMode>(NULL, *GameClassPath, NULL, 0, NULL);
	}
	const ACharacter* DefaultChar = GetDefault<AUTCharacter>();
	
	TSubclassOf<AUTGameMode> UTGameClass = *GameClass;
	if (UTGameClass)
	{
		DefaultChar = Cast<ACharacter>(Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *UTGameClass.GetDefaultObject()->PlayerPawnObject.ToStringReference().AssetLongPathname, NULL, LOAD_NoWarn)));
	}
	else
	{
		DefaultChar = Cast<ACharacter>(GameClass.GetDefaultObject()->DefaultPawnClass.GetDefaultObject());
	}

	if (DefaultChar != NULL && DefaultChar->GetCharacterMovement() != NULL)
	{
		JumpVelocity *= FMath::Sqrt(DefaultChar->GetCharacterMovement()->GravityScale);
	}
	// check if velocity is faster than physics will allow
	APhysicsVolume* PhysVolume = (RootComponent != NULL) ? RootComponent->GetPhysicsVolume() : GetWorld()->GetDefaultPhysicsVolume();
	if (JumpVelocity.Size() > PhysVolume->TerminalVelocity)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		Arguments.Add(TEXT("Speed"), FText::AsNumber(JumpVelocity.Size()));
		Arguments.Add(TEXT("TerminalVelocity"), FText::AsNumber(PhysVolume->TerminalVelocity));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("UTJumpPad", "TerminalVelocityWarning", "{ActorName} : Jump pad speed on default character would be {Speed} but terminal velocity is {TerminalVelocity}!"), Arguments)))
			->AddToken(FMapErrorToken::Create(FName(TEXT("JumpPadTerminalVelocity"))));
	}
}
#endif // WITH_EDITOR
