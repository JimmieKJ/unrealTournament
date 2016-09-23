// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/

#include "EnginePrivate.h"
#include "../../Renderer/Private/ScenePrivate.h"
#include "Engine/SceneCapture.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/DrawFrustumComponent.h"
#include "Engine/PlanarReflection.h"
#include "Components/PlanarReflectionComponent.h"
#include "PlanarReflectionSceneProxy.h"
#include "Components/BoxComponent.h"
#include "MessageLog.h"
#include "RenderingObjectVersion.h"

#define LOCTEXT_NAMESPACE "SceneCaptureComponent"

ASceneCapture::ASceneCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CamMesh0"));

	MeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	MeshComp->bHiddenInGame = true;
	MeshComp->CastShadow = false;
	MeshComp->PostPhysicsComponentTick.bCanEverTick = false;
	RootComponent = MeshComp;
}
// -----------------------------------------------

ASceneCapture2D::ASceneCapture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawFrustum = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("DrawFrust0"));
	DrawFrustum->AlwaysLoadOnClient = false;
	DrawFrustum->AlwaysLoadOnServer = false;
	DrawFrustum->SetupAttachment(GetMeshComp());

	CaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("NewSceneCaptureComponent2D"));
	CaptureComponent2D->SetupAttachment(GetMeshComp());
}

void ASceneCapture2D::OnInterpToggle(bool bEnable)
{
	CaptureComponent2D->SetVisibility(bEnable);
}

void ASceneCapture2D::UpdateDrawFrustum()
{
	if(DrawFrustum && CaptureComponent2D)
	{
		DrawFrustum->FrustumStartDist = GNearClippingPlane;
		
		// 1000 is the default frustum distance, ideally this would be infinite but that might cause rendering issues
		DrawFrustum->FrustumEndDist = (CaptureComponent2D->MaxViewDistanceOverride > DrawFrustum->FrustumStartDist)
			? CaptureComponent2D->MaxViewDistanceOverride : 1000.0f;

		DrawFrustum->FrustumAngle = CaptureComponent2D->FOVAngle;
		//DrawFrustum->FrustumAspectRatio = CaptureComponent2D->AspectRatio;
	}
}

void ASceneCapture2D::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->StaticMesh)
			{
				UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(CamMesh);
			}
		}
	}
#endif

	// Sync component with CameraActor frustum settings.
	UpdateDrawFrustum();
}
// -----------------------------------------------

ASceneCaptureCube::ASceneCaptureCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawFrustum = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("DrawFrust0"));
	DrawFrustum->AlwaysLoadOnClient = false;
	DrawFrustum->AlwaysLoadOnServer = false;
	DrawFrustum->SetupAttachment(GetMeshComp());

	CaptureComponentCube = CreateDefaultSubobject<USceneCaptureComponentCube>(TEXT("NewSceneCaptureComponentCube"));
	CaptureComponentCube->SetupAttachment(GetMeshComp());
}

void ASceneCaptureCube::OnInterpToggle(bool bEnable)
{
	CaptureComponentCube->SetVisibility(bEnable);
}

void ASceneCaptureCube::UpdateDrawFrustum()
{
	if(DrawFrustum && CaptureComponentCube)
	{
		DrawFrustum->FrustumStartDist = GNearClippingPlane;

		// 1000 is the default frustum distance, ideally this would be infinite but that might cause rendering issues
		DrawFrustum->FrustumEndDist = (CaptureComponentCube->MaxViewDistanceOverride > DrawFrustum->FrustumStartDist)
			? CaptureComponentCube->MaxViewDistanceOverride : 1000.0f;

		DrawFrustum->FrustumAngle = 90;
	}
}

void ASceneCaptureCube::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->StaticMesh)
			{
				UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(CamMesh);
			}
		}
	}
#endif

	// Sync component with CameraActor frustum settings.
	UpdateDrawFrustum();
}
#if WITH_EDITOR

