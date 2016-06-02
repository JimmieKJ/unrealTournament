// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_FlagRunStatus.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"

UUTHUDWidget_FlagRunStatus::UUTHUDWidget_FlagRunStatus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTHUDWidget_FlagRunStatus::DrawStatusMessage(float DeltaTime)
{
}

void UUTHUDWidget_FlagRunStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime)
{
	if (GameState)
	{
		uint8 OwnerTeam = UTHUDOwner->UTPlayerOwner->GetTeamNum();
		uint8 OffensiveTeam = GameState->bRedToCap ? 0 : 1;
		uint8 DefensiveTeam = GameState->bRedToCap ? 1 : 0;

		if (GameState->FlagBases.IsValidIndex(OffensiveTeam) && GameState->FlagBases[OffensiveTeam] != nullptr)
		{
			AUTCTFFlag* Flag = Cast<AUTCTFFlag>(GameState->FlagBases[OffensiveTeam]->GetCarriedObject());
			if (Flag)
			{
				DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, OffensiveTeam, FVector2D(0.0f, 0.0f), GameState->FlagBases[OffensiveTeam], Flag, Flag->Holder);
				DrawFlagWorld(GameState, PlayerViewPoint, PlayerViewRotation, OffensiveTeam, GameState->FlagBases[OffensiveTeam], Flag, Flag->Holder);
			}
		}
		if (GameState->FlagBases.IsValidIndex(DefensiveTeam) && GameState->FlagBases[DefensiveTeam] != nullptr)
		{
			DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, DefensiveTeam, GameState->FlagBases[DefensiveTeam], nullptr, nullptr);
		}
	}
}

bool UUTHUDWidget_FlagRunStatus::ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag)
{
	return (Flag->ObjectState == CarriedObjectState::Dropped) || (Flag->ObjectState == CarriedObjectState::Home) || Flag->bCurrentlyPinged || !bIsEnemyFlag;
}

void UUTHUDWidget_FlagRunStatus::DrawFlagBaseWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	if (FlagBase)
	{
		if (Flag == nullptr)
		{
			AUTCTFFlag* BaseFlag = Cast<AUTCTFFlag>(FlagBase->GetCarriedObject());
		}

		bScaleByDesignedResolution = false;

		float Dist = (FlagBase->GetActorLocation() - PlayerViewPoint).Size();
		float WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);

		bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
		bool bDrawEdgeArrow = false;
		FVector ScreenPosition(0.f);
		FVector WorldPosition = FlagBase->GetActorLocation() + FlagBase->GetActorRotation().RotateVector(Flag ? Flag->HomeBaseOffset : FVector(0.0f, 0.0f, 128.0f)) + FVector(0.f, 0.f, Flag ? Flag->Collision->GetUnscaledCapsuleHalfHeight() * 3.f : 0.0f);
		float CurrentWorldAlpha = InWorldAlpha;
		FVector ViewDir = PlayerViewRotation.Vector();
		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;
		ScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);

		float PctFromCenter = (ScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
		CurrentWorldAlpha = InWorldAlpha * FMath::Min(0.15f / WorldRenderScale + 12.f*PctFromCenter, 1.f);

		ScreenPosition.X -= RenderPosition.X;
		ScreenPosition.Y -= RenderPosition.Y;

		CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleTemplate.RenderOpacity = CurrentWorldAlpha;

		RenderObj_TextureAt(CircleTemplate, ScreenPosition.X, ScreenPosition.Y, CircleTemplate.GetWidth()* WorldRenderScale, CircleTemplate.GetHeight()* WorldRenderScale);
		RenderObj_TextureAt(CircleBorderTemplate, ScreenPosition.X, ScreenPosition.Y, CircleBorderTemplate.GetWidth()* WorldRenderScale, CircleBorderTemplate.GetHeight()* WorldRenderScale);

		if (TeamNum == 0)
		{
			RedTeamIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(RedTeamIconTemplate, ScreenPosition.X, ScreenPosition.Y, RedTeamIconTemplate.GetWidth()* WorldRenderScale, RedTeamIconTemplate.GetHeight()* WorldRenderScale);
		}
		else
		{
			BlueTeamIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(BlueTeamIconTemplate, ScreenPosition.X, ScreenPosition.Y, BlueTeamIconTemplate.GetWidth()* WorldRenderScale, BlueTeamIconTemplate.GetHeight()* WorldRenderScale);
		}

		if (bDrawEdgeArrow)
		{
			DrawEdgeArrow(ScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
		}
		if (Flag && Flag->ObjectState != CarriedObjectState::Home)
		{
			RenderObj_TextureAt(FlagGoneIconTemplate, ScreenPosition.X, ScreenPosition.Y, FlagGoneIconTemplate.GetWidth()* WorldRenderScale, FlagGoneIconTemplate.GetHeight()* WorldRenderScale);
		}

		CircleTemplate.RenderOpacity = 1.f;
		CircleBorderTemplate.RenderOpacity = 1.f;
		bScaleByDesignedResolution = true;
	}
}

