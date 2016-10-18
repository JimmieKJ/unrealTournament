// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTGauntletGame.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFRewardMessage.h"
#include "UTLocalPlayer.h"
#include "UnrealNetwork.h"

FName NAME_GauntletTrail = FName(TEXT("NAME_GauntletTrail"));
FName NAME_SecondsPerPip = FName(TEXT("SecondsPerPip"));

const float MAX_REDUNDANT_Z = 96.0;
const float MIN_GPS_POINT_THRESHOLD = 1024.0f * 1024.0f;

AUTGauntletFlag::AUTGauntletFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UClass> GFClass(TEXT("Blueprint'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/GhostFlag.GhostFlag_C'"));
	GhostFlagClass = GFClass.Object;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TimerPS(TEXT("ParticleSystem'/Game/RestrictedAssets/Weapons/Weapon_Base_Effects/Particles/P_Weapon_timer_01b_FlagRun.P_Weapon_timer_01b_FlagRun'"));
	TimerEffect = ObjectInitializer.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("TimerEffect"));
	if (TimerEffect != nullptr && TimerPS.Object != nullptr)
	{
		TimerEffect->SetTemplate(TimerPS.Object);
		TimerEffect->SetHiddenInGame(false);
		TimerEffect->SetupAttachment(RootComponent);
		TimerEffect->LDMaxDrawDistance = 4000.0f;
		TimerEffect->RelativeLocation.Z = 40.0f;
		TimerEffect->Mobility = EComponentMobility::Movable;
		TimerEffect->SetCastShadow(false);
	}

	bSingleGhostFlag = false;
	bTeamPickupSendsHome = false;
	bAnyoneCanPickup = true;
	WeightSpeedPctModifier = 1.0f;

	static ConstructorHelpers::FClassFinder<AUTFlagReturnTrail> TrailFinder(TEXT("/Game/RestrictedAssets/Effects/CTF/Blueprints/BP_FlagSplineCreator.BP_FlagSplineCreator_C"));
	TrailClass = TrailFinder.Class;
}

void AUTGauntletFlag::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGauntletFlag, SwapTimer);
	DOREPLIFETIME(AUTGauntletFlag, bPendingTeamSwitch);
}


void AUTGauntletFlag::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	bGradualAutoReturn = false;
	bDebugGPS = false;

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGauntletFlag::ValidateGPSPath, 2.5f, true);
	}
}

void AUTGauntletFlag::OnRep_Team()
{
	// Change the material to represent the team

	if (Team != nullptr)
	{
		uint8 TeamNum = GetTeamNum();
		UE_LOG(UT,Verbose,TEXT("[UTGauntletFlag::OnRep_Team]  Team = %i (%s)"), TeamNum, *Team->GetName());

		if ( TeamMaterials.IsValidIndex(TeamNum) )
		{
			UE_LOG(UT,Log,TEXT("Setting Material"));
			Mesh->SetMaterial(1, TeamMaterials[TeamNum]);
		}
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("[UTGauntletFlag::OnRep_Team]  Team = 255 (nullptr)"));

		UE_LOG(UT,Log,TEXT("Setting Neutral Material"));
		Mesh->SetMaterial(1, NeutralMaterial);
	}
}

void AUTGauntletFlag::NoLongerHeld(AController* InstigatedBy)
{
	Super::NoLongerHeld(InstigatedBy);

	if (LastHoldingPawn)
	{
		LastHoldingPawn->ResetMaxSpeedPctModifier();
	}

}

