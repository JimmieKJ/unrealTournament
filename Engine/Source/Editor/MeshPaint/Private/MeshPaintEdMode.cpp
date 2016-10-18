// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"

#include "StaticMeshResources.h"
#include "MeshPaintEdMode.h"
#include "Factories.h"
#include "ScopedTransaction.h"
#include "MeshPaintRendering.h"
#include "ImageUtils.h"
#include "Editor/UnrealEd/Public/Toolkits/ToolkitManager.h"
#include "RawMesh.h"
#include "Editor/UnrealEd/Public/ObjectTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "EditorSupportDelegates.h"
#include "EditorReimportHandler.h"

//Slate dependencies
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/SLevelViewport.h"
#include "MessageLog.h"

#include "Runtime/Engine/Classes/PhysicsEngine/BodySetup.h"
#include "SMeshPaint.h"
#include "ComponentReregisterContext.h"
#include "CanvasTypes.h"
#include "Engine/Selection.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshPaintAdapterFactory.h"
#include "Components/SplineMeshComponent.h"

#include "ViewportWorldInteraction.h"
#include "ViewportInteractableInterface.h"
#include "VREditorInteractor.h"
#include "VIBaseTransformGizmo.h"

#define LOCTEXT_NAMESPACE "MeshPaint_Mode"

DEFINE_LOG_CATEGORY_STATIC(LogMeshPaintEdMode, Log, All);

/** Static: Global mesh paint settings */
FMeshPaintSettings FMeshPaintSettings::StaticMeshPaintSettings;

/** Batched element parameters for texture paint shaders used for paint blending and paint mask generation */
class FMeshPaintBatchedElementParameters : public FBatchedElementParameters
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override
	{
		MeshPaintRendering::SetMeshPaintShaders(RHICmdList, InFeatureLevel, InTransform, InGamma, ShaderParams );
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintShaderParameters ShaderParams;
};

bool DoesMeshComponentUseTexture(UMeshComponent* MeshComponent, UTexture* Texture)
{
	TArray<UTexture*> UsedTextures;
	MeshComponent->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High);

	return UsedTextures.Contains(Texture);
}

/** Batched element parameters for texture paint shaders used for texture dilation */
class FMeshPaintDilateBatchedElementParameters : public FBatchedElementParameters
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override
	{
		MeshPaintRendering::SetMeshPaintDilateShaders(RHICmdList, InFeatureLevel, InTransform, InGamma, ShaderParams );
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintDilateShaderParameters ShaderParams;
};

/** Constructor */
FEdModeMeshPaint::FEdModeMeshPaint() 
	: FEdMode(),
	  bIsPainting( false ),
	  PaintingWithInteractorInVR( nullptr ),
	  bIsFloodFill( false ),
	  bPushInstanceColorsToMesh( false ),
	  PaintingStartTime( 0.0 ),
	  ModifiedStaticMeshes(),
	  TexturePaintingCurrentMeshComponent( NULL ),
	  TexturePaintingStaticMeshLOD(0),
	  PaintingTexture2D( NULL ),
	  bDoRestoreRenTargets( false ),
	  BrushRenderTargetTexture( NULL),
	  BrushMaskRenderTargetTexture( NULL ),
	  SeamMaskRenderTargetTexture( NULL ),
	  ScopedTransaction( NULL )
{
}

/** Destructor */
FEdModeMeshPaint::~FEdModeMeshPaint()
{
	CopiedColorsByComponent.Empty();
}

/** FGCObject interface */
void FEdModeMeshPaint::AddReferencedObjects( FReferenceCollector& Collector )
{
	// Call parent implementation
	FEdMode::AddReferencedObjects( Collector );

	for( int32 Index = 0; Index < ModifiedStaticMeshes.Num(); Index++ )
	{
		Collector.AddReferencedObject( ModifiedStaticMeshes[ Index ] );
	}
	Collector.AddReferencedObject( TexturePaintingCurrentMeshComponent );
	Collector.AddReferencedObject( PaintingTexture2D );
	Collector.AddReferencedObject( BrushRenderTargetTexture );
	Collector.AddReferencedObject( BrushMaskRenderTargetTexture );
	Collector.AddReferencedObject( SeamMaskRenderTargetTexture );
	for( TMap< UTexture2D*, PaintTexture2DData >::TIterator It( PaintTargetData ); It; ++It )
	{
		Collector.AddReferencedObject( It.Key() );
		It.Value().AddReferencedObjects( Collector );
	}

	for (const auto& Pair : ComponentToAdapterMap)
	{
		Pair.Value->AddReferencedObjects(Collector);
	}
}

bool FEdModeMeshPaint::UsesToolkits() const
{
	return true;
}

/** FEdMode: Called when the mode is entered */
void FEdModeMeshPaint::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	{
		
		// The user can manipulate the editor selection lock flag in paint mode so we save off the value here so it can be restored later
		bWasSelectionLockedOnStart = GEdSelectionLock;

		// Make sure texture list gets updated
		bShouldUpdateTextureList = true;

	}

	// Catch when objects are replaced when a construction script is rerun
	GEditor->OnObjectsReplaced().AddSP(this, &FEdModeMeshPaint::OnObjectsReplaced);

	// Hook into pre/post world save, so that the original collision volumes can be temporarily reinstated
	FEditorDelegates::PreSaveWorld.AddSP(this, &FEdModeMeshPaint::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.AddSP(this, &FEdModeMeshPaint::OnPostSaveWorld);

	// Catch assets if they are about to be (re)imported
	FEditorDelegates::OnAssetPostImport.AddSP(this, &FEdModeMeshPaint::OnPostImportAsset);
	FReimportManager::Instance()->OnPostReimport().AddSP(this, &FEdModeMeshPaint::OnPostReimportAsset);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &FEdModeMeshPaint::OnAssetRemoved);

	// Initialize adapter globals
	FMeshPaintAdapterFactory::InitializeAdapterGlobals();

	// Create adapters for currently selected mesh components
	CreateGeometryAdaptersForSelectedComponents();

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FMeshPaintToolKit(this));
		Toolkit->Init(Owner->GetToolkitHost());
	}

	// Change the engine to draw selected objects without a color boost, but unselected objects will
	// be darkened slightly.  This just makes it easier to paint on selected objects without the
	// highlight effect distorting the appearance.
	GEngine->OverrideSelectedMaterialColor( FLinearColor::Black );

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const bool bWantRealTime = true;
	const bool bRememberCurrentState = true;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Set show flags for all perspective viewports
	const bool bAllowColorViewModes = true;
	// Only alter level editor viewports.
	for( int32 ViewIndex = 0 ; ViewIndex < GEditor->LevelViewportClients.Num() ; ++ViewIndex )
	{
		FEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[ViewIndex];
		SetViewportShowFlags( bAllowColorViewModes, *ViewportClient );
	} 

	//When painting vertex colors we want to force the lod level of objects being painted to LOD0.
	if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors )
	{
		ApplyOrRemoveForceBestLOD(/*bApply=*/ true);
	}

	// Register to find out about editor modes being activated
	GetModeManager()->OnEditorModeChanged().AddRaw( this, &FEdModeMeshPaint::OnEditorModeChanged );

	// Register to find out about VR input events
	IVREditorMode* VREditorMode = static_cast<IVREditorMode*>( GetModeManager()->GetActiveMode( IVREditorModule::Get().GetVREditorModeID() ) );
	if( VREditorMode != nullptr )
	{
		VREditorMode->GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
		VREditorMode->GetWorldInteraction().OnViewportInteractionInputAction().AddRaw( this, &FEdModeMeshPaint::OnVRAction );

		// Hide the VR transform gizmo while we're in mesh paint mode.  It sort of gets in the way of painting.
		VREditorMode->GetWorldInteraction().SetTransformGizmoVisible( false );
	}
}

/** FEdMode: Called when the mode is exited */
void FEdModeMeshPaint::Exit()
{
	if( IVREditorModule::IsAvailable() )
	{
		IVREditorMode* VREditorMode = static_cast<IVREditorMode*>( GetModeManager()->GetActiveMode( IVREditorModule::Get().GetVREditorModeID() ) );
		if( VREditorMode != nullptr )
		{
			// Restore the transform gizmo visibility
			VREditorMode->GetWorldInteraction().SetTransformGizmoVisible( true );

			// Unregister from event handlers
			VREditorMode->GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
		}
	}

	GetModeManager()->OnEditorModeChanged().RemoveAll( this );

	//If we're painting vertex colors then propagate the painting done on LOD0 to all lower LODs. 
	//Then stop forcing the LOD level of the mesh to LOD0.
	if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors )
	{
		ApplyVertexColorsToAllLODs();
		ApplyOrRemoveForceBestLOD(/*bApply=*/ false);
	}

	// The user can manipulate the editor selection lock flag in paint mode so we make sure to restore it here
	GEdSelectionLock = bWasSelectionLockedOnStart;

	// Restore real-time viewport state if we changed it
	const bool bWantRealTime = false;
	const bool bRememberCurrentState = false;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Disable color view modes if we set those for all perspective viewports
	const bool bAllowColorViewModes = false;
	// Only alter level editor viewports.
	for( int32 ViewIndex = 0 ; ViewIndex < GEditor->LevelViewportClients.Num() ; ++ViewIndex )
	{
		FEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[ViewIndex];
		SetViewportShowFlags( bAllowColorViewModes, *ViewportClient );
	} 

	// Restore selection color
	GEngine->RestoreSelectedMaterialColor();

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// If the user has pending changes and the editor is not exiting, we want to do the commit for all the modified textures.
	if ((GetNumberOfPendingPaintChanges() > 0) && !GIsRequestingExit)
	{
		CommitAllPaintedTextures();
	}
	else
	{
		ClearAllTextureOverrides();
	}

	// Remove all adapters
	RemoveAllGeometryAdapters();

	PaintTargetData.Empty();

	// Remove any existing texture targets
	TexturePaintTargetList.Empty();

	// Clear out cached settings map
	StaticMeshSettingsMap.Empty();

	if( ScopedTransaction != NULL )
	{
		EndTransaction();
	}

	// Unbind delegates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);

	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FEditorDelegates::PreSaveWorld.RemoveAll(this);
	FEditorDelegates::PostSaveWorld.RemoveAll(this);
	GEditor->OnObjectsReplaced().RemoveAll(this);

	// Call parent implementation
	FEdMode::Exit();
}


void FEdModeMeshPaint::OnEditorModeChanged( FEdMode* EditorMode, bool bEntered )
{
	// VR Editor mode may have gone away, so re-register for events in case it's a different object than the one we were originally bound to
	check( EditorMode != nullptr );
	if( bEntered && EditorMode->GetID() == IVREditorModule::Get().GetVREditorModeID() )
	{
		IVREditorMode* VREditorMode = static_cast<IVREditorMode*>( EditorMode );
		if ( VREditorMode != nullptr )
		{
			VREditorMode->GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
			VREditorMode->GetWorldInteraction().OnViewportInteractionInputAction().AddRaw( this, &FEdModeMeshPaint::OnVRAction );
		}

		PaintingWithInteractorInVR = nullptr;
	}
}


/** FEdMode: Called when the mouse is moved over the viewport */
bool FEdModeMeshPaint::MouseMove( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y )
{
	// We only care about perspective viewports
	if( ViewportClient->IsPerspective() )
	{
		// ...
	}

	return false;
}



/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeMeshPaint::CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	// We only care about perspective viewports
	if( InViewportClient->IsPerspective() )
	{
		if( bIsPainting )
		{
			// Compute a world space ray from the screen space mouse coordinates
			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( 
				InViewportClient->Viewport, 
				InViewportClient->GetScene(),
				InViewportClient->EngineShowFlags)
				.SetRealtimeUpdate( InViewportClient->IsRealtime() ));
			FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
			FViewportCursorLocation MouseViewportRay( View, (FEditorViewportClient*)InViewport->GetClient(), InMouseX, InMouseY );

			
			// Paint!
			const bool bVisualCueOnly = false;
			const EMeshPaintAction::Type PaintAction = GetPaintAction(InViewport);

			// Apply stylus pressure
			const float StrengthScale = InViewport->IsPenActive() ? InViewport->GetTabletPressure() : 1.f;

			bool bAnyPaintableActorsUnderCursor = false;

			DoPaint( View->ViewMatrices.ViewOrigin, MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection(), NULL, PaintAction, bVisualCueOnly, StrengthScale, /* Out */ bAnyPaintableActorsUnderCursor );

			return true;
		}
	}

	return false;
}



/** FEdMode: Called when a mouse button is pressed */
bool FEdModeMeshPaint::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return true;
}



/** FEdMode: Called when the a mouse button is released */
bool FEdModeMeshPaint::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	EndPainting();
	return true;
}

void FEdModeMeshPaint::StartPainting()
{
	if(!bIsPainting)
	{
		bIsPainting = true;
		PaintingStartTime = FPlatformTime::Seconds();
	}
}

void FEdModeMeshPaint::EndPainting()
{
	if(bIsPainting)
	{
		bIsPainting = false;
		PaintingWithInteractorInVR = nullptr;
		FinishPaintingTexture();

		// Rebuild any static meshes that we painted on last stroke
		{
			for( int32 CurMeshIndex = 0; CurMeshIndex < ModifiedStaticMeshes.Num(); ++CurMeshIndex )
			{
				UStaticMesh* CurStaticMesh = ModifiedStaticMeshes[ CurMeshIndex ];

				// @todo MeshPaint: Do we need to do bother doing a full rebuild even with real-time turbo-rebuild?
				if( 0 )
				{
					// Rebuild the modified mesh
					CurStaticMesh->Build();
				}
			}

			ModifiedStaticMeshes.Empty();
		}
			
		// The user stopped requesting paint.  If we had a vertex paint transaction in progress, we will stop it.
		if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors && ScopedTransaction != NULL )
		{
			// Ends the vertex paint brush stroke transaction
			EndTransaction();
		}
	}
}

/** FEdMode: Called when a key is pressed */
bool FEdModeMeshPaint::InputKey( FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent )
{
	bool bHandled = false;

	const bool bIsLeftButtonDown = ( InKey == EKeys::LeftMouseButton && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftMouseButton );
	const bool bIsRightButtonDown = (InKey == EKeys::RightMouseButton && InEvent != IE_Released) || InViewport->KeyState(EKeys::RightMouseButton);
	const bool bIsCtrlDown = ((InKey == EKeys::LeftControl || InKey == EKeys::RightControl) && InEvent != IE_Released) || InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bIsShiftDown = ( ( InKey == EKeys::LeftShift || InKey == EKeys::RightShift ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftShift ) || InViewport->KeyState( EKeys::RightShift );
	const bool bIsAltDown = ( ( InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftAlt ) || InViewport->KeyState( EKeys::RightAlt );

	// Change Brush Size - We want to stay consistent with other brush utilities.  Here we model after landscape mode.
	if ((InEvent == IE_Pressed || InEvent == IE_Repeat) && (InKey == EKeys::LeftBracket || InKey == EKeys::RightBracket) )
	{
		const float BrushRadius = GetBrushRadiiDefault();

		float Diff = 0.05f; 
		if (InKey == EKeys::LeftBracket)
		{
			Diff = -Diff;
		}

		float NewValue = BrushRadius*(1.f+Diff);
		if (InKey == EKeys::LeftBracket)
		{
			NewValue = FMath::Min(NewValue, BrushRadius - 1.f);
		}
		else
		{
			NewValue = FMath::Max(NewValue, BrushRadius + 1.f);
		}

		SetBrushRadiiDefault( NewValue );

		bHandled = true;
	}

	if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		// Prev texture 
		if( InEvent == IE_Pressed && InKey == EKeys::Comma )
		{
			SelectPrevTexture();
			bHandled = true;
		}

		// Next texture 
		if( InEvent == IE_Pressed && InKey == EKeys::Period )
		{
			SelectNextTexture();
			bHandled = true;
		}

		if( bIsCtrlDown && bIsShiftDown && InEvent == IE_Pressed && InKey == EKeys::T )
		{
			FindSelectedTextureInContentBrowser();
			bHandled = true;
		}

		if( bIsCtrlDown && bIsShiftDown && InEvent == IE_Pressed && InKey == EKeys::C )
		{
			// Only process commit requests if the user isn't painting.
			if( PaintingTexture2D == NULL )
			{
				CommitAllPaintedTextures();
			}
			bHandled = true;
		}
	}

	// When painting we only care about perspective viewports
	if( !bIsAltDown && InViewportClient->IsPerspective() )
	{
		// Does the user want to paint right now?
		const bool bUserWantsPaint = bIsLeftButtonDown && !bIsRightButtonDown && !bIsAltDown;
		bool bAnyPaintableActorsUnderCursor = false;

		// Stop current tracking if the user is no longer painting
		if( bIsPainting && !bUserWantsPaint &&
			( InKey == EKeys::LeftMouseButton || InKey == EKeys::RightMouseButton || InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt ) )
		{
			bHandled = true;
			EndPainting();
		}
		else if( !bIsPainting && bUserWantsPaint )
		{
			// Re-initialize new tracking only if a new button was pressed, otherwise we continue the previous one.
			// First, see if the item we're clicking on is different to the currently selected one.
			const int32 HitX = InViewport->GetMouseX();
			const int32 HitY = InViewport->GetMouseY();
			const HHitProxy* HitProxy = InViewport->GetHitProxy(HitX, HitY);

			if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
			{
				const AActor* ClickedActor = (static_cast<const HActor*>(HitProxy))->Actor;
				USelection& SelectedActors = *Owner->GetSelectedActors();
				if (SelectedActors.IsSelected(ClickedActor))
				{
					// Clicked actor is currently selected, start painting.
					bHandled = true;
					StartPainting();

					// Go ahead and paint immediately
					{
						// Compute a world space ray from the screen space mouse coordinates
						FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( 
							InViewportClient->Viewport, 
							InViewportClient->GetScene(),
							InViewportClient->EngineShowFlags )
							.SetRealtimeUpdate( InViewportClient->IsRealtime() ));

						FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
						FViewportCursorLocation MouseViewportRay( View, (FEditorViewportClient*)InViewport->GetClient(), InViewport->GetMouseX(), InViewport->GetMouseY() );

						// Paint!
						const bool bVisualCueOnly = false;
						const EMeshPaintAction::Type PaintAction = GetPaintAction(InViewport);
						const float StrengthScale = 1.0f;
						DoPaint( View->ViewMatrices.ViewOrigin, MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection(), NULL, PaintAction, bVisualCueOnly, StrengthScale, /* Out */ bAnyPaintableActorsUnderCursor );
					}
				}
				else
				{
					// Otherwise we have clicked on a new actor, not necessarily one which is paintable, but certainly one which is selectable.
					// Pass the click up to the editor viewport client.
					bHandled = false;
				}
			}
		}

		if( !bAnyPaintableActorsUnderCursor )
		{
			bHandled = false;
		}

		// Also absorb other mouse buttons, and Ctrl/Alt/Shift events that occur while we're painting as these would cause
		// the editor viewport to start panning/dollying the camera
		{
			const bool bIsOtherMouseButtonEvent = ( InKey == EKeys::MiddleMouseButton || InKey == EKeys::RightMouseButton );
			const bool bCtrlButtonEvent = (InKey == EKeys::LeftControl || InKey == EKeys::RightControl);
			const bool bShiftButtonEvent = (InKey == EKeys::LeftShift || InKey == EKeys::RightShift);
			const bool bAltButtonEvent = (InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt);
			if( bIsPainting && ( bIsOtherMouseButtonEvent || bShiftButtonEvent || bAltButtonEvent ) )
			{
				bHandled = true;
			}

			if( bCtrlButtonEvent && !bIsPainting)
			{
				bHandled = false;
			}
			else if( bIsCtrlDown)
			{
				//default to assuming this is a paint command
				bHandled = true;

				// Allow Ctrl+B to pass through so we can support the finding of a selected static mesh in the content browser.
				if ( !(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent) && ( (InKey == EKeys::B) && (InEvent == IE_Pressed) ) )
				{
					bHandled = false;
				}

				// If we are not painting, we will let the CTRL-Z and CTRL-Y key presses through to support undo/redo.
				if ( !bIsPainting && ( InKey == EKeys::Z || InKey == EKeys::Y ) )
				{
					bHandled = false;
				}
			}
		}
	}


	return bHandled;
}




/** Mesh paint parameters */
class FMeshPaintParameters
{

public:

	EMeshPaintMode::Type PaintMode;
	EMeshPaintAction::Type PaintAction;
	FVector BrushPosition;
	FVector BrushNormal;
	FLinearColor BrushColor;
	float SquaredBrushRadius;
	float BrushRadialFalloffRange;
	float InnerBrushRadius;
	float BrushDepth;
	float BrushDepthFalloffRange;
	float InnerBrushDepth;
	float BrushStrength;
	FMatrix BrushToWorldMatrix;
	FMatrix InverseBrushToWorldMatrix;
	bool bWriteRed;
	bool bWriteGreen;
	bool bWriteBlue;
	bool bWriteAlpha;
	int32 TotalWeightCount;
	int32 PaintWeightIndex;
	int32 UVChannel;

};




/** Static: Determines if a world space point is influenced by the brush and reports metrics if so */
bool FEdModeMeshPaint::IsPointInfluencedByBrush( const FVector& InPosition,
												  const FMeshPaintParameters& InParams,
												  float& OutSquaredDistanceToVertex2D,
												  float& OutVertexDepthToBrush )
{
	// Project the vertex into the plane of the brush
	FVector BrushSpaceVertexPosition = InParams.InverseBrushToWorldMatrix.TransformPosition( InPosition );
	FVector2D BrushSpaceVertexPosition2D( BrushSpaceVertexPosition.X, BrushSpaceVertexPosition.Y );

	// Is the brush close enough to the vertex to paint?
	const float SquaredDistanceToVertex2D = BrushSpaceVertexPosition2D.SizeSquared();
	if( SquaredDistanceToVertex2D <= InParams.SquaredBrushRadius )
	{
		// OK the vertex is overlapping the brush in 2D space, but is it too close or
		// two far (depth wise) to be influenced?
		const float VertexDepthToBrush = FMath::Abs( BrushSpaceVertexPosition.Z );
		if( VertexDepthToBrush <= InParams.BrushDepth )
		{
			OutSquaredDistanceToVertex2D = SquaredDistanceToVertex2D;
			OutVertexDepthToBrush = VertexDepthToBrush;
			return true;
		}
	}

	return false;
}



