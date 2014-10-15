// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

UUTWeaponStateZooming::UUTWeaponStateZooming(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
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

			UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomOutSound, GetUTOwner(), SRT_None);
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
			UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomInSound, GetUTOwner(), SRT_None);
			ToggleZoomInSound(true);
		}
	}
}
void UUTWeaponStateZooming::PendingFireStopped()
{
	ZoomTickHandler.UnRegisterTickFunction();
	ToggleZoomInSound(false);
}

void UUTWeaponStateZooming::BeginFiringSequence(uint8 FireModeNum)
{
	// this isn't actually firing so immediately switch to other fire mode
	if (FireModeNum != GetOuterAUTWeapon()->GetCurrentFireMode() && GetOuterAUTWeapon()->FiringState.IsValidIndex(FireModeNum) && GetOuterAUTWeapon()->HasAmmo(FireModeNum))
	{
		GetOuterAUTWeapon()->GotoFireMode(FireModeNum);
	}
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

		UUTGameplayStatics::UTPlaySound(GetWorld(), ZoomOutSound, GetUTOwner(), SRT_None);
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
				float WorldTime = GetWorld()->TimeSeconds;
				FVector FireStart = GetOuterAUTWeapon()->GetFireStartLoc();
				for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
				{
					AUTCharacter* EnemyChar = Cast<AUTCharacter>(*It);
					if (EnemyChar != NULL && !EnemyChar->IsDead() && !EnemyChar->IsFeigningDeath() && EnemyChar->Mesh->LastRenderTime > WorldTime - 0.25f && EnemyChar != GetUTOwner() && (GS == NULL || !GS->OnSameTeam(EnemyChar, GetUTOwner())))
					{
						FVector HeadLoc = EnemyChar->GetHeadLocation();
						static FName NAME_SniperZoom(TEXT("SniperZoom"));
						if (!GetWorld()->LineTraceTest(FireStart, HeadLoc, ECC_Visibility, FCollisionQueryParams(NAME_SniperZoom, true, GetUTOwner())))
						{
							AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetUTOwner()->GetController());
							float NetPing = 0.001f*(OwnerState->ExactPing - (UTPC ? UTPC->MaxPredictionPing : 0.f));
							bool bDrawPingAdjust = bDrawPingAdjustedTargets && OwnerState != NULL && NetPing > 0.f && !EnemyChar->GetVelocity().IsZero();
							for (int32 i = 0; i < (bDrawPingAdjust ? 2 : 1); i++)
							{
								FVector Perpendicular = (HeadLoc - FireStart).SafeNormal() ^ FVector(0.0f, 0.0f, 1.0f);
								FVector PointA = C->Project(HeadLoc + Perpendicular * (EnemyChar->HeadRadius * EnemyChar->HeadScale * HeadScale));
								FVector PointB = C->Project(HeadLoc - Perpendicular * (EnemyChar->HeadRadius * EnemyChar->HeadScale * HeadScale));
								FVector2D UpperLeft(FMath::Min<float>(PointA.X, PointB.X), FMath::Min<float>(PointA.Y, PointB.Y));
								FVector2D BottomRight(FMath::Max<float>(PointA.X, PointB.X), FMath::Max<float>(PointA.Y, PointB.Y));
								// square-ify
								float MidY = (UpperLeft.Y + BottomRight.Y) * 0.5f;
								float SizeY = FMath::Max<float>(MidY - UpperLeft.Y, (BottomRight.X - UpperLeft.X) * 0.5f);
								UpperLeft.Y = MidY - SizeY;
								BottomRight.Y = MidY + SizeY;
								FCanvasTileItem HeadCircleItem(UpperLeft, TargetIndicator->Resource, BottomRight - UpperLeft, (i == 0) ? FLinearColor(1.0f, 0.0f, 0.0f, 1.0f) : FLinearColor(0.7f, 0.7f, 0.7f, 0.9f));
								HeadCircleItem.BlendMode = SE_BLEND_Translucent;
								C->DrawItem(HeadCircleItem);

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

			if (Camera->LockedFOV <= MinFOV)
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
			ZoomLoopComp = ConstructObject<UAudioComponent>(UAudioComponent::StaticClass(), this);
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