void AUTGauntletFlag::SetHolder(AUTCharacter* NewHolder)
{
	FString DebugMsg = TEXT("[UTGauntletFlag::SetHolder] ");

	// Set the team to match the team of the holder.
	if (NewHolder)
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		uint8 HolderTeamNum = NewHolder->GetTeamNum();
		if ( GameState && GameState->Teams.IsValidIndex(HolderTeamNum) )
		{
			uint8 FlagTeamNum = GetTeamNum();
			DebugMsg += FString::Printf(TEXT("Holder = %s Team = %i FlagTeamNum = %i"), (NewHolder->PlayerState ? *NewHolder->PlayerState->PlayerName : TEXT("<nullptr>")), HolderTeamNum, FlagTeamNum );

			// If this was our flag, force it to switch teams.
			if (FlagTeamNum == 255)
			{
				uint8 NewTeamNum = HolderTeamNum;
				SetTeam(GameState->Teams[NewTeamNum]);
	
				DebugMsg += FString::Printf(TEXT(" TeamInfo: %s"), GameState->Teams[NewTeamNum] ? *GameState->Teams[NewTeamNum]->GetName() : TEXT("<nullptr>") );
			}
		}
		else
		{
			DebugMsg += FString::Printf(TEXT("No Game State or Invalid Team (%i vs %i)"), HolderTeamNum, GameState ? GameState->Teams.Num() : -1);
		}

		bPendingTeamSwitch = false;
	}
	else
	{
		DebugMsg += TEXT("No Holder");
	}

	// NOTE: The UTCarriedObject::SetHolder() clears the ghosts and we don't want this to happen since we control this
	// in SetTeam.  So we flag it to ignore the call.  Hacky, I know but hey, #PreAlpha!

	bIgnoreClearGhostCalls = true;
	Super::SetHolder(NewHolder);
	bIgnoreClearGhostCalls = false;

	if (NewHolder) 
	{
		NewHolder->ResetMaxSpeedPctModifier();
	}

	UE_LOG(UT,Verbose, TEXT("%s"),*DebugMsg);

}

void AUTGauntletFlag::MoveToHome()
{
	Super::MoveToHome();
	SetTeam(nullptr);
}

void AUTGauntletFlag::OnObjectStateChanged()
{
	AUTCarriedObject::OnObjectStateChanged();
	GetMesh()->ClothBlendWeight = (ObjectState == CarriedObjectState::Held) ? ClothBlendHeld : ClothBlendHome;

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			if (ObjectState == CarriedObjectState::Dropped)
			{
				AUTGauntletGameState* SCTFGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
				PC->MyHUD->AddPostRenderedActor(this);
			}
			else
			{
				PC->MyHUD->RemovePostRenderedActor(this);
			}
		}
	}
}

void AUTGauntletFlag::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
/*
	if (PC->GetPawn())
	{
		float Scale = Canvas->ClipY / 1080.0f;
		FVector2D Size = FVector2D(44,41) * Scale;

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
			Canvas->DrawTile(HUD->HUDAtlas, ScreenPosition.X - (Size.X * 0.5f), ScreenPosition.Y - Size.Y, Size.X, Size.Y,843,87,44,41);
		}
	}	
*/
}


void AUTGauntletFlag::ChangeState(FName NewCarriedObjectState)
{
	Super::ChangeState(NewCarriedObjectState);
	if (Role == ROLE_Authority)
	{
		if (NewCarriedObjectState == CarriedObjectState::Dropped)
		{
			bPendingTeamSwitch = true;
		}
	}
}


void AUTGauntletFlag::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (GauntletGameState)
	{
		if (Role == ROLE_Authority)
		{
			float DefaultSwapTime = float(GauntletGameState->FlagSwapTime);

			if (ObjectState == CarriedObjectState::Dropped)
			{
				if (bPendingTeamSwitch)
				{
					SwapTimer -= DeltaSeconds;
					if (SwapTimer <= 0)
					{
						TeamReset();
					}
				}
			}
			else if (ObjectState == CarriedObjectState::Held)
			{
				if (SwapTimer < DefaultSwapTime)
				{
					SwapTimer += DeltaSeconds;
				}
			}
			else if (ObjectState == CarriedObjectState::Home)
			{
				SwapTimer = DefaultSwapTime;
			}

			SwapTimer = FMath::Clamp<float>(SwapTimer, 0, DefaultSwapTime);
		}

		if (ObjectState == CarriedObjectState::Home)
		{
			if (TimerEffect != nullptr)
			{
				if (GauntletGameState->RemainingPickupDelay > 0)
				{
					if (TimerEffect->bHiddenInGame)
					{
						TimerEffect->SetHiddenInGame(false);
						TimerEffect->SetFloatParameter(NAME_RespawnTime, 30.0f);
						TimerEffect->SetFloatParameter(NAME_SecondsPerPip, 3.0f);
					}

					float Progress = 1.0f - float(GauntletGameState->RemainingPickupDelay) / 30.0f;
					UE_LOG(UT,Log,TEXT("Progress %f"), Progress);
					TimerEffect->SetFloatParameter(NAME_Progress, Progress);			
				}
				else
				{
					TimerEffect->SetHiddenInGame(true);
				}
			}
		}
		else if (ObjectState == CarriedObjectState::Dropped)
		{
			if (TimerEffect != nullptr)
			{
				if (SwapTimer > 0)
				{
					if (TimerEffect->bHiddenInGame)
					{
						TimerEffect->SetHiddenInGame(false);
						TimerEffect->SetFloatParameter(NAME_RespawnTime, GauntletGameState->FlagSwapTime);
						TimerEffect->SetFloatParameter(NAME_SecondsPerPip, 1.0f);

					}
					float Progress = 1.0f - float(SwapTimer) / float(GauntletGameState->FlagSwapTime);
					UE_LOG(UT,Log,TEXT("Progress %f"), Progress);
					TimerEffect->SetFloatParameter(NAME_Progress, Progress);			
				}
				else
				{
					TimerEffect->SetHiddenInGame(true);
				}
			}
		}
	}
}