/** Paints the specified vertex!  Returns true if the vertex was in range. */
bool FEdModeMeshPaint::PaintVertex( const FVector& InVertexPosition,
									 const FMeshPaintParameters& InParams,
									 const bool bIsPainting,
									 FColor& InOutVertexColor )
{
	float SquaredDistanceToVertex2D;
	float VertexDepthToBrush;
	if( IsPointInfluencedByBrush( InVertexPosition, InParams, SquaredDistanceToVertex2D, VertexDepthToBrush ) )
	{
		if( bIsPainting )
		{
			// Compute amount of paint to apply
			float PaintAmount = 1.0f;

			// Apply radial-based falloff
			{
				// Compute the actual distance
				float DistanceToVertex2D = 0.0f;
				if( SquaredDistanceToVertex2D > KINDA_SMALL_NUMBER )
				{
					DistanceToVertex2D = FMath::Sqrt( SquaredDistanceToVertex2D );
				}

				if( DistanceToVertex2D > InParams.InnerBrushRadius )
				{
					const float RadialBasedFalloff = ( DistanceToVertex2D - InParams.InnerBrushRadius ) / InParams.BrushRadialFalloffRange;
					PaintAmount *= 1.0f - RadialBasedFalloff;
				}
			}

			// Apply depth-based falloff
			{
				if( VertexDepthToBrush > InParams.InnerBrushDepth )
				{
					const float DepthBasedFalloff = ( VertexDepthToBrush - InParams.InnerBrushDepth ) / InParams.BrushDepthFalloffRange;
					PaintAmount *= 1.0f - DepthBasedFalloff;
					
//						UE_LOG(LogMeshPaintEdMode, Log,  TEXT( "Painted Vertex:  DepthBasedFalloff=%.2f" ), DepthBasedFalloff );
				}
			}

			PaintAmount *= InParams.BrushStrength;

			
			// Paint!

			// NOTE: We manually perform our own conversion between FColor and FLinearColor (and vice versa) here
			//	  as we want values to be linear (not gamma corrected.)  These color values are often used as scalars
			//	  to blend between textures, etc, and must be linear!

			const FLinearColor OldColor = InOutVertexColor.ReinterpretAsLinear();
			FLinearColor NewColor = OldColor;



			if( InParams.PaintMode == EMeshPaintMode::PaintColors )
			{
				// Color painting

				if( InParams.bWriteRed )
				{
					if( OldColor.R < InParams.BrushColor.R )
					{
						NewColor.R = FMath::Min( InParams.BrushColor.R, OldColor.R + PaintAmount );
					}
					else
					{
						NewColor.R = FMath::Max( InParams.BrushColor.R, OldColor.R - PaintAmount );
					}
				}

				if( InParams.bWriteGreen )
				{
					if( OldColor.G < InParams.BrushColor.G )
					{
						NewColor.G = FMath::Min( InParams.BrushColor.G, OldColor.G + PaintAmount );
					}
					else
					{
						NewColor.G = FMath::Max( InParams.BrushColor.G, OldColor.G - PaintAmount );
					}
				}

				if( InParams.bWriteBlue )
				{
					if( OldColor.B < InParams.BrushColor.B )
					{
						NewColor.B = FMath::Min( InParams.BrushColor.B, OldColor.B + PaintAmount );
					}
					else
					{
						NewColor.B = FMath::Max( InParams.BrushColor.B, OldColor.B - PaintAmount );
					}
				}

				if( InParams.bWriteAlpha )
				{
					if( OldColor.A < InParams.BrushColor.A )
					{
						NewColor.A = FMath::Min( InParams.BrushColor.A, OldColor.A + PaintAmount );
					}
					else
					{
						NewColor.A = FMath::Max( InParams.BrushColor.A, OldColor.A - PaintAmount );
					}
				}
			}
			else if( InParams.PaintMode == EMeshPaintMode::PaintWeights )
			{
				// Weight painting
				
				
				// Total number of texture blend weights we're using
				check( InParams.TotalWeightCount > 0 );
				check( InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedWeights );

				// True if we should assume the last weight index is composed of one minus the sum of all
				// of the other weights.  This effectively allows an additional weight with no extra memory
				// used, but potentially requires extra pixel shader instructions to render.
				//
				// NOTE: If you change the default here, remember to update the MeshPaintWindow UI and strings
				//
				// NOTE: Materials must be authored to match the following assumptions!
				const bool bUsingOneMinusTotal =
					InParams.TotalWeightCount == 2 ||		// Two textures: Use a lerp() in pixel shader (single value)
					InParams.TotalWeightCount == 5;			// Five texture: Requires 1.0-sum( R+G+B+A ) in shader
				check( bUsingOneMinusTotal || InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedPhysicalWeights );

				// Prefer to use RG/RGB instead of AR/ARG when we're only using 2/3 physical weights
				const int32 TotalPhysicalWeights = bUsingOneMinusTotal ? InParams.TotalWeightCount - 1 : InParams.TotalWeightCount;
				const bool bUseColorAlpha =
					TotalPhysicalWeights != 2 &&			// Two physical weights: Use RG instead of AR
					TotalPhysicalWeights != 3;				// Three physical weights: Use RGB instead of ARG

				// Index of the blend weight that we're painting
				check( InParams.PaintWeightIndex >= 0 && InParams.PaintWeightIndex < MeshPaintDefs::MaxSupportedWeights );


				// Convert the color value to an array of weights
				float Weights[ MeshPaintDefs::MaxSupportedWeights ];
				{
					for( int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{											  
						if( CurWeightIndex == TotalPhysicalWeights )
						{
							// This weight's value is one minus the sum of all previous weights
							float OtherWeightsTotal = 0.0f;
							for( int32 OtherWeightIndex = 0; OtherWeightIndex < CurWeightIndex; ++OtherWeightIndex )
							{
								OtherWeightsTotal += Weights[ OtherWeightIndex ];
							}
							Weights[ CurWeightIndex ] = 1.0f - OtherWeightsTotal;
						}
						else
						{
							switch( CurWeightIndex )
							{
								case 0:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.A : OldColor.R;
									break;

								case 1:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.R : OldColor.G;
									break;

								case 2:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.G : OldColor.B;
									break;

								case 3:
									check( bUseColorAlpha );
									Weights[ CurWeightIndex ] = OldColor.B;
									break;

								default:
									UE_LOG(LogMeshPaintEdMode, Fatal, TEXT( "Invalid weight index" ) );
									break;
							}
						}
					}
				}
				

				// Go ahead any apply paint!
				{
					Weights[ InParams.PaintWeightIndex ] += PaintAmount;
					Weights[ InParams.PaintWeightIndex ] = FMath::Clamp( Weights[ InParams.PaintWeightIndex ], 0.0f, 1.0f );
				}


				// Now renormalize all of the other weights
				{
					float OtherWeightsTotal = 0.0f;
					for( int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						if( CurWeightIndex != InParams.PaintWeightIndex )
						{
							OtherWeightsTotal += Weights[ CurWeightIndex ];
						}
					}
					const float NormalizeTarget = 1.0f - Weights[ InParams.PaintWeightIndex ];
					for( int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						if( CurWeightIndex != InParams.PaintWeightIndex )
						{
							if( OtherWeightsTotal == 0.0f )
							{
								Weights[ CurWeightIndex ] = NormalizeTarget / ( InParams.TotalWeightCount - 1 );
							}
							else
							{
								Weights[ CurWeightIndex ] = Weights[ CurWeightIndex ] / OtherWeightsTotal * NormalizeTarget;
							}
						}
					}
				}


				// The total of the weights should now always equal 1.0
				{
					float WeightsTotal = 0.0f;
					for( int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						WeightsTotal += Weights[ CurWeightIndex ];
					}
					check( FMath::IsNearlyEqual( WeightsTotal, 1.0f, 0.01f ) );
				}


				// Convert the weights back to a color value					
				{
					for( int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						// We can skip the non-physical weights as it's already baked into the others
						if( CurWeightIndex != TotalPhysicalWeights )
						{
							switch( CurWeightIndex )
							{
								case 0:
									if( bUseColorAlpha )
									{
										NewColor.A = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.R = Weights[ CurWeightIndex ];
									}
									break;

								case 1:
									if( bUseColorAlpha )
									{
										NewColor.R = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.G = Weights[ CurWeightIndex ];
									}
									break;

								case 2:
									if( bUseColorAlpha )
									{
										NewColor.G = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.B = Weights[ CurWeightIndex ];
									}
									break;

								case 3:
									NewColor.B = Weights[ CurWeightIndex ];
									break;

								default:
									UE_LOG(LogMeshPaintEdMode, Fatal, TEXT( "Invalid weight index" ) );
									break;
							}
						}
					}
				}
				
			}




			// Save the new color
			InOutVertexColor.R = FMath::Clamp( FMath::RoundToInt( NewColor.R * 255.0f ), 0, 255 );
			InOutVertexColor.G = FMath::Clamp( FMath::RoundToInt( NewColor.G * 255.0f ), 0, 255 );
			InOutVertexColor.B = FMath::Clamp( FMath::RoundToInt( NewColor.B * 255.0f ), 0, 255 );
			InOutVertexColor.A = FMath::Clamp( FMath::RoundToInt( NewColor.A * 255.0f ), 0, 255 );


			// UE_LOG(LogMeshPaintEdMode, Log,  TEXT( "Painted Vertex:  OldColor=[%.2f,%.2f,%.2f,%.2f], NewColor=[%.2f,%.2f,%.2f,%.2f]" ), OldColor.R, OldColor.G, OldColor.B, OldColor.A, NewColor.R, NewColor.G, NewColor.B, NewColor.A );
		}

		return true;
	}


	// Out of range
	return false;
}

void FEdModeMeshPaint::CreateGeometryAdaptersForSelectedComponents()
{
	TArray<UMeshComponent*> SelectedComponents = GetSelectedMeshComponents();
	for (UMeshComponent* SelectedComponent : SelectedComponents)
	{
		AddGeometryAdapter(SelectedComponent);
	}
}

IMeshPaintGeometryAdapter* FEdModeMeshPaint::AddGeometryAdapter(UMeshComponent* MeshComponent)
{
	check(MeshComponent);
	check(!ComponentToAdapterMap.Contains(MeshComponent));
	TSharedPtr<IMeshPaintGeometryAdapter> NewAdapter = FMeshPaintAdapterFactory::CreateAdapterForMesh(MeshComponent, PaintingMeshLODIndex, /*TODO: Shouldn't be part of the construction contract: FMeshPaintSettings::Get().UVChannel*/ 0);
	if (NewAdapter.IsValid())
	{
		ComponentToAdapterMap.Add(MeshComponent, NewAdapter);
		NewAdapter->OnAdded();
	}
	return NewAdapter.Get();
}

void FEdModeMeshPaint::RemoveGeometryAdapter(UMeshComponent* MeshComponent)
{
	check(MeshComponent);
	TSharedPtr<IMeshPaintGeometryAdapter>* MeshAdapter = ComponentToAdapterMap.Find(MeshComponent);
	check(MeshAdapter);
	(*MeshAdapter)->OnRemoved();
	verify(ComponentToAdapterMap.Remove(MeshComponent) == 1);
}

IMeshPaintGeometryAdapter* FEdModeMeshPaint::FindGeometryAdapter(UMeshComponent* MeshComponent)
{
	TSharedPtr<IMeshPaintGeometryAdapter>* MeshAdapter = ComponentToAdapterMap.Find(MeshComponent);
	if (!MeshAdapter)
	{
		return nullptr;
	}
	else
	{
		return MeshAdapter->Get();
	}
}

void FEdModeMeshPaint::RemoveAllGeometryAdapters()
{
	// Remove all components from the map
	for (auto It = ComponentToAdapterMap.CreateIterator(); It; ++It)
	{
		It.Value()->OnRemoved();
		It.RemoveCurrent();
	}
}

void FEdModeMeshPaint::OnPreSaveWorld(uint32 SaveFlags, UWorld* World)
{
	RemoveAllGeometryAdapters();
}

void FEdModeMeshPaint::OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess)
{
	CreateGeometryAdaptersForSelectedComponents();
}

void FEdModeMeshPaint::OnPostImportAsset(UFactory* Factory, UObject* Object)
{
	if (Object->IsA(UStaticMesh::StaticClass()))
	{
		RemoveAllGeometryAdapters();
		CreateGeometryAdaptersForSelectedComponents();
	}
}

void FEdModeMeshPaint::OnPostReimportAsset(UObject* Object, bool bSuccess)
{
	if (bSuccess && Object->IsA(UStaticMesh::StaticClass()))
	{
		RemoveAllGeometryAdapters();
		CreateGeometryAdaptersForSelectedComponents();
	}
}

void FEdModeMeshPaint::OnAssetRemoved(const FAssetData& AssetData)
{
	RemoveAllGeometryAdapters();
	CreateGeometryAdaptersForSelectedComponents();
}

void FEdModeMeshPaint::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (const auto& OldToNew : OldToNewInstanceMap)
	{
		if (UMeshComponent* OldMeshComponent = Cast<UMeshComponent>(OldToNew.Key))
		{
			if (ComponentToAdapterMap.Contains(OldMeshComponent))
			{
				if (UMeshComponent* NewMeshComponent = Cast<UMeshComponent>(OldToNew.Value))
				{
					AddGeometryAdapter(NewMeshComponent);
				}

				RemoveGeometryAdapter(OldMeshComponent);
			}
		}
	}
}

/** Paint the mesh that impacts the specified ray */
void FEdModeMeshPaint::DoPaint( const FVector& InCameraOrigin,
								const FVector& InRayOrigin,
								const FVector& InRayDirection,
								FPrimitiveDrawInterface* PDI,
								const EMeshPaintAction::Type InPaintAction,
								const bool bVisualCueOnly,
								const float InStrengthScale,
								OUT bool& bAnyPaintableActorsUnderCursor)
{
	const float BrushRadius = GetBrushRadiiDefault();

	TArray<UMeshComponent*> PaintableComponents;

	// Fire out a ray to see if there is a *selected* component under the mouse cursor that can be painted.
	// NOTE: We can't use a GWorld line check for this as that would ignore components that have collision disabled
	FHitResult BestTraceResult;
	{
		const FVector TraceStart( InRayOrigin );
		const FVector TraceEnd( InRayOrigin + InRayDirection * HALF_WORLD_MAX );

		TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedMeshComponents();

		for (UMeshComponent* MeshComponent : SelectedMeshComponents)
		{
			IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
			if (!MeshAdapter)
			{
				continue;
			}

			//@TODO: MESHPAINT: This is copied from the original logic, but the split between this loop and the next still feels a bit off
			if (InPaintAction == EMeshPaintAction::Fill)
			{
				PaintableComponents.AddUnique(MeshComponent);
			}
			else if (InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh)
			{
				PaintableComponents.AddUnique(MeshComponent);
			}

			// Ray trace
			FHitResult TraceHitResult(1.0f);

			static FName DoPaintName(TEXT("DoPaint"));
			if (MeshAdapter->LineTraceComponent(TraceHitResult, TraceStart, TraceEnd, FCollisionQueryParams(DoPaintName, true)))
			{
				// Find the closest impact
				if ((BestTraceResult.GetComponent() == nullptr) || (TraceHitResult.Time < BestTraceResult.Time))
				{
					BestTraceResult = TraceHitResult;
				}
			}
		}

		if (BestTraceResult.GetComponent() != NULL)
		{
			// If we're using texture paint, just use the best trace result we found as we currently only
			// support painting a single mesh at a time in that mode.
			if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
			{
				UMeshComponent* ComponentToPaint = CastChecked<UMeshComponent>(BestTraceResult.GetComponent());

				if (ComponentToAdapterMap.Contains(ComponentToPaint))
				{
					PaintableComponents.AddUnique(ComponentToPaint);
				}
			}
			else
			{
				FBox BrushBounds = FBox::BuildAABB( BestTraceResult.Location, FVector( BrushRadius * 1.25f, BrushRadius * 1.25f, BrushRadius * 1.25f ) );

				// Vertex paint mode, so we want all valid components overlapping the brush hit location
				for (auto TestComponent : SelectedMeshComponents)
				{
					const FBox ComponentBounds = TestComponent->Bounds.GetBox();
					
					if (ComponentToAdapterMap.Contains(TestComponent) && ComponentBounds.Intersect(BrushBounds))
					{
						// OK, this mesh potentially overlaps the brush!
						PaintableComponents.AddUnique(TestComponent);
					}
				}
			}
		}
	}

	bAnyPaintableActorsUnderCursor = (PaintableComponents.Num() > 0);

	// Are we actually applying paint here?
	const bool bShouldApplyPaint = bAnyPaintableActorsUnderCursor && ( (bIsPainting && !bVisualCueOnly) || 
		(InPaintAction == EMeshPaintAction::Fill) || 
		(InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh) );

	// See if a Fill or PushInstanceColorsToMesh operation is requested, if so we will start an Undo/Redo transaction here
	const bool bDoSingleFrameTransaction = ( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors ) &&
		PaintableComponents.Num() > 0 &&
		( ( InPaintAction == EMeshPaintAction::Fill ) || ( InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh ) );

	const bool bDoMultiFrameTransaction = ( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors ) &&
		PaintableComponents.Num() > 0 &&
		( ( InPaintAction == EMeshPaintAction::Erase ) || ( InPaintAction == EMeshPaintAction::Paint ) );

	// Starts an Undo/Redo transaction with the appropriate label if we don't have any transactions in progress.
	if( bShouldApplyPaint && ( bDoSingleFrameTransaction || bDoMultiFrameTransaction ) && ScopedTransaction == NULL )
	{
		FText TransDesc;
		if( InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh )
		{
			TransDesc = LOCTEXT( "MeshPaintMode_VertexPaint_TransactionPushInstColorToMesh", "Copy Instance Colors To Mesh" );
		}
		else if( InPaintAction == EMeshPaintAction::Fill )
		{
			TransDesc = LOCTEXT( "MeshPaintMode_VertexPaint_TransactionFill", "Fill Vertex Colors" );
		}
		else if( InPaintAction == EMeshPaintAction::Erase || InPaintAction == EMeshPaintAction::Paint )
		{
			TransDesc = LOCTEXT( "MeshPaintMode_VertexPaint_TransactionPaintStroke", "Vertex Paint" );
		}
		BeginTransaction( TransDesc );
	}

	// Iterate over the selected meshes under the cursor and paint them!
	for (UMeshComponent* MeshComponent : PaintableComponents)
	{
		IMeshPaintGeometryAdapter* MeshAdapter = ComponentToAdapterMap.FindRef(MeshComponent).Get();
		if (!ensure(MeshAdapter))
		{
			continue;
		}

		MeshAdapter->SetCurrentUVChannelIndex(FMeshPaintSettings::Get().UVChannel);

		// Brush properties
		const float BrushDepth = BrushRadius;	// NOTE: Actually half of the total depth (like a radius)
		const float BrushFalloffAmount = FMeshPaintSettings::Get().BrushFalloffAmount;
		const FLinearColor BrushColor = ((InPaintAction == EMeshPaintAction::Paint) || (InPaintAction == EMeshPaintAction::Fill))? FMeshPaintSettings::Get().PaintColor : FMeshPaintSettings::Get().EraseColor;

		// NOTE: We square the brush strength to maximize slider precision in the low range
		const float BrushStrength =
			FMeshPaintSettings::Get().BrushStrength * FMeshPaintSettings::Get().BrushStrength *
			InStrengthScale;

		// Display settings
		const float VisualBiasDistance = 0.15f;
		const float NormalLineSize( BrushRadius * 0.35f );	// Make the normal line length a function of brush size
		const FLinearColor NormalLineColor( 0.3f, 1.0f, 0.3f );
		const FLinearColor BrushCueColor = bIsPainting ? FLinearColor( 1.0f, 1.0f, 0.3f ) : FLinearColor( 0.3f, 1.0f, 0.3f );
		const FLinearColor InnerBrushCueColor = bIsPainting ? FLinearColor( 0.5f, 0.5f, 0.1f ) : FLinearColor( 0.1f, 0.5f, 0.1f );

		FVector BrushXAxis, BrushYAxis;
		BestTraceResult.Normal.FindBestAxisVectors( BrushXAxis, BrushYAxis );
		const FVector BrushVisualPosition = BestTraceResult.Location + BestTraceResult.Normal * VisualBiasDistance;


		// Precache model -> world transform
		const FMatrix ComponentToWorldMatrix = MeshComponent->ComponentToWorld.ToMatrixWithScale();


		// Compute the camera position in actor space.  We need this later to check for
		// backfacing triangles.
		const FVector ComponentSpaceCameraPosition( ComponentToWorldMatrix.InverseTransformPosition( InCameraOrigin ) );
		const FVector ComponentSpaceBrushPosition( ComponentToWorldMatrix.InverseTransformPosition( BestTraceResult.Location ) );
		
		// @todo MeshPaint: Input vector doesn't work well with non-uniform scale
		const float ComponentSpaceBrushRadius = ComponentToWorldMatrix.InverseTransformVector( FVector( BrushRadius, 0.0f, 0.0f ) ).Size();
		const float ComponentSpaceSquaredBrushRadius = ComponentSpaceBrushRadius * ComponentSpaceBrushRadius;


		if( PDI != NULL )
		{
			// Draw brush circle
			const int32 NumCircleSides = 64;
			DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, BrushCueColor, BrushRadius, NumCircleSides, SDPG_World );

			// Also draw the inner brush radius
			const float InnerBrushRadius = BrushRadius - BrushFalloffAmount * BrushRadius;
			DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, InnerBrushCueColor, InnerBrushRadius, NumCircleSides, SDPG_World );

			// If we just started painting then also draw a little brush effect
			if( bIsPainting )
			{
				const float EffectDuration = 0.2f;

				const double CurTime = FPlatformTime::Seconds();
				const float TimeSinceStartedPainting = (float)( CurTime - PaintingStartTime );
				if( TimeSinceStartedPainting <= EffectDuration )
				{
					// Invert the effect if we're currently erasing
					float EffectAlpha = TimeSinceStartedPainting / EffectDuration;
					if( InPaintAction == EMeshPaintAction::Erase )
					{
						EffectAlpha = 1.0f - EffectAlpha;
					}

					const FLinearColor EffectColor( 0.1f + EffectAlpha * 0.4f, 0.1f + EffectAlpha * 0.4f, 0.1f + EffectAlpha * 0.4f );
					const float EffectRadius = BrushRadius * EffectAlpha * EffectAlpha;	// Squared curve here (looks more interesting)
					DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, EffectColor, EffectRadius, NumCircleSides, SDPG_World );
				}
			}

			// Draw trace surface normal
			const FVector NormalLineEnd( BrushVisualPosition + BestTraceResult.Normal * NormalLineSize );
			PDI->DrawLine( BrushVisualPosition, NormalLineEnd, NormalLineColor, SDPG_World );
		}



		// Mesh paint settings
		FMeshPaintParameters Params;
		{
			Params.PaintMode = FMeshPaintSettings::Get().PaintMode;
			Params.PaintAction = InPaintAction;
			Params.BrushPosition = BestTraceResult.Location;
			Params.BrushNormal = BestTraceResult.Normal;
			Params.BrushColor = BrushColor;
			Params.SquaredBrushRadius = BrushRadius * BrushRadius;
			Params.BrushRadialFalloffRange = BrushFalloffAmount * BrushRadius;
			Params.InnerBrushRadius = BrushRadius - Params.BrushRadialFalloffRange;
			Params.BrushDepth = BrushDepth;
			Params.BrushDepthFalloffRange = BrushFalloffAmount * BrushDepth;
			Params.InnerBrushDepth = BrushDepth - Params.BrushDepthFalloffRange;
			Params.BrushStrength = BrushStrength;
			Params.BrushToWorldMatrix = FMatrix( BrushXAxis, BrushYAxis, Params.BrushNormal, Params.BrushPosition );
			Params.InverseBrushToWorldMatrix = Params.BrushToWorldMatrix.InverseFast();
			Params.bWriteRed = FMeshPaintSettings::Get().bWriteRed;
			Params.bWriteGreen = FMeshPaintSettings::Get().bWriteGreen;
			Params.bWriteBlue = FMeshPaintSettings::Get().bWriteBlue;
			Params.bWriteAlpha = FMeshPaintSettings::Get().bWriteAlpha;
			Params.TotalWeightCount = FMeshPaintSettings::Get().TotalWeightCount;

			// Select texture weight index based on whether or not we're painting or erasing
			{
				const int32 PaintWeightIndex = 
					( InPaintAction == EMeshPaintAction::Paint ) ? FMeshPaintSettings::Get().PaintWeightIndex : FMeshPaintSettings::Get().EraseWeightIndex;

				// Clamp the weight index to fall within the total weight count
				Params.PaintWeightIndex = FMath::Clamp( PaintWeightIndex, 0, Params.TotalWeightCount - 1 );
			}

			// @todo MeshPaint: Ideally we would default to: TexturePaintingCurrentMeshComponent->StaticMesh->LightMapCoordinateIndex
			//		Or we could indicate in the GUI which channel is the light map set (button to set it?)
			Params.UVChannel = FMeshPaintSettings::Get().UVChannel;
		}

		if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors)
		{
			if (MeshAdapter->SupportsVertexPaint())
			{
				if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
				{
					if (UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh)
					{
						//@TODO: MESHPAINT: Direct assumptions about StaticMeshComponent
						check(StaticMesh->GetNumLODs() > PaintingMeshLODIndex);
						FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];

						// Painting vertex colors
						PaintMeshVertices(StaticMeshComponent, Params, bShouldApplyPaint, LODModel, ComponentSpaceCameraPosition, ComponentToWorldMatrix, ComponentSpaceSquaredBrushRadius, ComponentSpaceBrushPosition, PDI, VisualBiasDistance, *MeshAdapter);
					}
				}
			}
		}
		else
		{
			// Painting textures
			PaintMeshTexture(MeshComponent, Params, bShouldApplyPaint, ComponentSpaceCameraPosition, ComponentToWorldMatrix, ComponentSpaceSquaredBrushRadius, ComponentSpaceBrushPosition, *MeshAdapter);
		}
	}

	// Ends an Undo/Redo transaction, but only for Fill or PushInstanceColorsToMesh operations. Multi frame transactions will end when the user stops painting.
	if( bDoSingleFrameTransaction )
	{
		EndTransaction();
	}
}

