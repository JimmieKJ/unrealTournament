// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFRoundGameState.h"
#include "UTHUDWidget_FlagRunStatus.h"
#include "UTFlagRunGameState.h"

UUTHUDWidget_FlagRunStatus::UUTHUDWidget_FlagRunStatus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NormalLineBrightness = 0.025f;
	LineGlow = 0.4f;
	PulseLength = 1.5f;
	bAlwaysDrawFlagHolderName = false;
}

bool UUTHUDWidget_FlagRunStatus::ShouldDraw_Implementation(bool bShowScores)
{
	bool bResult = Super::ShouldDraw_Implementation(bShowScores);
	if (!bResult)
	{
		LastFlagStatusChange = GetWorld()->GetTimeSeconds();
	}
	return bResult;
}

void UUTHUDWidget_FlagRunStatus::DrawStatusMessage(float DeltaTime)
{
}

void UUTHUDWidget_FlagRunStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime)
{
	if (GameState)
	{
		uint8 OwnerTeam = UTHUDOwner->UTPlayerOwner->GetTeamNum();
		AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(GameState);
		uint8 OffensiveTeam = (FRGS && FRGS->bRedToCap) ? 0 : 1;
		uint8 DefensiveTeam = (FRGS && FRGS->bRedToCap) ? 1 : 0;

		if (GameState->FlagBases.IsValidIndex(OffensiveTeam) && GameState->FlagBases[OffensiveTeam] != nullptr)
		{
			AUTCTFFlag* Flag = Cast<AUTCTFFlag>(GameState->FlagBases[OffensiveTeam]->GetCarriedObject());
			if (Flag && (Flag->ObjectState != CarriedObjectState::Delivered))
			{
				DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, OffensiveTeam, FVector2D(0.0f, 100.0f), GameState->FlagBases[OffensiveTeam], Flag, Flag->Holder);
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
		float WorldRenderScale = RenderScale * MaxIconScale;

		bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
		bool bDrawEdgeArrow = false;
		FVector DrawScreenPosition(0.f);
		FVector WorldPosition = FlagBase->GetActorLocation() + FlagBase->GetActorRotation().RotateVector(Flag ? Flag->HomeBaseOffset : FVector(0.0f, 0.0f, 128.0f)) + FVector(0.f, 0.f, Flag ? Flag->Collision->GetUnscaledCapsuleHalfHeight() * 3.f : 0.0f);
		float CurrentWorldAlpha = InWorldAlpha;
		FVector ViewDir = PlayerViewRotation.Vector();
		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;
		DrawScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);

		float PctFromCenter = (DrawScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
		CurrentWorldAlpha = InWorldAlpha * FMath::Min(8.f*PctFromCenter, 1.f);

		DrawScreenPosition.X -= RenderPosition.X;
		DrawScreenPosition.Y -= RenderPosition.Y;

		CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleTemplate.RenderOpacity = CurrentWorldAlpha;

		RenderObj_TextureAt(CircleTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleTemplate.GetWidth()* WorldRenderScale, CircleTemplate.GetHeight()* WorldRenderScale);
		RenderObj_TextureAt(CircleBorderTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleBorderTemplate.GetWidth()* WorldRenderScale, CircleBorderTemplate.GetHeight()* WorldRenderScale);

		if (TeamNum == 0)
		{
			RedTeamIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(RedTeamIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, RedTeamIconTemplate.GetWidth()* WorldRenderScale, RedTeamIconTemplate.GetHeight()* WorldRenderScale);
		}
		else
		{
			BlueTeamIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(BlueTeamIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, BlueTeamIconTemplate.GetWidth()* WorldRenderScale, BlueTeamIconTemplate.GetHeight()* WorldRenderScale);
		}

		if (bDrawEdgeArrow)
		{
			DrawEdgeArrow(WorldPosition, DrawScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
		}
		if (Flag && Flag->ObjectState != CarriedObjectState::Home)
		{
			RenderObj_TextureAt(FlagGoneIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, FlagGoneIconTemplate.GetWidth()* WorldRenderScale, FlagGoneIconTemplate.GetHeight()* WorldRenderScale);
		}

		CircleTemplate.RenderOpacity = 1.f;
		CircleBorderTemplate.RenderOpacity = 1.f;
		bScaleByDesignedResolution = true;
	}
}

void UUTHUDWidget_FlagRunStatus::DrawFlagWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
	bool bIsEnemyFlag = Flag && GameState && !GameState->OnSameTeam(Flag, UTPlayerOwner);
	bool bShouldDrawFlagIcon = ShouldDrawFlag(Flag, bIsEnemyFlag);
	if (Flag && GameState && (bSpectating || bShouldDrawFlagIcon) && (Flag->Holder != UTHUDOwner->GetScorerPlayerState()))
	{
		bScaleByDesignedResolution = false;
		FlagIconTemplate.RenderColor = (GameState->Teams.IsValidIndex(TeamNum) && GameState->Teams[TeamNum]) ? GameState->Teams[TeamNum]->TeamColor : FLinearColor::Green;

		// Draw the flag / flag base in the world
		float Dist = (Flag->GetActorLocation() - PlayerViewPoint).Size();
		float WorldRenderScale = RenderScale * MaxIconScale;
		bool bDrawEdgeArrow = false;
		float OldFlagAlpha = FlagIconTemplate.RenderOpacity;
		float CurrentWorldAlpha = InWorldAlpha;
		FVector ViewDir = PlayerViewRotation.Vector();
		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;

		AUTCharacter* Holder = (Flag->ObjectState == CarriedObjectState::Held) ? Cast<AUTCharacter>(Flag->GetAttachmentReplication().AttachParent) : nullptr;
		FVector WorldPosition = (Holder != nullptr)
							? Holder->GetMesh()->GetComponentLocation() + FVector(0.f, 0.f, Holder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f)
							: Flag->GetActorLocation() + FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 0.75f);
		FVector DrawScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);
		DrawScreenPosition.Y -= 36.f*RenderScale;

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

		float PctFromCenter = (DrawScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
		CurrentWorldAlpha = InWorldAlpha * FMath::Min(8.f*PctFromCenter, 1.f);
		float ViewDist = (PlayerViewPoint - WorldPosition).Size();

		// don't overlap player beacon
		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		float X, Y;
		float Scale = Canvas->ClipX / 1920.f;
		Canvas->TextSize(TinyFont, FString("+999   A999"), X, Y, Scale, Scale);

		DrawScreenPosition.X -= RenderPosition.X;
		DrawScreenPosition.Y -= RenderPosition.Y;

		FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleTemplate.RenderOpacity = CurrentWorldAlpha;
		CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;

		float InWorldFlagScale = WorldRenderScale * (StatusScale - 0.5f*(StatusScale - 1.f));
		RenderObj_TextureAt(CircleTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleTemplate.GetWidth()* InWorldFlagScale, CircleTemplate.GetHeight()* InWorldFlagScale);
		RenderObj_TextureAt(CircleBorderTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleBorderTemplate.GetWidth()* InWorldFlagScale, CircleBorderTemplate.GetHeight()* InWorldFlagScale);

		if (bDrawEdgeArrow)
		{
			DrawEdgeArrow(WorldPosition, DrawScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
		}
		FText FlagStatusMessage = Flag->GetHUDStatusMessage(UTHUDOwner);
		if (!FlagStatusMessage.IsEmpty())
		{
			DrawText(FlagStatusMessage, DrawScreenPosition.X, DrawScreenPosition.Y - ((CircleTemplate.GetHeight() + 40) * WorldRenderScale), AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*WorldRenderScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
		}
		if (Flag && Flag->ObjectState == CarriedObjectState::Held)
		{
			TakenIconTemplate.RenderOpacity = CurrentWorldAlpha;
			RenderObj_TextureAt(TakenIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.1f * TakenIconTemplate.GetWidth()* InWorldFlagScale, 1.1f * TakenIconTemplate.GetHeight()* InWorldFlagScale);
			RenderObj_TextureAt(FlagIconTemplate, DrawScreenPosition.X - 0.25f * FlagIconTemplate.GetWidth()* InWorldFlagScale, DrawScreenPosition.Y - 0.25f * FlagIconTemplate.GetHeight()* InWorldFlagScale, FlagIconTemplate.GetWidth()* InWorldFlagScale, FlagIconTemplate.GetHeight()* InWorldFlagScale);
		}
		else
		{
			RenderObj_TextureAt(FlagIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.25f*FlagIconTemplate.GetWidth()* InWorldFlagScale, 1.25f*FlagIconTemplate.GetHeight()* InWorldFlagScale);

			if (Flag->ObjectState == CarriedObjectState::Dropped)
			{
				float DroppedAlpha = DroppedIconTemplate.RenderOpacity;
				DroppedIconTemplate.RenderOpacity = CurrentWorldAlpha;
				RenderObj_TextureAt(DroppedIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.25f*DroppedIconTemplate.GetWidth()* InWorldFlagScale, 1.25f*DroppedIconTemplate.GetHeight()* InWorldFlagScale);
				DroppedIconTemplate.RenderOpacity = DroppedAlpha;
				bool bCloseToFlag = !bIsEnemyFlag && Cast<AUTCharacter>(UTPlayerOwner->GetPawn()) && Flag->IsNearTeammate((AUTCharacter *)(UTPlayerOwner->GetPawn()));
				FLinearColor TimeColor = bCloseToFlag ? FLinearColor::Green : FLinearColor::White;
				DrawText(GetFlagReturnTime(Flag), DrawScreenPosition.X, DrawScreenPosition.Y, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f*CurrentWorldAlpha, TimeColor, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
			}
			else if (Flag->ObjectState == CarriedObjectState::Home)
			{
				AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(GameState);
				if (RCTFGameState && (RCTFGameState->RemainingPickupDelay > 0))
				{
					DrawText(FText::AsNumber(RCTFGameState->RemainingPickupDelay), DrawScreenPosition.X, DrawScreenPosition.Y, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
				}
			}
		}
		FlagIconTemplate.RenderOpacity = OldFlagAlpha;
		CircleTemplate.RenderOpacity = 1.f;
		CircleBorderTemplate.RenderOpacity = 1.f;

		if (LastFlagStatus != Flag->ObjectState)
		{
			LastFlagStatus = Flag->ObjectState;
			LastFlagStatusChange = GetWorld()->GetTimeSeconds();
		}
		float TimeSinceChange = GetWorld()->GetTimeSeconds() - LastFlagStatusChange;
		float LineBrightness = NormalLineBrightness;
		if (TimeSinceChange < 0.1f)
		{
			LineBrightness = LineGlow * TimeSinceChange * 10.f;
		}
		else if (TimeSinceChange < 0.1f + PulseLength)
		{
			LineBrightness = NormalLineBrightness + LineGlow * FMath::Max(0.f, 1.f - (TimeSinceChange - 0.1f) / PulseLength);
		}

		// draw line from hud to this loc - can't used Canvas line drawing code because it doesn't support translucency
		FVector LineEndPoint(DrawScreenPosition.X+RenderPosition.X, DrawScreenPosition.Y+RenderPosition.Y, 0.f);
		FVector LineStartPoint(0.5f*Canvas->ClipX, 0.12f*Canvas->ClipY, 0.f);
		FLinearColor LineColor = FlagIconTemplate.RenderColor;
		LineColor.A = LineBrightness;
		FBatchedElements* BatchedElements = Canvas->Canvas->GetBatchedElements(FCanvas::ET_Line);
		FHitProxyId HitProxyId = Canvas->Canvas->GetHitProxyId();
		BatchedElements->AddTranslucentLine(LineEndPoint, LineStartPoint, LineColor, HitProxyId, 8.f);
	}
	else if (bIsEnemyFlag)
	{
		bEnemyFlagWasDrawn = false;
	}
}

