// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponStateZooming.h"

void FZoomTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (ZoomState != NULL && !ZoomState->HasAnyFlags(RF_PendingKill | RF_Unreachable) && ZoomState->GetUTOwner() != NULL && ZoomState->GetUTOwner()->GetWeapon() == ZoomState->GetOuterAUTWeapon())
	{
		ZoomState->TickZoom(DeltaTime);
	}
}
FString FZoomTickFunction::DiagnosticMessage()
{
	return *FString::Printf(TEXT("%s::ZoomTick()"), *GetPathNameSafe(ZoomState));
}

UUTWeaponStateZooming::UUTWeaponStateZooming(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ZoomStartFOV = 80.0f;
	MinFOV = 12.f;
	ZoomTime = 0.9f;
	bDrawHeads = true;
	bDrawPingAdjustedTargets = true;
}

void UUTWeaponStateZooming::PendingFireStarted()
{
	if (bIsZoomed)
	{
		bIsZoomed = false;
		if (GetUTOwner()->IsLocallyControlled())
		{
			GetOuterAUTWeapon()->SetActorHiddenInGame(false);
			APlayerCameraManager* Camera = GetUTOwner()->GetPlayerCameraManager();
			if (Camera != NULL)
			{
				Camera->UnlockFOV();
			}

			UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomOutSound, GetUTOwner(), SRT_None, false, FVector::ZeroVector, NULL, NULL, false);
		}
	}
	else
	{
		bIsZoomed = true;
		StartZoomTime = GetWorld()->TimeSeconds;
		ZoomTickHandler.ZoomState = this;
		ZoomTickHandler.RegisterTickFunction(GetOuterAUTWeapon()->GetLevel());

		if (GetUTOwner()->IsLocallyControlled())
		{
			GetOuterAUTWeapon()->SetActorHiddenInGame(true);
			UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomInSound, GetUTOwner(), SRT_None, false, FVector::ZeroVector, NULL, NULL, false);
			ToggleZoomInSound(true);
		}
	}
}

void UUTWeaponStateZooming::PendingFireStopped()
{
	ZoomTickHandler.UnRegisterTickFunction();
	ToggleZoomInSound(false);
}

bool UUTWeaponStateZooming::BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
{
	// this isn't actually firing so immediately switch to other fire mode
	if (FireModeNum != GetOuterAUTWeapon()->GetCurrentFireMode() && GetOuterAUTWeapon()->FiringState.IsValidIndex(FireModeNum) && GetOuterAUTWeapon()->HasAmmo(FireModeNum))
	{
		GetOuterAUTWeapon()->GotoFireMode(FireModeNum);
	}
	return false;
}

void UUTWeaponStateZooming::EndFiringSequence(uint8 FireModeNum)
{
	GetOuterAUTWeapon()->GotoActiveState();
}

void UUTWeaponStateZooming::WeaponBecameInactive()
{
	if (bIsZoomed && GetUTOwner()->IsLocallyControlled())
	{
		bIsZoomed = false;
		ZoomTickHandler.UnRegisterTickFunction();
		APlayerCameraManager* Camera = GetUTOwner()->GetPlayerCameraManager();
		if (Camera != NULL)
		{
			Camera->UnlockFOV();
		}
		GetOuterAUTWeapon()->SetActorHiddenInGame(false);

		UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomOutSound, GetUTOwner(), SRT_None, false, FVector::ZeroVector, NULL, NULL, false);
	}
}

