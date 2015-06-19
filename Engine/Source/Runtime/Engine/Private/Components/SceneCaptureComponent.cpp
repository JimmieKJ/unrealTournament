// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	DrawFrustum->AttachParent = GetMeshComp();

	CaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("NewSceneCaptureComponent2D"));
	CaptureComponent2D->AttachParent = GetMeshComp();
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
	DrawFrustum->AttachParent = GetMeshComp();

	CaptureComponentCube = CreateDefaultSubobject<USceneCaptureComponentCube>(TEXT("NewSceneCaptureComponentCube"));
	CaptureComponentCube->AttachParent = GetMeshComp();
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
	if(bFinished)
	{
		CaptureComponentCube->UpdateContent();
	}
}
#endif
// -----------------------------------------------
USceneCaptureComponent::USceneCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ShowFlags(FEngineShowFlags(ESFIM_Game))
{
	bCaptureEveryFrame = true;
	MaxViewDistanceOverride = -1;

	// Disable features that are not desired when capturing the scene
	ShowFlags.MotionBlur = 0; // motion blur doesn't work correctly with scene captures.
	ShowFlags.SeparateTranslucency = 0;
	ShowFlags.HMDDistortion = 0;
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
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;
	// previous behavior was to capture from raw scene color 
	CaptureSource = SCS_SceneColorHDR;
	// default to full blend weight..
	PostProcessBlendWeight = 1.0f;
}

void USceneCaptureComponent2D::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Update content on register to have at least one frames worth of good data.
	// Without updating here this component would not work in a blueprint construction script which recreates the component after each move in the editor
	UpdateContent();
#endif
}

void USceneCaptureComponent2D::SendRenderTransform_Concurrent()
{	
	UpdateContent();

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponent2D::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		UpdateComponentToWorld(false);
	}
}

static TMultiMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<USceneCaptureComponent2D>> SceneCapturesToUpdateMap;

void USceneCaptureComponent2D::UpdateContent()
{
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		SceneCapturesToUpdateMap.AddUnique(World, this);
	}	
}

void USceneCaptureComponent2D::UpdateDeferredCaptures( FSceneInterface* Scene )
{
	UWorld* World = Scene->GetWorld();
	if( World && SceneCapturesToUpdateMap.Num() > 0 )
	{
		// Only update the scene captures assoicated with the current scene.
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
void USceneCaptureComponent2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateContent();
}
#endif // WITH_EDITOR


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
	UpdateContent();
#endif
}

void USceneCaptureComponentCube::SendRenderTransform_Concurrent()
{	
	UpdateContent();

	Super::SendRenderTransform_Concurrent();
}

void USceneCaptureComponentCube::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCaptureEveryFrame)
	{
		UpdateComponentToWorld(false);
	}
}

static TMultiMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<USceneCaptureComponentCube> > CubedSceneCapturesToUpdateMap;

void USceneCaptureComponentCube::UpdateContent()
{
	if (World && World->Scene && IsVisible())
	{
		// Defer until after updates finish
		CubedSceneCapturesToUpdateMap.AddUnique( World, this );
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

	UpdateContent();
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