static bool PropagateColorsToRawMesh(UStaticMesh* StaticMesh, int32 LODIndex, FStaticMeshComponentLODInfo& ComponentLODInfo)
{
	check(ComponentLODInfo.OverrideVertexColors);
	check(StaticMesh->SourceModels.IsValidIndex(LODIndex));
	check(StaticMesh->RenderData);
	check(StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex));

	UE_LOG(LogMeshPaintEdMode,Log,TEXT("Pushing colors to raw mesh: %s (LOD%d)"), *StaticMesh->GetName(), LODIndex);

	bool bPropagatedColors = false;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
	FStaticMeshRenderData& RenderData = *StaticMesh->RenderData;
	FStaticMeshLODResources& RenderModel = RenderData.LODResources[LODIndex];
	FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

	if (RenderData.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
	{
		// Use the wedge map if it is available as it is lossless.
		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

		int32 NumWedges = RawMesh.WedgeIndices.Num();
		if (RenderData.WedgeMap.Num() == NumWedges)
		{
			int32 NumExistingColors = RawMesh.WedgeColors.Num();
			if (NumExistingColors < NumWedges)
			{
				RawMesh.WedgeColors.AddUninitialized(NumWedges - NumExistingColors);
			}
			for (int32 i = 0; i < NumWedges; ++i)
			{
				FColor WedgeColor = FColor::White;
				int32 Index = RenderData.WedgeMap[i];
				if (Index != INDEX_NONE)
				{
					WedgeColor = ColorVertexBuffer.VertexColor(Index);
				}
				RawMesh.WedgeColors[i] = WedgeColor;
			}
			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
			bPropagatedColors = true;
		}
		else
		{
			UE_LOG(LogMeshPaintEdMode,Warning,TEXT("Wedge map size %d is wrong. Expected %d."),RenderData.WedgeMap.Num(),RawMesh.WedgeIndices.Num());
		}
	}
	else
	{
		// If there's no raw mesh data, don't try to do any fixup here
		if (SrcModel.RawMeshBulkData->IsEmpty())
		{
			return false;
		}

		// Fall back to mapping based on position.
		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

		TArray<FColor> NewVertexColors;
		FPositionVertexBuffer TempPositionVertexBuffer;
		TempPositionVertexBuffer.Init(RawMesh.VertexPositions);
		RemapPaintedVertexColors(
			ComponentLODInfo.PaintedVertices,
			*ComponentLODInfo.OverrideVertexColors,
			TempPositionVertexBuffer,
			/*OptionalVertexBuffer=*/ NULL,
			NewVertexColors
			);
		if (NewVertexColors.Num() == RawMesh.VertexPositions.Num())
		{
			int32 NumWedges = RawMesh.WedgeIndices.Num();
			RawMesh.WedgeColors.Empty(NumWedges);
			RawMesh.WedgeColors.AddZeroed(NumWedges);
			for (int32 i = 0; i < NumWedges; ++i)
			{
				int32 Index = RawMesh.WedgeIndices[i];
				RawMesh.WedgeColors[i] = NewVertexColors[Index];
			}
			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
			bPropagatedColors = true;
		}
	}
	return bPropagatedColors;
}

/** Paints mesh vertices */
void FEdModeMeshPaint::PaintMeshVertices( 
	UStaticMeshComponent* StaticMeshComponent, 
	const FMeshPaintParameters& Params, 
	const bool bShouldApplyPaint, 
	FStaticMeshLODResources& LODModel, 
	const FVector& ComponentSpaceCameraPosition, 
	const FMatrix& ComponentToWorldMatrix,
	const float ComponentSpaceSquaredBrushRadius,
	const FVector& ComponentSpaceBrushPosition,
	FPrimitiveDrawInterface* PDI, 
	const float VisualBiasDistance,
	const IMeshPaintGeometryAdapter& GeometryInfo)
{
	const bool bOnlyFrontFacing = FMeshPaintSettings::Get().bOnlyFrontFacingTriangles;
	const bool bUsingInstancedVertexColors = ( FMeshPaintSettings::Get().VertexPaintTarget == EMeshVertexPaintTarget::ComponentInstance ) && (Params.PaintAction != EMeshPaintAction::PushInstanceColorsToMesh);

	const float InfluencedVertexCuePointSize = 3.5f;

	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;


	// Paint the mesh
	uint32 NumVerticesInfluencedByBrush = 0;
	{
		TScopedPointer< FStaticMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
		TScopedPointer< FComponentReregisterContext > ComponentReregisterContext;


		FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;
		if( bUsingInstancedVertexColors)
		{
			if( bShouldApplyPaint )
			{
				// We're only changing instanced vertices on this specific mesh component, so we
				// only need to detach our mesh component
				ComponentReregisterContext.Reset( new FComponentReregisterContext( StaticMeshComponent ) );

				// Mark the mesh component as modified
				StaticMeshComponent->SetFlags(RF_Transactional);
				StaticMeshComponent->Modify();

				// Ensure LODData has enough entries in it, free not required.
				StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());

				InstanceMeshLODInfo = &StaticMeshComponent->LODData[ PaintingMeshLODIndex ];

				// Destroy the instance vertex  color array if it doesn't fit
				if(InstanceMeshLODInfo->OverrideVertexColors
					&& InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() != LODModel.GetNumVertices())
				{
					InstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();
				}

				// Destroy the cached paint data every paint. Painting redefines the source data.
				if ( InstanceMeshLODInfo->OverrideVertexColors )
				{
					InstanceMeshLODInfo->PaintedVertices.Empty();
				}

				if(InstanceMeshLODInfo->OverrideVertexColors)
				{
					InstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
					FlushRenderingCommands();
				}
				else
				{
					// Setup the instance vertex color array if we don't have one yet
					InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

					if((int32)LODModel.ColorVertexBuffer.GetNumVertices() >= LODModel.GetNumVertices())
					{
						// copy mesh vertex colors to the instance ones
						InstanceMeshLODInfo->OverrideVertexColors->InitFromColorArray(&LODModel.ColorVertexBuffer.VertexColor(0), LODModel.GetNumVertices());
					}
					else
					{
						bool bConvertSRGB = false;
						FColor FillColor = Params.BrushColor.ToFColor(bConvertSRGB);
						// Original mesh didn't have any colors, so just use a default color
						InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor::White, LODModel.GetNumVertices());
					}

				}
				// See if the component has to cache its mesh vertex positions associated with override colors
				StaticMeshComponent->CachePaintedDataIfNecessary();
				StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
			}
			else
			{
				if( StaticMeshComponent->LODData.Num() > PaintingMeshLODIndex )
				{
					InstanceMeshLODInfo = &StaticMeshComponent->LODData[ PaintingMeshLODIndex ];
				}
			}
		}
		else
		{
			if( bShouldApplyPaint )
			{
				// We're changing the mesh itself, so ALL static mesh components in the scene will need
				// to be unregistered for this (and reregistered afterwards.)
				RecreateRenderStateContext.Reset( new FStaticMeshComponentRecreateRenderStateContext( StaticMesh ) );

				// Dirty the mesh
				StaticMesh->SetFlags(RF_Transactional);
				StaticMesh->Modify();

				if( Params.PaintAction == EMeshPaintAction::PushInstanceColorsToMesh )
				{
					StaticMeshComponent->SetFlags(RF_Transactional);
					StaticMeshComponent->Modify();
				}

				// Add to our modified list
				ModifiedStaticMeshes.AddUnique( StaticMesh );

				// Release the static mesh's resources.
				StaticMesh->ReleaseResources();

				// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
				// allocated, and potentially accessing the UStaticMesh.
				StaticMesh->ReleaseResourcesFence.Wait();

			}
		}



		// Paint the mesh vertices
		{
			if (Params.PaintAction == EMeshPaintAction::Fill)
			{
				//flood fill
				bool bConvertSRGB = false;
				FColor FillColor = Params.BrushColor.ToFColor(bConvertSRGB);
				FColor NewMask = FColor(Params.bWriteRed ? 255 : 0, Params.bWriteGreen ? 255 : 0, Params.bWriteBlue ? 255 : 0, Params.bWriteAlpha ? 255 : 0);
				FColor KeepMaskColor (~NewMask.DWColor());

				FColor MaskedFillColor = FillColor;
				MaskedFillColor.R &= NewMask.R;
				MaskedFillColor.G &= NewMask.G;
				MaskedFillColor.B &= NewMask.B;
				MaskedFillColor.A &= NewMask.A;

				//make sure there is room if we're painting on the source mesh
				if( !bUsingInstancedVertexColors && LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
				{
					// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
					LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.GetNumVertices());
				}

				uint32 NumVertices = LODModel.GetNumVertices();

				for (uint32 ColorIndex = 0; ColorIndex < NumVertices; ++ColorIndex)
				{
					FColor CurrentColor;
					if( bUsingInstancedVertexColors )
					{
						check(InstanceMeshLODInfo->OverrideVertexColors);
						check((uint32)ColorIndex < InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());

						CurrentColor = InstanceMeshLODInfo->OverrideVertexColors->VertexColor( ColorIndex );
					}
					else
					{
						CurrentColor = LODModel.ColorVertexBuffer.VertexColor( ColorIndex );
					}

					CurrentColor.R &= KeepMaskColor.R;
					CurrentColor.G &= KeepMaskColor.G;
					CurrentColor.B &= KeepMaskColor.B;
					CurrentColor.A &= KeepMaskColor.A;
					CurrentColor += MaskedFillColor;

					if( bUsingInstancedVertexColors )
					{
						check( InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == InstanceMeshLODInfo->PaintedVertices.Num() );
						InstanceMeshLODInfo->OverrideVertexColors->VertexColor( ColorIndex ) = CurrentColor;
						InstanceMeshLODInfo->PaintedVertices[ ColorIndex ].Color = CurrentColor;
					}
					else
					{
						LODModel.ColorVertexBuffer.VertexColor( ColorIndex ) = CurrentColor;
					}
				}
				FEditorSupportDelegates::RedrawAllViewports.Broadcast();
			}
			else if (Params.PaintAction == EMeshPaintAction::PushInstanceColorsToMesh)
			{
				InstanceMeshLODInfo = &StaticMeshComponent->LODData[ PaintingMeshLODIndex ];
				if (InstanceMeshLODInfo->OverrideVertexColors)
				{
					// Try using the mapping generated when building the mesh.
					if(PropagateColorsToRawMesh(StaticMesh, PaintingMeshLODIndex, *InstanceMeshLODInfo))
					{
						for (int32 LODIndex = 1; LODIndex < StaticMesh->RenderData->LODResources.Num(); LODIndex++)
						{
							PropagateColorsToRawMesh(StaticMesh, LODIndex, *InstanceMeshLODInfo);
						}
						RemoveComponentInstanceVertexColors(StaticMeshComponent);
						StaticMesh->Build();
					}
				}
				FEditorSupportDelegates::RedrawAllViewports.Broadcast();
			}
			else
			{
				// Get a list of (optionally front-facing) triangles that are within a reasonable distance to the brush
				TArray<uint32> InfluencedTriangles = GeometryInfo.SphereIntersectTriangles(
					ComponentSpaceSquaredBrushRadius,
					ComponentSpaceBrushPosition,
					ComponentSpaceCameraPosition,
					bOnlyFrontFacing);

				// Make sure we're dealing with triangle lists
				FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
				const int32 NumIndexBufferIndices = Indices.Num();
				check(NumIndexBufferIndices % 3 == 0);

				// Get a list of unique vertices indexed by the influenced triangles
				TSet<int32> InfluencedVertices;
				InfluencedVertices.Reserve(InfluencedTriangles.Num());
				for (int32 InfluencedTriangle : InfluencedTriangles)
				{
					InfluencedVertices.Add(Indices[InfluencedTriangle * 3 + 0]);
					InfluencedVertices.Add(Indices[InfluencedTriangle * 3 + 1]);
					InfluencedVertices.Add(Indices[InfluencedTriangle * 3 + 2]);
				}

				// Get mesh vertex array
				const TArray<FVector>& MeshVertices = GeometryInfo.GetMeshVertices();

				for (int32 VertexIndex : InfluencedVertices)
				{
					// Grab the mesh vertex and transform it to world space
					FVector	ModelSpaceVertexPosition = MeshVertices[ VertexIndex ];
					FVector WorldSpaceVertexPosition = ComponentToWorldMatrix.TransformPosition( ModelSpaceVertexPosition );

					FColor OriginalVertexColor = FColor( 255, 255, 255 );

					// Grab vertex color (read/write)
					if( bUsingInstancedVertexColors )
					{
						if( InstanceMeshLODInfo 
						&& InstanceMeshLODInfo->OverrideVertexColors
						&& InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == LODModel.GetNumVertices() )
						{
							// Actor mesh component LOD
							OriginalVertexColor = InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex );
						}
					}
					else
					{
						// Static mesh
						if( bShouldApplyPaint )
						{
							if( LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
							{
								// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
								LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.GetNumVertices());

								// @todo MeshPaint: Make sure this is the best place to do this
								BeginInitResource( &LODModel.ColorVertexBuffer );
							}
						}

						if( LODModel.ColorVertexBuffer.GetNumVertices() > 0 )
						{
							check( (int32)LODModel.ColorVertexBuffer.GetNumVertices() > VertexIndex );
							OriginalVertexColor = LODModel.ColorVertexBuffer.VertexColor( VertexIndex );
						}
					}


					// Paint the vertex!
					FColor NewVertexColor = OriginalVertexColor;
					bool bVertexInRange = false;
					{
						FColor PaintedVertexColor = OriginalVertexColor;
						bVertexInRange = PaintVertex( WorldSpaceVertexPosition, Params, bShouldApplyPaint, PaintedVertexColor );
						if( bShouldApplyPaint )
						{
							NewVertexColor = PaintedVertexColor;
						}
					}


					if( bVertexInRange )
					{
						++NumVerticesInfluencedByBrush;

						// Update the mesh!
						if( bShouldApplyPaint )
						{
							if( bUsingInstancedVertexColors )
							{
								check(InstanceMeshLODInfo->OverrideVertexColors);
								check((uint32)VertexIndex < InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());
								check( InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == InstanceMeshLODInfo->PaintedVertices.Num() );

								InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex ) = NewVertexColor;
								InstanceMeshLODInfo->PaintedVertices[ VertexIndex ].Color = NewVertexColor;
							}
							else
							{
								LODModel.ColorVertexBuffer.VertexColor( VertexIndex ) = NewVertexColor;
							}
						}


						// Draw vertex visual cue
						if( PDI != NULL )
						{
							const FLinearColor InfluencedVertexCueColor( NewVertexColor );
							const FVector VertexVisualPosition = WorldSpaceVertexPosition + Params.BrushNormal * VisualBiasDistance;
							PDI->DrawPoint( VertexVisualPosition, InfluencedVertexCueColor, InfluencedVertexCuePointSize, SDPG_World );
						}
					}
				} 
			}
		}

		if( bShouldApplyPaint )
		{
			if( bUsingInstancedVertexColors )
			{
				BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
			}
			else
			{
				// Reinitialize the static mesh's resources.
				StaticMesh->InitResources();
			}
		}
	}
}


/** Paints mesh texture */
void FEdModeMeshPaint::PaintMeshTexture( UMeshComponent* MeshComponent, const FMeshPaintParameters& Params, const bool bShouldApplyPaint, const FVector& ComponentSpaceCameraPosition, const FMatrix& ComponentToWorldMatrix, const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const IMeshPaintGeometryAdapter& GeometryInfo )
{
	UTexture2D* TargetTexture2D = GetSelectedTexture();
	
	// No reason to continue if we dont have a target texture or we are not painting
	if ((TargetTexture2D == NULL) || !bShouldApplyPaint)
	{
		return;
	}

	const bool bOnlyFrontFacing = FMeshPaintSettings::Get().bOnlyFrontFacingTriangles;

	PaintTexture2DData* TextureData = GetPaintTargetData(TargetTexture2D);

	// Store info that tells us if the element material uses our target texture so we don't have to do a usestexture() call for each tri
	TArray<bool> SectionUsesTargetTexture;
	SectionUsesTargetTexture.AddZeroed(MeshComponent->GetNumMaterials());
	for (int32 SectionIndex = 0; SectionIndex < SectionUsesTargetTexture.Num(); SectionIndex++)
	{
		SectionUsesTargetTexture[SectionIndex] = false;

		// @todo MeshPaint: if LODs can use different materials/textures then this will cause us problems
		if (UMaterialInterface* SectionMat = MeshComponent->GetMaterial(SectionIndex))
		{
			SectionUsesTargetTexture[SectionIndex] |= DoesMaterialUseTexture(SectionMat, TargetTexture2D);

			if ((SectionUsesTargetTexture[SectionIndex] == false) && (TextureData != NULL) && (TextureData->PaintRenderTargetTexture != NULL))
			{
				// If we didn't get a match on our selected texture, we'll check to see if the the material uses a
				//  render target texture override that we put on during painting.
				SectionUsesTargetTexture[SectionIndex] |= DoesMaterialUseTexture(SectionMat, TextureData->PaintRenderTargetTexture);
			}
		}
	}

	// Get a list of (optionally front-facing) triangles that are within a reasonable distance to the brush
	TArray<uint32> InfluencedTriangles = GeometryInfo.SphereIntersectTriangles(
		ComponentSpaceSquaredBrushRadius,
		ComponentSpaceBrushPosition,
		ComponentSpaceCameraPosition,
		bOnlyFrontFacing);

	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);

	if ((StaticMeshComponent != nullptr) && (StaticMeshComponent->StaticMesh != nullptr))
	{
		//@TODO: Find a better way to move this generically to the adapter
		check(StaticMeshComponent->StaticMesh->GetNumLODs() > PaintingMeshLODIndex);
		FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];
		const int32 NumSections = LODModel.Sections.Num();

		// Filter out triangles whose subelements don't use our paint target texture in their material
		for (int32 TriIndex = 0; TriIndex < InfluencedTriangles.Num(); ++TriIndex)
		{
			bool bKeepTri = false;

			// Check to see if the sub-element that this triangle belongs to actually uses our paint target texture in its material
			for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
			{
				FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

				if ((InfluencedTriangles[TriIndex] >= Section.FirstIndex / 3) &&
					(InfluencedTriangles[TriIndex] < Section.FirstIndex / 3 + Section.NumTriangles))
				{
					// The triangle belongs to this element, now we need to check to see if the element material uses our target texture.
					if ((TargetTexture2D != NULL) && SectionUsesTargetTexture.IsValidIndex(Section.MaterialIndex) && SectionUsesTargetTexture[Section.MaterialIndex])
					{
						bKeepTri = true;
					}

					// Triangles can only be part of one element so we do not need to continue to other elements.
					break;
				}
			}

			if (!bKeepTri)
			{
				InfluencedTriangles.RemoveAtSwap(TriIndex);
				TriIndex--;
			}
		}
	}

	if ((TexturePaintingCurrentMeshComponent != nullptr) && (TexturePaintingCurrentMeshComponent != MeshComponent))
	{
		// Mesh has changed, so finish up with our previous texture
		FinishPaintingTexture();
		bIsPainting = false;
	}

	if (TexturePaintingCurrentMeshComponent == nullptr)
	{
		StartPaintingTexture(MeshComponent, GeometryInfo);
	}

	if (TexturePaintingCurrentMeshComponent != nullptr)
	{
		PaintTexture(Params, InfluencedTriangles, ComponentToWorldMatrix, GeometryInfo);
	}
}