void AUTGauntletFlag::SetTeam(AUTTeamInfo* NewTeam)
{
	Super::SetTeam(NewTeam);

	UE_LOG(UT,Verbose,TEXT("[AUTGauntletFlag::SetTeam] %s"), NewTeam == nullptr ? TEXT("NULL") : *FString::Printf(TEXT("%i"), NewTeam->GetTeamNum() ));

	// Fake the replication
	if (Role == ROLE_Authority)
	{
		uint8 NewTeamNum = NewTeam == nullptr ? 255 : NewTeam->GetTeamNum();

		AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();

		if (GauntletGameState != nullptr)
		{
			// If we are back to neutral, clear the flags and put 2 destination flags
			if (NewTeamNum == 255)
			{
				ClearGhostFlags();
				for (int32 i=0; i < 2; i++)
				{
					AUTCTFFlagBase* DestinationBase = GauntletGameState->GetFlagBase(i);
					if (DestinationBase)
					{
						PutGhostFlagAt(FFlagTrailPos(DestinationBase->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f)), false, true, 1 - i);
					}
				}
			}
			else
			{
				ClearGhostFlags();
				bFriendlyCanPickup = true;
				bAnyoneCanPickup = false;
				AUTCTFFlagBase* DestinationBase = GauntletGameState->GetFlagBase(1 - NewTeamNum);
				if (DestinationBase)
				{
					PutGhostFlagAt(FFlagTrailPos(DestinationBase->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f)), false, true, NewTeamNum);
				}
			}
		}

		AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();
		if (Game)
		{
			OnRep_Team();

			// Notify the game.
			Game->FlagTeamChanged(NewTeam ? NewTeam->GetTeamNum() : 255);
		}
	}
}

void AUTGauntletFlag::TeamReset()
{
	AUTGauntletGameState* SCTFGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (SCTFGameState && ObjectState == CarriedObjectState::Dropped)
	{

		bFriendlyCanPickup = false;
		bAnyoneCanPickup = true;

		bPendingTeamSwitch = false;
		SwapTimer = float(SCTFGameState->FlagSwapTime);

		SetTeam(nullptr);
		PlayCaptureEffect();
	}
}


FText AUTGauntletFlag::GetHUDStatusMessage(AUTHUD* HUD)
{
	if (GetTeamNum() == 255) // Anyone can pick this up
	{
		return NSLOCTEXT("UTGauntletFlag","AvailableForPickup","Grab the Flag");
	}
	if (bPendingTeamSwitch)
	{
		return NSLOCTEXT("UTGauntletFlag","Neutral","Becoming Neutral in");
	}
	return FText::GetEmpty();
}

void AUTGauntletFlag::ClearGhostFlags()
{
	if (!bIgnoreClearGhostCalls) Super::ClearGhostFlags();
}

void AUTGauntletFlag::OnHolderChanged()
{
	Super::OnHolderChanged();
	GenerateGPSPath();
}

