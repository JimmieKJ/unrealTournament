// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "MaterialEditorModule.h"
#include "MaterialEditorActions.h"
#include "MouseDeltaTracker.h"
#include "SMaterialEditorViewport.h"
#include "PreviewScene.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "AssetToolsModule.h"
#include "MaterialEditor.h"
#include "SMaterialEditorViewportToolBar.h"
#include "ComponentReregisterContext.h"
#include "SDockTab.h"
#include "Engine/StaticMesh.h"
#include "Engine/Selection.h"
#include "Engine/TextureCube.h"
/** Viewport Client for the preview viewport */
class FMaterialEditorViewportClient : public FEditorViewportClient
{
public:
	FMaterialEditorViewportClient(TWeakPtr<IMaterialEditor> InMaterialEditor, FPreviewScene& InPreviewScene);

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily) override;
	
	void SetShowGrid(bool bShowGrid);

	/**
	* Focuses the viewport to the center of the bounding box/sphere ensuring that the entire bounds are in view
	*
	* @param Bounds   The bounds to focus
	* @param bInstant Whether or not to focus the viewport instantly or over time
	*/
	void FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant = false);

private:

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<IMaterialEditor> MaterialEditorPtr;
};

FMaterialEditorViewportClient::FMaterialEditorViewportClient(TWeakPtr<IMaterialEditor> InMaterialEditor, FPreviewScene& InPreviewScene)
	: FEditorViewportClient( nullptr, &InPreviewScene )
	, MaterialEditorPtr(InMaterialEditor)
{
	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
	
	SetViewMode(VMI_Lit);
	
	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;
}



void FMaterialEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}


void FMaterialEditorViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	FEditorViewportClient::Draw(Viewport, Canvas);
	MaterialEditorPtr.Pin()->DrawMessages(Viewport, Canvas);
}

bool FMaterialEditorViewportClient::ShouldOrbitCamera() const
{
	return true;

	/*if (GetDefault<ULevelEditorViewportSettings>()->bUseUE3OrbitControls)
	{
		// this editor orbits always if ue3 orbit controls are enabled
		return true;
	}

	return FEditorViewportClient::ShouldOrbitCamera();*/
}

FLinearColor FMaterialEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface();
	if (MaterialInterface)
	{
		const EBlendMode PreviewBlendMode = (EBlendMode)MaterialInterface->GetBlendMode();
		if (PreviewBlendMode == BLEND_Modulate)
		{
			BackgroundColor = FLinearColor::White;
		}
		else if (PreviewBlendMode == BLEND_Translucent)
		{
			BackgroundColor = FColor(64, 64, 64);
		}
	}
	return BackgroundColor;
}