/** Starts painting a texture */
void FEdModeMeshPaint::StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintGeometryAdapter& GeometryInfo)
{
	check( InMeshComponent != NULL );
	check( TexturePaintingCurrentMeshComponent == NULL );
	check( PaintingTexture2D == NULL );

	const auto FeatureLevel = GetWorld()->FeatureLevel;
	
	UTexture2D* Texture2D = GetSelectedTexture();
	if( Texture2D == NULL )
	{
		return;
	}

	bool bStartedPainting = false;
	PaintTexture2DData* TextureData = GetPaintTargetData( Texture2D );

	// Check all the materials on the mesh to see if the user texture is there
	int32 MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	while( MaterialToCheck != NULL )
	{
		bool bIsTextureUsed = DoesMeshComponentUseTexture(InMeshComponent, Texture2D);

		if (!bIsTextureUsed && (TextureData != nullptr) && (TextureData->PaintRenderTargetTexture != nullptr))
		{
			bIsTextureUsed = DoesMeshComponentUseTexture(InMeshComponent, TextureData->PaintRenderTargetTexture);
		}

		if (bIsTextureUsed && !bStartedPainting)
		{
			bool bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			if( !bIsSourceTextureStreamedIn )
			{
				// We found that this texture is used in one of the meshes materials but not fully loaded, we will
				//   attempt to fully stream in the texture before we try to do anything with it.
				Texture2D->SetForceMipLevelsToBeResident(30.0f);
				Texture2D->WaitForStreaming();

				// We do a quick sanity check to make sure it is streamed fully streamed in now.
				bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			}

			if( bIsSourceTextureStreamedIn )
			{
				const int32 TextureWidth = Texture2D->Source.GetSizeX();
				const int32 TextureHeight = Texture2D->Source.GetSizeY();
				
				if( TextureData == NULL )
				{
					TextureData = AddPaintTargetData( Texture2D );
				}
				check( TextureData != NULL );

				// Create our render target texture
				if( TextureData->PaintRenderTargetTexture == NULL ||
					TextureData->PaintRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
					TextureData->PaintRenderTargetTexture->GetSurfaceHeight() != TextureHeight )
				{
					TextureData->PaintRenderTargetTexture = NULL;
					TextureData->PaintRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
					TextureData->PaintRenderTargetTexture->bNeedsTwoCopies = true;
					const bool bForceLinearGamma = true;
					TextureData->PaintRenderTargetTexture->InitCustomFormat( TextureWidth, TextureHeight, PF_A16B16G16R16, bForceLinearGamma );
					TextureData->PaintRenderTargetTexture->UpdateResourceImmediate();
		
					//Duplicate the texture we are painting and store it in the transient package. This texture is a backup of the data incase we want to revert before commiting.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)StaticDuplicateObject(Texture2D, GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *Texture2D->GetName()));
				}
				TextureData->PaintRenderTargetTexture->AddressX = Texture2D->AddressX;
				TextureData->PaintRenderTargetTexture->AddressY = Texture2D->AddressY;

				const int32 BrushTargetTextureWidth = TextureWidth;
				const int32 BrushTargetTextureHeight = TextureHeight;

				// Create the rendertarget used to store our paint delta
				if( BrushRenderTargetTexture == NULL ||
					BrushRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
					BrushRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight )
				{
					BrushRenderTargetTexture = NULL;
					BrushRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
					const bool bForceLinearGamma = true;
					BrushRenderTargetTexture->ClearColor = FLinearColor::Black;
					BrushRenderTargetTexture->bNeedsTwoCopies = true;
					BrushRenderTargetTexture->InitCustomFormat( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_A16B16G16R16, bForceLinearGamma );
					BrushRenderTargetTexture->UpdateResourceImmediate();
					BrushRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
					BrushRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
				}

				const bool bEnableSeamPainting = FMeshPaintSettings::Get().bEnableSeamPainting;
				if( bEnableSeamPainting )
				{
				
					// Create the rendertarget used to store a mask for our paint delta area 
					if( BrushMaskRenderTargetTexture == NULL ||
						BrushMaskRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
						BrushMaskRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight )
					{
						BrushMaskRenderTargetTexture = NULL;
						BrushMaskRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
						const bool bForceLinearGamma = true;
						BrushMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
						BrushMaskRenderTargetTexture->bNeedsTwoCopies = true;
						BrushMaskRenderTargetTexture->InitCustomFormat( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_B8G8R8A8, bForceLinearGamma );
						BrushMaskRenderTargetTexture->UpdateResourceImmediate();
						BrushMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
						BrushMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
					}								

					// Create the rendertarget used to store a texture seam mask
					if( SeamMaskRenderTargetTexture == NULL ||
						SeamMaskRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
						SeamMaskRenderTargetTexture->GetSurfaceHeight() != TextureHeight )
					{
						SeamMaskRenderTargetTexture = NULL;
						SeamMaskRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
						const bool bForceLinearGamma = true;
						SeamMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
						SeamMaskRenderTargetTexture->bNeedsTwoCopies = true;
						SeamMaskRenderTargetTexture->InitCustomFormat( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_B8G8R8A8, bForceLinearGamma );
						SeamMaskRenderTargetTexture->UpdateResourceImmediate();
						SeamMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
						SeamMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
					}
					bGenerateSeamMask = true;
				}
				
				bStartedPainting = true;				
			}
		}

		// @todo MeshPaint: Here we override the textures on the mesh with the render target.  The problem is that other meshes in the scene that use
		//    this texture do not get the override. Do we want to extend this to all other selected meshes or maybe even to all meshes in the scene?
		if (bIsTextureUsed && bStartedPainting && !TextureData->PaintingMaterials.Contains(MaterialToCheck))
		{
			TextureData->PaintingMaterials.AddUnique( MaterialToCheck ); 

			GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, TextureData->PaintRenderTargetTexture);
		}

		MaterialIndex++;
		MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	}

	if( bStartedPainting )
	{
		TexturePaintingCurrentMeshComponent = InMeshComponent;

		check( Texture2D != NULL );
		PaintingTexture2D = Texture2D;
		// OK, now we need to make sure our render target is filled in with data
		SetupInitialRenderTargetData( TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture );
	}

}


/** Paints on a texture */
void FEdModeMeshPaint::PaintTexture( const FMeshPaintParameters& InParams,
									 const TArray< uint32 >& InInfluencedTriangles,
									 const FMatrix& InComponentToWorldMatrix,
									 const IMeshPaintGeometryAdapter& GeometryInfo)
{
	// We bail early if there are no influenced triangles
	if( InInfluencedTriangles.Num() <= 0 )
	{
		return;
	}

	const auto FeatureLevel = GetWorld()->FeatureLevel;


	PaintTexture2DData* TextureData = GetPaintTargetData( PaintingTexture2D );
	check( TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL );

	// Copy the current image to the brush rendertarget texture.
	{
		check( BrushRenderTargetTexture != NULL );
		CopyTextureToRenderTargetTexture( TextureData->PaintRenderTargetTexture, BrushRenderTargetTexture, FeatureLevel );
	}

	const bool bEnableSeamPainting = FMeshPaintSettings::Get().bEnableSeamPainting;
	const FMatrix WorldToBrushMatrix = InParams.InverseBrushToWorldMatrix;

	// Grab the actual render target resource from the textures.  Note that we're absolutely NOT ALLOWED to
	// dereference these pointers.  We're just passing them along to other functions that will use them on the render
	// thread.  The only thing we're allowed to do is check to see if they are NULL or not.
	FTextureRenderTargetResource* BrushRenderTargetResource = BrushRenderTargetTexture->GameThread_GetRenderTargetResource();
	check( BrushRenderTargetResource != NULL );
	
	// Create a canvas for the brush render target.
	FCanvas BrushPaintCanvas(BrushRenderTargetResource, NULL, 0, 0, 0, FeatureLevel);

	// Parameters for brush paint
	TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintBatchedElementParameters( new FMeshPaintBatchedElementParameters() );
	{
		MeshPaintBatchedElementParameters->ShaderParams.CloneTexture = BrushRenderTargetTexture;
		MeshPaintBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
		MeshPaintBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
		MeshPaintBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
		MeshPaintBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
		MeshPaintBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
		MeshPaintBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
		MeshPaintBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
		MeshPaintBatchedElementParameters->ShaderParams.GenerateMaskFlag = false;
	}

	FBatchedElements* BrushPaintBatchedElements = BrushPaintCanvas.GetBatchedElements(FCanvas::ET_Triangle, MeshPaintBatchedElementParameters, NULL, SE_BLEND_Opaque);
	BrushPaintBatchedElements->AddReserveVertices( InInfluencedTriangles.Num() * 3 );
	BrushPaintBatchedElements->AddReserveTriangles(InInfluencedTriangles.Num(), NULL, SE_BLEND_Opaque);

	FHitProxyId BrushPaintHitProxyId = BrushPaintCanvas.GetHitProxyId();

	TSharedPtr<FCanvas> BrushMaskCanvas;
	TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintMaskBatchedElementParameters;
	FBatchedElements* BrushMaskBatchedElements = NULL;
	FHitProxyId BrushMaskHitProxyId;
	FTextureRenderTargetResource* BrushMaskRenderTargetResource = NULL;

	if( bEnableSeamPainting )
	{
		BrushMaskRenderTargetResource = BrushMaskRenderTargetTexture->GameThread_GetRenderTargetResource();
		check( BrushMaskRenderTargetResource != NULL );

		// Create a canvas for the brush mask rendertarget and clear it to black.
		BrushMaskCanvas = TSharedPtr<FCanvas>(new FCanvas(BrushMaskRenderTargetResource, NULL, 0, 0, 0, FeatureLevel));
		BrushMaskCanvas->Clear( FLinearColor::Black );

		// Parameters for the mask
		MeshPaintMaskBatchedElementParameters = TRefCountPtr< FMeshPaintBatchedElementParameters >( new FMeshPaintBatchedElementParameters() );
		{
			MeshPaintMaskBatchedElementParameters->ShaderParams.CloneTexture = TextureData->PaintRenderTargetTexture;
			MeshPaintMaskBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
			MeshPaintMaskBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
			MeshPaintMaskBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
			MeshPaintMaskBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
			MeshPaintMaskBatchedElementParameters->ShaderParams.GenerateMaskFlag = true;
		}

		BrushMaskBatchedElements = BrushMaskCanvas->GetBatchedElements(FCanvas::ET_Triangle, MeshPaintMaskBatchedElementParameters, NULL, SE_BLEND_Opaque);
		BrushMaskBatchedElements->AddReserveVertices( InInfluencedTriangles.Num() * 3 );
		BrushMaskBatchedElements->AddReserveTriangles(InInfluencedTriangles.Num(), NULL, SE_BLEND_Opaque);
		
		BrushMaskHitProxyId = BrushMaskCanvas->GetHitProxyId();
	}

	// Process the influenced triangles - storing off a large list is much slower than processing in a single loop
	for( int32 CurIndex = 0; CurIndex < InInfluencedTriangles.Num(); ++CurIndex )
	{
		const uint32 TriIndex = InInfluencedTriangles[ CurIndex ];
		FTexturePaintTriangleInfo CurTriangle;
		GeometryInfo.GetTriangleInfo(TriIndex, CurTriangle);

		FVector2D UVMin( 99999.9f, 99999.9f );
		FVector2D UVMax( -99999.9f, -99999.9f );
			 
		// Transform the triangle and update the UV bounds
		for( int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
		{				
			// Transform the triangle to world space
			CurTriangle.TriVertices[ TriVertexNum ] = InComponentToWorldMatrix.TransformPosition( CurTriangle.TriVertices[ TriVertexNum ] );

			// Update bounds
			float U = CurTriangle.TriUVs[ TriVertexNum ].X;
			float V = CurTriangle.TriUVs[ TriVertexNum ].Y;

			if( U < UVMin.X )
			{
				UVMin.X = U;
			}
			if( U > UVMax.X )
			{
				UVMax.X = U;
			}
			if( V < UVMin.Y )
			{
				UVMin.Y = V;
			}
			if( V > UVMax.Y )
			{
				UVMax.Y = V;
			}
		}
		
		// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
		FVector2D UVOffset( 0.0f, 0.0f );
		if( UVMax.X > 1.0f )
		{
			UVOffset.X = -FMath::FloorToFloat( UVMin.X );
		}
		else if( UVMin.X < 0.0f )
		{
			UVOffset.X = 1.0f + FMath::FloorToFloat( -UVMax.X );
		}
			
		if( UVMax.Y > 1.0f )
		{
			UVOffset.Y = -FMath::FloorToFloat( UVMin.Y );
		}
		else if( UVMin.Y < 0.0f )
		{
			UVOffset.Y = 1.0f + FMath::FloorToFloat( -UVMax.Y );
		}
		
		// Note that we "wrap" the texture coordinates here to handle the case where the user
		// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
		// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
		// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
		for( int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
		{
			CurTriangle.TriUVs[ TriVertexNum ].X += UVOffset.X;
			CurTriangle.TriUVs[ TriVertexNum ].Y += UVOffset.Y;

			// @todo: Need any half-texel offset adjustments here? Some info about offsets and MSAA here: http://drilian.com/2008/11/25/understanding-half-pixel-and-half-texel-offsets/
			// @todo: MeshPaint: Screen-space texture coords: http://diaryofagraphicsprogrammer.blogspot.com/2008/09/calculating-screen-space-texture.html
			CurTriangle.TrianglePoints[ TriVertexNum ].X = CurTriangle.TriUVs[ TriVertexNum ].X * TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
			CurTriangle.TrianglePoints[ TriVertexNum ].Y = CurTriangle.TriUVs[ TriVertexNum ].Y * TextureData->PaintRenderTargetTexture->GetSurfaceHeight();
		}

		// Vertex positions
		FVector4 Vert0(CurTriangle.TrianglePoints[ 0 ].X,CurTriangle.TrianglePoints[ 0 ].Y,0,1);
		FVector4 Vert1(CurTriangle.TrianglePoints[ 1 ].X,CurTriangle.TrianglePoints[ 1 ].Y,0,1);
		FVector4 Vert2(CurTriangle.TrianglePoints[ 2 ].X,CurTriangle.TrianglePoints[ 2 ].Y,0,1);

		// Vertex color
		FLinearColor Col0( CurTriangle.TriVertices[ 0 ].X, CurTriangle.TriVertices[ 0 ].Y, CurTriangle.TriVertices[ 0 ].Z );
		FLinearColor Col1( CurTriangle.TriVertices[ 1 ].X, CurTriangle.TriVertices[ 1 ].Y, CurTriangle.TriVertices[ 1 ].Z );
		FLinearColor Col2(CurTriangle.TriVertices[2].X, CurTriangle.TriVertices[2].Y, CurTriangle.TriVertices[2].Z);

		// Brush Paint triangle
		{
			int32 V0 = BrushPaintBatchedElements->AddVertex(Vert0, CurTriangle.TriUVs[0], Col0, BrushPaintHitProxyId);
			int32 V1 = BrushPaintBatchedElements->AddVertex(Vert1, CurTriangle.TriUVs[1], Col1, BrushPaintHitProxyId);
			int32 V2 = BrushPaintBatchedElements->AddVertex(Vert2, CurTriangle.TriUVs[2], Col2, BrushPaintHitProxyId);

			BrushPaintBatchedElements->AddTriangle(V0, V1, V2, MeshPaintBatchedElementParameters, SE_BLEND_Opaque);
		}

		// Brush Mask triangle
		if( bEnableSeamPainting )
		{
			int32 V0 = BrushMaskBatchedElements->AddVertex(Vert0,CurTriangle.TriUVs[ 0 ],Col0,BrushMaskHitProxyId);
			int32 V1 = BrushMaskBatchedElements->AddVertex(Vert1,CurTriangle.TriUVs[ 1 ],Col1,BrushMaskHitProxyId);
			int32 V2 = BrushMaskBatchedElements->AddVertex(Vert2,CurTriangle.TriUVs[ 2 ],Col2,BrushMaskHitProxyId);

			BrushMaskBatchedElements->AddTriangle(V0,V1,V2, MeshPaintMaskBatchedElementParameters, SE_BLEND_Opaque);
		}
	}

	// Tell the rendering thread to draw any remaining batched elements
	{
		BrushPaintCanvas.Flush_GameThread(true);

		TextureData->bIsPaintingTexture2DModified = true;
	}

	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand1,
			FTextureRenderTargetResource*, BrushRenderTargetResource, BrushRenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICmdList.CopyToResolveTarget(
			BrushRenderTargetResource->GetRenderTargetTexture(),	// Source texture
			BrushRenderTargetResource->TextureRHI,
			true,													// Do we need the source image content again?
			FResolveParams() );										// Resolve parameters
		});
	}	
	

	if( bEnableSeamPainting )
	{
		BrushMaskCanvas->Flush_GameThread(true);

		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateMeshPaintRTCommand2,
				FTextureRenderTargetResource*, BrushMaskRenderTargetResource, BrushMaskRenderTargetResource,
			{
				// Copy (resolve) the rendered image from the frame buffer to its render target texture
				RHICmdList.CopyToResolveTarget(
					BrushMaskRenderTargetResource->GetRenderTargetTexture(),		// Source texture
					BrushMaskRenderTargetResource->TextureRHI,
					true,												// Do we need the source image content again?
					FResolveParams() );									// Resolve parameters
			});

		}
	}
	
	if( !bEnableSeamPainting )
	{
		// Seam painting is not enabled so we just copy our delta paint info to the paint target.
		CopyTextureToRenderTargetTexture(BrushRenderTargetTexture, TextureData->PaintRenderTargetTexture, FeatureLevel);
	}
	else
	{

		// Constants used for generating quads across entire paint rendertarget
		const float MinU = 0.0f;
		const float MinV = 0.0f;
		const float MaxU = 1.0f;
		const float MaxV = 1.0f;
		const float MinX = 0.0f;
		const float MinY = 0.0f;
		const float MaxX = TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
		const float MaxY = TextureData->PaintRenderTargetTexture->GetSurfaceHeight();

		if( bGenerateSeamMask == true )
		{
			// Generate the texture seam mask.  This is a slow operation when the object has many triangles so we only do it
			//  once when painting is started.
			GenerateSeamMask(TexturePaintingCurrentMeshComponent, InParams.UVChannel, SeamMaskRenderTargetTexture);
			bGenerateSeamMask = false;
		}
		
		FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
		check( RenderTargetResource != NULL );
		// Dilate the paint stroke into the texture seams.
		{
			// Create a canvas for the render target.
			FCanvas Canvas3(RenderTargetResource, NULL, 0, 0, 0, FeatureLevel);


			TRefCountPtr< FMeshPaintDilateBatchedElementParameters > MeshPaintDilateBatchedElementParameters( new FMeshPaintDilateBatchedElementParameters() );
			{
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture0 = BrushRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture1 = SeamMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture2 = BrushMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.WidthPixelOffset = (float) (1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceWidth());
				MeshPaintDilateBatchedElementParameters->ShaderParams.HeightPixelOffset = (float) (1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceHeight());

			}

			// Draw a quad to copy the texture over to the render target
			TArray< FCanvasUVTri >	TriangleList;
			FCanvasUVTri SingleTri;
			SingleTri.V0_Pos = FVector2D( MinX, MinY );
			SingleTri.V0_UV = FVector2D( MinU, MinV );
			SingleTri.V0_Color = FLinearColor::White;

			SingleTri.V1_Pos = FVector2D( MaxX, MinY );
			SingleTri.V1_UV = FVector2D( MaxU, MinV );
			SingleTri.V1_Color = FLinearColor::White;

			SingleTri.V2_Pos = FVector2D( MaxX, MaxY );
			SingleTri.V2_UV = FVector2D( MaxU, MaxV );
			SingleTri.V2_Color =FLinearColor::White;
			TriangleList.Add( SingleTri );

			SingleTri.V0_Pos = FVector2D( MaxX, MaxY );
			SingleTri.V0_UV = FVector2D( MaxU, MaxV );
			SingleTri.V0_Color = FLinearColor::White;

			SingleTri.V1_Pos = FVector2D( MinX, MaxY );
			SingleTri.V1_UV = FVector2D( MinU, MaxV );
			SingleTri.V1_Color = FLinearColor::White;

			SingleTri.V2_Pos = FVector2D( MinX, MinY );
			SingleTri.V2_UV = FVector2D( MinU, MinV );
			SingleTri.V2_Color = FLinearColor::White;
			TriangleList.Add( SingleTri );

			FCanvasTriangleItem TriItemList( TriangleList, NULL );
			TriItemList.BatchedElementParameters = MeshPaintDilateBatchedElementParameters;
			TriItemList.BlendMode = SE_BLEND_Opaque;
			Canvas3.DrawItem( TriItemList );
			

			// Tell the rendering thread to draw any remaining batched elements
			Canvas3.Flush_GameThread(true);

		}

		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateMeshPaintRTCommand3,
				FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
			{
				// Copy (resolve) the rendered image from the frame buffer to its render target texture
				RHICmdList.CopyToResolveTarget(
					RenderTargetResource->GetRenderTargetTexture(),		// Source texture
					RenderTargetResource->TextureRHI,
					true,												// Do we need the source image content again?
					FResolveParams() );									// Resolve parameters
			});

		}

	}
	FlushRenderingCommands();
}

void FEdModeMeshPaint::CommitAllPaintedTextures()
{
	if( PaintTargetData.Num() > 0)
	{
		check( PaintingTexture2D == NULL );

		FScopedTransaction Transaction( LOCTEXT( "MeshPaintMode_TexturePaint_Transaction", "Texture Paint" ) );

		GWarn->BeginSlowTask( LOCTEXT( "BeginMeshPaintMode_TexturePaint_CommitTask", "Committing Texture Paint Changes" ), true );

		int32 CurStep = 1;
		int32 TotalSteps = GetNumberOfPendingPaintChanges();

		for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
		{
			PaintTexture2DData* TextureData = &It.Value();
			
			// Commit the texture
			if( TextureData->bIsPaintingTexture2DModified == true )
			{
				GWarn->StatusUpdate( CurStep++, TotalSteps, FText::Format(LOCTEXT( "MeshPaintMode_TexturePaint_CommitStatus", "Committing Texture Paint Changes: {0}" ), FText::FromName(TextureData->PaintingTexture2D->GetFName()) ) );

				const int32 TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
				const int32 TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
				TArray< FColor > TexturePixels;
				TexturePixels.AddUninitialized( TexWidth * TexHeight );

				// Copy the contents of the remote texture to system memory
				// NOTE: OutRawImageData must be a preallocated buffer!

				FlushRenderingCommands();
				// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
				//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
				//  rendering thread.
				FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
				check(RenderTargetResource != NULL);
				RenderTargetResource->ReadPixels( TexturePixels );

				{
					// For undo
					TextureData->PaintingTexture2D->SetFlags(RF_Transactional);
					TextureData->PaintingTexture2D->Modify();

					// Store source art
					FColor* Colors = (FColor*)TextureData->PaintingTexture2D->Source.LockMip(0);					
					check(TextureData->PaintingTexture2D->Source.CalcMipSize(0)==TexturePixels.Num()*sizeof(FColor));
					FMemory::Memcpy(Colors, TexturePixels.GetData(), TexturePixels.Num() * sizeof(FColor));
					TextureData->PaintingTexture2D->Source.UnlockMip(0);

					// If render target gamma used was 1.0 then disable SRGB for the static texture
					// @todo MeshPaint: We are not allowed to dereference the RenderTargetResource pointer, figure out why we need this when the GetDisplayGamma() function is hard coded to return 2.2.
					TextureData->PaintingTexture2D->SRGB = FMath::Abs( RenderTargetResource->GetDisplayGamma() - 1.0f ) >= KINDA_SMALL_NUMBER;

					TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = true;

					// Update the texture (generate mips, compress if needed)
					TextureData->PaintingTexture2D->PostEditChange();

					TextureData->bIsPaintingTexture2DModified = false;

					// Reduplicate the duplicate so that if we cancel our future changes, it will restore to how the texture looked at this point.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)StaticDuplicateObject(TextureData->PaintingTexture2D, GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *TextureData->PaintingTexture2D->GetName()));

				}
			}
		}

		ClearAllTextureOverrides();

		GWarn->EndSlowTask();

	}
}

/** Used to tell the texture paint system that we will need to restore the rendertargets */
void FEdModeMeshPaint::RestoreRenderTargets()
{
	bDoRestoreRenTargets = true;
}

/** Clears all texture overrides for this component */
void FEdModeMeshPaint::ClearMeshTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		TArray<UTexture*> UsedTextures;
		InMeshComponent->GetUsedTextures(/*out*/ UsedTextures, EMaterialQualityLevel::High);

		for (UTexture* Texture : UsedTextures)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{
				GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, nullptr);
			}
		}
	}
}

/** Clears all texture overrides, removing any pending texture paint changes */
void FEdModeMeshPaint::ClearAllTextureOverrides()
{


	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		for( int32 MaterialIndex = 0; MaterialIndex < TextureData->PaintingMaterials.Num(); MaterialIndex++)
		{
			UMaterialInterface* PaintingMaterialInterface = TextureData->PaintingMaterials[MaterialIndex];
			PaintingMaterialInterface->OverrideTexture( TextureData->PaintingTexture2D, NULL, GetWorld()->FeatureLevel );//findme
		}

		TextureData->PaintingMaterials.Empty();
	}
}

/** Sets all texture overrides available for the mesh. */
void FEdModeMeshPaint::SetAllTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		TArray<UTexture*> UsedTextures;
		InMeshComponent->GetUsedTextures(/*out*/ UsedTextures, EMaterialQualityLevel::High);

		for (UTexture* Texture : UsedTextures)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{
				if (PaintTexture2DData* TextureData = GetPaintTargetData(Texture2D))
				{
					GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, TextureData->PaintRenderTargetTexture);
				}
			}
		}
	}
}

/** Sets the override for a specific texture for any materials using it in the mesh, clears the override if it has no overrides. */
void FEdModeMeshPaint::SetSpecificTextureOverrideForMesh(const IMeshPaintGeometryAdapter& GeometryInfo, UTexture2D* Texture)
{
	// If there is texture data, that means we have an override ready, so set it. 
	// If there is no data, then remove the override so we can at least see the texture without the changes to the other texture.
	// This is important because overrides are shared between material instances with the same parent. We want to disable a override in place,
	// making the action more comprehensive to the user.
	PaintTexture2DData* TextureData = GetPaintTargetData(Texture);
	UTextureRenderTarget2D* TextureForOverrideOrNull = ((TextureData != nullptr) && (TextureData->PaintingMaterials.Num() > 0)) ? TextureData->PaintRenderTargetTexture : nullptr;

	GeometryInfo.ApplyOrRemoveTextureOverride(Texture, TextureForOverrideOrNull);
}