void ASceneCaptureCube::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if(bFinished && CaptureComponentCube->bCaptureOnMovement)
	{
		CaptureComponentCube->CaptureSceneDeferred();
	}
}
#endif
// -----------------------------------------------
USceneCaptureComponent::USceneCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ShowFlags(FEngineShowFlags(ESFIM_Game))
{
	bCaptureEveryFrame = true;
	bCaptureOnMovement = true;
	MaxViewDistanceOverride = -1;

	// Disable features that are not desired when capturing the scene
	ShowFlags.SetMotionBlur(0); // motion blur doesn't work correctly with scene captures.
	ShowFlags.SetSeparateTranslucency(0);
	ShowFlags.SetHMDDistortion(0);

    CaptureStereoPass = EStereoscopicPass::eSSP_FULL;
}

void USceneCaptureComponent::PostLoad()
{
	Super::PostLoad();

	// Make sure any loaded saved flag settings are reflected in our FEngineShowFlags
	UpdateShowFlags();
}

void USceneCaptureComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USceneCaptureComponent* This = CastChecked<USceneCaptureComponent>(InThis);

	FSceneViewStateInterface* Ref = This->ViewState.GetReference();

	if (Ref)
	{
		Ref->AddReferencedObjects(Collector);
	}

	Super::AddReferencedObjects(This, Collector);
}

void USceneCaptureComponent::HideComponent(UPrimitiveComponent* InComponent)
{
	if (InComponent)
	{
		HiddenComponents.AddUnique(InComponent);
	}
}

void USceneCaptureComponent::HideActorComponents(AActor* InActor)
{
	if (InActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor->GetComponents(PrimitiveComponents);
		for (int32 ComponentIndex = 0, NumComponents = PrimitiveComponents.Num(); ComponentIndex < NumComponents; ++ComponentIndex)
		{
			HiddenComponents.AddUnique(PrimitiveComponents[ComponentIndex]);
		}
	}
}

void USceneCaptureComponent::ShowOnlyComponent(UPrimitiveComponent* InComponent)
{
	if (InComponent)
	{
		ShowOnlyComponents.Add(InComponent);
	}
}

void USceneCaptureComponent::ShowOnlyActorComponents(AActor* InActor)
{
	if (InActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor->GetComponents(PrimitiveComponents);
		for (int32 ComponentIndex = 0, NumComponents = PrimitiveComponents.Num(); ComponentIndex < NumComponents; ++ComponentIndex)
		{
			ShowOnlyComponents.Add(PrimitiveComponents[ComponentIndex]);
		}
	}
}

FSceneViewStateInterface* USceneCaptureComponent::GetViewState()
{
	FSceneViewStateInterface* ViewStateInterface = ViewState.GetReference();
	if (bCaptureEveryFrame && ViewStateInterface == NULL)
	{
		ViewState.Allocate();
		ViewStateInterface = ViewState.GetReference();
	}
	else if (!bCaptureEveryFrame && ViewStateInterface)
	{
		ViewState.Destroy();
		ViewStateInterface = NULL;
	}
	return ViewStateInterface;
}

void USceneCaptureComponent::UpdateShowFlags()
{
	for (FEngineShowFlagsSetting ShowFlagSetting : ShowFlagSettings)
	{
		int32 SettingIndex = ShowFlags.FindIndexByName(*(ShowFlagSetting.ShowFlagName));
		if (SettingIndex != INDEX_NONE)
		{ 
			ShowFlags.SetSingleFlag(SettingIndex, ShowFlagSetting.Enabled);
		}
	}
}

#if WITH_EDITOR
void USceneCaptureComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// If our ShowFlagSetting UStruct changed, (or if PostEditChange was called without specifying a property) update the actual show flags
	if (MemberPropertyName.IsEqual("ShowFlagSettings") || MemberPropertyName.IsNone())
	{
		UpdateShowFlags();
	}
}
#endif

bool USceneCaptureComponent::GetSettingForShowFlag(FString FlagName, FEngineShowFlagsSetting** ShowFlagSettingOut)
{
	bool HasSetting = false;
	for (int32 ShowFlagSettingsIndex = 0; ShowFlagSettingsIndex < ShowFlagSettings.Num(); ++ShowFlagSettingsIndex)
	{
		if (ShowFlagSettings[ShowFlagSettingsIndex].ShowFlagName.Equals(FlagName))
		{
			HasSetting = true;
			*ShowFlagSettingOut = &(ShowFlagSettings[ShowFlagSettingsIndex]);
			break;
		}
	}	
	return HasSetting;
}

// -----------------------------------------------