bool UUTWeaponStateZooming::DrawHUD(UUTHUDWidget* WeaponHudWidget)
{
	if (bIsZoomed && OverlayMat != NULL)
	{
		UCanvas* C = WeaponHudWidget->GetCanvas();
		if (OverlayMI == NULL)
		{
			OverlayMI = UMaterialInstanceDynamic::Create(OverlayMat, this);
		}
		AUTPlayerState* PS = Cast<AUTPlayerState>(GetUTOwner()->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			static FName NAME_TeamColor(TEXT("TeamColor"));
			OverlayMI->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
		}
		FCanvasTileItem Item(FVector2D(0.0f, 0.0f), OverlayMI->GetRenderProxy(false), FVector2D(C->ClipX, C->ClipY));
		// expand X axis size to be widest supported aspect ratio (16:9)
		float OrigSizeX = Item.Size.X;
		Item.Size.X = FMath::Max<float>(Item.Size.X, Item.Size.Y * 16.0f / 9.0f);
		Item.Position.X -= (Item.Size.X - OrigSizeX) * 0.5f;
		Item.UV0 = FVector2D(0.0f, 0.0f);
		Item.UV1 = FVector2D(1.0f, 1.0f);
		C->DrawItem(Item);

		if (bDrawHeads && TargetIndicator != NULL)
		{
			float HeadScale = GetOuterAUTWeapon()->GetHeadshotScale();
			if (HeadScale > 0.0f)
			{
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				APlayerState* OwnerState = GetUTOwner()->PlayerState;
				AUTPlayerController* UTPC = GetUTOwner()->GetLocalViewer();
				float WorldTime = GetWorld()->TimeSeconds;
				FVector FireStart = GetOuterAUTWeapon()->GetFireStartLoc();
				for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
				{
					AUTCharacter* EnemyChar = Cast<AUTCharacter>(*It);
					if (EnemyChar != NULL && !EnemyChar->IsDead() && !EnemyChar->IsFeigningDeath() && EnemyChar->GetMesh()->LastRenderTime > WorldTime - 0.25f && EnemyChar != GetUTOwner() && (GS == NULL || !GS->OnSameTeam(EnemyChar, GetUTOwner())))
					{
						FVector HeadLoc = EnemyChar->GetHeadLocation();
						static FName NAME_SniperZoom(TEXT("SniperZoom"));
						if (!GetWorld()->LineTraceTestByChannel(FireStart, HeadLoc, COLLISION_TRACE_WEAPONNOCHARACTER, FCollisionQueryParams(NAME_SniperZoom, true, GetUTOwner())))
						{
							bool bDrawPingAdjust = bDrawPingAdjustedTargets && OwnerState != NULL && !EnemyChar->GetVelocity().IsZero();
							float NetPing = 0.0f;
							if (bDrawPingAdjust)
							{
								NetPing = 0.001f * (OwnerState->ExactPing - (UTPC ? UTPC->MaxPredictionPing : 0.f));
								bDrawPingAdjust = NetPing > 0.f;
							}
							for (int32 i = 0; i < (bDrawPingAdjust ? 2 : 1); i++)
							{
								FVector Perpendicular = (HeadLoc - FireStart).GetSafeNormal() ^ FVector(0.0f, 0.0f, 1.0f);
								FVector PointA = C->Project(HeadLoc + Perpendicular * (EnemyChar->HeadRadius * EnemyChar->HeadScale * HeadScale));
								FVector PointB = C->Project(HeadLoc - Perpendicular * (EnemyChar->HeadRadius * EnemyChar->HeadScale * HeadScale));
								FVector2D UpperLeft(FMath::Min<float>(PointA.X, PointB.X), FMath::Min<float>(PointA.Y, PointB.Y));
								FVector2D BottomRight(FMath::Max<float>(PointA.X, PointB.X), FMath::Max<float>(PointA.Y, PointB.Y));
								float MidY = (UpperLeft.Y + BottomRight.Y) * 0.5f;

								// skip drawing if too off-center
								if ((FMath::Abs(MidY - 0.5f*C->SizeY) < 0.11f*C->SizeY) && (FMath::Abs(0.5f*(UpperLeft.X + BottomRight.X) - 0.5f*C->SizeX) < 0.11f*C->SizeX))
								{
									// square-ify
									float SizeY = FMath::Max<float>(MidY - UpperLeft.Y, (BottomRight.X - UpperLeft.X) * 0.5f);
									UpperLeft.Y = MidY - SizeY;
									BottomRight.Y = MidY + SizeY;
									FLinearColor TargetColor = EnemyChar->bIsWearingHelmet ? FLinearColor(1.0f, 1.0f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
									FCanvasTileItem HeadCircleItem(UpperLeft, TargetIndicator->Resource, BottomRight - UpperLeft, (i == 0) ? TargetColor : FLinearColor(0.7f, 0.7f, 0.7f, 0.9f));
									HeadCircleItem.BlendMode = SE_BLEND_Translucent;
									C->DrawItem(HeadCircleItem);
								}
								if (OwnerState != NULL)
								{
									HeadLoc += EnemyChar->GetVelocity() * NetPing;
								}
							}
						}
					}
				}
			}
		}

		// temp until there's a decent crosshair in the material
		return true;
	}
	else
	{
		return true;
	}
}

void UUTWeaponStateZooming::TickZoom(float DeltaTime)
{
	// only mess with the FOV on the client; it doesn't matter to networking and is easier to maintain
	if (GetUTOwner()->IsLocallyControlled())
	{
		APlayerCameraManager* Camera = GetUTOwner()->GetPlayerCameraManager();
		if (Camera != NULL)
		{
			float StartFOV = (ZoomStartFOV > 0.0f) ? FMath::Min<float>(Camera->DefaultFOV, ZoomStartFOV) : Camera->DefaultFOV;
			Camera->SetFOV(StartFOV - (StartFOV - MinFOV) * FMath::Min<float>((GetWorld()->TimeSeconds - StartZoomTime) / ZoomTime, 1.0f));

			if (Camera->GetLockedFOV() <= MinFOV)
			{
				OnZoomingFinished();
			}
		}
	}
}

void UUTWeaponStateZooming::OnZoomingFinished()
{
	ZoomTickHandler.UnRegisterTickFunction();
	ToggleZoomInSound(false);
}

void UUTWeaponStateZooming::ToggleZoomInSound(bool bNowOn)
{
	if (ZoomLoopSound != NULL && Cast<APlayerController>(GetUTOwner()->Controller) && GetUTOwner()->IsLocallyControlled())
	{
		if (ZoomLoopComp == NULL)
		{
			ZoomLoopComp = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass());
			ZoomLoopComp->bAutoDestroy = false;
			ZoomLoopComp->bAutoActivate = false;
			ZoomLoopComp->Sound = ZoomLoopSound;
			ZoomLoopComp->bAllowSpatialization = false;
		}
		if (bNowOn)
		{
			ZoomLoopComp->RegisterComponent();
			// note we don't need to attach to anything because we disabled spatialization
			ZoomLoopComp->Play();
		}
		else if (ZoomLoopComp->IsRegistered())
		{
			ZoomLoopComp->Stop();
			ZoomLoopComp->DetachFromParent();
			ZoomLoopComp->UnregisterComponent();
		}
	}
}