int32 FEdModeMeshPaint::GetNumberOfPendingPaintChanges()
{
	int32 Result = 0;
	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		// Commit the texture
		if( TextureData->bIsPaintingTexture2DModified == true )
		{
			Result++;
		}
	}
	return Result;
}


/** Finishes painting a texture */
void FEdModeMeshPaint::FinishPaintingTexture( )
{
	if( TexturePaintingCurrentMeshComponent != NULL )
	{
		check( PaintingTexture2D != NULL );

		PaintTexture2DData* TextureData = GetPaintTargetData( PaintingTexture2D );
		check( TextureData );

		// Commit to the texture source art but don't do any compression, compression is saved for the CommitAllPaintedTextures function.
		if( TextureData->bIsPaintingTexture2DModified == true )
		{
			const int32 TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
			const int32 TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
			TArray< FColor > TexturePixels;
			TexturePixels.AddUninitialized( TexWidth * TexHeight );

			FlushRenderingCommands();
			// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
			//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
			//  rendering thread.
			FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
			check(RenderTargetResource != NULL);
			RenderTargetResource->ReadPixels(TexturePixels);


			{
				FScopedTransaction Transaction( LOCTEXT( "MeshPaintMode_TexturePaint_Transaction", "Texture Paint" ) );

				// For undo
				TextureData->PaintingTexture2D->SetFlags(RF_Transactional);
				TextureData->PaintingTexture2D->Modify();

				// Store source art
				FColor* Colors = (FColor*)TextureData->PaintingTexture2D->Source.LockMip(0);
				check(TextureData->PaintingTexture2D->Source.CalcMipSize(0)==TexturePixels.Num()*sizeof(FColor));
				FMemory::Memcpy(Colors, TexturePixels.GetData(), TexturePixels.Num() * sizeof(FColor));
				TextureData->PaintingTexture2D->Source.UnlockMip(0);

				// If render target gamma used was 1.0 then disable SRGB for the static texture
				TextureData->PaintingTexture2D->SRGB = FMath::Abs( RenderTargetResource->GetDisplayGamma() - 1.0f ) >= KINDA_SMALL_NUMBER;

				TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = true;
			}
		}

		PaintingTexture2D = NULL;
		TexturePaintingCurrentMeshComponent = NULL;
	}
}




/** FEdMode: Called when mouse drag input it applied */
bool FEdModeMeshPaint::InputDelta( FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	// We only care about perspective viewports
	if( InViewportClient->IsPerspective() )
	{
		// ...
	}

	return false;
}

/** FEdMode: Called after an Undo operation */
void FEdModeMeshPaint::PostUndo()
{
	FEdMode::PostUndo();
	bDoRestoreRenTargets = true;
	RemoveAllGeometryAdapters();
	CreateGeometryAdaptersForSelectedComponents();
}

/** Returns true if we need to force a render/update through based fill/copy */
bool FEdModeMeshPaint::IsForceRendered (void) const
{
	return (bIsFloodFill || bPushInstanceColorsToMesh || bIsPainting);
}


/** FEdMode: Render the mesh paint tool */
void FEdModeMeshPaint::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	/** Call parent implementation */
	FEdMode::Render( View, Viewport, PDI );

	FEditorViewportClient* ViewportClient = (FEditorViewportClient*)Viewport->GetClient();
	check( ViewportClient != nullptr );

	// We only care about perspective viewports
	const bool bIsPerspectiveViewport = ( View->ViewMatrices.ProjMatrix.M[ 3 ][ 3 ] < ( 1.0f - SMALL_NUMBER ) );
	if( bIsPerspectiveViewport )
	{
		// Make sure perspective viewports are still set to real-time
		const bool bWantRealTime = true;
		const bool bRememberCurrentState = false;
		ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );


		// Set viewport show flags
		const bool bAllowColorViewModes = ( FMeshPaintSettings::Get().ResourceType != EMeshPaintResource::Texture );
		SetViewportShowFlags( bAllowColorViewModes, *ViewportClient );

		struct FPaintRay
		{
			FVector CameraLocation;
			FVector RayStart;
			FVector RayDirection;
			UViewportInteractor* ViewportInteractor;
		};

		static TArray<FPaintRay> PaintRays;
		PaintRays.Reset();
		


		IVREditorMode* VREditorMode = static_cast<IVREditorMode*>(GetModeManager()->GetActiveMode(IVREditorModule::Get().GetVREditorModeID()));

		// Check to see if VREditorMode is active. If so, we're painting with interactor
		bool bIsInVRMode = false;
		if ( IVREditorModule::IsAvailable() )
		{
			if ( VREditorMode != nullptr && VREditorMode->IsFullyInitialized() )
			{
				bIsInVRMode = true;
				for ( UViewportInteractor* Interactor : VREditorMode->GetWorldInteraction().GetInteractors() )
				{
					// Skip other interactors if we are painting with one
					if ( PaintingWithInteractorInVR && Interactor != PaintingWithInteractorInVR )
					{
						continue;
					}

					FVector LaserPointerStart, LaserPointerEnd;
					if ( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
					{
						const FVector LaserPointerDirection = ( LaserPointerEnd - LaserPointerStart ).GetSafeNormal();

						FPaintRay& NewPaintRay = *new( PaintRays ) FPaintRay();
						NewPaintRay.CameraLocation = VREditorMode->GetHeadTransform().GetLocation();
						NewPaintRay.RayStart = LaserPointerStart;
						NewPaintRay.RayDirection = LaserPointerDirection;
						NewPaintRay.ViewportInteractor = Interactor;
					}
				}
			}
		}

		// Check to see if we're painting with mouse
		if (!bIsInVRMode)
		{
			// Make sure the cursor is visible OR we're flood filling.  No point drawing a paint cue when there's no cursor.
			if (Viewport->IsCursorVisible() || IsForceRendered())
			{
				if (!PDI->IsHitTesting())
				{
					// Grab the mouse cursor position
					FIntPoint MousePosition;
					Viewport->GetMousePos(MousePosition);

					// Is the mouse currently over the viewport? or flood filling
					if (IsForceRendered() || (MousePosition.X >= 0 && MousePosition.Y >= 0 && MousePosition.X < (int32)Viewport->GetSizeXY().X && MousePosition.Y < (int32)Viewport->GetSizeXY().Y))
					{
						// Compute a world space ray from the screen space mouse coordinates
						FViewportCursorLocation MouseViewportRay(View, ViewportClient, MousePosition.X, MousePosition.Y);

						FPaintRay& NewPaintRay = *new( PaintRays ) FPaintRay();
						NewPaintRay.CameraLocation = View->ViewMatrices.ViewOrigin;
						NewPaintRay.RayStart = MouseViewportRay.GetOrigin();
						NewPaintRay.RayDirection = MouseViewportRay.GetDirection();
						NewPaintRay.ViewportInteractor = nullptr;
					}
				}
			}
		}

		// Apply paint pressure and start painting (or if not currently painting, draw a preview of where paint will be applied)
		for( const FPaintRay& PaintRay : PaintRays )
		{
			// Unless "Flow" mode is enabled, we'll only draw a visual cue while rendering and won't
			// do any actual painting.  When "Flow" is turned on we will paint here, too!
			const bool bVisualCueOnly = !bIsPainting || (PaintingWithInteractorInVR == nullptr && !FMeshPaintSettings::Get().bEnableFlow);
			float StrengthScale = (PaintingWithInteractorInVR == nullptr && FMeshPaintSettings::Get().bEnableFlow) ? FMeshPaintSettings::Get().FlowAmount : 1.0f;

			bool bIsHoveringOverUIVR = false;
			const UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( PaintRay.ViewportInteractor );
			if( VRInteractor != nullptr )
			{
				bIsHoveringOverUIVR = VRInteractor->IsHoveringOverUI();
			}

			// Don't draw visual cue for paint brush when the interactor is hovering over UI
			if( !bVisualCueOnly || PaintRay.ViewportInteractor == nullptr || !bIsHoveringOverUIVR )
			{
				// Don't draw visual cue if we're hovering over a viewport interactable, such as a dockable window selection bar
				bool bIsHoveringOverViewportInteractable = false;
				if( bVisualCueOnly && PaintRay.ViewportInteractor != nullptr )
				{
					FHitResult HitResult = PaintRay.ViewportInteractor->GetHitResultFromLaserPointer();
					if( HitResult.Actor.IsValid() )
					{
						UViewportWorldInteraction& WorldInteraction = VREditorMode->GetWorldInteraction();

						if( WorldInteraction.IsInteractableComponent( HitResult.GetComponent() ) )
						{
							AActor* Actor = HitResult.Actor.Get();

							// Make sure we're not hovering over some other viewport interactable, such as a dockable window selection bar or close button
							IViewportInteractableInterface* ActorInteractable = Cast<IViewportInteractableInterface>( Actor );
							bIsHoveringOverViewportInteractable = ( ActorInteractable != nullptr );
						}
					}
				}

				if( !bIsHoveringOverViewportInteractable )
				{
					// Apply VR controller trigger pressure if we're painting in VR
					if (bIsPainting && PaintingWithInteractorInVR && VREditorMode != nullptr)
					{
						StrengthScale *= VRInteractor->GetSelectAndMoveTriggerValue();
					}

					// Apply stylus pressure if it's active
					else if (Viewport->IsPenActive())
					{
						StrengthScale *= Viewport->GetTabletPressure();
					}

					const EMeshPaintAction::Type PaintAction = GetPaintAction(Viewport);
					bool bAnyPaintableActorsUnderCursor = false;
					DoPaint(PaintRay.CameraLocation, PaintRay.RayStart, PaintRay.RayDirection, PDI, PaintAction, bVisualCueOnly, StrengthScale, /* Out */ bAnyPaintableActorsUnderCursor);
				}
			}
		}
	}
}

/** Returns valid MeshComponents in the current selection */
TArray<UMeshComponent*> FEdModeMeshPaint::GetSelectedMeshComponents() const
{
	TArray<UMeshComponent*> Result;

	for (FSelectionIterator SelectionIt(*Owner->GetSelectedActors()); SelectionIt; ++SelectionIt)
	{
		AActor* CurActor = CastChecked<AActor>(*SelectionIt);

		// Ignore actors that are hidden or not selected
		if (CurActor->bHidden || !CurActor->IsSelected())
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		CurActor->GetComponents<UMeshComponent>(MeshComponents);

		Result.Append(MeshComponents);
	}

	return Result;
}


/** Saves out cached mesh settings for the given actor */
void FEdModeMeshPaint::SaveSettingsForActor( AActor* InActor )
{
	if( InActor != NULL )
	{
		TInlineComponentArray<UMeshComponent*> MeshComponents;
		InActor->GetComponents<UMeshComponent>(MeshComponents);

		for( UMeshComponent* MeshComponent : MeshComponents )
		{
			// Get the currently selected texture
			UTexture2D* SelectedTexture = GetSelectedTexture();

			// Get all the used materials for this component
			TArray<UMaterialInterface*> UsedMaterials;
			MeshComponent->GetUsedMaterials( UsedMaterials );

			// Check this mesh's textures against the selected one before we save the settings to make sure it's a valid texture
			for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
			{
				if(UsedMaterials[ MatIndex ] == NULL)
				{
					continue;
				}

				TArray<UTexture*> UsedTextures;
				UsedMaterials[MatIndex]->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, false, GetWorld()->FeatureLevel, false);

				for( int32 TexIndex = 0; TexIndex < UsedTextures.Num(); TexIndex++ )
				{
					UTexture2D* Texture2D = Cast<UTexture2D>( UsedTextures[ TexIndex ] );
					if(Texture2D == NULL )
					{
						UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( UsedTextures[ TexIndex ] );
						if( TextureRenderTarget2D )
						{
							Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );
						}
					}

					if( SelectedTexture == Texture2D )
					{
						// Save the settings for this mesh with its valid texture
						StaticMeshSettings MeshSettings = StaticMeshSettings(SelectedTexture, FMeshPaintSettings::Get().UVChannel);
						StaticMeshSettingsMap.Add(MeshComponent, MeshSettings);
						return;
					}
				}		
			}

			// No valid Texture found, attempt to find the previous texture setting or leave it as NULL to be handled by the default texture on selection
			StaticMeshSettings* FoundMeshSettings = StaticMeshSettingsMap.Find(MeshComponent);
			UTexture2D* SavedTexture = FoundMeshSettings != NULL ? FoundMeshSettings->SelectedTexture : NULL;
			StaticMeshSettings MeshSettings = StaticMeshSettings(SavedTexture, FMeshPaintSettings::Get().UVChannel);
			StaticMeshSettingsMap.Add(MeshComponent, MeshSettings);
		}
	}
}

void FEdModeMeshPaint::UpdateSettingsForMeshComponent( UMeshComponent* InMeshComponent, UTexture2D* InOldTexture, UTexture2D* InNewTexture )
{
	if( InMeshComponent != NULL )
	{
		// Get all the used materials for this component
		TArray<UMaterialInterface*> UsedMaterials;
		InMeshComponent->GetUsedMaterials(UsedMaterials);

		// Check this mesh's textures against the selected one before we save the settings to make sure it's a valid texture
		for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
		{
			if(UsedMaterials[ MatIndex ] == NULL)
			{
				continue;
			}

			TArray<UTexture*> UsedTextures;
			UsedMaterials[MatIndex]->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, false, GetWorld()->FeatureLevel, false);

			for( int32 TexIndex = 0; TexIndex < UsedTextures.Num(); TexIndex++ )
			{
				UTexture2D* Texture2D = Cast<UTexture2D>( UsedTextures[ TexIndex ] );
				if(Texture2D == NULL )
				{
					UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( UsedTextures[ TexIndex ] );
					if( TextureRenderTarget2D )
					{
						Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );
					}
				}

				if( InOldTexture == Texture2D )
				{
					// Save the settings for this mesh with its valid texture
					StaticMeshSettings MeshSettings = StaticMeshSettings(InNewTexture, FMeshPaintSettings::Get().UVChannel);
					StaticMeshSettingsMap.Add(InMeshComponent, MeshSettings);
					return;
				}
			}		
		}
	}
}

/** FEdMode: Handling SelectActor */
bool FEdModeMeshPaint::Select( AActor* InActor, bool bInSelected )
{
	TInlineComponentArray<UMeshComponent*> MeshComponents;
	InActor->GetComponents<UMeshComponent>(MeshComponents);

	for(const auto& MeshComponent : MeshComponents)
	{
		check(MeshComponent);

		if (!bInSelected)
		{
			IMeshPaintGeometryAdapter* GeomInfo = ComponentToAdapterMap.FindRef(MeshComponent).Get();
			if (GeomInfo)
			{
				if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
				{
					// When un-selecting a mesh, save it's settings based on the current properties
					ClearMeshTextureOverrides(*GeomInfo, MeshComponent);
					SaveSettingsForActor(InActor);
				}
				else if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors)
				{
					// Propagate painting to lower LODs and stop forcing the rendered mesh to LOD0.
					ApplyVertexColorsToAllLODs(*GeomInfo, MeshComponent);
					ApplyOrRemoveForceBestLOD(*GeomInfo, MeshComponent, /*bApply=*/ false);
					{
						FComponentReregisterContext ReregisterContext(MeshComponent);
					}
				}

				RemoveGeometryAdapter(MeshComponent);
			}
		}
		else
		{
			if (!ComponentToAdapterMap.Contains(MeshComponent))
			{
				IMeshPaintGeometryAdapter* GeomInfo = AddGeometryAdapter(MeshComponent);

				if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
				{
					SetAllTextureOverrides(*GeomInfo, MeshComponent);
				}
				else if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors)
				{
					//Painting is done on LOD0 so force the mesh to render only LOD0.
					ApplyOrRemoveForceBestLOD(*GeomInfo, MeshComponent, /*bApply=*/ true);
					{
						FComponentReregisterContext ReregisterContext(MeshComponent);
					}
				}
			}
		}
	}
	
	return false;
}

/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
bool FEdModeMeshPaint::IsSelectionAllowed( AActor* InActor, bool bInSelection ) const
{
	return true;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeMeshPaint::ActorSelectionChangeNotify()
{
	if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		// Make sure we update the texture list to case for the new actor
		bShouldUpdateTextureList = true;

		// Update any settings on the current selection
		StaticMeshSettings* MeshSettings = NULL;

		// For now, just grab the first mesh we find with some cached settings
		TArray<UMeshComponent*> MeshComponents = GetSelectedMeshComponents();
		for( UMeshComponent* MeshComponent : MeshComponents )
		{
			MeshSettings = StaticMeshSettingsMap.Find(MeshComponent);
			if( MeshSettings != NULL )
			{
				break;
			}
		}

		if( MeshSettings != NULL)
		{
			// Set UVChannel to our cached setting
			FMeshPaintSettings::Get().UVChannel = MeshSettings->SelectedUVChannel;

			// Loop through our list of textures and match up from the user cache
			bool bFoundSavedTexture = false;
			for ( TArray<FTextureTargetListInfo>::TIterator It(TexturePaintTargetList); It; ++It)
			{
				It->bIsSelected = false;
				if(It->TextureData == MeshSettings->SelectedTexture)
				{
					// Found the texture we were looking for, continue through to 'un-select' the other textures.
					It->bIsSelected = bFoundSavedTexture = true;
				}
			}

			// Saved texture wasn't found, default to first selection. Don't have to 'un-select' anything since we already did so above.
			if(!bFoundSavedTexture && TexturePaintTargetList.Num() > 0)
			{
				TexturePaintTargetList[0].bIsSelected = true;
			}

			// Update texture list below to reflect any selection changes
			bShouldUpdateTextureList = true;
		}
		else if( MeshComponents.Num() > 0 )
		{
			// No cached settings, default UVChannel to 0 and Texture Target list to first selection
			FMeshPaintSettings::Get().UVChannel = 0;

			int32 Index = 0;
			for ( TArray<FTextureTargetListInfo>::TIterator It(TexturePaintTargetList); It; ++It)
			{
				It->bIsSelected = Index == 0;
				++Index;
			}
			// Update texture list below to reflect any selection changes
			bShouldUpdateTextureList = true;
		}
	}
}


/** Forces real-time perspective viewports */
void FEdModeMeshPaint::ForceRealTimeViewports( const bool bEnable, const bool bStoreCurrentState )
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	TSharedPtr< ILevelViewport > ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
	if (ViewportWindow.IsValid())
	{
		FEditorViewportClient &Viewport = ViewportWindow->GetLevelViewportClient();
		if( Viewport.IsPerspective() )
		{				
			if( bEnable )
			{
				Viewport.SetRealtime( bEnable, bStoreCurrentState );
			}
			else
			{
				const bool bAllowDisable = true;
				Viewport.RestoreRealtime( bAllowDisable );
			}

		}
	}
}

/** Sets show flags for perspective viewports */
void FEdModeMeshPaint::SetViewportShowFlags( const bool bAllowColorViewModes, FEditorViewportClient& Viewport )
{
	if( Viewport.IsPerspective() )
	{	
		// Update viewport show flags
		{
			// show flags forced on during vertex color modes

			EMeshPaintColorViewMode::Type ColorViewMode = FMeshPaintSettings::Get().ColorViewMode;
			if( !bAllowColorViewModes )
			{
				ColorViewMode = EMeshPaintColorViewMode::Normal;
			}

			if(ColorViewMode == EMeshPaintColorViewMode::Normal)
			{
				if (Viewport.EngineShowFlags.VertexColors)
				{
					// If we're transitioning to normal mode then restore the backup
					// Clear the flags relevant to vertex color modes
					Viewport.EngineShowFlags.SetVertexColors(false);
						
					// Restore the vertex color mode flags that were set when we last entered vertex color mode
					ApplyViewMode(Viewport.GetViewMode(), Viewport.IsPerspective(), Viewport.EngineShowFlags);
					GVertexColorViewMode = EVertexColorViewMode::Color;
				}
			}
			else
			{
				Viewport.EngineShowFlags.SetMaterials(true);
				Viewport.EngineShowFlags.SetLighting(false);
				Viewport.EngineShowFlags.SetBSPTriangles(true);
				Viewport.EngineShowFlags.SetVertexColors(true);
				Viewport.EngineShowFlags.SetPostProcessing(false);
				Viewport.EngineShowFlags.SetHMDDistortion(false);

				switch( ColorViewMode )
				{
					case EMeshPaintColorViewMode::RGB:
						{
							GVertexColorViewMode = EVertexColorViewMode::Color;
						}
						break;

					case EMeshPaintColorViewMode::Alpha:
						{
							GVertexColorViewMode = EVertexColorViewMode::Alpha;
						}
						break;

					case EMeshPaintColorViewMode::Red:
						{
							GVertexColorViewMode = EVertexColorViewMode::Red;
						}
						break;

					case EMeshPaintColorViewMode::Green:
						{
							GVertexColorViewMode = EVertexColorViewMode::Green;
						}
						break;

					case EMeshPaintColorViewMode::Blue:
						{
							GVertexColorViewMode = EVertexColorViewMode::Blue;
						}
						break;
				}
			}
		}
	}
}



/** Makes sure that the render target is ready to paint on */
void FEdModeMeshPaint::SetupInitialRenderTargetData( UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget )
{
	check( InTextureSource != NULL );
	check( InRenderTarget != NULL );

	if( InTextureSource->Source.IsValid() )
	{
		// Great, we have source data!  We'll use that as our image source.

		// Create a texture in memory from the source art
		{
			// @todo MeshPaint: This generates a lot of memory thrash -- try to cache this texture and reuse it?
			UTexture2D* TempSourceArtTexture = CreateTempUncompressedTexture( InTextureSource );
			check( TempSourceArtTexture != NULL );

			// Copy the texture to the render target using the GPU
			CopyTextureToRenderTargetTexture(TempSourceArtTexture, InRenderTarget, GetWorld()->FeatureLevel);

			// NOTE: TempSourceArtTexture is no longer needed (will be GC'd)
		}
	}
	else
	{
		// Just copy (render) the texture in GPU memory to our render target.  Hopefully it's not
		// compressed already!
		check( InTextureSource->IsFullyStreamedIn() );
		CopyTextureToRenderTargetTexture(InTextureSource, InRenderTarget, GetWorld()->FeatureLevel);
	}

}



/** Static: Creates a temporary texture used to transfer data to a render target in memory */
UTexture2D* FEdModeMeshPaint::CreateTempUncompressedTexture( UTexture2D* SourceTexture )
{
	check( SourceTexture->Source.IsValid() );

	// Decompress PNG image
	TArray<uint8> RawData;
	SourceTexture->Source.GetMipData(RawData, 0);

	// We are using the source art so grab the original width/height
	const int32 Width = SourceTexture->Source.GetSizeX();
	const int32 Height = SourceTexture->Source.GetSizeY();
	const bool bUseSRGB = SourceTexture->SRGB;

	check( Width > 0 && Height > 0 && RawData.Num() > 0 );

	// Allocate the new texture
	UTexture2D* NewTexture2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	
	// Fill in the base mip for the texture we created
	uint8* MipData = ( uint8* )NewTexture2D->PlatformData->Mips[ 0 ].BulkData.Lock( LOCK_READ_WRITE );
	for( int32 y=0; y<Height; y++ )
	{
		uint8* DestPtr = &MipData[(Height - 1 - y) * Width * sizeof(FColor)];
		const FColor* SrcPtr = &( (FColor*)( RawData.GetData() ) )[ ( Height - 1 - y ) * Width ];
		for( int32 x=0; x<Width; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			*DestPtr++ = SrcPtr->A;
			SrcPtr++;
		}
	}
	NewTexture2D->PlatformData->Mips[ 0 ].BulkData.Unlock();

	// Set options
	NewTexture2D->SRGB = bUseSRGB;
	NewTexture2D->CompressionNone = true;
	NewTexture2D->MipGenSettings = TMGS_NoMipmaps;
	NewTexture2D->CompressionSettings = TC_Default;

	// Update the remote texture data
	NewTexture2D->UpdateResource();
	return NewTexture2D;
}