void AUTGauntletFlag::GenerateGPSPath()
{
	if (bDebugGPS && GetWorld()->PersistentLineBatcher != nullptr)
	{
		FlushDebugStrings(GetWorld());
		FlushPersistentDebugLines(GetWorld());
	}

	CleanupTrail();
	GPSRoute.Empty();
	if (Holder != nullptr)
	{
		// Look to see if the holder is the local player
		AUTPlayerController* HolderPC = Cast<AUTPlayerController>(Holder->GetOwner());
		if (HolderPC != nullptr && HolderPC->Player != nullptr && Cast<UUTLocalPlayer>(HolderPC->Player) != nullptr)
		{

			AUTCharacter* Pawn = Cast<AUTCharacter>(HolderPC->GetPawn());
			if (Pawn != nullptr)
			{

				// Generate a follow path...

				if (NavData == nullptr)
				{ 
					NavData = GetUTNavData(GetWorld());
				}
			
				if (NavData)
				{
					FHitResult Hit;
					FCollisionQueryParams CollisionParms(NAME_GauntletTrail, true, this);

					AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
					if (GameState)
					{
						AUTCTFFlagBase* DestinationBase = GameState->GetFlagBase(1 - Holder->GetTeamNum());
						if (DestinationBase != nullptr)
						{
							FSingleEndpointEval NodeEval(DestinationBase);
							float Weight = 0.0f;
							if (NavData->FindBestPath(nullptr, Pawn->GetNavAgentPropertiesRef(), nullptr, NodeEval, Pawn->GetNavAgentLocation(), Weight, false, GPSRoute) && GPSRoute.Num() > 0)
							{
								TArray<FVector> Points;
								FUTPathLink CurrentPath = FUTPathLink();
								FVector NextLocation = Pawn->GetNavAgentLocation();

								// First, we go through and let the path system generate move points for each of the route nodes.  No rejection happens
								// in this phase.

								for (int32 i = 0 ; i < GPSRoute.Num(); i++)
								{
									FVector NodeLoc = GPSRoute[i].GetLocation(nullptr);
									TArray<FComponentBasedPosition> MoveTargetPoints;

									float TotalDistance = 0.0f;
									if (NavData->GetMovePoints(NextLocation, nullptr, Pawn->GetNavAgentPropertiesRef(), GPSRoute[i], GPSRoute, MoveTargetPoints, CurrentPath, &TotalDistance))
									{
										for (int32 j = 0; j < MoveTargetPoints.Num(); j++)
										{
											FVector PointLocation = MoveTargetPoints[j].Get(); 
											if (bDebugGPS)
											{
												FVector DebugLoc = PointLocation + FVector(0.0f,0.0f,96.0f);
												DrawDebugSphere(GetWorld(), DebugLoc,8,16,FColor::White,true);
												DrawDebugLine(GetWorld(), DebugLoc, PointLocation, FColor::White, true);
												DrawDebugString(GetWorld(), DebugLoc + FVector(0.0f,0.0f,64.0f), FString::Printf(TEXT("[%i,%i]"),i,j), nullptr, FColor::White, 200000, true);
											}
										
											MoveToFloor(PointLocation);
											Points.Add(PointLocation);
										}
									}
									else if (bDebugGPS)
									{
										UE_LOG(UT,Log,TEXT("GetMovePoints Failed for index %i"), i);
									}

									NextLocation = NodeLoc;
								}

								// Next we need to make sure we can connect each node.  It's valid to get path points that are reachable by travel
								// but not by direct trace (for example, slopes).  The problem is the trail system get's confused in these cases
								// and will just draw through the geo, so make an attempt at building out the list of nodes.

								int32 Point = 1;
								while (Points.IsValidIndex(Point))
								{
									// Look to see if the points can connect.
									bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Points[Point-1], Points[Point], COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
									if (bHit)
									{
										// We can't (probably a slope).  So sort the two points by Z access and then try to add points between them

										FVector A = Points[Point-1];
										FVector B = FVector();
										if (Points[Point].Z > A.Z)
										{
											B = A;
											A = Points[Point];
										}
										else
										{
											B = Points[Point];
										}

										FVector ExtraPoint = FVector(); 
										// try to find a point between that works
										if (PlaceExtraPoint(A, B, ExtraPoint, 0))
										{
											// Insert the point
											MoveToFloor(ExtraPoint);
											Points.Insert(ExtraPoint, Point);
										}
										else
										{
											// as a fallback, just grab the mid point and hope for the best.  
											ExtraPoint = A + (B-A).GetSafeNormal() * ((B-A).Size() * 0.5f);
											if (bDebugGPS)
											{
												DrawDebugString(GetWorld(), ExtraPoint, TEXT("Using Fallback"), nullptr, FColor::White, 20000, true);
											}
										}
									}

									// So, why pad out the path with extra points.  Again, this is for better management on long slopes.  In the case of
									// and extended distance slope, the two end points will cause the trail to float high in the air  And while the spline could
									// be adjusted, it will still be a stretch.  So we pad in a point every 1Kuu 
									if ( (Points[Point-1] - Points[Point]).SizeSquared() > (1024.0f * 1024.0f) )
									{
										// Add a point 1500 units from the last point.
										FVector SplitPoint = Points[Point-1] + (Points[Point] - Points[Point-1]).GetSafeNormal() * 1024.0f;
										MoveToFloor(SplitPoint);
										Points.Insert(SplitPoint, Point);
										if (bDebugGPS)
										{
											DrawDebugString(GetWorld(), SplitPoint, TEXT("Padding"), nullptr, FColor::White, 20000, true);
										}
									}
									Point++;
								}

								// Almsot done.  Go over all of the points, and remove and redundant ones.  A redundant point is any point that is < 1Kuu 
								// from the previous point and Z change of less than MAX_REDUNDANT_Z
								/*
								Point = 0;
								while (Point < Points.Num()-2)
								{
									FVector CurrentPoint = Points[Point];
									FVector TestPoint = Points[Point+2];

									FVector UNDir = (CurrentPoint - TestPoint);
									float Dist = UNDir.SizeSquared();
									float DistZ = FMath::Abs<float>(CurrentPoint.Z - TestPoint.Z);

									// If the next point is too close on the 2D plane but not too far outside the Z axis, then check
									// to see if it's redundant.

									if ( Dist < MIN_GPS_POINT_THRESHOLD && DistZ < MAX_REDUNDANT_Z )	
									{
										if (bDebugGPS)
										{
											DrawDebugLine(GetWorld(), CurrentPoint, TestPoint, FColor::Purple, true,-1.0, 2.0f, 1.0f);
											FVector DebugMid = CurrentPoint + (UNDir.GetSafeNormal() * (UNDir.Size() * 0.5f)) + FVector(0.0f,0.0f,-48.0f);
											DrawDebugString(GetWorld(), DebugMid, FString::Printf(TEXT("%.2f / %.2f"), UNDir.Size(), DistZ), nullptr, FColor::Purple, 20000.f, true);
										}

										bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CurrentPoint, TestPoint, COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
										if (!bHit)
										{
											// We didn't hit anything in the trace so we have direct LOS.  Go ahead and remove this point.
											if (bDebugGPS)
											{
												DrawDebugSphere(GetWorld(), Points[Point+1], 16,32, FColor::Red, true);
												DrawDebugString(GetWorld(), Points[Point+1] + FVector(0.0f,0.0f,-48.0f),TEXT("Rejected"), nullptr, FColor::Red, 200000.0, true);
											}

											Points.RemoveAt(Point+1,1);
											Point--;
										}
									}

									Point++;
								}
								*/
								// Finally, add a point at the player's location.  But push the point slightly past the player so that
								// they will see it if their back is to the next point in the node.  A gentle reminder to turn around and go.

								if (Points.Num() > 0)
								{
									FVector Direction = (Pawn->GetActorLocation() - Points[0]);
									FVector StartPoint = Points[0] + ( Direction.GetSafeNormal() * (Direction.Size() + 256.0f));
									Points.Insert(StartPoint, 0);
								}

								if (bDebugGPS)
								{
									// Display the navigation network
									for (int32 i=0; i < Points.Num(); i++)
									{
										DrawDebugSphere(GetWorld(), Points[i], 16,32,FColor::Yellow, true);
										DrawDebugString(GetWorld(), Points[i] + FVector(0.0f,0.0f,-36.0f), FString::Printf(TEXT("%i"), i), nullptr, FColor::White, 200000, true);

										if (i > 0)
										{
											FVector Dir = Points[i-1] - Points[i];
											float ZDist = FMath::Abs<float>(Points[i-1].Z - Points[i].Z);
											float Dist = Dir.Size();
											Dir = Dir.GetSafeNormal();

											DrawDebugLine(GetWorld(), Points[i-1], Points[i], FColor::Yellow, true);
											DrawDebugString(GetWorld(), Points[i] + (Dir * Dist * 0.5f), FString::Printf(TEXT("%.2f / %.2f"), Dist, ZDist), nullptr, FColor::White, 200000, true);
										}
									}
								}

								FActorSpawnParameters Params;
								Params.Owner = this;
								Trail = GetWorld()->SpawnActor<AUTFlagReturnTrail>(TrailClass, DestinationBase->GetActorLocation(), FRotator::ZeroRotator, Params);
								Trail->Flag = this;
								Trail->SetTeam(Holder->Team);
								Trail->SetPoints(Points);

							}			
							else
							{
								UE_LOG(UT,Log,TEXT("Failed to build path"));
							}
						}
						else
						{
							UE_LOG(UT,Log,TEXT("No Destination Base"));
						}
					}
				}
				else
				{
					UE_LOG(UT,Log,TEXT("NO NODE DATA"));
				}
			}
		}
	}
}

