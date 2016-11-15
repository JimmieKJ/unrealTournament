// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTCTFGameState.h"
#include "UTSecurityCameraComponent.h"

UUTHUDWidget_CTFFlagStatus::UUTHUDWidget_CTFFlagStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	YouHaveFlagText = NSLOCTEXT("CTFScore","YouHaveFlagText","You have the flag!");
	YouHaveFlagTextAlt = NSLOCTEXT("CTFScore", "YouHaveFlagTextAlt", "You have the flag, take it to the enemy base!");
	EnemyHasFlagText = NSLOCTEXT("CTFScore", "EnemyHasFlagText", "The enemy has your flag, recover it!");
	BothFlagsText = NSLOCTEXT("CTFScore","BothFlagsText","You have the enemy flag - hold it until your flag is returned!");

	ScreenPosition = FVector2D(0.5f, 0.0f);
	Size = FVector2D(0.f, 0.f);
	Origin = FVector2D(0.5f, 0.5f);
	AnimationAlpha = 0.0f;
	StatusScale = 1.f;
	InWorldAlpha = 0.8f;
	MaxIconScale = 0.75f;

	bSuppressMessage = false;
	bAlwaysDrawFlagHolderName = true;

	OldFlagState[0] = CarriedObjectState::Home;
	OldFlagState[1] = CarriedObjectState::Home;
	StatusChangedScale = 10.f;
	CurrentStatusScale[0] = 1.f;
	CurrentStatusScale[1] = 1.f;
	ScaleDownTime = 0.5f;
}

void UUTHUDWidget_CTFFlagStatus::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_CTFFlagStatus::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* GameState = Cast<AUTCTFGameState>(UTGameState);
	if (GameState == NULL) return;

	if (bStatusDir)
	{
		StatusScale = FMath::Min(StatusScale+2.f*DeltaTime, 2.0f);
		bStatusDir = (StatusScale < 1.8f);
	}
	else
	{
		StatusScale = FMath::Max(StatusScale - 2.f*DeltaTime, 1.f);
		bStatusDir = (StatusScale < 1.2f);
	}
	for (int32 i = 0; i < 2; i++)
	{
		CurrentStatusScale[i] = FMath::Max(1.f, CurrentStatusScale[i] - DeltaTime*(StatusChangedScale - 1.f) / ScaleDownTime);
	}

	FVector ViewPoint;
	FRotator ViewRotation;

	UTPlayerOwner->GetPlayerViewPoint(ViewPoint, ViewRotation);
	DrawIndicators(GameState, ViewPoint, ViewRotation, DeltaTime);
	DrawStatusMessage(DeltaTime);
}

void UUTHUDWidget_CTFFlagStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime)
{
	for (int32 Team=0;Team<2;Team++)
	{
		if (GameState->Teams.IsValidIndex(Team) && TeamPositions.IsValidIndex(Team))
		{
			AUTTeamInfo* TeamInfo = GameState->Teams[Team];
			AUTPlayerState* FlagHolder = GameState->GetFlagHolder(Team);
			AUTCTFFlagBase* FlagBase = GameState->GetFlagBase(Team);
			AUTCTFFlag* Flag = FlagBase ? Cast<AUTCTFFlag>(FlagBase->GetCarriedObject()) : nullptr;
			if (TeamInfo && FlagBase && Flag && (Flag->ObjectState != CarriedObjectState::Delivered))
			{
				DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, TeamInfo->GetTeamNum(), TeamPositions[Team], FlagBase, Flag, FlagHolder);
				DrawFlagWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamInfo->GetTeamNum(), FlagBase, Flag, FlagHolder);
				DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamInfo->GetTeamNum(), FlagBase, Flag, FlagHolder);
			}
		}
	}
}