FSceneView* FMaterialEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FMaterialEditorViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void FMaterialEditorViewportClient::FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant /*= false*/)
{
	const FVector Position = Bounds.Origin;
	float Radius = Bounds.SphereRadius;

	float AspectToUse = AspectRatio;
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (!bUseControllingActorViewInfo && ViewportSize.X > 0 && ViewportSize.Y > 0)
	{
		AspectToUse = Viewport->GetDesiredAspectRatio();
	}

	const bool bEnable = false;
	ToggleOrbitCamera(bEnable);

	/**
	* We need to make sure we are fitting the sphere into the viewport completely, so if the height of the viewport is less
	* than the width of the viewport, we scale the radius by the aspect ratio in order to compensate for the fact that we have
	* less visible vertically than horizontally.
	*/
	if (AspectToUse > 1.0f)
	{
		Radius *= AspectToUse;
	}

	/**
	* Now that we have a adjusted radius, we are taking half of the viewport's FOV,
	* converting it to radians, and then figuring out the camera's distance from the center
	* of the bounding sphere using some simple trig.  Once we have the distance, we back up
	* along the camera's forward vector from the center of the sphere, and set our new view location.
	*/
	const float HalfFOVRadians = FMath::DegreesToRadians(ViewFOV / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Sin(HalfFOVRadians);
	FViewportCameraTransform& ViewTransform = GetViewTransform();
	FVector CameraOffsetVector = ViewTransform.GetRotation().Vector() * -DistanceFromSphere;

	ViewTransform.SetLookAt(Position);
	ViewTransform.TransitionToLocation(Position + CameraOffsetVector, bInstant);

	// Tell the viewport to redraw itself.
	Invalidate();
}

void SMaterialEditorViewport::Construct(const FArguments& InArgs)
{
	MaterialEditorPtr = InArgs._MaterialEditor;

	bShowGrid = false;
	bShowBackground = false;
	PreviewPrimType = TPT_None;

	// Rotate the light in the preview scene so that it faces the preview object
	PreviewScene.SetLightDirection(FRotator(-40.0f, 27.5f, 0.0f));

	SEditorViewport::Construct( SEditorViewport::FArguments() );


	PreviewMeshComponent = ConstructObject<UMaterialEditorMeshComponent>(
		UMaterialEditorMeshComponent::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient );
	PreviewSkeletalMeshComponent = ConstructObject<USkeletalMeshComponent>(
		USkeletalMeshComponent::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient );

	UMaterialInterface* Material = MaterialEditorPtr.Pin()->GetMaterialInterface();
	if (Material)
	{
		SetPreviewMaterial(Material);
	}

	SetPreviewMesh( GUnrealEd->GetThumbnailManager()->EditorSphere, NULL );
}

SMaterialEditorViewport::~SMaterialEditorViewport()
{
	PreviewMeshComponent->OverrideMaterials.Empty();
	PreviewSkeletalMeshComponent->OverrideMaterials.Empty();

	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void SMaterialEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( PreviewMeshComponent );
	Collector.AddReferencedObject( PreviewSkeletalMeshComponent );
}

void SMaterialEditorViewport::RefreshViewport()
{
	//reregister the preview components, so if the preview material changed it will be propagated to the render thread
	PreviewMeshComponent->MarkRenderStateDirty();
	PreviewSkeletalMeshComponent->MarkRenderStateDirty();
	SceneViewport->InvalidateDisplay();
}

void SMaterialEditorViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

bool SMaterialEditorViewport::SetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	// Only permit the use of a skeletal mesh if the material has bUsedWithSkeltalMesh.
	if (!MaterialEditorPtr.Pin()->ApproveSetPreviewMesh(InStaticMesh, InSkeletalMesh))
	{
		return false;
	}
	
	FTransform Transform = FTransform::Identity;
	bUseSkeletalMeshAsPreview = InSkeletalMesh != NULL;
	if ( bUseSkeletalMeshAsPreview )
	{
		// Remove the static mesh from the preview scene.
		PreviewScene.RemoveComponent( PreviewMeshComponent );
		PreviewScene.AddComponent( PreviewSkeletalMeshComponent, Transform );
		
		// Update the toolbar state implicitly through PreviewPrimType.
		PreviewPrimType = TPT_None;

		// Set the new preview skeletal mesh.
		PreviewSkeletalMeshComponent->SetSkeletalMesh( InSkeletalMesh );
	}
	else
	{
		// Update the toolbar state implicitly through PreviewPrimType.
		if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->EditorCylinder )
		{
			PreviewPrimType = TPT_Cylinder;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->EditorCube )
		{
			PreviewPrimType = TPT_Cube;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->EditorSphere )
		{
			PreviewPrimType = TPT_Sphere;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->EditorPlane )
		{
			// Need to rotate the plane so that it faces the camera
			Transform.SetRotation(FQuat(FRotator(0, 90, 0)));
			PreviewPrimType = TPT_Plane;
		}
		else
		{
			PreviewPrimType = TPT_None;
		}

		// Remove the skeletal mesh from the preview scene.
		PreviewScene.RemoveComponent( PreviewSkeletalMeshComponent );
		PreviewScene.AddComponent( PreviewMeshComponent, Transform );

		// Set the new preview static mesh.
		FComponentReregisterContext ReregisterContext( PreviewMeshComponent );
		PreviewMeshComponent->StaticMesh = InStaticMesh;
	}

	return true;
}