USceneCaptureComponent2D::USceneCaptureComponent2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FOVAngle = 90.0f;
	OrthoWidth = 512;
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;
	// previous behavior was to capture from raw scene color 
	CaptureSource = SCS_SceneColorHDR;
	// default to full blend weight..
	PostProcessBlendWeight = 1.0f;
	CaptureStereoPass = EStereoscopicPass::eSSP_FULL;
}

void USceneCaptureComponent2D::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Update content on register to have at least one frames worth of good data.
	// Without updating here this component would not work in a blueprint construction script which recreates the component after each move in the editor
	CaptureSceneDeferred();
#endif
}

void USceneCaptureComponent2D::SendRenderTransform_Concurrent()
{	
	if (bCaptureOnMovement)
	{
		CaptureSceneDeferred();
	}

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponent2D::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		CaptureSceneDeferred();
	}
}

static TMultiMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<USceneCaptureComponent2D>> SceneCapturesToUpdateMap;

void USceneCaptureComponent2D::CaptureSceneDeferred()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		// Needs some CS because of parallel updates.
		static FCriticalSection CriticalSection;
		FScopeLock ScopeLock(&CriticalSection);
		SceneCapturesToUpdateMap.AddUnique(World, this);
	}	
}

void USceneCaptureComponent2D::CaptureScene()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
		World->SendAllEndOfFrameUpdates();
		World->Scene->UpdateSceneCaptureContents(this);
	}	

	if (bCaptureEveryFrame)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("CaptureScene", "CaptureScene: Scene capture with bCaptureEveryFrame enabled was told to update - major inefficiency."));
	}
}

void USceneCaptureComponent2D::UpdateDeferredCaptures( FSceneInterface* Scene )
{
	UWorld* World = Scene->GetWorld();
	if( World && SceneCapturesToUpdateMap.Num() > 0 )
	{
		// Only update the scene captures associated with the current scene.
		// Updating others not associated with the scene would cause invalid data to be rendered into the target
		TArray< TWeakObjectPtr<USceneCaptureComponent2D> > SceneCapturesToUpdate;
		SceneCapturesToUpdateMap.MultiFind( World, SceneCapturesToUpdate );
		
		for( TWeakObjectPtr<USceneCaptureComponent2D> Component : SceneCapturesToUpdate )
		{
			if( Component.IsValid() )
			{
				Scene->UpdateSceneCaptureContents( Component.Get() );
			}
		}
		
		// All scene captures for this world have been updated
		SceneCapturesToUpdateMap.Remove( World );
	}
}

#if WITH_EDITOR

bool USceneCaptureComponent2D::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, FOVAngle))
		{
			return ProjectionType == ECameraProjectionMode::Perspective;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, OrthoWidth))
		{
			return ProjectionType == ECameraProjectionMode::Orthographic;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneCaptureComponent2D, CompositeMode))
		{
			return CaptureSource == SCS_SceneColorHDR;
		}
	}

	return true;
}

void USceneCaptureComponent2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CaptureSceneDeferred();
}
#endif // WITH_EDITOR

void USceneCaptureComponent2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		PostProcessSettings.OnAfterLoad();
	}
}

// -----------------------------------------------

APlanarReflection::APlanarReflection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowPreviewPlane = true;

	PlanarReflectionComponent = CreateDefaultSubobject<UPlanarReflectionComponent>(TEXT("NewPlanarReflectionComponent"));
	RootComponent = PlanarReflectionComponent;

	UBoxComponent* DrawInfluenceBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DrawBox0"));
	DrawInfluenceBox->SetupAttachment(PlanarReflectionComponent);
	DrawInfluenceBox->bUseEditorCompositing = true;
	DrawInfluenceBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlanarReflectionComponent->PreviewBox = DrawInfluenceBox;

	GetMeshComp()->SetWorldRotation(FRotator(0, 0, 0));
	GetMeshComp()->SetWorldScale3D(FVector(4, 4, 1));
	GetMeshComp()->SetupAttachment(PlanarReflectionComponent);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			FName NAME_ReflectionCapture;
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics()
				: NAME_ReflectionCapture(TEXT("ReflectionCapture"))
				, DecalTexture(TEXT("/Engine/EditorResources/S_ReflActorIcon"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bAbsoluteScale = true;
		SpriteComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif
}

void APlanarReflection::OnInterpToggle(bool bEnable)
{
	PlanarReflectionComponent->SetVisibility(bEnable);
}

void APlanarReflection::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor
#if WITH_EDITOR
	if(GetMeshComp())
	{
		if (!IsRunningCommandlet())
		{
			if( !GetMeshComp()->StaticMesh)
			{
				UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PlanarReflectionPlane.PlanarReflectionPlane"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetStaticMesh(PlaneMesh);
				UMaterial* PlaneMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMeshes/ColorCalibrator/M_ChromeBall.M_ChromeBall"), NULL, LOAD_None, NULL);
				GetMeshComp()->SetMaterial(0, PlaneMaterial);
			}
		}

		GetMeshComp()->bVisible = bShowPreviewPlane;
	}
#endif
}