void UUTHUDWidget_CTFFlagStatus::DrawFlagStatus(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, FVector2D IndicatorPosition, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	// draw flag state in HUD
	float XPos = IndicatorPosition.X;
	float YPos = (8.f * RenderScale) + 0.5f * FlagIconTemplate.GetHeight();
	FlagIconTemplate.RenderColor = (GameState && GameState->Teams.IsValidIndex(TeamNum) && GameState->Teams[TeamNum] != nullptr) ? GameState->Teams[TeamNum]->TeamColor : FLinearColor::Green;

	// Draw the upper indicator
	if (Flag)
	{
		float FlagStatusScale = StatusScale;
		float AppliedStatusScale = 1.f;
		if (TeamNum < 2)
		{
			if (OldFlagState[TeamNum] != Flag->ObjectState)
			{
				CurrentStatusScale[TeamNum] = StatusChangedScale;
				OldFlagState[TeamNum] = Flag->ObjectState;
			}
			AppliedStatusScale = CurrentStatusScale[TeamNum];
			FlagStatusScale *= AppliedStatusScale;
		}
		if (Flag->ObjectState == CarriedObjectState::Held)
		{
			YPos += 0.5f * AppliedStatusScale * FlagIconTemplate.GetHeight();
			TakenIconTemplate.RenderColor = 0.8f * FLinearColor::Yellow;
			RenderObj_TextureAt(TakenIconTemplate, XPos + 0.1f * FlagIconTemplate.GetWidth(), YPos + 0.1f * FlagIconTemplate.GetHeight(), 1.1f * FlagStatusScale * TakenIconTemplate.GetWidth(), 1.1f * FlagStatusScale * TakenIconTemplate.GetHeight());
		
			if (FlagHolder)
			{
				FlagHolderNameTemplate.Text = ((FlagHolder == UTHUDOwner->UTPlayerOwner->UTPlayerState) && !bAlwaysDrawFlagHolderName) ? YouHaveFlagText : FText::FromString(FlagHolder->PlayerName);
				RenderObj_Text(FlagHolderNameTemplate, IndicatorPosition);
			}

			float CarriedX = XPos - 0.25f * FlagIconTemplate.GetWidth() * FlagStatusScale;
			float CarriedY = YPos - 0.25f * FlagIconTemplate.GetHeight() * FlagStatusScale;

			RenderObj_TextureAt(FlagIconTemplate, CarriedX, CarriedY, FlagStatusScale * FlagIconTemplate.GetWidth(), FlagStatusScale * FlagIconTemplate.GetHeight());
		}
		else
		{
			YPos += 0.5f * AppliedStatusScale * DroppedIconTemplate.GetHeight();
			RenderObj_TextureAt(FlagIconTemplate, XPos, YPos, 1.5f*AppliedStatusScale*FlagIconTemplate.GetWidth(), 1.5f*AppliedStatusScale*FlagIconTemplate.GetHeight());
			if (Flag->ObjectState == CarriedObjectState::Dropped)
			{
				RenderObj_TextureAt(DroppedIconTemplate, XPos, YPos, 1.5f*AppliedStatusScale*DroppedIconTemplate.GetWidth(), 1.5f*AppliedStatusScale *DroppedIconTemplate.GetHeight());
				UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
				DrawText(GetFlagReturnTime(Flag), XPos, YPos, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 2.f*AppliedStatusScale, 1.f, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
			}
		}
	}
}

void UUTHUDWidget_CTFFlagStatus::DrawFlagWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	if (Flag)
	{
		bScaleByDesignedResolution = false;

		bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
		bool bIsEnemyFlag = !GameState->OnSameTeam(Flag, UTPlayerOwner);

		FlagIconTemplate.RenderColor = GameState->Teams.IsValidIndex(TeamNum) ? GameState->Teams[TeamNum]->TeamColor : FLinearColor::Green;
		CameraIconTemplate.RenderColor = FlagIconTemplate.RenderColor;

		// Draw the flag / flag base in the world
		float Dist = (Flag->GetActorLocation() - PlayerViewPoint).Size();
		float WorldRenderScale = RenderScale * MaxIconScale;

		AUTCharacter* Holder = NULL;

		bool bDrawInWorld = false;
		bool bDrawEdgeArrow = false;

		FVector DrawScreenPosition(0.f);
		FVector WorldPosition = Flag->GetActorLocation() + Flag->GetActorRotation().RotateVector(Flag->HomeBaseOffset) + FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 3.f );

		float OldFlagAlpha = FlagIconTemplate.RenderOpacity;
		float CurrentWorldAlpha = InWorldAlpha;

		FVector ViewDir = PlayerViewRotation.Vector();

		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;
		bool bShouldDrawFlagIcon = ShouldDrawFlag(Flag, bIsEnemyFlag);

		if ((bSpectating || bShouldDrawFlagIcon) && (Flag->Holder != UTHUDOwner->GetScorerPlayerState()) && (Flag->ObjectState != CarriedObjectState::Home))
		{
			WorldPosition = Flag->GetActorLocation();
			if (Flag->ObjectState == CarriedObjectState::Held)
			{
				Holder = Cast<AUTCharacter>(Flag->GetAttachmentReplication().AttachParent);
				if (Holder)
				{
					WorldPosition = Holder->GetMesh()->GetComponentLocation() + FVector(0.f, 0.f, Holder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f);
				}
			}
			else
			{
				WorldPosition += FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 0.75f);
			}
			bDrawInWorld = true;
			DrawScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);
		}
		else
		{
			DrawScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);
		}

		// Look to see if we should be displaying the in-world indicator for the flag.
		float CurrentWorldTime = GameState->GetWorld()->GetTimeSeconds();
		if (bIsEnemyFlag)
		{
			if (bDrawInWorld && !bEnemyFlagWasDrawn)
			{
				EnemyFlagStartDrawTime = CurrentWorldTime;
			}
			if (CurrentWorldTime - EnemyFlagStartDrawTime < 0.3f)
			{
				WorldRenderScale *= (1.f + 3.f * (0.3f - CurrentWorldTime + EnemyFlagStartDrawTime));
			}
			bEnemyFlagWasDrawn = bDrawInWorld;
		}

		if (bDrawInWorld)
		{
			float PctFromCenter = (DrawScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
			CurrentWorldAlpha = InWorldAlpha * FMath::Min(6.f*PctFromCenter, 1.f);

			float ViewDist = (PlayerViewPoint - WorldPosition).Size();

			// don't overlap player beacon
			UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
			float X, Y;
			float Scale = Canvas->ClipX / 1920.f;
			Canvas->TextSize(TinyFont, FString("+999   A999"), X, Y, Scale, Scale);
			if (!bDrawEdgeArrow)
			{
				float MinDistSq = FMath::Square(0.06f*GetCanvas()->ClipX);
				float ActualDistSq = FMath::Square(DrawScreenPosition.X - 0.5f*GetCanvas()->ClipX) + FMath::Square(DrawScreenPosition.Y - 0.5f*GetCanvas()->ClipY);
				if (ActualDistSq > MinDistSq)
				{
					if (!Holder || (ViewDist < Holder->TeamPlayerIndicatorMaxDistance))
					{
						DrawScreenPosition.Y -= 3.5f*Y;
					}
					else
					{
						DrawScreenPosition.Y -= (ViewDist < Holder->SpectatorIndicatorMaxDistance) ? 2.5f*Y : 1.5f*Y;
					}
				}
			}
			DrawScreenPosition.X -= RenderPosition.X;
			DrawScreenPosition.Y -= RenderPosition.Y;

			FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
			CircleTemplate.RenderOpacity = CurrentWorldAlpha;
			CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;

			float InWorldFlagScale = WorldRenderScale;
			RenderObj_TextureAt(CircleTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleTemplate.GetWidth()* InWorldFlagScale, CircleTemplate.GetHeight()* InWorldFlagScale);
			RenderObj_TextureAt(CircleBorderTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleBorderTemplate.GetWidth()* InWorldFlagScale, CircleBorderTemplate.GetHeight()* InWorldFlagScale);
			
			if (bDrawEdgeArrow)
			{
				DrawEdgeArrow(WorldPosition, DrawScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
			}
			else
			{
				FText FlagStatusMessage = Flag->GetHUDStatusMessage(UTHUDOwner);
				if (!FlagStatusMessage.IsEmpty())
				{
					DrawText(FlagStatusMessage, DrawScreenPosition.X, DrawScreenPosition.Y - ((CircleTemplate.GetHeight() + 40) * WorldRenderScale), AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*WorldRenderScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);				
				}
			}
			if (Flag && Flag->ObjectState == CarriedObjectState::Held)
			{
				TakenIconTemplate.RenderOpacity = CurrentWorldAlpha;
				RenderObj_TextureAt(TakenIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.1f * TakenIconTemplate.GetWidth()* InWorldFlagScale, 1.1f * TakenIconTemplate.GetHeight()* InWorldFlagScale);
				RenderObj_TextureAt(FlagIconTemplate, DrawScreenPosition.X - 0.25f * FlagIconTemplate.GetWidth()* InWorldFlagScale, DrawScreenPosition.Y - 0.25f * FlagIconTemplate.GetHeight()* InWorldFlagScale, FlagIconTemplate.GetWidth()* InWorldFlagScale, FlagIconTemplate.GetHeight()* InWorldFlagScale);
			}
			else
			{
				RenderObj_TextureAt(FlagIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.25f*FlagIconTemplate.GetWidth()* WorldRenderScale, 1.25f*FlagIconTemplate.GetHeight()* WorldRenderScale);

				if (Flag->ObjectState == CarriedObjectState::Dropped)
				{
					float DroppedAlpha = DroppedIconTemplate.RenderOpacity;
					DroppedIconTemplate.RenderOpacity = CurrentWorldAlpha;
					RenderObj_TextureAt(DroppedIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, 1.25f*DroppedIconTemplate.GetWidth()* InWorldFlagScale, 1.25f*DroppedIconTemplate.GetHeight()* InWorldFlagScale);
					DroppedIconTemplate.RenderOpacity = DroppedAlpha;
					DrawText(GetFlagReturnTime(Flag), DrawScreenPosition.X, DrawScreenPosition.Y, TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
				}
			}
		}
		FlagIconTemplate.RenderOpacity = OldFlagAlpha;
		CircleTemplate.RenderOpacity = 1.f;
		CircleBorderTemplate.RenderOpacity = 1.f;

		bScaleByDesignedResolution = true;
	}
}

bool UUTHUDWidget_CTFFlagStatus::ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag)
{
	return (Flag->ObjectState == CarriedObjectState::Dropped) || Flag->bCurrentlyPinged || (bIsEnemyFlag ? Flag->bEnemyCanPickup : Flag->bFriendlyCanPickup);
}

