// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraEditorModule.h"
#include "MouseDeltaTracker.h"
#include "SNiagaraEffectEditorViewport.h"
#include "PreviewScene.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "NiagaraEditor.h"
#include "ComponentReregisterContext.h"
#include "NiagaraComponent.h"
#include "NiagaraEffect.h"
#include "SDockTab.h"
#include "Engine/TextureCube.h"

/** Viewport Client for the preview viewport */
class FNiagaraEffectEditorViewportClient : public FEditorViewportClient
{
public:
	FNiagaraEffectEditorViewportClient(TWeakPtr<INiagaraEffectEditor> InEffectEditor, FPreviewScene& InPreviewScene, const TSharedRef<SNiagaraEffectEditorViewport>& InNiagaraEditorViewport);
	
	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily) override;
	
	void SetShowGrid(bool bShowGrid);
	
private:
	
	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<INiagaraEffectEditor> EffectEditorPtr;
};

FNiagaraEffectEditorViewportClient::FNiagaraEffectEditorViewportClient(TWeakPtr<INiagaraEffectEditor> InEffectEditor, FPreviewScene& InPreviewScene, const TSharedRef<SNiagaraEffectEditorViewport>& InNiagaraEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InNiagaraEditorViewport))
	, EffectEditorPtr(InEffectEditor)
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



void FNiagaraEffectEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	
	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}


void FNiagaraEffectEditorViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	FEditorViewportClient::Draw(Viewport, Canvas);
}

bool FNiagaraEffectEditorViewportClient::ShouldOrbitCamera() const
{
	return true;
}


FLinearColor FNiagaraEffectEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FNiagaraEffectEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FNiagaraEffectEditorViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void SNiagaraEffectEditorViewport::Construct(const FArguments& InArgs)
{
	bShowGrid = false;
	bShowBackground = false;
	
	// Rotate the light in the preview scene so that it faces the preview object
	PreviewScene.SetLightDirection(FRotator(-40.0f, 27.5f, 0.0f));
	
	SEditorViewport::Construct( SEditorViewport::FArguments() );

	PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
}

SNiagaraEffectEditorViewport::~SNiagaraEffectEditorViewport()
{
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void SNiagaraEffectEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( PreviewComponent );
}

void SNiagaraEffectEditorViewport::RefreshViewport()
{
	//reregister the preview components, so if the preview material changed it will be propagated to the render thread
	PreviewComponent->MarkRenderStateDirty();
	SceneViewport->InvalidateDisplay();
}

void SNiagaraEffectEditorViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SEditorViewport::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}

void SNiagaraEffectEditorViewport::SetPreviewEffect(FNiagaraEffectInstance *InPreviewEffect)
{
	check( PreviewComponent );

	FTransform Transform = FTransform::Identity;
	PreviewScene.RemoveComponent(PreviewComponent);
	PreviewScene.AddComponent(PreviewComponent, Transform);

	{
		FComponentReregisterContext ReregisterContext(PreviewComponent);
	}

	PreviewComponent->SetEffectInstance(InPreviewEffect);
	PreviewComponent->GetEffectInstance()->Init(PreviewComponent);
}


void SNiagaraEffectEditorViewport::ToggleRealtime()
{
	EditorViewportClient->ToggleRealtime();
}

/*
TSharedRef<FUICommandList> SNiagaraEffectEditorViewport::GetEffectEditorCommands() const
{
	check(EffectEditorPtr.IsValid());
	return EffectEditorPtr.GetToolkitCommands();
}
*/

void SNiagaraEffectEditorViewport::OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab )
{
	ParentTab = OwnerTab;
}

bool SNiagaraEffectEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SNiagaraEffectEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// TODO: implement the necessary commands here
	/*	
	const FNiagaraEffectEditorCommands& Commands = FNiagaraEffectEditorCommands::Get();
	TSharedRef<FUICommandList> ToolkitCommandList = GetEffectEditorCommands();
	
	// Add the commands to the toolkit command list so that the toolbar buttons can find them
	
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
								  */
}

void SNiagaraEffectEditorViewport::OnFocusViewportToSelection()
{
	if( PreviewComponent )
	{
		EditorViewportClient->FocusViewportOnBox( PreviewComponent->Bounds.GetBox() );
	}
}

void SNiagaraEffectEditorViewport::TogglePreviewGrid()
{
	bShowGrid = !bShowGrid;
	EditorViewportClient->SetShowGrid(bShowGrid);
	RefreshViewport();
}

bool SNiagaraEffectEditorViewport::IsTogglePreviewGridChecked() const
{
	return bShowGrid;
}

void SNiagaraEffectEditorViewport::TogglePreviewBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshViewport();
}

bool SNiagaraEffectEditorViewport::IsTogglePreviewBackgroundChecked() const
{
	return bShowBackground;
}

TSharedRef<FEditorViewportClient> SNiagaraEffectEditorViewport::MakeEditorViewportClient() 
{
	EditorViewportClient = MakeShareable( new FNiagaraEffectEditorViewportClient(nullptr, PreviewScene, SharedThis(this)) );
	
	EditorViewportClient->SetViewLocation( FVector::ZeroVector );
	EditorViewportClient->SetViewRotation( FRotator::ZeroRotator );
	EditorViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	EditorViewportClient->bSetListenerPosition = false;
	
	EditorViewportClient->SetRealtime( true );
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SNiagaraEffectEditorViewport::IsVisible );
	
	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SNiagaraEffectEditorViewport::MakeViewportToolbar()
{
	//return SNew(SNiagaraEffectEditorViewportToolBar)
	//.Viewport(SharedThis(this));
	return SNew(SBox);
}

EVisibility SNiagaraEffectEditorViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}