void AUTGauntletFlag::ValidateGPSPath()
{
	if (Holder != nullptr && !bDisableGPS)
	{
		// Look to see if the holder is the local player
		AUTPlayerController* HolderPC = Cast<AUTPlayerController>(Holder->GetOwner());
		if (HolderPC != nullptr && HolderPC->Player != nullptr && Cast<UUTLocalPlayer>(HolderPC->Player) != nullptr)
		{
			AUTCharacter* Pawn = Cast<AUTCharacter>(HolderPC->GetPawn());
			if (Pawn != nullptr && Pawn->GetCharacterMovement()->MovementMode == MOVE_Walking)
			{
				// Generate a follow path...
				if (NavData == nullptr)
				{ 
					NavData = GetUTNavData(GetWorld());
				}
			
				if (NavData)
				{
					FVector Extent = NavData->GetPOIExtent(Pawn);
					NavNodeRef NodeRef = NavData->FindAnchorPoly(Pawn->GetNavAgentLocation(),nullptr, FNavAgentProperties(Extent.X, Extent.Z * 2.0f));

					// Look to see if this is in the current list.
					bool bStillOnNetwork = false;
					for (int32 i = 0; i < GPSRoute.Num(); i++)
					{
						if (GPSRoute[i].Node.Get() == NavData->GetNodeFromPoly(NodeRef))
						{
							// We are still on the navigation network.
							bStillOnNetwork = true;
							break;
						}
					}

					if (!bStillOnNetwork)
					{
						GenerateGPSPath();
						return;
					}
				}
			}
		}
	}
}