void UUTHUDWidget_CTFFlagStatus::DrawFlagBaseWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder)
{
	if (FlagBase)
	{
		bScaleByDesignedResolution = false;

		FLinearColor TeamColor = FLinearColor::Green;
		if (GameState->Teams.IsValidIndex(TeamNum))
		{
			TeamColor = GameState->Teams[TeamNum]->TeamColor;
		}
	
		FlagIconTemplate.RenderColor = TeamColor;

		float Dist = (FlagBase->GetActorLocation() - PlayerViewPoint).Size();
		float WorldRenderScale = RenderScale * MaxIconScale;

		bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
		bool bIsEnemyFlag = !GameState->OnSameTeam(FlagBase, UTPlayerOwner);
		bool bDrawEdgeArrow = false;
		FVector DrawScreenPosition(0.f);

		AUTCTFFlag* DefaultFlag = AUTCTFFlag::StaticClass()->GetDefaultObject<AUTCTFFlag>();
		FVector WorldPosition = FlagBase->GetActorLocation() + FlagBase->GetActorRotation().RotateVector(DefaultFlag->HomeBaseOffset) + FVector(0.f, 0.f, DefaultFlag->Collision->GetUnscaledCapsuleHalfHeight() * 2.5f);
		float OldFlagAlpha = FlagIconTemplate.RenderOpacity;
		float CurrentWorldAlpha = InWorldAlpha;
		FVector ViewDir = PlayerViewRotation.Vector();
		float Edge = CircleTemplate.GetWidth()* WorldRenderScale;
		DrawScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, TeamNum);
		if (!bDrawEdgeArrow || (Flag && Flag->ObjectState == CarriedObjectState::Home) || !bIsEnemyFlag)
		{
			float PctFromCenter = (DrawScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
			CurrentWorldAlpha = InWorldAlpha * FMath::Min(6.f*PctFromCenter, 1.f);
	
			DrawScreenPosition.X -= RenderPosition.X;
			DrawScreenPosition.Y -= RenderPosition.Y;
	
			FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
			CircleBorderTemplate.RenderOpacity = CurrentWorldAlpha;
			CircleTemplate.RenderOpacity = CurrentWorldAlpha;
				
			RenderObj_TextureAt(CircleTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleTemplate.GetWidth()* WorldRenderScale, CircleTemplate.GetHeight()* WorldRenderScale);
			RenderObj_TextureAt(CircleBorderTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, CircleBorderTemplate.GetWidth()* WorldRenderScale, CircleBorderTemplate.GetHeight()* WorldRenderScale);
			RenderObj_TextureAt(FlagIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, FlagIconTemplate.GetWidth()* WorldRenderScale, FlagIconTemplate.GetHeight()* WorldRenderScale);
	
			if (bDrawEdgeArrow)
			{
				DrawEdgeArrow(WorldPosition, DrawScreenPosition, CurrentWorldAlpha, WorldRenderScale, TeamNum);
			}
			else
			{
				FText BaseMessage = GetBaseMessage(FlagBase, Flag);
				if (!BaseMessage.IsEmpty())
				{
					DrawText(BaseMessage, DrawScreenPosition.X, DrawScreenPosition.Y - ((CircleTemplate.GetHeight() + 40) * WorldRenderScale), AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*WorldRenderScale, 0.5f + 0.5f*CurrentWorldAlpha, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
				}
			}
			if (Flag && Flag->ObjectState != CarriedObjectState::Home)
			{
				RenderObj_TextureAt(FlagGoneIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, FlagGoneIconTemplate.GetWidth()* WorldRenderScale, FlagGoneIconTemplate.GetHeight()* WorldRenderScale);
			}
	
			FlagIconTemplate.RenderOpacity = OldFlagAlpha;
			CircleTemplate.RenderOpacity = 1.f;
			CircleBorderTemplate.RenderOpacity = 1.f;
		}
		bScaleByDesignedResolution = true;
	}
}

void UUTHUDWidget_CTFFlagStatus::DrawStatusMessage(float DeltaTime)
{
	AUTCTFGameState* GS = Cast<AUTCTFGameState>(UTGameState);
	if (bSuppressMessage || GS == NULL) return;

	// Draw the Flag Status Message
	if (GS->IsMatchInProgress() && UTHUDOwner != NULL && UTHUDOwner->PlayerOwner != NULL)
	{
		APawn* ViewedPawn = Cast<APawn>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		AUTPlayerState* ViewedPS = ViewedPawn ? Cast<AUTPlayerState>(ViewedPawn->PlayerState) : NULL;
		AUTPlayerState* OwnerPS = ViewedPS ? ViewedPS : UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (OwnerPS != NULL && OwnerPS->Team != NULL)
		{
			uint8 MyTeamNum = OwnerPS->GetTeamNum();
			uint8 OtherTeamNum = MyTeamNum == 0 ? 1 : 0;
			FText StatusText = (OwnerPS->CarriedObject != NULL) ? YouHaveFlagText : FText::GetEmpty();
			if (!StatusText.IsEmpty())
			{
				AnimationAlpha += (DeltaTime * 3);
		
				float Alpha = FMath::Sin(AnimationAlpha);
				Alpha = FMath::Abs<float>(Alpha);

				FlagStatusText.RenderOpacity = Alpha;
				FlagStatusText.RenderColor = FLinearColor::Yellow;
				FlagStatusText.Text = StatusText;
				FlagStatusText.bDrawShadow = true;
				FlagStatusText.ShadowDirection = FVector2D(1.f, 2.f);
				FlagStatusText.ShadowColor = FLinearColor::Black;
				RenderObj_Text(FlagStatusText);
			}
			else
			{
				AnimationAlpha = 0.0f;
			}
		}
	}	
}

FVector UUTHUDWidget_CTFFlagStatus::GetAdjustedScreenPosition(const FVector& WorldPosition, const FVector& ViewPoint, const FVector& ViewDir, float Dist, float Edge, bool& bDrawEdgeArrow, int32 Team)
{
	FVector Cross = (ViewDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
	FVector DrawScreenPosition;
	float ExtraPadding = 0.1f * Canvas->ClipX;
	DrawScreenPosition = GetCanvas()->Project(WorldPosition);
	FVector FlagDir = WorldPosition - ViewPoint;
	if ((ViewDir | FlagDir) < 0.f)
	{
		bool bWasLeft = (Team == 0) ? bRedWasLeft : bBlueWasLeft;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bWasLeft ? Edge + ExtraPadding : GetCanvas()->ClipX - Edge - ExtraPadding;
		DrawScreenPosition.Y = 0.5f*GetCanvas()->ClipY;
		DrawScreenPosition.Z = 0.0f;
		return DrawScreenPosition;
	}
	else if ((DrawScreenPosition.X < 0.f) || (DrawScreenPosition.X > GetCanvas()->ClipX))
	{
		bool bLeftOfScreen = (DrawScreenPosition.X < 0.f);
		float OffScreenDistance = bLeftOfScreen ? -1.f*DrawScreenPosition.X : DrawScreenPosition.X - GetCanvas()->ClipX;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bLeftOfScreen ? Edge+ ExtraPadding : GetCanvas()->ClipX - Edge - ExtraPadding;
		//Y approaches 0.5*Canvas->ClipY as further off screen
		float MaxOffscreenDistance = GetCanvas()->ClipX;
		DrawScreenPosition.Y = 0.5f*GetCanvas()->ClipY + FMath::Clamp((MaxOffscreenDistance - OffScreenDistance)/ MaxOffscreenDistance, 0.f, 1.f) * (DrawScreenPosition.Y - 0.5f*GetCanvas()->ClipY);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, 0.25f*GetCanvas()->ClipY, 0.75f*GetCanvas()->ClipY);
		if (Team == 0)
		{
			bRedWasLeft = bLeftOfScreen;
		}
		else
		{
			bBlueWasLeft = bLeftOfScreen;
		}
	}
	else
	{
		if (Team == 0)
		{
			bRedWasLeft = false;
		}
		else
		{
			bBlueWasLeft = false;
		}
		DrawScreenPosition.X = FMath::Clamp(DrawScreenPosition.X, Edge, GetCanvas()->ClipX - Edge);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, Edge, GetCanvas()->ClipY - Edge);
		DrawScreenPosition.Z = 0.0f;

		// keep out of center
		float MinDistSq = FMath::Square(0.05f*GetCanvas()->ClipX);
		float ActualDistSq = FMath::Square(DrawScreenPosition.X - 0.5f*GetCanvas()->ClipX) + FMath::Square(DrawScreenPosition.Y - 0.5f*GetCanvas()->ClipY);
		if (ActualDistSq < MinDistSq)
		{
			float Scaling = FMath::Sqrt(MinDistSq / FMath::Max(ActualDistSq, 1.f));
			DrawScreenPosition.X = 0.5f*GetCanvas()->ClipX + (DrawScreenPosition.X - 0.5f*GetCanvas()->ClipX)*Scaling;
			DrawScreenPosition.Y = 0.5f*GetCanvas()->ClipY + (DrawScreenPosition.Y - 0.5f*GetCanvas()->ClipY)*Scaling;
		}
	}
	return DrawScreenPosition;
}