/** Static: Copies a texture to a render target texture */
void FEdModeMeshPaint::CopyTextureToRenderTargetTexture( UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture, ERHIFeatureLevel::Type FeatureLevel )
{	
	check( SourceTexture != NULL );
	check( RenderTargetTexture != NULL );

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	check( RenderTargetResource != NULL );


	{
		// Create a canvas for the render target and clear it to black
		FCanvas Canvas(RenderTargetResource, NULL, 0, 0, 0, FeatureLevel);

		const uint32 Width = RenderTargetTexture->GetSurfaceWidth();
		const uint32 Height = RenderTargetTexture->GetSurfaceHeight();

		// @todo MeshPaint: Need full color/alpha writes enabled to get alpha
		// @todo MeshPaint: Texels need to line up perfectly to avoid bilinear artifacts
		// @todo MeshPaint: Potential gamma issues here
		// @todo MeshPaint: Probably using CLAMP address mode when reading from source (if texels line up, shouldn't matter though.)

		// @todo MeshPaint: Should use scratch texture built from original source art (when possible!)
		//		-> Current method will have compression artifacts!


		// Grab the texture resource.  We only support 2D textures and render target textures here.
		FTexture* TextureResource = NULL;
		UTexture2D* Texture2D = Cast<UTexture2D>( SourceTexture );
		if( Texture2D != NULL )
		{
			TextureResource = Texture2D->Resource;
		}
		else
		{
			UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( SourceTexture );
			TextureResource = TextureRenderTarget2D->GameThread_GetRenderTargetResource();
		}
		check( TextureResource != NULL );


		// Draw a quad to copy the texture over to the render target
		{		
			const float MinU = 0.0f;
			const float MinV = 0.0f;
			const float MaxU = 1.0f;
			const float MaxV = 1.0f;
			const float MinX = 0.0f;
			const float MinY = 0.0f;
			const float MaxX = Width;
			const float MaxY = Height;

			FCanvasUVTri Tri1;
			FCanvasUVTri Tri2;
			Tri1.V0_Pos = FVector2D( MinX, MinY );
			Tri1.V0_UV = FVector2D( MinU, MinV );
			Tri1.V1_Pos = FVector2D( MaxX, MinY );
			Tri1.V1_UV = FVector2D( MaxU, MinV );
			Tri1.V2_Pos = FVector2D( MaxX, MaxY );
			Tri1.V2_UV = FVector2D( MaxU, MaxV );
			
			Tri2.V0_Pos = FVector2D( MaxX, MaxY );
			Tri2.V0_UV = FVector2D( MaxU, MaxV );
			Tri2.V1_Pos = FVector2D( MinX, MaxY );
			Tri2.V1_UV = FVector2D( MinU, MaxV );
			Tri2.V2_Pos = FVector2D( MinX, MinY );
			Tri2.V2_UV = FVector2D ( MinU, MinV );
			Tri1.V0_Color = Tri1.V1_Color = Tri1.V2_Color = Tri2.V0_Color = Tri2.V1_Color = Tri2.V2_Color = FLinearColor::White;
			TArray< FCanvasUVTri > List;
			List.Add( Tri1 );
			List.Add( Tri2 );
			FCanvasTriangleItem TriItem( List, TextureResource );
			TriItem.BlendMode = SE_BLEND_Opaque;
			Canvas.DrawItem( TriItem );			
		}

		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush_GameThread(true);
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand,
			FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICmdList.CopyToResolveTarget(
				RenderTargetResource->GetRenderTargetTexture(),		// Source texture
				RenderTargetResource->TextureRHI,					// Dest texture
				true,												// Do we need the source image content again?
				FResolveParams() );									// Resolve parameters
		});

	}	  
}

/** Will generate a mask texture, used for texture dilation, and store it in the passed in rendertarget */
bool FEdModeMeshPaint::GenerateSeamMask(UMeshComponent* MeshComponent, int32 UVSet, UTextureRenderTarget2D* RenderTargetTexture)
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);
	if (StaticMeshComponent == nullptr)
	{
		return false;
	}

	check(StaticMeshComponent != NULL);
	check(StaticMeshComponent->StaticMesh != NULL);
	check(RenderTargetTexture != NULL);
	check(StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex].VertexBuffer.GetNumTexCoords() > (uint32)UVSet);
		
	bool RetVal = false;

	FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];

	const uint32 Width = RenderTargetTexture->GetSurfaceWidth();
	const uint32 Height = RenderTargetTexture->GetSurfaceHeight();

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	check( RenderTargetResource != NULL );

	int32 NumElements = StaticMeshComponent->GetNumMaterials();
	UTexture2D* TargetTexture2D = GetSelectedTexture();
	PaintTexture2DData* TextureData = GetPaintTargetData( TargetTexture2D );

	// Store info that tells us if the element material uses our target texture so we don't have to do a usestexture() call for each tri.  We will
	// use this info to eliminate triangles that do not use our texture.
	TArray< bool > ElementUsesTargetTexture;
	ElementUsesTargetTexture.AddZeroed( NumElements );
	for ( int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++ )
	{
		ElementUsesTargetTexture[ ElementIndex ] = false;

		UMaterialInterface* ElementMat = StaticMeshComponent->GetMaterial( ElementIndex );
		if( ElementMat != NULL )
		{
			ElementUsesTargetTexture[ ElementIndex ] |=  DoesMaterialUseTexture(ElementMat, TargetTexture2D );

			if( ElementUsesTargetTexture[ ElementIndex ] == false && TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL)
			{
				// If we didn't get a match on our selected texture, we'll check to see if the the material uses a
				//  render target texture override that we put on during painting.
				ElementUsesTargetTexture[ ElementIndex ] |=  DoesMaterialUseTexture(ElementMat, TextureData->PaintRenderTargetTexture );
			}
		}
	}

	// Make sure we're dealing with triangle lists
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	const uint32 NumIndexBufferIndices = Indices.Num();
	check( NumIndexBufferIndices % 3 == 0 );
	const uint32 NumTriangles = NumIndexBufferIndices / 3;

	static TArray< int32 > InfluencedTriangles;
	InfluencedTriangles.Empty( NumTriangles );

	// For each triangle in the mesh
	for( uint32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex )
	{
		// At least one triangle vertex was influenced.
		bool bAddTri = false;

		// Check to see if the sub-element that this triangle belongs to actually uses our paint target texture in its material
		for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
		{
			//FStaticMeshElement& Element = LODModel.Elements[ ElementIndex ];
			FStaticMeshSection& Element = LODModel.Sections[ ElementIndex ];


			if( ( TriIndex >= Element.FirstIndex / 3 ) && 
				( TriIndex < Element.FirstIndex / 3 + Element.NumTriangles ) )
			{

				// The triangle belongs to this element, now we need to check to see if the element material uses our target texture.
				if( TargetTexture2D != NULL && ElementUsesTargetTexture[ ElementIndex ] == true)
				{
					bAddTri = true;
				}

				// Triangles can only be part of one element so we do not need to continue to other elements.
				break;
			}

		}

		if( bAddTri )
		{
			InfluencedTriangles.Add( TriIndex );
		}

	}

	{
		// Create a canvas for the render target and clear it to white
		FCanvas Canvas(RenderTargetResource, NULL, 0, 0, 0, GetWorld()->FeatureLevel);
		Canvas.Clear( FLinearColor::White);
		
		TArray<FCanvasUVTri> TriList;
		FCanvasUVTri EachTri;
		EachTri.V0_Color = FLinearColor::Black;
		EachTri.V1_Color = FLinearColor::Black;
		EachTri.V2_Color = FLinearColor::Black;
		
		for( int32 CurIndex = 0; CurIndex < InfluencedTriangles.Num(); ++CurIndex )
		{
			const int32 TriIndex = InfluencedTriangles[ CurIndex ];

			// Grab the vertex indices and points for this triangle
			FVector2D TriUVs[ 3 ];
			FVector2D UVMin( 99999.9f, 99999.9f );
			FVector2D UVMax( -99999.9f, -99999.9f );
			for( int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
			{																		 
				const int32 VertexIndex = Indices[ TriIndex * 3 + TriVertexNum ];
				TriUVs[ TriVertexNum ] = LODModel.VertexBuffer.GetVertexUV( VertexIndex, UVSet );

				// Update bounds
				float U = TriUVs[ TriVertexNum ].X;
				float V = TriUVs[ TriVertexNum ].Y;

				if( U < UVMin.X )
				{
					UVMin.X = U;
				}
				if( U > UVMax.X )
				{
					UVMax.X = U;
				}
				if( V < UVMin.Y )
				{
					UVMin.Y = V;
				}
				if( V > UVMax.Y )
				{
					UVMax.Y = V;
				}

			}

			// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
			FVector2D UVOffset( 0.0f, 0.0f );
			if( UVMax.X > 1.0f )
			{
				UVOffset.X = -FMath::FloorToInt( UVMin.X );
			}
			else if( UVMin.X < 0.0f )
			{
				UVOffset.X = 1.0f + FMath::FloorToInt( -UVMax.X );
			}

			if( UVMax.Y > 1.0f )
			{
				UVOffset.Y = -FMath::FloorToInt( UVMin.Y );
			}
			else if( UVMin.Y < 0.0f )
			{
				UVOffset.Y = 1.0f + FMath::FloorToInt( -UVMax.Y );
			}


			// Note that we "wrap" the texture coordinates here to handle the case where the user
			// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
			// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
			// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
			FVector2D TrianglePoints[ 3 ];
			for( int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
			{
				TriUVs[ TriVertexNum ].X += UVOffset.X;
				TriUVs[ TriVertexNum ].Y += UVOffset.Y;

				TrianglePoints[ TriVertexNum ].X = TriUVs[ TriVertexNum ].X * Width;
				TrianglePoints[ TriVertexNum ].Y = TriUVs[ TriVertexNum ].Y * Height;
			}

			EachTri.V0_Pos = TrianglePoints[ 0 ];
			EachTri.V0_UV = TriUVs[ 0 ];
			EachTri.V0_Color = FLinearColor::Black;
			EachTri.V1_Pos = TrianglePoints[ 1 ];
			EachTri.V1_UV = TriUVs[ 1 ];
			EachTri.V1_Color = FLinearColor::Black;
			EachTri.V2_Pos = TrianglePoints[ 2 ];
			EachTri.V2_UV = TriUVs[ 2 ];
			EachTri.V2_Color = FLinearColor::Black;
			TriList.Add( EachTri );
		}
		// Setup the tri render item with the list of tris
		FCanvasTriangleItem TriItem( TriList, RenderTargetResource );
		TriItem.BlendMode = SE_BLEND_Opaque;
		// And render it
		Canvas.DrawItem( TriItem );
		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush_GameThread(true);
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand5,
			FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICmdList.CopyToResolveTarget(
				RenderTargetResource->GetRenderTargetTexture(),		// Source texture
				RenderTargetResource->TextureRHI,
				true,												// Do we need the source image content again?
				FResolveParams() );									// Resolve parameters
		});

	}

	return RetVal;
}

/** Helper function to get the current paint action for use in DoPaint */
EMeshPaintAction::Type FEdModeMeshPaint::GetPaintAction(FViewport* InViewport)
{
	check(InViewport);
	const bool bShiftDown = InViewport->KeyState( EKeys::LeftShift ) || InViewport->KeyState( EKeys::RightShift );
	EMeshPaintAction::Type PaintAction;
	if (bIsFloodFill)
	{
		PaintAction = EMeshPaintAction::Fill;
		//turn off so we don't do this next frame!
		bIsFloodFill = false;
	}
	else if (bPushInstanceColorsToMesh)
	{
		PaintAction = EMeshPaintAction::PushInstanceColorsToMesh;
		//turn off so we don't do this next frame!
		bPushInstanceColorsToMesh = false;
	}
	else
	{
		PaintAction = bShiftDown ? EMeshPaintAction::Erase : EMeshPaintAction::Paint;

		// When painting using VR, allow the modifier button to activate Erase mode
		if( PaintingWithInteractorInVR )
		{
			IVREditorMode* VREditorMode = static_cast<IVREditorMode*>( GetModeManager()->GetActiveMode( IVREditorModule::Get().GetVREditorModeID() ) );
			UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( PaintingWithInteractorInVR );
			if( VREditorMode != nullptr && VRInteractor != nullptr )
			{
				const bool bIsModifierPressed = VRInteractor->IsModifierPressed();
				PaintAction = bIsModifierPressed ? EMeshPaintAction::Erase : EMeshPaintAction::Paint;
			}
		}
	}
	return PaintAction;

}

/** Removes vertex colors associated with the object */
void FEdModeMeshPaint::RemoveInstanceVertexColors(UObject* Obj) const
{
	AActor* Actor = Cast<AActor>(Obj);
	if(Actor != NULL)
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for(const auto& StaticMeshComponent : StaticMeshComponents)
		{
			if(StaticMeshComponent != NULL)
			{
				RemoveComponentInstanceVertexColors(StaticMeshComponent);
			}
		}
	}
}

void FEdModeMeshPaint::RemoveComponentInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent) const
{
	if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL && StaticMeshComponent->StaticMesh->GetNumLODs() > PaintingMeshLODIndex )
	{
		// Mark the mesh component as modified
		StaticMeshComponent->Modify();

		// If this is called from the Remove button being clicked the SMC wont be in a Reregister context,
		// but when it gets called from a Paste or Copy to Source operation it's already inside a more specific
		// SMCRecreateScene context so we shouldn't put it inside another one.
		if (StaticMeshComponent->IsRenderStateCreated())
		{
			// Detach all instances of this static mesh from the scene.
			FComponentReregisterContext ComponentReregisterContext(StaticMeshComponent);

			StaticMeshComponent->RemoveInstanceVertexColors();
		}
		else
		{
			StaticMeshComponent->RemoveInstanceVertexColors();
		}
	}
}

/** Removes vertex colors associated with the currently selected mesh */
void FEdModeMeshPaint::RemoveInstanceVertexColors() const
{
	FScopedTransaction Transaction( LOCTEXT( "MeshPaintMode_VertexPaint_TransactionRemoveInstColors", "Remove Instance Vertex Colors" ) );

	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		RemoveInstanceVertexColors( SelectedActors.GetSelectedObject( CurSelectedActorIndex ) );
	}
}

/** Copies vertex colors associated with the currently selected mesh */
void FEdModeMeshPaint::CopyInstanceVertexColors()
{
	CopiedColorsByComponent.Empty();

	USelection& SelectedActors = *Owner->GetSelectedActors();
	if( SelectedActors.Num() != 1 )
	{
		// warning - works only with 1 actor selected..!
	}
	else
	{		
		AActor* SelectedActor = Cast<AActor>(SelectedActors.GetSelectedObject(0));
		if( SelectedActor != NULL )
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			SelectedActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for(const auto& StaticMeshComponent : StaticMeshComponents)
			{
				if( StaticMeshComponent )
				{
					FPerComponentVertexColorData& PerComponentData = CopiedColorsByComponent[CopiedColorsByComponent.Add(FPerComponentVertexColorData(StaticMeshComponent->StaticMesh, StaticMeshComponent->GetBlueprintCreatedComponentIndex()))];

					int32 NumLODs = StaticMeshComponent->StaticMesh->GetNumLODs();
					for( int32 CurLODIndex = 0; CurLODIndex < NumLODs; ++CurLODIndex )
					{
						FPerLODVertexColorData& LodColorData =  PerComponentData.PerLODVertexColorData[PerComponentData.PerLODVertexColorData.AddZeroed()];

						UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
						FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[ CurLODIndex ];
						FColorVertexBuffer* ColBuffer = &LODModel.ColorVertexBuffer;

						FPositionVertexBuffer* PosBuffer = &LODModel.PositionVertexBuffer;

						// Is there an override buffer? If so, copy colors from there instead...
						if( StaticMeshComponent->LODData.Num() > CurLODIndex )
						{
							FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[ CurLODIndex ];
							if( ComponentLODInfo.OverrideVertexColors )
							{
								ColBuffer = ComponentLODInfo.OverrideVertexColors;
							}
						}

						// Copy the colour buffer
						if( ColBuffer && PosBuffer )
						{
							uint32 NumColVertices = ColBuffer->GetNumVertices();
							uint32 NumPosVertices = PosBuffer->GetNumVertices();

							if (NumColVertices == NumPosVertices)
							{
								// valid color buffer matching the pos verts
								for( uint32 VertexIndex = 0; VertexIndex < NumColVertices; VertexIndex++ )
								{
									LodColorData.ColorsByIndex.Add( ColBuffer->VertexColor( VertexIndex ) );
									LodColorData.ColorsByPosition.Add( PosBuffer->VertexPosition( VertexIndex ), ColBuffer->VertexColor( VertexIndex ) );
								}						
							}
							else
							{
								// mismatched or empty color buffer - just use white
								for( uint32 VertexIndex = 0; VertexIndex < NumPosVertices; VertexIndex++ )
								{
									LodColorData.ColorsByIndex.Add( FColor::White );
									LodColorData.ColorsByPosition.Add( PosBuffer->VertexPosition( VertexIndex ), FColor::White );
								}
							}
						}
					}
				}
			}
		}	
	}
}

/** Pastes vertex colors to the currently selected mesh */
void FEdModeMeshPaint::PasteInstanceVertexColors()
{
	const int32 NumComponentsInCopyBuffer = CopiedColorsByComponent.Num();
	if(0 == NumComponentsInCopyBuffer)
	{
		return;
	}

	FScopedTransaction Transaction( LOCTEXT( "MeshPaintMode_VertexPaint_TransactionPasteInstColors", "Paste Instance Vertex Colors" ) );

	USelection& SelectedActors = *Owner->GetSelectedActors();

	TScopedPointer< FComponentReregisterContext > ComponentReregisterContext;

	for( int32 ActorIndex = 0; ActorIndex < SelectedActors.Num(); ActorIndex++ )
	{
		UObject* CurrentObject = SelectedActors.GetSelectedObject( ActorIndex );
		AActor* CurrentActor = Cast< AActor >( CurrentObject );
		if( CurrentActor != NULL )
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			CurrentActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for(const auto& StaticMeshComponent : StaticMeshComponents)
			{
				if( StaticMeshComponent )
				{
					int32 NumLods = StaticMeshComponent->StaticMesh->GetNumLODs();
					if (0 == NumLods)
					{
						continue;
					}

					// attempt to find a matching component in our clipboard data
					const int32 BlueprintCreatedComponentIndex = StaticMeshComponent->GetBlueprintCreatedComponentIndex();
					FPerComponentVertexColorData* FoundColors = NULL;
					for(auto& CopiedColors : CopiedColorsByComponent)
					{
						if(CopiedColors.OriginalMesh.Get() == StaticMeshComponent->StaticMesh &&
							CopiedColors.ComponentIndex == BlueprintCreatedComponentIndex)
						{
							FoundColors = &CopiedColors;
							break;
						}
					}

					if(FoundColors != NULL)
					{
						ComponentReregisterContext.Reset( new FComponentReregisterContext( StaticMeshComponent ) );
						StaticMeshComponent->SetFlags(RF_Transactional);
						StaticMeshComponent->Modify();
						StaticMeshComponent->SetLODDataCount( NumLods, NumLods );
						RemoveComponentInstanceVertexColors( StaticMeshComponent );

						for( int32 CurLODIndex = 0; CurLODIndex < NumLods; ++CurLODIndex )
						{
							FStaticMeshLODResources& LodRenderData = StaticMeshComponent->StaticMesh->RenderData->LODResources[CurLODIndex];
							FStaticMeshComponentLODInfo& ComponentLodInfo = StaticMeshComponent->LODData[CurLODIndex];

							TArray< FColor > ReOrderedColors;
							TArray< FColor >* PasteFromBufferPtr = &ReOrderedColors;

							const int32 NumLodsInCopyBuffer = FoundColors->PerLODVertexColorData.Num();
							if (CurLODIndex >= NumLodsInCopyBuffer)
							{
								// no corresponding LOD in color paste buffer CopiedColorsByLOD
								// create array of all white verts
								ReOrderedColors.AddUninitialized(LodRenderData.GetNumVertices());

								for (int32 TargetVertIdx = 0; TargetVertIdx < LodRenderData.GetNumVertices(); TargetVertIdx++)
								{
									ReOrderedColors[TargetVertIdx] = FColor::White;
								}
							}
							else if (LodRenderData.GetNumVertices() == FoundColors->PerLODVertexColorData[CurLODIndex].ColorsByIndex.Num())
							{
								// verts counts match - copy from color array by index
								PasteFromBufferPtr = &(FoundColors->PerLODVertexColorData[CurLODIndex].ColorsByIndex);
							}
							else
							{
								// verts counts mismatch - build translation/fixup list of colors in ReOrderedColors
								ReOrderedColors.AddUninitialized(LodRenderData.GetNumVertices());

								// make ReOrderedColors contain one FColor for each vertex in the target mesh
								// matching the position of the target's vert to the position values in LodColorData.ColorsByPosition
								for (int32 TargetVertIdx = 0; TargetVertIdx < LodRenderData.GetNumVertices(); TargetVertIdx++)
								{
									const FColor* FoundColor =
										 FoundColors->PerLODVertexColorData[CurLODIndex].ColorsByPosition.Find(LodRenderData.PositionVertexBuffer.VertexPosition(TargetVertIdx));

									if (FoundColor)
									{
										// A matching color for this vertex was found
										ReOrderedColors[TargetVertIdx] = *FoundColor;
									}
									else
									{
										// A matching color for this vertex could not be found. Make this vertex white
										ReOrderedColors[TargetVertIdx] = FColor::White;
									}
								}
							}

							if( ComponentLodInfo.OverrideVertexColors )
							{
								ComponentLodInfo.ReleaseOverrideVertexColorsAndBlock();
							}
							if( ComponentLodInfo.OverrideVertexColors )
							{
								ComponentLodInfo.BeginReleaseOverrideVertexColors();
								FlushRenderingCommands();
							}
							else
							{
								ComponentLodInfo.OverrideVertexColors = new FColorVertexBuffer;
								ComponentLodInfo.OverrideVertexColors->InitFromColorArray( *PasteFromBufferPtr );
							}
							BeginInitResource( ComponentLodInfo.OverrideVertexColors );				
						}

						StaticMeshComponent->CachePaintedDataIfNecessary();
						StaticMeshComponent->StaticMeshDerivedDataKey = StaticMeshComponent->StaticMesh->RenderData->DerivedDataKey;
					}
				}
			}
		}
	}
}

/** Returns whether the instance vertex colors associated with the currently selected mesh need to be fixed up or not */
bool FEdModeMeshPaint::RequiresInstanceVertexColorsFixup() const
{
	bool bRequiresFixup = false;

	// Find each static mesh component of any selected actors
	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast< AActor >( SelectedActors.GetSelectedObject( CurSelectedActorIndex ) );
		if(SelectedActor != NULL)
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			SelectedActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for(const auto& StaticMeshComponent : StaticMeshComponents)
			{
				// If a static mesh component was found and it requires fixup, exit out and indicate as such
				if( StaticMeshComponent && StaticMeshComponent->RequiresOverrideVertexColorsFixup() )
				{
					bRequiresFixup = true;
					break;
				}
			}
		}
	}

	return bRequiresFixup;
}

