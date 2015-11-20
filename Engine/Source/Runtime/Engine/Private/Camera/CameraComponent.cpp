// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "Camera/CameraComponent.h"
#include "Components/DrawFrustumComponent.h"

#define LOCTEXT_NAMESPACE "CameraComponent"

static void SetDeprecatedControllerViewRotation(UCameraComponent& Component, bool bValue);


//////////////////////////////////////////////////////////////////////////
// UCameraComponent

UCameraComponent::UCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
		CameraMesh = EditorCameraMesh.Object;
	}
#endif

	FieldOfView = 90.0f;
	AspectRatio = 1.777778f;
	OrthoWidth = 512.0f;
	OrthoNearClipPlane = 0.0f;
	OrthoFarClipPlane = WORLD_MAX;
	bConstrainAspectRatio = false;
	bUseFieldOfViewForLOD = true;
	PostProcessBlendWeight = 1.0f;
	bUseControllerViewRotation_DEPRECATED = true; // the previous default value before bUsePawnControlRotation replaced this var.
	bUsePawnControlRotation = false;
	bAutoActivate = true;

	// Init deprecated var, for old code that may refer to it.
	SetDeprecatedControllerViewRotation(*this, bUsePawnControlRotation);
}

#if WITH_EDITORONLY_DATA
void UCameraComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();

	if (ProxyMeshComponent)
	{
		ProxyMeshComponent->DestroyComponent();
	}
	if (DrawFrustum)
	{
		DrawFrustum->DestroyComponent();
	}
}
#endif

void UCameraComponent::OnRegister()
{
#if WITH_EDITORONLY_DATA
	if (ProxyMeshComponent == NULL)
	{
		ProxyMeshComponent = NewObject<UStaticMeshComponent>(GetOuter(), NAME_None, RF_Transactional);
		ProxyMeshComponent->AttachTo(this);
		ProxyMeshComponent->AlwaysLoadOnClient = false;
		ProxyMeshComponent->AlwaysLoadOnServer = false;
		ProxyMeshComponent->StaticMesh = CameraMesh;
		ProxyMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		ProxyMeshComponent->bHiddenInGame = true;
		ProxyMeshComponent->CastShadow = false;
		ProxyMeshComponent->PostPhysicsComponentTick.bCanEverTick = false;
		ProxyMeshComponent->CreationMethod = CreationMethod;
		ProxyMeshComponent->RegisterComponentWithWorld(GetWorld());
	}

	if (DrawFrustum == NULL)
	{
		DrawFrustum = NewObject<UDrawFrustumComponent>(GetOuter(), NAME_None, RF_Transactional);
		DrawFrustum->AttachTo(this);
		DrawFrustum->AlwaysLoadOnClient = false;
		DrawFrustum->AlwaysLoadOnServer = false;
		DrawFrustum->CreationMethod = CreationMethod;
		DrawFrustum->RegisterComponentWithWorld(GetWorld());
	}

	RefreshVisualRepresentation();
#endif

	Super::OnRegister();

	// Init deprecated var, for old code that may refer to it.
	SetDeprecatedControllerViewRotation(*this, bUsePawnControlRotation);
}

void UCameraComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_EDITORONLY_DATA
	// have to removed the sub-components that we added in OnRegister (for 
	// reinstancing, where we CopyPropertiesForUnrelatedObjects()... don't want
	// these copied since we'll generate them on the next OnRegister)
	if (ProxyMeshComponent != NULL)
	{
		ProxyMeshComponent->DetachFromParent();
		ProxyMeshComponent->DestroyComponent();
		ProxyMeshComponent = NULL;
	}

	if (DrawFrustum != NULL)
	{
		DrawFrustum->DetachFromParent();
		DrawFrustum->DestroyComponent();
		DrawFrustum = NULL;
	}
#endif
}