void UUTHUDWidget_CTFFlagStatus::DrawEdgeArrow(FVector InWorldPosition, FVector InDrawScreenPosition, float CurrentWorldAlpha, float WorldRenderScale, int32 Team)
{
	bool bLeftOfScreen = (InDrawScreenPosition.X < 0.f);
	float DroppedAlpha = DroppedIconTemplate.RenderOpacity;
	DroppedIconTemplate.RenderOpacity = CurrentWorldAlpha;
	DroppedIconTemplate.Rotation = bLeftOfScreen ? 90.f : -90.f;
	float ArrowOffsetY = bLeftOfScreen ? 0.5f : -0.5f;
	ArrowOffsetY = ArrowOffsetY * CircleBorderTemplate.GetHeight()* WorldRenderScale + 0.5f*DroppedIconTemplate.GetHeight();
	RenderObj_TextureAt(DroppedIconTemplate, InDrawScreenPosition.X + 0.5f*CircleTemplate.GetWidth()* WorldRenderScale, InDrawScreenPosition.Y + ArrowOffsetY, 2.f*DroppedIconTemplate.GetWidth()* WorldRenderScale, 2.f*DroppedIconTemplate.GetHeight()* WorldRenderScale);
	DroppedIconTemplate.Rotation = 0.f;
	DroppedIconTemplate.RenderOpacity = DroppedAlpha;
}
/*
	FVector OffScreenPosition = GetCanvas()->Project(InWorldPosition);

	ArrowTemplate.Atlas = UTHUDOwner->HUDAtlas;
	ArrowTemplate.UVs = FTextureUVs(100.f, 836.f, 72.f, 96.f);
	ArrowTemplate.RenderOffset = FVector2D(0.5f, 0.625f);
	ArrowTemplate.RotPivot =  FVector2D(0.5f, 0.625f);
	ArrowTemplate.RenderScale = 1.05f;
	FVector Dir = (OffScreenPosition - InWorldPosition).GetSafeNormal();
	FRotator Rot = Dir.Rotation();
	RenderObj_TextureAtWithRotation(ArrowTemplate, FVector2D(InDrawScreenPosition.X, InDrawScreenPosition.Y), Rot.Yaw);
}*/

FText UUTHUDWidget_CTFFlagStatus::GetFlagReturnTime(AUTCTFFlag* Flag)
{
	return Flag ? FText::AsNumber(Flag->FlagReturnTime) : FText::GetEmpty();
}

FText UUTHUDWidget_CTFFlagStatus::GetBaseMessage(AUTCTFFlagBase* Base, AUTCTFFlag* Flag)
{
	return Base->GetHUDStatusMessage(UTHUDOwner);
}