/** Attempts to fix up the instance vertex colors associated with the currently selected mesh, if necessary */
void FEdModeMeshPaint::FixupInstanceVertexColors() const
{
	// Find each static mesh component of any selected actors
	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast< AActor >( SelectedActors.GetSelectedObject( CurSelectedActorIndex ) );
		if(SelectedActor != NULL)
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			SelectedActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for(const auto& StaticMeshComponent : StaticMeshComponents)
			{
				// If a static mesh component was found, attempt to fixup its override colors
				if(StaticMeshComponent != NULL)
				{
					StaticMeshComponent->FixupOverrideColorsIfNecessary();
				}
			}
		}
	}
}

void FEdModeMeshPaint::ApplyOrRemoveForceBestLOD(bool bApply)
{
	TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedMeshComponents();

	for (UMeshComponent* MeshComponent : SelectedMeshComponents)
	{
		IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
		if (MeshAdapter)
		{
			ApplyOrRemoveForceBestLOD(*MeshAdapter, MeshComponent, bApply);
		}
	}
}

void FEdModeMeshPaint::ApplyOrRemoveForceBestLOD(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* MeshComponent, bool bApply)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		//=0 means do not force the LOD.
		//>0 means force the lod to x-1.
		StaticMeshComponent->ForcedLodModel = bApply ? 1 : 0;
	}
}

void FEdModeMeshPaint::ApplyVertexColorsToAllLODs()
{	
	TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedMeshComponents();

	for (UMeshComponent* MeshComponent : SelectedMeshComponents)
	{
		IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
		if (MeshAdapter)
		{
			ApplyVertexColorsToAllLODs(*MeshAdapter, MeshComponent);
		}
	}
}

void FEdModeMeshPaint::ApplyVertexColorsToAllLODs(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(InMeshComponent);

	// If a static mesh component was found, apply LOD0 painting to all lower LODs.
	if( StaticMeshComponent && StaticMeshComponent->StaticMesh && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors )
	{
		uint32 NumLODs = StaticMeshComponent->StaticMesh->RenderData->LODResources.Num();
		StaticMeshComponent->Modify();

		// Ensure LODData has enough entries in it, free not required.
		StaticMeshComponent->SetLODDataCount(NumLODs, StaticMeshComponent->LODData.Num());
		for( uint32 i = 1 ; i < NumLODs ; ++i )
		{
			FStaticMeshComponentLODInfo* CurrInstanceMeshLODInfo = &StaticMeshComponent->LODData[ i ];
			FStaticMeshLODResources& CurrRenderData = StaticMeshComponent->StaticMesh->RenderData->LODResources[ i ];
			// Destroy the instance vertex  color array if it doesn't fit
			if(CurrInstanceMeshLODInfo->OverrideVertexColors
				&& CurrInstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() != CurrRenderData.GetNumVertices())
			{
				CurrInstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();
			}	

			if(CurrInstanceMeshLODInfo->OverrideVertexColors)
			{
				CurrInstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
			}
			else
			{
				// Setup the instance vertex color array if we don't have one yet
				CurrInstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;
			}
		}
			
		FlushRenderingCommands();
		FStaticMeshComponentLODInfo& SourceCompLODInfo = StaticMeshComponent->LODData[ 0 ];
		FStaticMeshLODResources& SourceRenderData = StaticMeshComponent->StaticMesh->RenderData->LODResources[ 0 ];
		for( uint32 i=1 ; i < NumLODs ; ++i )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = StaticMeshComponent->LODData[ i ];
			FStaticMeshLODResources& CurRenderData = StaticMeshComponent->StaticMesh->RenderData->LODResources[ i ];

			check(CurCompLODInfo.OverrideVertexColors);

			TArray<FColor> NewOverrideColors;

			if( SourceCompLODInfo.PaintedVertices.Num() > 0 )
			{
				RemapPaintedVertexColors(
				SourceCompLODInfo.PaintedVertices,
					*SourceCompLODInfo.OverrideVertexColors,
					CurRenderData.PositionVertexBuffer,
					&CurRenderData.VertexBuffer,
					NewOverrideColors
					);
			}
			if (NewOverrideColors.Num())
			{
				CurCompLODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);
			}

			// Initialize the vert. colors
			BeginInitResource( CurCompLODInfo.OverrideVertexColors );
		}
	}
}

/** Fills the vertex colors associated with the currently selected mesh*/
void FEdModeMeshPaint::FillInstanceVertexColors()
{
	//force this on for next render
	bIsFloodFill = true;
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

/** Pushes instance vertex colors to the  mesh*/
void FEdModeMeshPaint::PushInstanceVertexColorsToMesh()
{
	int32 NumBaseVertexColorBytes = 0;
	int32 NumInstanceVertexColorBytes = 0;
	bool bHasInstanceMaterialAndTexture = false;

	// Check that there's actually a mesh selected and that it has instanced vertex colors before actually proceeding
	const bool bMeshSelected = GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );
	if ( bMeshSelected && NumInstanceVertexColorBytes > 0 )
	{
		FSuppressableWarningDialog::FSetupInfo SetupInfo( LOCTEXT("PushInstanceVertexColorsPrompt_Message", "Copying the instance vertex colors to the source mesh will replace any of the source mesh's pre-existing vertex colors and affect every instance of the source mesh." ),
													 LOCTEXT("PushInstanceVertexColorsPrompt_Title", "Warning: Copying vertex data overwrites all instances" ), "Warning_PushInstanceVertexColorsPrompt" );

		SetupInfo.ConfirmText = LOCTEXT("PushInstanceVertexColorsPrompt_ConfirmText", "Continue");
		SetupInfo.CancelText = LOCTEXT("PushInstanceVertexColorsPrompt_CancelText", "Abort");
		SetupInfo.CheckBoxText = LOCTEXT("PushInstanceVertexColorsPrompt_CheckBoxText","Always copy vertex colors without prompting");

		FSuppressableWarningDialog VertexColorCopyWarning( SetupInfo );

		// Prompt the user to see if they really want to push the vert colors to the source mesh and to explain
		// the ramifications of doing so. This uses a suppressible dialog so that the user has the choice to always ignore the warning.
		if( VertexColorCopyWarning.ShowModal() != FSuppressableWarningDialog::Cancel )
		{
			//force this on for next render
			bPushInstanceColorsToMesh = true;
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}
	}
}

/** Creates a paintable material/texture for the selected mesh */
void FEdModeMeshPaint::CreateInstanceMaterialAndTexture() const
{
	// @todo MeshPaint: NOT supported at this time.
	return;
}



/** Removes instance of paintable material/texture for the selected mesh */
void FEdModeMeshPaint::RemoveInstanceMaterialAndTexture() const
{
	USelection& SelectedActors = *Owner->GetSelectedActors();
	for (int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex)
	{
		if (AActor* SelectedActor = Cast<AActor>(SelectedActors.GetSelectedObject(CurSelectedActorIndex)))
		{
			TInlineComponentArray<UMeshComponent*> MeshComponents;
			SelectedActor->GetComponents<UMeshComponent>(MeshComponents);
			for (UMeshComponent* MeshComponent : MeshComponents)
			{
				// @todo: this function
			}
		}
	}
}



/** Returns information about the currently selected mesh */
bool FEdModeMeshPaint::GetSelectedMeshInfo( int32& OutTotalBaseVertexColorBytes, int32& OutTotalInstanceVertexColorBytes, bool& bOutHasInstanceMaterialAndTexture ) const
{
	OutTotalInstanceVertexColorBytes = 0;
	OutTotalBaseVertexColorBytes = 0;
	bOutHasInstanceMaterialAndTexture = false;

	int32 NumValidMeshes = 0;

	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast< AActor >( SelectedActors.GetSelectedObject( CurSelectedActorIndex ) );
		if(SelectedActor != NULL)
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			SelectedActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for(const auto& StaticMeshComponent : StaticMeshComponents)
			{
				if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL && StaticMeshComponent->StaticMesh->GetNumLODs() > PaintingMeshLODIndex )
				{
					// count the base mesh color data
					FStaticMeshLODResources& LODModel = StaticMeshComponent->StaticMesh->RenderData->LODResources[ PaintingMeshLODIndex ];
					OutTotalBaseVertexColorBytes += LODModel.ColorVertexBuffer.GetNumVertices();

					// count the instance color data
					if( StaticMeshComponent->LODData.Num() > PaintingMeshLODIndex )
					{
						const FStaticMeshComponentLODInfo& InstanceMeshLODInfo = StaticMeshComponent->LODData[ PaintingMeshLODIndex ];
						if( InstanceMeshLODInfo.OverrideVertexColors )
						{
							OutTotalInstanceVertexColorBytes += InstanceMeshLODInfo.OverrideVertexColors->GetAllocatedSize();
						}
					}

					++NumValidMeshes;
				}
			}
		}
	}

	return ( NumValidMeshes > 0 );
}

void FEdModeMeshPaint::SetBrushRadiiDefault( float InBrushRadius )
{
	float MinBrushRadius, MaxBrushRadius;
	GetBrushRadiiLimits(MinBrushRadius, MaxBrushRadius);	

	InBrushRadius = (float)FMath::Clamp(InBrushRadius, MinBrushRadius, MaxBrushRadius);
	GConfig->SetFloat( TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), InBrushRadius, GEditorPerProjectIni );
}

float FEdModeMeshPaint::GetBrushRadiiDefault() const
{
	float MinBrushRadius, MaxBrushRadius;
	GetBrushRadiiLimits(MinBrushRadius, MaxBrushRadius);

	float BrushRadius = 128.f;
	GConfig->GetFloat( TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), BrushRadius, GEditorPerProjectIni );
	BrushRadius = (float)FMath::Clamp(BrushRadius, MinBrushRadius, MaxBrushRadius);
	return BrushRadius;
}

void FEdModeMeshPaint::GetBrushRadiiSliderLimits( float& OutMinBrushSliderRadius, float& OutMaxBrushSliderRadius ) const
{
	float MinBrushRadius, MaxBrushRadius;
	GetBrushRadiiLimits(MinBrushRadius, MaxBrushRadius);

	OutMinBrushSliderRadius = 1.f;
	GConfig->GetFloat( TEXT("UnrealEd.MeshPaint"), TEXT("MinBrushRadius"), OutMinBrushSliderRadius, GEditorIni );
	OutMinBrushSliderRadius = (float)FMath::Clamp(OutMinBrushSliderRadius, MinBrushRadius, MaxBrushRadius);

	OutMaxBrushSliderRadius = 256.f;
	GConfig->GetFloat( TEXT("UnrealEd.MeshPaint"), TEXT("MaxBrushRadius"), OutMaxBrushSliderRadius, GEditorIni );
	OutMaxBrushSliderRadius = (float)FMath::Clamp(OutMaxBrushSliderRadius, MinBrushRadius, MaxBrushRadius);

	if ( OutMaxBrushSliderRadius < OutMinBrushSliderRadius )
	{
		Swap(OutMaxBrushSliderRadius, OutMinBrushSliderRadius);
	}
}

void FEdModeMeshPaint::GetBrushRadiiLimits( float& OutMinBrushRadius, float& OutMaxBrushRadius ) const
{
	OutMinBrushRadius = 0.01f;
	OutMaxBrushRadius = 250000.f;
}

/** Returns whether there are colors in the copy buffer */
bool FEdModeMeshPaint::CanPasteVertexColors() const
{ 
	for (int32 ComponentIndex = 0; ComponentIndex < CopiedColorsByComponent.Num(); ComponentIndex++)
	{
		const FPerComponentVertexColorData& ComponentData = CopiedColorsByComponent[ComponentIndex];
		for (int32 LODIndex = 0; LODIndex < ComponentData.PerLODVertexColorData.Num(); LODIndex++)
		{
			if (0 < ComponentData.PerLODVertexColorData[LODIndex].ColorsByIndex.Num())
			{
				return true;
			}
		}
	}
	return false; 
}

void FImportVertexTextureHelper::PickVertexColorFromTex(FColor& NewVertexColor, uint8* MipData, FVector2D & UV, UTexture2D* Tex, uint8& ColorMask)
{	
	check(MipData);
	NewVertexColor = FColor(0,0,0,0);

	if ((UV.X >= 0.0f) && (UV.X < 1.0f) && (UV.Y >= 0.0f) && (UV.Y < 1.0f))
	{
		const int32 X = Tex->GetSizeX()*UV.X;
		const int32 Y = Tex->GetSizeY()*UV.Y;

		const int32 Index = ((Y * Tex->GetSizeX()) + X) * 4;
		uint8 B = MipData[Index + 0];
		uint8 G = MipData[Index + 1];
		uint8 R = MipData[Index + 2];
		uint8 A = MipData[Index + 3];

		if (ColorMask & ChannelsMask::ERed)
		{
			NewVertexColor.R = R;
		}

		if (ColorMask & ChannelsMask::EGreen)
		{
			NewVertexColor.G = G;
		}

		if (ColorMask & ChannelsMask::EBlue)
		{
			NewVertexColor.B = B;
		}
		
		if (ColorMask & ChannelsMask::EAlpha)
		{
			NewVertexColor.A = A;
		}
	}	
}


void FImportVertexTextureHelper::ImportVertexColors(FEditorModeTools* ModeTools, const FString& Filename, int32 UVIndex, int32 ImportLOD, uint8 ColorMask)
{
	FMessageLog EditorErrors("EditorErrors");
	EditorErrors.NewPage(LOCTEXT("MeshPaintImportLogLabel", "Mesh Paint: Import Vertex Colors"));

	if (Filename.Len() == 0)
	{
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrPathInvalid", "Path invalid."));
		EditorErrors.Notify();
		return;
	}

	TArray<UStaticMeshComponent*> Components;
	for ( FSelectionIterator It( *ModeTools->GetSelectedActors() ) ; It ; ++It )
	{
		AActor* Actor = Cast<AActor>( *It );
		if(Actor)
		{
			TArray<UStaticMeshComponent*> ThisActorsComponents;
			Actor->GetComponents<UStaticMeshComponent>(ThisActorsComponents);
			Components.Append(ThisActorsComponents);
		}
	}

	if (Components.Num() < 1) 
	{
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrNoActors", "No valid actors selected."));
		EditorErrors.Notify();
		return;
	}

	if (Filename.IsEmpty())
	{
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrNoTga", "No tga file specified."));
		EditorErrors.Notify();
		return;
	}

	if (ColorMask == 0)
	{
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrNoChannels", "No Channels Mask selected."));
		EditorErrors.Notify();
		return;
	}

	bool bComponent =(FMeshPaintSettings::Get().VertexPaintTarget == EMeshVertexPaintTarget::ComponentInstance);

	const FString FullFilename = Filename;
	UTexture2D* Tex = ImportObject<UTexture2D>( GEngine, NAME_None, RF_Public, *FullFilename, NULL, NULL, TEXT("NOMIPMAPS=1 NOCOMPRESSION=1") );
	// If we can't load the file from the disk, create a small empty image as a placeholder and return that instead.
	if( !Tex )
	{
		//UE_LOG(LogMeshPaintEdMode, Warning, TEXT("Error: Importing Failed : Couldn't load '%s'"), *FullFilename );
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrBadTexture", "Couldn't load specified file."));
		EditorErrors.Notify();
		return;
	}

	if (Tex->Source.GetFormat() != TSF_BGRA8)
	{
		EditorErrors.Warning(LOCTEXT("MeshPaint_ImportErrBadFormat", "File format not supported, use RGBA uncompressed file."));
		EditorErrors.Notify();
		return;
	}

	FScopedTransaction Transaction( LOCTEXT( "MeshPaintMode_VertexPaint_TransactionImportFromTGA", "Import Vertex Colors" ) );

	TArray<uint8> SrcMipData;
	Tex->Source.GetMipData(SrcMipData, 0);
	uint8* MipData = SrcMipData.GetData();
	TArray <UStaticMesh*> ModifiedStaticMeshes;

	for(const auto& StaticMeshComponent : Components)
	{
		if(StaticMeshComponent == NULL)
		{
			continue;
		}

		UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh; 
		if(StaticMesh == NULL)
		{
			continue;
		}

		if (ImportLOD >= StaticMesh->GetNumLODs() )
		{
			continue;
		}

		FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[ ImportLOD ];

		TScopedPointer< FStaticMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
		TScopedPointer< FComponentReregisterContext > ComponentReregisterContext;

		FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;

		if (UVIndex >= (int32)LODModel.VertexBuffer.GetNumTexCoords()) 
		{
			continue;
		}

		if (bComponent)
		{
			ComponentReregisterContext.Reset( new FComponentReregisterContext( StaticMeshComponent ) );
			StaticMeshComponent->Modify();

			// Ensure LODData has enough entries in it, free not required.
			StaticMeshComponent->SetLODDataCount(ImportLOD + 1, StaticMeshComponent->LODData.Num());

			InstanceMeshLODInfo = &StaticMeshComponent->LODData[ ImportLOD ];
			InstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();

			if(InstanceMeshLODInfo->OverrideVertexColors)
			{
				InstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
				FlushRenderingCommands();
			}
			else
			{
				// Setup the instance vertex color array if we don't have one yet
				InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

				if((int32)LODModel.ColorVertexBuffer.GetNumVertices() >= LODModel.GetNumVertices())
				{
					// copy mesh vertex colors to the instance ones
					InstanceMeshLODInfo->OverrideVertexColors->InitFromColorArray(&LODModel.ColorVertexBuffer.VertexColor(0), LODModel.GetNumVertices());
				}
				else
				{
					// Original mesh didn't have any colors, so just use a default color
					InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor( 255, 255, 255, 255 ), LODModel.GetNumVertices());
				}
			}
		}
		else
		{
			if (ImportLOD >= StaticMesh->GetNumLODs() )
			{
				continue;
			}

			if (ModifiedStaticMeshes.Find(StaticMesh) != INDEX_NONE)
			{
				continue;
			}
			else
			{
				ModifiedStaticMeshes.AddUnique(StaticMesh);
			}
			// We're changing the mesh itself, so ALL static mesh components in the scene will need
			// to be detached for this (and reattached afterwards.)
			RecreateRenderStateContext.Reset( new FStaticMeshComponentRecreateRenderStateContext( StaticMesh ) );

			// Dirty the mesh
			StaticMesh->Modify();


			// Release the static mesh's resources.
			StaticMesh->ReleaseResources();

			// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
			// allocated, and potentially accessing the UStaticMesh.
			StaticMesh->ReleaseResourcesFence.Wait();		

			if( LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
			{
				// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
				LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.GetNumVertices());

				// @todo MeshPaint: Make sure this is the best place to do this
				BeginInitResource( &LODModel.ColorVertexBuffer );
			}

		}

		FColor NewVertexColor;
		for( uint32 VertexIndex = 0 ; VertexIndex < LODModel.VertexBuffer.GetNumVertices() ; ++VertexIndex )
		{
			FVector2D UV = LODModel.VertexBuffer.GetVertexUV(VertexIndex,UVIndex) ;
			PickVertexColorFromTex(NewVertexColor, MipData, UV, Tex, ColorMask);
			if (bComponent)
			{
				InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex ) = NewVertexColor;
			}
			else
			{
				// TODO_STATICMESH: This needs to propagate to the raw mesh.
				LODModel.ColorVertexBuffer.VertexColor( VertexIndex ) = NewVertexColor;
			}
		}
		if (bComponent)
		{
			BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
		}
		else
		{
			StaticMesh->InitResources();
		}
	}
}

/** 
 * Will update the list of available texture paint targets based on selection 
 */
void FEdModeMeshPaint::UpdateTexturePaintTargetList()
{
	if( bShouldUpdateTextureList && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		CacheActorInfo();

		// We capture the user texture selection before the refresh.  If this texture appears in the
		//  list after the update we will make it the initial selection.
		UTexture2D* PreviouslySelectedTexture = GetSelectedTexture();

		TexturePaintTargetList.Empty();

		TArray<FPaintableTexture> TexturesInSelection;

		if ( ActorBeingEdited.IsValid() )
		{
			AActor* Actor = ActorBeingEdited.Get();
			const FMeshSelectedMaterialInfo* MeshData = CurrentlySelectedActorsMaterialInfo.Find(ActorBeingEdited);
			if ( MeshData != NULL )
			{
				// Get the selected material index and selected actor from the cached actor info
				const int32 MaterialIndex = MeshData->SelectedMaterialIndex;

				// we only operate on mesh components.
				TInlineComponentArray<UMeshComponent*> MeshComponents;
				Actor->GetComponents<UMeshComponent>(MeshComponents);

				int32 DefaultIndex = INDEX_NONE;
				for (const auto& MeshComponent : MeshComponents)
				{
					// Get the geometry adapter
					IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
					if (MeshAdapter)
					{
						// We already know the material we are painting on, take it off the static mesh component
						UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);

						if (Material != NULL)
						{
							// Find all the unique textures used in the top material level of the selected actor materials
							int32 DefaultIndexHere = INDEX_NONE;
							MeshAdapter->QueryPaintableTextures(MaterialIndex, /*out*/ DefaultIndexHere, /*inout*/ TexturesInSelection);
							if (DefaultIndex == INDEX_NONE)
							{
								DefaultIndex = DefaultIndexHere;
							}
						}
					}
				}

				// Generate the list of target paint textures that will be displaying in the UI
				for( int32 TexIndex = 0; TexIndex < TexturesInSelection.Num(); TexIndex++ )
				{
					UTexture2D* Texture2D = Cast<UTexture2D>( TexturesInSelection[ TexIndex ].Texture );
					int32 UVChannelIndex = TexturesInSelection[ TexIndex ].UVChannelIndex;
					// If this is not a UTexture2D we check to see if it is a rendertarget texture
					if( Texture2D == NULL )
					{
						UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( TexturesInSelection[ TexIndex ].Texture );
						if( TextureRenderTarget2D )
						{
							// Since this is a rendertarget, we lookup the original texture that we overrode during the paint operation
							Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );

							// Since we looked up a texture via a rendertarget, it is possible that this texture already exists in our list.  If so 
							//  we will not add it and continue processing other elements.
							if( Texture2D != NULL && TexturesInSelection.Contains( FPaintableTexture(Texture2D, UVChannelIndex) ) )
							{
								continue;
							}
						}
					}

					if( Texture2D != NULL )
					{
						// @todo MeshPaint: We rely on filtering out normal maps by name here.  Obviously a user can name a diffuse with _N_ in the name so
						//   this is not a good option.  We attempted to find all the normal maps from the material above with GetAllNormalParameterNames(),
						//   but that always seems to return an empty list.  This needs to be revisited.
	
						// Some normalmaps in the content will fail checks we do in the if statement below.  So we also check to make sure 
						//   the name does not end with "_N", and that the following substrings do not appear in the name "_N_" "_N0".
						FString Texture2DName;
						Texture2D->GetName(Texture2DName);
						Texture2DName = Texture2DName.ToUpper();

						// Make sure the texture is not a normalmap, we don't support painting on those at the moment.
						if( Texture2D->IsNormalMap() == true 
							|| Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap
							|| Texture2D->LODGroup == TEXTUREGROUP_CharacterNormalMap
							|| Texture2D->LODGroup == TEXTUREGROUP_WeaponNormalMap
							|| Texture2D->LODGroup == TEXTUREGROUP_VehicleNormalMap
							|| Texture2DName.Contains( TEXT("_N0" ))
							|| Texture2DName.Contains( TEXT("_N_" ))
							|| Texture2DName.Contains( TEXT("_NORMAL" ))
							|| (Texture2DName.Right(2)).Contains( TEXT("_N" )) )
						{
							continue;
						}
			
						// Add the texture to our list
						new(TexturePaintTargetList) FTextureTargetListInfo(Texture2D, UVChannelIndex);

						// We stored off the user's selection before we began the update.  Since we cleared the list we lost
						//  that selection info. If the same texture appears in our list after update, we will select it again.
						if( PreviouslySelectedTexture != NULL && Texture2D == PreviouslySelectedTexture )
						{
							TexturePaintTargetList[ TexturePaintTargetList.Num() - 1 ].bIsSelected = true;
						}
					}
				}

				//if there are no default textures, revert to the old method of just selecting the first texture.
				if (DefaultIndex == INDEX_NONE)
				{
					DefaultIndex = 0;
				}

				//We refreshed the list, if nothing else is set we default to the first texture that has IsDefaultMeshPaintTexture set.
				if ((TexturePaintTargetList.Num() > 0) && (GetSelectedTexture() == NULL))
				{
					if (ensure(TexturePaintTargetList.IsValidIndex(DefaultIndex)))
					{
						TexturePaintTargetList[DefaultIndex].bIsSelected = true;
					}
				}

			}
		}
		
		bShouldUpdateTextureList = false;
	}
}