void UCameraComponent::PostLoad()
{
	Super::PostLoad();

	const int32 LinkerUE4Ver = GetLinkerUE4Version();

	if (LinkerUE4Ver < VER_UE4_RENAME_CAMERA_COMPONENT_VIEW_ROTATION)
	{
		bUsePawnControlRotation = bUseControllerViewRotation_DEPRECATED;
	}

	// Init deprecated var, for old code that may refer to it.
	SetDeprecatedControllerViewRotation(*this, bUsePawnControlRotation);
}

#if WITH_EDITORONLY_DATA

 void UCameraComponent::SetCameraMesh(UStaticMesh* Mesh)
 {
	 if (Mesh != CameraMesh)
	 {
		 CameraMesh = Mesh;

		 if (ProxyMeshComponent)
		 {
			 ProxyMeshComponent->SetStaticMesh(Mesh);
		 }
	 }
 }
void UCameraComponent::RefreshVisualRepresentation()
{
	if (DrawFrustum != NULL)
	{
		const float FrustumDrawDistance = 1000.0f;
		if (ProjectionMode == ECameraProjectionMode::Perspective)
		{
			DrawFrustum->FrustumAngle = FieldOfView;
			DrawFrustum->FrustumStartDist = 10.f;
			DrawFrustum->FrustumEndDist = DrawFrustum->FrustumStartDist + FrustumDrawDistance;
		}
		else
		{
			DrawFrustum->FrustumAngle = -OrthoWidth;
			DrawFrustum->FrustumStartDist = OrthoNearClipPlane;
			DrawFrustum->FrustumEndDist = FMath::Min(OrthoFarClipPlane - OrthoNearClipPlane, FrustumDrawDistance);
		}
		DrawFrustum->FrustumAspectRatio = AspectRatio;
		DrawFrustum->MarkRenderStateDirty();
	}
}

void UCameraComponent::OverrideFrustumColor(FColor OverrideColor)
{
	if (DrawFrustum != NULL)
	{
		DrawFrustum->FrustumColor = OverrideColor;
	}
}

void UCameraComponent::RestoreFrustumColor()
{
	if (DrawFrustum != NULL)
	{
		//@TODO: 
		const FColor DefaultFrustumColor(255, 0, 255, 255);
		DrawFrustum->FrustumColor = DefaultFrustumColor;
		//ACameraActor* DefCam = Cam->GetClass()->GetDefaultObject<ACameraActor>();
		//Cam->DrawFrustum->FrustumColor = DefCam->DrawFrustum->FrustumColor;
	}
}
#endif	// WITH_EDITORONLY_DATA

#if WITH_EDITOR
void UCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if WITH_EDITORONLY_DATA
	RefreshVisualRepresentation();
#endif	// WITH_EDITORONLY_DATA
}
#endif	// WITH_EDITOR

void UCameraComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		PostProcessSettings.OnAfterLoad();
	}
}

void UCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	if (bUsePawnControlRotation)
	{
		if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
		{
			const FRotator PawnViewRotation = OwningPawn->GetViewRotation();
			if (!PawnViewRotation.Equals(GetComponentRotation()))
			{
				SetWorldRotation(PawnViewRotation);
			}
		}
	}

	DesiredView.Location = GetComponentLocation();
	DesiredView.Rotation = GetComponentRotation();

	DesiredView.FOV = FieldOfView;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;

	// See if the CameraActor wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

#if WITH_EDITOR
void UCameraComponent::CheckForErrors()
{
	Super::CheckForErrors();

	if (AspectRatio <= 0.0f)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_CameraAspectRatioIsZero", "Camera has AspectRatio=0 - please set this to something non-zero" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::CameraAspectRatioIsZero));
	}
}
#endif	// WITH_EDITOR


void SetDeprecatedControllerViewRotation(UCameraComponent& Component, bool bValue)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Component.bUseControllerViewRotation = bValue;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


#undef LOCTEXT_NAMESPACE