bool SMaterialEditorViewport::SetPreviewMesh(const TCHAR* InMeshName)
{
	bool bSuccess = false;
	if ( InMeshName && FString(InMeshName).Len() > 0 )
	{
		UStaticMesh* StaticMesh = LoadObject<UStaticMesh>( NULL, InMeshName);
		if ( StaticMesh )
		{
			bSuccess = SetPreviewMesh( StaticMesh, NULL );
		}
		else
		{
			USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>( NULL, InMeshName );
			if ( SkeletalMesh )
			{
				bSuccess = SetPreviewMesh( NULL, SkeletalMesh );
			}
		}
	}
	return bSuccess;
}

void SMaterialEditorViewport::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	check( PreviewMeshComponent );
	check( PreviewSkeletalMeshComponent );

	PreviewMeshComponent->OverrideMaterials.Empty();
	PreviewMeshComponent->OverrideMaterials.Add( InMaterialInterface );
	PreviewSkeletalMeshComponent->OverrideMaterials.Empty();
	PreviewSkeletalMeshComponent->OverrideMaterials.Add( InMaterialInterface );
}

void SMaterialEditorViewport::ToggleRealtime()
{
	EditorViewportClient->ToggleRealtime();
}

TSharedRef<FUICommandList> SMaterialEditorViewport::GetMaterialEditorCommands() const
{
	check(MaterialEditorPtr.IsValid());
	return MaterialEditorPtr.Pin()->GetToolkitCommands();
}

void SMaterialEditorViewport::OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab )
{
	ParentTab = OwnerTab;
}

bool SMaterialEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SMaterialEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();
	TSharedRef<FUICommandList> ToolkitCommandList = GetMaterialEditorCommands();

	// Add the commands to the toolkit command list so that the toolbar buttons can find them
	ToolkitCommandList->MapAction(
		Commands.SetCylinderPreview,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::OnSetPreviewPrimitive, TPT_Cylinder ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsPreviewPrimitiveChecked, TPT_Cylinder ) );

	ToolkitCommandList->MapAction(
		Commands.SetSpherePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::OnSetPreviewPrimitive, TPT_Sphere ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsPreviewPrimitiveChecked, TPT_Sphere ) );

	ToolkitCommandList->MapAction(
		Commands.SetPlanePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::OnSetPreviewPrimitive, TPT_Plane ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsPreviewPrimitiveChecked, TPT_Plane ) );

	ToolkitCommandList->MapAction(
		Commands.SetCubePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::OnSetPreviewPrimitive, TPT_Cube ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsPreviewPrimitiveChecked, TPT_Cube ) );

	ToolkitCommandList->MapAction(
		Commands.SetPreviewMeshFromSelection,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::OnSetPreviewMeshFromSelection ),
		FCanExecuteAction());

	ToolkitCommandList->MapAction(
		Commands.TogglePreviewGrid,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::TogglePreviewGrid ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsTogglePreviewGridChecked ) );

	ToolkitCommandList->MapAction(
		Commands.TogglePreviewBackground,
		FExecuteAction::CreateSP( this, &SMaterialEditorViewport::TogglePreviewBackground ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditorViewport::IsTogglePreviewBackgroundChecked ) );
}

void SMaterialEditorViewport::OnFocusViewportToSelection()
{
	if( PreviewMeshComponent || PreviewSkeletalMeshComponent )
	{
		EditorViewportClient->FocusViewportOnBounds( bUseSkeletalMeshAsPreview ? PreviewSkeletalMeshComponent->Bounds : PreviewMeshComponent->Bounds );
	}
}