void AUTGauntletFlag::MoveToFloor(FVector& PointLocation)
{
	FHitResult Hit;
	FCollisionQueryParams CollisionParms(NAME_GauntletTrail, true, this);

	// Trace down from the point to the floor
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, PointLocation, PointLocation + FVector(0.0f, 0.0f, -1024.0f), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
	if (bHit)
	{
		PointLocation = Hit.Location + FVector(0.0f, 0.0f, 64.0f);
	}
}

bool AUTGauntletFlag::PlaceExtraPoint(const FVector& A, const FVector& B, FVector& ExtraPoint, int32 Step)
{
	FHitResult Hit;
	FCollisionQueryParams CollisionParms(NAME_GauntletTrail, true, this);

	Step++;
	if (Step < 10)
	{
		float Dist = (B-A).Size();
		Dist *= float(Step) * 0.1f;
		ExtraPoint = A + (B-A).GetSafeNormal() * Dist;
		ExtraPoint.Z = A.Z;

		if (bDebugGPS)
		{
			DrawDebugSphere(GetWorld(), ExtraPoint, 8,32,FColor::Green, true);
			DrawDebugLine(GetWorld(), A, ExtraPoint, FColor::Green, true);
			DrawDebugLine(GetWorld(), B, ExtraPoint, FColor::Green, true);
		}

		if ( GetWorld()->LineTraceSingleByChannel(Hit, A, ExtraPoint, COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms) ||
				GetWorld()->LineTraceSingleByChannel(Hit, B, ExtraPoint, COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms) )
		{
			// move forward a bit more...
			return PlaceExtraPoint(A,B,ExtraPoint, Step);
		}

		// Found a point that can be seen by both
		return true;
	}

	// Failed to find a point..
	return false;

}

void AUTGauntletFlag::SendHome()
{
	Super::SendHome();
	CleanupTrail();
}


void AUTGauntletFlag::Destroyed()
{
	Super::Destroyed();
	CleanupTrail();
}

void AUTGauntletFlag::CleanupTrail()
{
	if (Trail)
	{
		Trail->EndTrail();
		Trail->SetLifeSpan(1.f); //failsafe
	}
}