#if WITH_EDITOR
void APlanarReflection::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyScale(FVector(DeltaScale.X, DeltaScale.Y, 0), PivotLocation, bAltDown, bShiftDown, bCtrlDown);

	UPlanarReflectionComponent* ReflectionComponent = Cast<UPlanarReflectionComponent>(GetPlanarReflectionComponent());
	check(ReflectionComponent);
	const FVector ModifiedScale = FVector(0, 0, DeltaScale.Z) * ( AActor::bUsePercentageBasedScaling ? 500.0f : 50.0f );
	FMath::ApplyScaleToFloat(ReflectionComponent->DistanceFromPlaneFadeoutStart, ModifiedScale);
	FMath::ApplyScaleToFloat(ReflectionComponent->DistanceFromPlaneFadeoutEnd, ModifiedScale);
	PostEditChange();
}

void APlanarReflection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GetMeshComp())
	{
		GetMeshComp()->bVisible = bShowPreviewPlane;
		GetMeshComp()->MarkRenderStateDirty();
	}
}

#endif

// -----------------------------------------------

// 0 is reserved to mean invalid
int32 NextPlanarReflectionId = 0;

UPlanarReflectionComponent::UPlanarReflectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCaptureEveryFrame = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;
	RenderTarget = NULL;
	PrefilterRoughness = .01f;
	PrefilterRoughnessDistance = 10000;
	ScreenPercentage = 50;
	NormalDistortionStrength = 500;
	DistanceFromPlaneFadeoutStart = 60;
	DistanceFromPlaneFadeoutEnd = 100;
	AngleFromPlaneFadeStart = 20;
	AngleFromPlaneFadeEnd = 30;
	ProjectionWithExtraFOV = FMatrix::Identity;

	ShowFlags.SetLightShafts(0);

	NextPlanarReflectionId++;
	PlanarReflectionId = NextPlanarReflectionId;
}


void UPlanarReflectionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::ChangedPlanarReflectionFadeDefaults)
	{
		DistanceFromPlaneFadeoutEnd = DistanceFromPlaneFadeEnd_DEPRECATED;
		DistanceFromPlaneFadeoutStart = DistanceFromPlaneFadeStart_DEPRECATED;
	}
}

void UPlanarReflectionComponent::CreateRenderState_Concurrent()
{
	UpdatePreviewShape();

	Super::CreateRenderState_Concurrent();

	if (ShouldComponentAddToScene() && ShouldRender())
	{
		SceneProxy = new FPlanarReflectionSceneProxy(this, RenderTarget);
		GetWorld()->Scene->AddPlanarReflection(this);
	}
}

void UPlanarReflectionComponent::SendRenderTransform_Concurrent()
{	
	UpdatePreviewShape();

	if (SceneProxy)
	{
		GetWorld()->Scene->UpdatePlanarReflectionTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

void UPlanarReflectionComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SceneProxy)
	{
		GetWorld()->Scene->RemovePlanarReflection(this);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyPlanarReflectionCommand,
			FPlanarReflectionSceneProxy*,SceneProxy,SceneProxy,
		{
			delete SceneProxy;
		});

		SceneProxy = NULL;
	}
}

#if WITH_EDITOR

void UPlanarReflectionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Recreate the view state to reset temporal history so that property changes can be seen immediately
	ViewState.Destroy();
	ViewState.Allocate();
}

#endif