void SMaterialEditorViewport::OnSetPreviewPrimitive(EThumbnailPrimType PrimType)
{
	if (SceneViewport.IsValid())
	{
		UStaticMesh* Primitive = NULL;
		switch (PrimType)
		{
		case TPT_Cylinder: Primitive = GUnrealEd->GetThumbnailManager()->EditorCylinder; break;
		case TPT_Sphere: Primitive = GUnrealEd->GetThumbnailManager()->EditorSphere; break;
		case TPT_Plane: Primitive = GUnrealEd->GetThumbnailManager()->EditorPlane; break;
		case TPT_Cube: Primitive = GUnrealEd->GetThumbnailManager()->EditorCube; break;
		}
		if (Primitive)
		{
			SetPreviewMesh(Primitive , NULL );
			RefreshViewport();
		}
	}
}

bool SMaterialEditorViewport::IsPreviewPrimitiveChecked(EThumbnailPrimType PrimType) const
{
	return PreviewPrimType == PrimType;
}

void SMaterialEditorViewport::OnSetPreviewMeshFromSelection()
{
	bool bFoundPreviewMesh = false;
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	// Look for a selected static mesh.
	UStaticMesh* SelectedStaticMesh = GEditor->GetSelectedObjects()->GetTop<UStaticMesh>();
	UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface();
	if ( SelectedStaticMesh )
	{
		SetPreviewMesh( SelectedStaticMesh, NULL );
		MaterialInterface->PreviewMesh = SelectedStaticMesh->GetPathName();
		bFoundPreviewMesh = true;
	}
	else
	{
		// No static meshes were selected; look for a selected skeletal mesh.
		USkeletalMesh* SelectedSkeletalMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
		if ( SelectedSkeletalMesh )
		{
			//Automatically set the usage flag.
			if( MaterialInterface->GetMaterial() )
			{
				bool bNeedsRecompile = false;
				MaterialInterface->GetMaterial()->SetMaterialUsage( bNeedsRecompile, MATUSAGE_SkeletalMesh );
			}

			SetPreviewMesh( NULL, SelectedSkeletalMesh );
			MaterialInterface->PreviewMesh = SelectedSkeletalMesh->GetPathName();
			bFoundPreviewMesh = true;
		}
	}

	if ( bFoundPreviewMesh )
	{
		FMaterialEditor::UpdateThumbnailInfoPreviewMesh(MaterialInterface);

		MaterialInterface->MarkPackageDirty();
		RefreshViewport();
	}
	else
	{
		FSuppressableWarningDialog::FSetupInfo Info(NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Message", "You need to select a mesh in the content browser to preview it."),
			NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound", "Warning: No Preview Mesh Found"), "Warning_NoPreviewMeshFound");
		Info.ConfirmText = NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Confirm", "Continue");
		
		FSuppressableWarningDialog NoPreviewMeshWarning( Info );
		NoPreviewMeshWarning.ShowModal();
	}
}

void SMaterialEditorViewport::TogglePreviewGrid()
{
	bShowGrid = !bShowGrid;
	EditorViewportClient->SetShowGrid(bShowGrid);
	RefreshViewport();
}

bool SMaterialEditorViewport::IsTogglePreviewGridChecked() const
{
	return bShowGrid;
}

void SMaterialEditorViewport::TogglePreviewBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshViewport();
}

bool SMaterialEditorViewport::IsTogglePreviewBackgroundChecked() const
{
	return bShowBackground;
}

TSharedRef<FEditorViewportClient> SMaterialEditorViewport::MakeEditorViewportClient() 
{
	EditorViewportClient = MakeShareable( new FMaterialEditorViewportClient(MaterialEditorPtr, PreviewScene) );
	
	EditorViewportClient->SetViewLocation( FVector::ZeroVector );
	EditorViewportClient->SetViewRotation( FRotator::ZeroRotator );
	EditorViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	EditorViewportClient->bSetListenerPosition = false;

	EditorViewportClient->SetRealtime( false );
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SMaterialEditorViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SMaterialEditorViewport::MakeViewportToolbar()
{
	return SNew(SMaterialEditorViewportToolBar)
		.Viewport(SharedThis(this));
}

EVisibility SMaterialEditorViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}