/** Returns index of the currently selected Texture Target */
int32 FEdModeMeshPaint::GetCurrentTextureTargetIndex() const
{
	int32 TextureTargetIndex = 0;
	for ( TArray<FTextureTargetListInfo>::TConstIterator It(TexturePaintTargetList); It; ++It )
	{
		if( It->bIsSelected )
		{
			break;
		}
		TextureTargetIndex++;
	}
	return TextureTargetIndex;
}

/** Returns highest number of UV Sets based on current selection */
int32 FEdModeMeshPaint::GetMaxNumUVSets() const
{
	int32 MaxNumUVSets = 0;

	TArray<UMeshComponent*> MeshComponents = GetSelectedMeshComponents();
	for( UMeshComponent* MeshComponent : MeshComponents )
	{
		// Get the number of UV sets for this mesh
		int32 NumUVSets = 0;

		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			NumUVSets = StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex].VertexBuffer.GetNumTexCoords();
		}
		else
		{
			//@TODO: MESHPAINT: Need a generic way to query this, right now assuming 1 UV set for anything other than static meshes
			NumUVSets = 1;
		}

		MaxNumUVSets = FMath::Max(NumUVSets, MaxNumUVSets);
	}

	return MaxNumUVSets;
}


/** Will return the list of available texture paint targets */
TArray<FTextureTargetListInfo>* FEdModeMeshPaint::GetTexturePaintTargetList()
{
	return &TexturePaintTargetList;
}

/** Will return the selected target paint texture if there is one. */
UTexture2D* FEdModeMeshPaint::GetSelectedTexture()
{
	// Loop through our list of textures and see which one the user has selected
	for( int32 targetIndex = 0; targetIndex < TexturePaintTargetList.Num(); targetIndex++ )
	{
		if(TexturePaintTargetList[targetIndex].bIsSelected)
		{
			return TexturePaintTargetList[targetIndex].TextureData;
		}
	}
	return NULL;
}

void FEdModeMeshPaint::SetSelectedTexture( const UTexture2D* Texture )
{
	// Loop through our list of textures and see which one the user wants to select
	for( int32 targetIndex = 0; targetIndex < TexturePaintTargetList.Num(); targetIndex++ )
	{
		FTextureTargetListInfo& TextureTarget = TexturePaintTargetList[targetIndex];
		if (TextureTarget.TextureData == Texture)
		{
			TextureTarget.bIsSelected = true;
			FMeshPaintSettings::Get().UVChannel = TextureTarget.UVChannelIndex;
		}
		else
		{
			TextureTarget.bIsSelected = false;
		}
	}

	if (ActorBeingEdited.IsValid())
	{
		SaveSettingsForActor(ActorBeingEdited.Get());
	}
}

/** will find the currently selected paint target texture in the content browser */
void FEdModeMeshPaint::FindSelectedTextureInContentBrowser()
{
	UTexture2D* SelectedTexture = GetSelectedTexture();
	if( NULL != SelectedTexture )
	{
		TArray<UObject*> Objects;
		Objects.Add( SelectedTexture );
		GEditor->SyncBrowserToObjects( Objects );
	}
}

/**
 * Used to change the currently selected paint target texture.
 *
 * @param	bToTheRight 	True if a shift to next texture desired, false if a shift to the previous texture is desired.
 * @param	bCycle		 	If set to False, this function will stop at the first or final element.  It will cycle to the opposite end of the list if set to true.
 */
void FEdModeMeshPaint::ShiftSelectedTexture( bool bToTheRight, bool bCycle )
{
	if( TexturePaintTargetList.Num() <= 1 )
	{
		return;
	}

	FTextureTargetListInfo* Prev = NULL;
	FTextureTargetListInfo* Curr = NULL;
	FTextureTargetListInfo* Next = NULL;
	int32 SelectedIndex = -1;

	// Loop through our list of textures and see which one the user has selected, while we are at it we keep track of the prev/next textures
	for( int32 TargetIndex = 0; TargetIndex < TexturePaintTargetList.Num(); TargetIndex++ )
	{
		Curr = &TexturePaintTargetList[ TargetIndex ];
		if( TargetIndex < TexturePaintTargetList.Num() - 1 )
		{
			Next = &TexturePaintTargetList[ TargetIndex + 1 ];
		}
		else
		{
			Next = &TexturePaintTargetList[ 0 ];
		}

		if( TargetIndex == 0 )
		{
			Prev = &TexturePaintTargetList[ TexturePaintTargetList.Num() - 1 ];
		}


		if( Curr->bIsSelected )
		{
			SelectedIndex = TargetIndex;
			
			// Once we find the selected texture we bail.  At this point Next, Prev, and Curr will all be set correctly.
			break;
		}

		Prev = Curr;
	}

	// Nothing is selected so we won't be changing anything.
	if( SelectedIndex == -1 )
	{
		return;
	}

	check( Curr && Next && Prev );

	if( bToTheRight == true )
	{
		// Shift to the right(Next texture)
		if( bCycle == true || SelectedIndex != TexturePaintTargetList.Num() - 1  )
		{
			Curr->bIsSelected = false;
			Next->bIsSelected = true;
		}

	}
	else
	{
		//  Shift to the left(Prev texture)
		if( bCycle == true || SelectedIndex != 0 )
		{
			Curr->bIsSelected = false;
			Prev->bIsSelected = true;
		}
	}
}

/**
 * Used to get a reference to data entry associated with the texture.  Will create a new entry if one is not found.
 *
 * @param	inTexture 		The texture we want to retrieve data for.
 * @return					Returns a reference to the paint data associated with the texture.  This reference
 *								is only valid until the next change to any key in the map.  Will return NULL if 
 *								the an entry for this texture is not found or when inTexture is NULL.
 */
FEdModeMeshPaint::PaintTexture2DData* FEdModeMeshPaint::GetPaintTargetData(  UTexture2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}
	
	PaintTexture2DData* TextureData = PaintTargetData.Find( inTexture );
	return TextureData;
}

/**
 * Used to add an entry to to our paint target data.
 *
 * @param	inTexture 		The texture we want to create data for.
 * @return					Returns a reference to the newly created entry.  If an entry for the input texture already exists it will be returned instead.
 *								Will return NULL only when inTexture is NULL.   This reference is only valid until the next change to any key in the map.
 *								 
 */
FEdModeMeshPaint::PaintTexture2DData* FEdModeMeshPaint::AddPaintTargetData(  UTexture2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}

	PaintTexture2DData* TextureData = GetPaintTargetData( inTexture );
	if( TextureData == NULL )
	{
		// If we didn't find data associated with this texture we create a new entry and return a reference to it.
		//   Note: This reference is only valid until the next change to any key in the map.
		TextureData = &PaintTargetData.Add( inTexture,  PaintTexture2DData( inTexture, false ) );
	}
	return TextureData;
}

/**
 * Used to get the original texture that was overridden with a render target texture.
 *
 * @param	inTexture 		The render target that was used to override the original texture.
 * @return					Returns a reference to texture that was overridden with the input render target texture.  Returns NULL if we don't find anything.
 *								 
 */
UTexture2D* FEdModeMeshPaint::GetOriginalTextureFromRenderTarget( UTextureRenderTarget2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}

	UTexture2D* Texture2D = NULL;

	// We loop through our data set and see if we can find this rendertarget.  If we can, then we add the corresponding UTexture2D to the UI list.
	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		if( TextureData->PaintRenderTargetTexture != NULL &&
			TextureData->PaintRenderTargetTexture == inTexture )
		{
			Texture2D = TextureData->PaintingTexture2D;

			// We found the the matching texture so we can stop searching
			break;
		}
	}

	return Texture2D;
}

/**
* Ends the outstanding transaction, if one exists.
*/
void FEdModeMeshPaint::EndTransaction()
{
	check( ScopedTransaction != NULL );
	delete ScopedTransaction;
	ScopedTransaction = NULL;
}

/**
* Begins a new transaction, if no outstanding transaction exists.
*/
void FEdModeMeshPaint::BeginTransaction(const FText& Description)
{	
	// In paint mode we only allow the BeginTransaction to be called with the EndTransaction pair. We should never be
	// in a state where a second transaction was started before the first was ended.
	check( ScopedTransaction == NULL );
	if ( ScopedTransaction == NULL )
	{
		ScopedTransaction = new FScopedTransaction( Description );
	}
}

/** FEdModeMeshPaint: Called once per frame */
void FEdModeMeshPaint::Tick(FEditorViewportClient* ViewportClient,float DeltaTime)
{
	FEdMode::Tick(ViewportClient,DeltaTime);

	// Will set the texture override up for the selected texture, important for the drop down combo-list and selecting between material instances.
	if (FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
	{
		TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedMeshComponents();

		for (UMeshComponent* MeshComponent : SelectedMeshComponents)
		{
			IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
			if (MeshAdapter)
			{
				SetSpecificTextureOverrideForMesh(*MeshAdapter, GetSelectedTexture());
			}
		}
	}

	if( bDoRestoreRenTargets && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		if( PaintingTexture2D == NULL )
		{
			for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
			{
				PaintTexture2DData* TextureData = &It.Value();
				if( TextureData->PaintRenderTargetTexture != NULL )
				{

					bool bIsSourceTextureStreamedIn = TextureData->PaintingTexture2D->IsFullyStreamedIn();

					if( !bIsSourceTextureStreamedIn )
					{
						//   Make sure it is fully streamed in before we try to do anything with it.
						TextureData->PaintingTexture2D->SetForceMipLevelsToBeResident(30.0f);
						TextureData->PaintingTexture2D->WaitForStreaming();
					}

					//Use the duplicate texture here because as we modify the texture and do undo's, it will be different over the original.
					SetupInitialRenderTargetData( TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture );

				}
			}
		}
		// We attempted a restore of the rendertargets so go ahead and clear the flag
		bDoRestoreRenTargets = false;
	}
}

bool FEdModeMeshPaint::ProcessEditDelete()
{
	RemoveAllGeometryAdapters();

	if (GUnrealEd->CanDeleteSelectedActors(GetWorld(), true, false))
	{
		GEditor->edactDeleteSelected(GetWorld());
	}

	CreateGeometryAdaptersForSelectedComponents();

	return true;
}

void FEdModeMeshPaint::DuplicateTextureMaterialCombo()
{
	UTexture2D* SelectedTexture = GetSelectedTexture();
	
	if ( (NULL != SelectedTexture) && ActorBeingEdited.IsValid() )
	{
		const FMeshSelectedMaterialInfo* MeshData = CurrentlySelectedActorsMaterialInfo.Find(ActorBeingEdited);
		if (MeshData != NULL)
		{
			int32 MaterialIndex = MeshData->SelectedMaterialIndex;

			AActor* Actor = ActorBeingEdited.Get();
			TInlineComponentArray<UMeshComponent*> MeshComponents;
			if (Actor != nullptr)
			{
				Actor->GetComponents<UMeshComponent>(MeshComponents);
			}

			if (MeshComponents.Num() > 0)
			{
				UMeshComponent* MeshComponent = MeshComponents[0];

				IMeshPaintGeometryAdapter* MeshAdapter = FindGeometryAdapter(MeshComponent);
				if (!MeshAdapter)
				{
					return;
				}


				UMaterialInterface* MaterialToCheck = MeshComponent->GetMaterial(MaterialIndex);

				bool bIsSourceTextureStreamedIn = SelectedTexture->IsFullyStreamedIn();

				if( !bIsSourceTextureStreamedIn )
				{
					// We found that this texture is used in one of the meshes materials but not fully loaded, we will
					//   attempt to fully stream in the texture before we try to do anything with it.
					SelectedTexture->SetForceMipLevelsToBeResident(30.0f);
					SelectedTexture->WaitForStreaming();

					// We do a quick sanity check to make sure it is streamed fully streamed in now.
					bIsSourceTextureStreamedIn = SelectedTexture->IsFullyStreamedIn();
				}
		
				UMaterial* NewMaterial = NULL;

				//Duplicate the texture.
				UTexture2D* NewTexture;
				{
					TArray< UObject* > SelectedObjects, OutputObjects;
					SelectedObjects.Add(SelectedTexture);
					ObjectTools::DuplicateObjects(SelectedObjects, TEXT(""), TEXT(""), true, &OutputObjects);

					if(OutputObjects.Num() > 0)
					{
						NewTexture = (UTexture2D*)OutputObjects[0];

						TArray<uint8> TexturePixels;
						SelectedTexture->Source.GetMipData(TexturePixels, 0);					
						uint8* DestData = NewTexture->Source.LockMip(0);					
						check(NewTexture->Source.CalcMipSize(0)==TexturePixels.Num()*sizeof(uint8));
						FMemory::Memcpy(DestData, TexturePixels.GetData(), TexturePixels.Num() * sizeof( uint8 ) );
						NewTexture->Source.UnlockMip(0);
						NewTexture->SRGB = SelectedTexture->SRGB;
						NewTexture->PostEditChange();
					}
					else
					{
						//The user backed out, end this quietly.
						return;
					}
				}

				// Create the new material instance
				UMaterialInstanceConstant* NewMaterialInstance = NULL;
				{
					UClass* FactoryClass = UMaterialInstanceConstantFactoryNew::StaticClass();

					UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
					if ( Factory->ConfigureProperties() )
					{
						FString AssetName;
						FString PackagePath;

						FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
						AssetToolsModule.Get().CreateUniqueAssetName(MaterialToCheck->GetOutermost()->GetName(), TEXT("_Inst"), PackagePath, AssetName);
						PackagePath = FPackageName::GetLongPackagePath(MaterialToCheck->GetPathName());
						NewMaterialInstance = CastChecked<UMaterialInstanceConstant>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory ));
					}

					if(!NewMaterialInstance)
					{
						//appErrorf(TEXT("Could not duplicate %s"), *MaterialToCheck->GetName());
						return;
					}
				
					// Make sure we keep it around for editing even if we later ditch it. 
					NewMaterialInstance->SetFlags(RF_Standalone);

					//We want all uses of this texture to be replaced so go through the entire list.
					NewMaterialInstance->SetParentEditorOnly( MaterialToCheck );
					for(int32 IndexMP(0); IndexMP < MP_MAX; ++IndexMP)
					{
						TArray<UTexture*> OutTextures;
						TArray<FName> OutTextureParamNames;
						MaterialToCheck->GetTexturesInPropertyChain((EMaterialProperty)IndexMP, OutTextures, &OutTextureParamNames, NULL);
						for(int32 ValueIndex(0); ValueIndex < OutTextureParamNames.Num(); ++ValueIndex)
						{
							UTexture* OutTexture;
							if(MaterialToCheck->GetTextureParameterValue(OutTextureParamNames[ValueIndex], OutTexture) == true && OutTexture == SelectedTexture)
							{
								// Bind texture to the material instance
								NewMaterialInstance->SetTextureParameterValueEditorOnly(OutTextureParamNames[ValueIndex], NewTexture);
							}
						}
					}
					NewMaterialInstance->MarkPackageDirty();
					NewMaterialInstance->PostEditChange();
				}

				bool bMaterialChanged = false;
				ClearMeshTextureOverrides(*MeshAdapter, MeshComponent);

				MeshComponent->SetMaterial(MaterialIndex, NewMaterialInstance);
				UpdateSettingsForMeshComponent(MeshComponent, SelectedTexture, NewTexture);

				MeshComponent->MarkPackageDirty();
			
				ActorSelectionChangeNotify();
			}
		}
	}
}

void FEdModeMeshPaint::CreateNewTexture()
{
	UTexture2D* SelectedTexture = GetSelectedTexture();
	if( SelectedTexture != NULL )
	{
		UClass* FactoryClass = UTexture2DFactoryNew::StaticClass();
			
		UTexture2DFactoryNew* Factory = NewObject<UTexture2DFactoryNew>();
		if ( Factory->ConfigureProperties() )
		{
			FString AssetName;
			FString PackagePath;

			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
			AssetToolsModule.Get().CreateUniqueAssetName(SelectedTexture->GetOutermost()->GetName(), TEXT("_New"), PackagePath, AssetName);
			PackagePath = FPackageName::GetLongPackagePath(SelectedTexture->GetPathName());
			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UTexture2D::StaticClass(), Factory );

			TArray<UObject*> Objects;
			Objects.Add( NewAsset );
			GEditor->SyncBrowserToObjects( Objects );
		}	
	}	
}

void FEdModeMeshPaint::SetEditingMesh( TWeakObjectPtr<AActor> InActor )
{
	ActorBeingEdited = InActor;
	bShouldUpdateTextureList = true;
}

void FEdModeMeshPaint::SetEditingMaterialIndex( int32 SelectedIndex )
{
	if (CurrentlySelectedActorsMaterialInfo.Contains(ActorBeingEdited))
	{
		CurrentlySelectedActorsMaterialInfo[ActorBeingEdited].SelectedMaterialIndex = SelectedIndex;
		bShouldUpdateTextureList = true;
	}
}

int32 FEdModeMeshPaint::GetEditingMaterialIndex() const
{
	if (CurrentlySelectedActorsMaterialInfo.Contains(ActorBeingEdited))
	{
		return CurrentlySelectedActorsMaterialInfo[ActorBeingEdited].SelectedMaterialIndex;
	}
	return 0;
}

int32 FEdModeMeshPaint::GetEditingActorsNumberOfMaterials() const
{
	if (CurrentlySelectedActorsMaterialInfo.Contains(ActorBeingEdited))
	{
		return CurrentlySelectedActorsMaterialInfo[ActorBeingEdited].NumMaterials;
	}
	return 0;
}

void FEdModeMeshPaint::CacheActorInfo()
{
	TMap<TWeakObjectPtr<AActor>,FMeshSelectedMaterialInfo> TempMap;

	TArray<UMeshComponent*> MeshComponents = GetSelectedMeshComponents();
	for (const auto& MeshComponent : MeshComponents)
	{
		AActor* CurActor = CastChecked< AActor >(MeshComponent->GetOuter());
		if (CurActor != NULL)
		{
			if (!CurrentlySelectedActorsMaterialInfo.Contains(CurActor))
			{
				// Get the materials used by the mesh
				TArray<UMaterialInterface*> UsedMaterials;
				MeshComponent->GetUsedMaterials(UsedMaterials);

				TempMap.Add(CurActor,FMeshSelectedMaterialInfo(UsedMaterials.Num()));
			}
			else
			{
				TempMap.Add(CurActor,CurrentlySelectedActorsMaterialInfo[CurActor]);
			}
		}
	}

	CurrentlySelectedActorsMaterialInfo.Empty(TempMap.Num());
	CurrentlySelectedActorsMaterialInfo.Append(TempMap);

	if ( (!ActorBeingEdited.IsValid() || !CurrentlySelectedActorsMaterialInfo.Contains(ActorBeingEdited)) && CurrentlySelectedActorsMaterialInfo.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Keys;
		CurrentlySelectedActorsMaterialInfo.GetKeys(Keys);
		ActorBeingEdited = Keys[0];
	}
}

TArray<TWeakObjectPtr<AActor>> FEdModeMeshPaint::GetEditingActors() const
{
	TArray<TWeakObjectPtr<AActor>> MeshNames;
	CurrentlySelectedActorsMaterialInfo.GetKeys(MeshNames);
	return MeshNames;
}

TWeakObjectPtr<AActor> FEdModeMeshPaint::GetEditingActor() const
{
	return ActorBeingEdited;
}


void FEdModeMeshPaint::OnVRAction( class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, 
	const FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled )
{
	IVREditorMode* VREditorMode = static_cast<IVREditorMode*>( GetModeManager()->GetActiveMode( IVREditorModule::Get().GetVREditorModeID() ) );
	UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( Interactor );

	if( VREditorMode != nullptr && VRInteractor != nullptr )
	{
		if( Action.ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed )
		{
			if( !bIsPainting && Action.Event == IE_Pressed && !VRInteractor->IsHoveringOverUI() )
			{
				// Check to see that we're clicking on a selected object.  You can only paint on selected things.  Otherwise,
				// we'll fall through to the normal interaction code which might cause the object to become selected.
				bool bIsClickingOnSelectedObject = false;
				{
					FHitResult HitResult = VRInteractor->GetHitResultFromLaserPointer();
					if( HitResult.Actor.IsValid() )
					{
						UViewportWorldInteraction& WorldInteraction = VREditorMode->GetWorldInteraction();
						
						if( WorldInteraction.IsInteractableComponent( HitResult.GetComponent() ) )
						{
							AActor* Actor = HitResult.Actor.Get();

							// Make sure we're not hovering over some other viewport interactable, such as a dockable window selection bar or close button
							IViewportInteractableInterface* ActorInteractable = Cast<IViewportInteractableInterface>( Actor );
							if( ActorInteractable == nullptr )
							{
								if( Actor != WorldInteraction.GetTransformGizmoActor() )  // Don't change any actor selection state if the user clicked on a gizmo
								{
									if( Actor->IsSelected() )
									{
										bIsClickingOnSelectedObject = true;
									}
								}
							}
						}
					}
				}

				if( bIsClickingOnSelectedObject )
				{
					bWasHandled = true;
					bOutIsInputCaptured = true;

					// Don't allow a "full press" to happen at all with this trigger pull.  We don't need full press for anything, and it
					// would interrupt our light press.
					VRInteractor->SetAllowTriggerFullPress( false );

					StartPainting();
					PaintingWithInteractorInVR = Interactor;

					// Go ahead and paint immediately
					FVector LaserPointerStart, LaserPointerEnd;
					if( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
					{
						const FVector LaserPointerDirection = ( LaserPointerEnd - LaserPointerStart ).GetSafeNormal();

						// Apply VR controller trigger pressure
						float StrengthScale = VRInteractor->GetSelectAndMoveTriggerValue();

						// Paint!
						const bool bVisualCueOnly = false;
						const EMeshPaintAction::Type PaintAction = GetPaintAction( ViewportClient.Viewport );
						bool bAnyPaintableActorsUnderCursor = false;
						DoPaint( VREditorMode->GetHeadTransform().GetLocation(), LaserPointerStart, LaserPointerDirection, NULL, PaintAction, bVisualCueOnly, StrengthScale, /* Out */ bAnyPaintableActorsUnderCursor );
					}
				}
			}

			// Stop current tracking if the user is no longer painting
			else if( bIsPainting && Action.Event == IE_Released && PaintingWithInteractorInVR && PaintingWithInteractorInVR == Interactor )
			{
				EndPainting();

				bWasHandled = true;
				bOutIsInputCaptured = false;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