void UPlanarReflectionComponent::BeginDestroy()
{
	if (RenderTarget)
	{
		BeginReleaseResource(RenderTarget);
	}
	
	// Begin a fence to track the progress of the BeginReleaseResource being processed by the RT
	ReleaseResourcesFence.BeginFence();

	Super::BeginDestroy();
}

bool UPlanarReflectionComponent::IsReadyForFinishDestroy()
{
	// Wait until the fence is complete before allowing destruction
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.IsFenceComplete();
}

void UPlanarReflectionComponent::FinishDestroy()
{
	Super::FinishDestroy();

	delete RenderTarget;
	RenderTarget = NULL;
}

void UPlanarReflectionComponent::UpdatePreviewShape()
{
	if (PreviewBox)
	{
		PreviewBox->InitBoxExtent(FVector(500 * 4, 500 * 4, DistanceFromPlaneFadeoutEnd));
	}
}

// -----------------------------------------------


USceneCaptureComponentCube::USceneCaptureComponentCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	bTickInEditor = true;
}

void USceneCaptureComponentCube::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	// Update content on register to have at least one frames worth of good data.
	// Without updating here this component would not work in a blueprint construction script which recreates the component after each move in the editor
	CaptureSceneDeferred();
#endif
}

void USceneCaptureComponentCube::SendRenderTransform_Concurrent()
{	
	if (bCaptureOnMovement)
	{
		CaptureSceneDeferred();
	}

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponentCube::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		CaptureSceneDeferred();
	}
}

static TMultiMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<USceneCaptureComponentCube> > CubedSceneCapturesToUpdateMap;

void USceneCaptureComponentCube::CaptureSceneDeferred()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		// Needs some CS because of parallel updates.
		static FCriticalSection CriticalSection;
		FScopeLock ScopeLock(&CriticalSection);
		CubedSceneCapturesToUpdateMap.AddUnique( World, this );
	}	
}

void USceneCaptureComponentCube::CaptureScene()
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible())
	{
		// We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
		World->SendAllEndOfFrameUpdates();
		World->Scene->UpdateSceneCaptureContents(this);
	}	

	if (bCaptureEveryFrame)
	{
		FMessageLog("Blueprint").Warning(LOCTEXT("CaptureScene", "CaptureScene: Scene capture with bCaptureEveryFrame enabled was told to update - major inefficiency."));
	}
}

void USceneCaptureComponentCube::UpdateDeferredCaptures( FSceneInterface* Scene )
{
	UWorld* World = Scene->GetWorld();
	
	if( World && CubedSceneCapturesToUpdateMap.Num() > 0 )
	{
		// Only update the scene captures associated with the current scene.
		// Updating others not associated with the scene would cause invalid data to be rendered into the target
		TArray< TWeakObjectPtr<USceneCaptureComponentCube> > SceneCapturesToUpdate;
		CubedSceneCapturesToUpdateMap.MultiFind( World, SceneCapturesToUpdate );
		
		for( TWeakObjectPtr<USceneCaptureComponentCube> Component : SceneCapturesToUpdate )
		{
			if( Component.IsValid() )
			{
				Scene->UpdateSceneCaptureContents( Component.Get() );
			}
		}
		
		// All scene captures for this world have been updated
		CubedSceneCapturesToUpdateMap.Remove( World );
	}

}

#if WITH_EDITOR
void USceneCaptureComponentCube::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CaptureSceneDeferred();
}
#endif // WITH_EDITOR

/** Returns MeshComp subobject **/
UStaticMeshComponent* ASceneCapture::GetMeshComp() const { return MeshComp; }
/** Returns CaptureComponent2D subobject **/
USceneCaptureComponent2D* ASceneCapture2D::GetCaptureComponent2D() const { return CaptureComponent2D; }
/** Returns DrawFrustum subobject **/
UDrawFrustumComponent* ASceneCapture2D::GetDrawFrustum() const { return DrawFrustum; }
/** Returns CaptureComponentCube subobject **/
USceneCaptureComponentCube* ASceneCaptureCube::GetCaptureComponentCube() const { return CaptureComponentCube; }
/** Returns DrawFrustum subobject **/
UDrawFrustumComponent* ASceneCaptureCube::GetDrawFrustum() const { return DrawFrustum; }

#undef LOCTEXT_NAMESPACE