void UUTHUDWidget_FlagRunStatus::DrawFlagWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
	bool bIsEnemyFlag = Flag && !GameState->OnSameTeam(Flag, UTPlayerOwner);
	bool bShouldDrawFlagIcon = ShouldDrawFlag(Flag, bIsEnemyFlag);
	if (Flag && (bSpectating || bShouldDrawFlagIcon) && (Flag->Holder != UTHUDOwner->GetScorerPlayerState()))
	{
		bScaleByDesignedResolution = false;
		FlagIconTemplate.RenderColor = GameState->Teams.IsValidIndex(TeamNum) ? GameState->Teams[TeamNum]->TeamColor : FLinearColor::Green;

		// Draw the flag / flag base in the world
		float Dist = (Flag->GetActorLocation() - PlayerViewPoint).Size();
		float WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);
		bool bDrawEdgeArrow = false;
		float OldFlagAlpha = FlagIconTemplate.RenderOpacity;
		float CurrentWorldAlpha = InWorldAlpha;
		FVector ViewDir = PlayerViewRotation.Vector();
		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;

		AUTCharacter* Holder = (Flag->ObjectState == CarriedObjectState::Held) ? Cast<AUTCharacter>(Flag->AttachmentReplication.AttachParent) : nullptr;
		FVector WorldPosition = (Holder != nullptr)
							? Holder->GetMesh()->GetComponentLocation() + FVector(0.f, 0.f, Holder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f)
							: Flag->GetActorLocation() + FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 0.75f);
		FVector ScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);

		// Look to see if we should be displaying the in-world indicator for the flag.
		float CurrentWorldTime = GameState->GetWorld()->GetTimeSeconds();
		if (bIsEnemyFlag)
		{
			if (!bEnemyFlagWasDrawn)
			{
				EnemyFlagStartDrawTime = CurrentWorldTime;
			}
			if (CurrentWorldTime - EnemyFlagStartDrawTime < 0.3f)
			{
				WorldRenderScale *= (1.f + 3.f * (0.3f - CurrentWorldTime + EnemyFlagStartDrawTime));
			}
			bEnemyFlagWasDrawn = true;
		}

		float PctFromCenter = (ScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
		CurrentWorldAlpha = InWorldAlpha * FMath::Min(0.15f / WorldRenderScale + 12.f*PctFromCenter, 1.f);

		ScreenPosition.X -= RenderPosition.X;
		ScreenPosition.Y -= RenderPosition.Y;
		float ViewDist = (PlayerViewPoint - WorldPosition).Size();

		// don't overlap player beacon
		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		float X, Y;
		float Scale = Canvas->ClipX / 1920.f;
		Canvas->TextSize(TinyFont, FString("+999   A999"), X, Y, Scale, Scale);
		if (!bDrawEdgeArrow)
		{
			if (!Holder || (ViewDist < Holder->TeamPlayerIndicatorMaxDistance))
			{
				ScreenPosition.Y -= 3.5f*Y;
			}
			else
			{
				ScreenPosition.Y -= (ViewDist < Holder->SpectatorIndicatorMaxDistance) ? 2.5f*Y : 1.5f*Y;
			}
		}

		FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;

		float InWorldFlagScale = GameState->bOneFlagGameMode ? WorldRenderScale*StatusScale : WorldRenderScale;
		RenderObj_TextureAt(CircleTemplate, ScreenPosition.X, ScreenPosition.Y, CircleTemplate.GetWidth()* InWorldFlagScale, CircleTemplate.GetHeight()* InWorldFlagScale);
		RenderObj_TextureAt(CircleBorderTemplate, ScreenPosition.X, ScreenPosition.Y, CircleBorderTemplate.GetWidth()* InWorldFlagScale, CircleBorderTemplate.GetHeight()* InWorldFlagScale);

		if (bDrawEdgeArrow)
		{
			DrawEdgeArrow(ScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
		}
		FText FlagStatusMessage = Flag->GetHUDStatusMessage(UTHUDOwner);
		if (!FlagStatusMessage.IsEmpty())
		{
			DrawText(FlagStatusMessage, ScreenPosition.X, ScreenPosition.Y - ((CircleTemplate.GetHeight() + 40) * WorldRenderScale), AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*WorldRenderScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
		}
		if (Flag && Flag->ObjectState == CarriedObjectState::Held)
		{
			TakenIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(TakenIconTemplate, ScreenPosition.X, ScreenPosition.Y, 1.1f * TakenIconTemplate.GetWidth()* InWorldFlagScale, 1.1f * TakenIconTemplate.GetHeight()* InWorldFlagScale);
			RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X - 0.25f * FlagIconTemplate.GetWidth()* InWorldFlagScale, ScreenPosition.Y - 0.25f * FlagIconTemplate.GetHeight()* InWorldFlagScale, FlagIconTemplate.GetWidth()* InWorldFlagScale, FlagIconTemplate.GetHeight()* InWorldFlagScale);
		}
		else
		{
			RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X, ScreenPosition.Y, 1.25f*FlagIconTemplate.GetWidth()* InWorldFlagScale, 1.25f*FlagIconTemplate.GetHeight()* InWorldFlagScale);

			if (Flag->ObjectState == CarriedObjectState::Dropped)
			{
				float DroppedAlpha = DroppedIconTemplate.RenderOpacity;
				DroppedIconTemplate.RenderOpacity = CurrentWorldAlpha;
				RenderObj_TextureAt(DroppedIconTemplate, ScreenPosition.X, ScreenPosition.Y, 1.25f*DroppedIconTemplate.GetWidth()* InWorldFlagScale, 1.25f*DroppedIconTemplate.GetHeight()* InWorldFlagScale);
				DroppedIconTemplate.RenderOpacity = DroppedAlpha;
				DrawText(GetFlagReturnTime(Flag), ScreenPosition.X, ScreenPosition.Y, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
			}
			else if (Flag->ObjectState == CarriedObjectState::Home)
			{
				AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(GameState);
				if (RCTFGameState && (RCTFGameState->RemainingPickupDelay > 0))
				{
					DrawText(FText::AsNumber(RCTFGameState->RemainingPickupDelay), ScreenPosition.X, ScreenPosition.Y, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
				}
			}
		}
		FlagIconTemplate.RenderOpacity = OldFlagAlpha;
		CircleTemplate.RenderOpacity = 1.f;
		CircleBorderTemplate.RenderOpacity = 1.f;

		// draw line from hud to this loc - can't used Canvas line drawing code because it doesn't support translucency
		FVector LineEndPoint(ScreenPosition.X+RenderPosition.X, ScreenPosition.Y+RenderPosition.Y, 0.f);
		FVector LineStartPoint(0.5f*Canvas->ClipX, 0.12f*Canvas->ClipY, 0.f);
		FLinearColor LineColor = FlagIconTemplate.RenderColor;
		LineColor.A = 0.05f;
		FBatchedElements* BatchedElements = Canvas->Canvas->GetBatchedElements(FCanvas::ET_Line);
		FHitProxyId HitProxyId = Canvas->Canvas->GetHitProxyId();
		BatchedElements->AddTranslucentLine(LineEndPoint, LineStartPoint, LineColor, HitProxyId, 8.f);
	}
	else if (bIsEnemyFlag)
	{
		bEnemyFlagWasDrawn = false;
	}
}
