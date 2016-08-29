// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_DetectedWarning.h"
#include "UTCTFGameState.h"
#include "UTSecurityCameraComponent.h"

UUTHUDWidget_DetectedWarning::UUTHUDWidget_DetectedWarning(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DetectedText = NSLOCTEXT("DetectedWarning","DetectedText","Detected");

	ScreenPosition = FVector2D(0.0f, 0.0f);
	Size = FVector2D(0.f, 0.f);
	Origin = FVector2D(0.5f, 0.5f);
	AnimationAlpha = 0.0f;
	
	ScalingStartDist = 4000.f;
	ScalingEndDist = 15000.f;
	MaxIconScale = 1.f;
	MinIconScale = 0.75f;
	bSuppressMessage = false;
}

void UUTHUDWidget_DetectedWarning::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_DetectedWarning::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* GameState = Cast<AUTCTFGameState>(UTGameState);
	if (GameState == NULL) return;
	if (!IsLocalPlayerFlagCarrier()) return;

	FVector ViewPoint;
	FRotator ViewRotation;

	AdjustRenderScaleForAnimation();

	UTPlayerOwner->GetPlayerViewPoint(ViewPoint, ViewRotation);
	DrawDetectedNoticeWorld(GameState, ViewPoint, ViewRotation);
}

void UUTHUDWidget_DetectedWarning::AdjustRenderScaleForAnimation()
{
	float Sin = FMath::Sin(GetWorld()->GetTimeSeconds() * PulseSpeed);
	if (Sin < 0.f)
	{
		Sin *= -1.f;
	}
	RenderScale = FMath::Clamp(Sin, 0.f, 1.f) + MinPulseRenderScale;
}

bool UUTHUDWidget_DetectedWarning::IsLocalPlayerFlagCarrier() const
{
	AUTCTFGameState* GameState = GetWorld()->GetGameState<AUTCTFGameState>();
	//Check both team's flag carriers and see if the owner of this HUD widget is either of them.
	return (GameState && UTPlayerOwner && UTPlayerOwner->UTPlayerState && ((GameState->GetFlagHolder(0) == UTPlayerOwner->UTPlayerState) || (GameState->GetFlagHolder(1) == UTPlayerOwner->UTPlayerState)));
}

void UUTHUDWidget_DetectedWarning::DrawDetectedNoticeWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation)
{
	if ((UTPlayerOwner == nullptr) || (UTPlayerOwner->UTPlayerState == nullptr) || (UTPlayerOwner->UTPlayerState->CarriedObject == nullptr))
	{
		return;
	}

	const AUTCTFFlag* CTFFlag = Cast<AUTCTFFlag>(UTPlayerOwner->UTPlayerState->CarriedObject);
	const UUTSecurityCameraComponent* DetectingCamera = UTPlayerOwner->UTPlayerState->CarriedObject->GetDetectingCamera();
	if (CTFFlag && CTFFlag->bCurrentlyPinged && DetectingCamera)
	{
		DrawDetectedCameraNotice(GameState, PlayerViewPoint, PlayerViewRotation, DetectingCamera);
	}
}

void UUTHUDWidget_DetectedWarning::DrawDetectedCameraNotice(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, const UUTSecurityCameraComponent* DetectingCamera)
{
	bScaleByDesignedResolution = false;

	float Dist = (DetectingCamera->K2_GetComponentLocation() - PlayerViewPoint).Size();
	float WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);

	FVector WorldPosition = DetectingCamera->K2_GetComponentLocation();
	FVector DrawScreenPosition = GetCanvas()->Project(WorldPosition);

	FVector ViewDir = PlayerViewRotation.Vector();
	FVector CameraDir = WorldPosition - PlayerViewPoint;
	
	AdjustScreenPosition(DrawScreenPosition, WorldPosition, ViewDir, CameraDir);

	RenderObj_TextureAt(WarningIconTemplate, DrawScreenPosition.X, DrawScreenPosition.Y, WarningIconTemplate.UVs.UL * WorldRenderScale, WarningIconTemplate.UVs.VL * WorldRenderScale);
}

void UUTHUDWidget_DetectedWarning::AdjustScreenPosition( /*OUT*/ FVector& OutScreenPosition, const FVector& WorldPosition, const FVector& ViewDir, const FVector& CameraDir)
{
	const float EdgeOffsetX = EdgeOffsetPercentX * GetCanvas()->ClipX;
	const float MinEdgeOffsetY = (GetCanvas()->ClipY * MinEdgeOffsetPercentY) + WarningIconTemplate.UVs.VL;
	const float MaxEdgeOffsetY = (GetCanvas()->ClipY * MaxEdgeOffsetPercentY) - WarningIconTemplate.UVs.VL;
	
	//Check if camera is behind us
	if ((ViewDir | CameraDir) < 0.f)
	{
		const float HalfScreen = GetCanvas()->ClipX / 2;
		const bool bIsLeft = ((UTPlayerOwner->GetActorRightVector() | CameraDir) < 0);

		//Force X position to side of the screen
		OutScreenPosition.X = bIsLeft ? EdgeOffsetX : GetCanvas()->ClipX - EdgeOffsetX;

		//if the camera is behind us the Y direction is flipped. Lets adjust that
		OutScreenPosition.Y = FMath::Clamp(OutScreenPosition.Y, MinEdgeOffsetY, MaxEdgeOffsetY);
		const float InvertedYScreenPercent = FMath::Clamp(((MaxEdgeOffsetY - OutScreenPosition.Y) / MaxEdgeOffsetY), 0.f, 1.f);
		OutScreenPosition.Y = GetCanvas()->ClipY * InvertedYScreenPercent;
	}
	
	OutScreenPosition.X = FMath::Clamp(OutScreenPosition.X, EdgeOffsetX, GetCanvas()->ClipX - EdgeOffsetX);
	OutScreenPosition.Y = FMath::Clamp(OutScreenPosition.Y, MinEdgeOffsetY, MaxEdgeOffsetY);
	OutScreenPosition.Z = 0.0f;
}