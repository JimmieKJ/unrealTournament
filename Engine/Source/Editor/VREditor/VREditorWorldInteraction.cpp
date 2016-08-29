// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorWorldInteraction.h"
#include "VREditorMode.h"
#include "VREditorUISystem.h"
#include "VREditorFloatingUI.h"
#include "VREditorDockableWindow.h"
#include "VREditorInteractor.h"
#include "VIBaseTransformGizmo.h"
#include "SnappingUtils.h"
#include "ScopedTransaction.h"

// For actor placement
#include "ObjectTools.h"
#include "AssetSelection.h"
#include "IPlacementModeModule.h"
#include "Kismet/GameplayStatics.h"

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Editor/LevelEditor/Public/LevelEditorActions.h"

#define LOCTEXT_NAMESPACE "VREditor"


namespace VREd
{
	static FAutoConsoleVariable SizeOfActorsOverContentBrowserThumbnail( TEXT( "VREd.SizeOfActorsOverContentBrowserThumbnail" ), 6.0f, TEXT( "How large objects should be when rendered 'thumbnail size' over the Content Browser" ) );
	static FAutoConsoleVariable OverrideSizeOfPlacedActors( TEXT( "VREd.OverrideSizeOfPlacedActors" ), 0.0f, TEXT( "If set to a value greater than zero, sets the size in cm of a placed actor (relative to your world's scale)" ) );
	static FAutoConsoleVariable HoverHapticFeedbackStrength( TEXT( "VREd.HoverHapticFeedbackStrength" ), 0.1f, TEXT( "Default strength for haptic feedback when hovering" ) );
	static FAutoConsoleVariable HoverHapticFeedbackTime( TEXT( "VREd.HoverHapticFeedbackTime" ), 0.2f, TEXT( "The minimum time between haptic feedback for hovering" ) );
	static FAutoConsoleVariable PivotPointTransformGizmo( TEXT( "VREd.PivotPointTransformGizmo" ), 1, TEXT( "If the pivot point transform gizmo is used instead of the bounding box gizmo" ) );
	static FAutoConsoleVariable DragHapticFeedbackStrength( TEXT( "VREd.DragHapticFeedbackStrength" ), 1.0f, TEXT( "Default strength for haptic feedback when starting to drag objects" ) );
	static FAutoConsoleVariable PlacementInterpolationDuration( TEXT( "VREd.PlacementInterpolationDuration" ), 0.6f, TEXT( "How long we should interpolate newly-placed objects to their target location." ) );
}

UVREditorWorldInteraction::UVREditorWorldInteraction( const FObjectInitializer& Initializer ) : 
	Super( Initializer ),
	Owner( nullptr ),
	DropMaterialOrMaterialSound( nullptr ),
	FloatingUIAssetDraggedFrom( nullptr ),
	PlacingMaterialOrTextureAsset( nullptr )
{
	// Find out when the user drags stuff out of a content browser
	FEditorDelegates::OnAssetDragStarted.AddUObject( this, &UVREditorWorldInteraction::OnAssetDragStartedFromContentBrowser );

	// Load sounds
	DropMaterialOrMaterialSound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_grab_Cue" ) );
	check( DropMaterialOrMaterialSound != nullptr );
}

void UVREditorWorldInteraction::Shutdown()
{
	Super::Shutdown();

	FEditorDelegates::OnAssetDragStarted.RemoveAll( this );

	PlacingMaterialOrTextureAsset = nullptr;
	FloatingUIAssetDraggedFrom = nullptr;
	DropMaterialOrMaterialSound = nullptr;
	Owner = nullptr;
}

bool UVREditorWorldInteraction::IsInteractableComponent( const UActorComponent* Component ) const
{
	bool bResult = false;
	bResult = UViewportWorldInteraction::IsInteractableComponent( Component );

	// Don't bother if the base class already found out that this is a custom interactable
	if ( bResult )
	{
		// Don't interact with frozen actors or our UI
		if ( Component != nullptr )
		{
			static const bool bIsVREditorDemo = FParse::Param( FCommandLine::Get(), TEXT( "VREditorDemo" ) );	// @todo vreditor: Remove this when no longer needed (console variable, too!)
			const bool bIsFrozen = bIsVREditorDemo && Component->GetOwner()->GetActorLabel().StartsWith( TEXT( "Frozen_" ) );
			bResult = !bIsFrozen && !GetOwner().GetUISystem().IsWidgetAnEditorUIWidget( Component ); // Don't allow user to move around our UI widgets //@todo VREditor: This wouldn't be necessary if the UI used the interactable interface
		}
	}
	
	return bResult;
}

void UVREditorWorldInteraction::StopDragging( UViewportInteractor* Interactor )
{
	EViewportInteractionDraggingMode InteractorDraggingMode = Interactor->GetDraggingMode();

	// If we were placing a material, go ahead and do that now
	if ( InteractorDraggingMode == EViewportInteractionDraggingMode::Material )
	{
		PlaceDraggedMaterialOrTexture( Interactor );
	}

	// If we were placing something, bring the window back
	if ( InteractorDraggingMode == EViewportInteractionDraggingMode::ActorsAtLaserImpact ||
		InteractorDraggingMode == EViewportInteractionDraggingMode::Material )
	{
		GetOwner().GetUISystem().ShowEditorUIPanel( FloatingUIAssetDraggedFrom, Cast<UVREditorInteractor>( Interactor->GetOtherInteractor() ), 
			true, false, false, false );
		FloatingUIAssetDraggedFrom = nullptr;
	}


	Super::StopDragging( Interactor );
}

void UVREditorWorldInteraction::StartDraggingMaterialOrTexture( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, const FVector HitLocation, UObject* MaterialOrTextureAsset )
{
	check( MaterialOrTextureAsset != nullptr );

	FVector LaserPointerStart, LaserPointerEnd;
	const bool bHaveLaserPointer = Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd );
	if( bHaveLaserPointer )
	{
		PlacingMaterialOrTextureAsset = MaterialOrTextureAsset;

		FViewportInteractorData& InteractorData = Interactor->GetInteractorData();

		Interactor->SetDraggingMode( EViewportInteractionDraggingMode::Material );

		// Starting a new drag, so make sure the other hand doesn't think it's assisting us
		FViewportInteractorData& OtherInteractorData = Interactor->GetOtherInteractor()->GetInteractorData();
		OtherInteractorData.bWasAssistingDrag = false;

		InteractorData.bIsFirstDragUpdate = true;
		InteractorData.bWasAssistingDrag = false;
		InteractorData.DragRayLength = ( HitLocation - LaserPointerStart ).Size();
		InteractorData.LastLaserPointerStart = LaserPointerStart;
		InteractorData.LastDragToLocation = HitLocation;
		InteractorData.LaserPointerImpactAtDragStart = HitLocation;
		InteractorData.DragTranslationVelocity = FVector::ZeroVector;
		InteractorData.DragRayLengthVelocity = 0.0f;
		InteractorData.bIsDrivingVelocityOfSimulatedTransformables = false;

		InteractorData.DraggingTransformGizmoComponent = nullptr;
	
		InteractorData.TransformGizmoInteractionType = ETransformGizmoInteractionType::None;
		InteractorData.GizmoStartTransform = FTransform::Identity;
		InteractorData.GizmoStartLocalBounds = FBox( 0 );

		InteractorData.GizmoSpaceFirstDragUpdateOffsetAlongAxis = FVector::ZeroVector;	// Will be determined on first update
		InteractorData.GizmoSpaceDragDeltaFromStartOffset = FVector::ZeroVector;	// Set every frame while dragging

		bDraggedSinceLastSelection = false;
		LastDragGizmoStartTransform = FTransform::Identity;

		// Play a haptic effect when objects aTrackingTransactionre picked up
		Interactor->PlayHapticEffect( VREd::DragHapticFeedbackStrength->GetFloat() );
	}
}

void UVREditorWorldInteraction::OnAssetDragStartedFromContentBrowser( const TArray<FAssetData>& DraggedAssets, UActorFactory* FactoryToUse )
{
	const bool bIsPreview = false;	 // @todo vreditor placement: Consider supporting a "drop preview" actor (so you can cancel placement interactively)

	bool bTransactionStarted = false;

	// Figure out which controller pressed the button and started dragging
	// @todo vreditor placement: This logic could misfire.  Ideally we would be routed information from the pointer event, so we can determine the hand.
	UVREditorInteractor* PlacingWithInteractor = nullptr;
	for ( UViewportInteractor* Interactor : GetInteractors() )
	{
		UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( Interactor );
		FViewportActionKeyInput* SelectAndMoveAction = Interactor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove );
		FViewportActionKeyInput* SelectAndMoveLightlyPressedAction = Interactor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove_LightlyPressed );

		if ( VRInteractor && ( ( SelectAndMoveAction && SelectAndMoveAction->bIsInputCaptured ) 
			|| ( SelectAndMoveLightlyPressedAction && SelectAndMoveLightlyPressedAction->bIsInputCaptured ) ) )
		{
			if ( VRInteractor->IsClickingOnUI() && !VRInteractor->IsRightClickingOnUI() ) 
			{
				PlacingWithInteractor = VRInteractor;
				break;
			}
		}
	}


	// We're always expecting a hand to be hovering at the time we receive this event
	if( PlacingWithInteractor )
	{
		// Cancel UI input
		{
			FViewportActionKeyInput* SelectAndMoveAction = PlacingWithInteractor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove );
			FViewportActionKeyInput* SelectAndMoveLightlyPressedAction = PlacingWithInteractor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove_LightlyPressed );

			PlacingWithInteractor->SetIsClickingOnUI( false );
			PlacingWithInteractor->SetIsRightClickingOnUI( false );

			if ( SelectAndMoveAction )
			{
				SelectAndMoveAction->bIsInputCaptured = false;
			}

			if ( SelectAndMoveLightlyPressedAction )
			{
				SelectAndMoveLightlyPressedAction->bIsInputCaptured = false;
			}
		}

		FloatingUIAssetDraggedFrom = PlacingWithInteractor->GetHoveringOverWidgetComponent();
		// Hide the UI panel that's being used to drag
		GetOwner().GetUISystem().ShowEditorUIPanel( FloatingUIAssetDraggedFrom, PlacingWithInteractor, false, false, true, false );

		TArray< UObject* > DroppedObjects;
		TArray< AActor* > AllNewActors;

		UObject* DraggingSingleMaterialOrTexture = nullptr;

		FVector PlaceAt = PlacingWithInteractor->GetHoverLocation();

		// Only place the object at the laser impact point if we're NOT going to interpolate to the impact
		// location.  When interpolation is enabled, it looks much better to blend to the new location
		const bool bShouldInterpolateFromDragLocation = VREd::PlacementInterpolationDuration->GetFloat() > KINDA_SMALL_NUMBER;
		if( !bShouldInterpolateFromDragLocation )
		{
			FVector HitLocation = FVector::ZeroVector;
			if( FindPlacementPointUnderLaser( PlacingWithInteractor, /* Out */ HitLocation ) )
			{
				PlaceAt = HitLocation;
			}
		}

		for( const FAssetData& AssetData : DraggedAssets )
		{
			bool bCanPlace = true;

			UObject* AssetObj = AssetData.GetAsset();
			if( !ObjectTools::IsAssetValidForPlacing( GetViewportWorld(), AssetData.ObjectPath.ToString() ) )
			{
				bCanPlace = false;
			}
			else
			{
				UClass* ClassObj = Cast<UClass>( AssetObj );
				if( ClassObj )
				{
					if( !ObjectTools::IsClassValidForPlacing( ClassObj ) )
					{
						bCanPlace = false;
					}

					AssetObj = ClassObj->GetDefaultObject();
				}
			}

			const bool bIsMaterialOrTexture = ( AssetObj->IsA( UMaterialInterface::StaticClass() ) || AssetObj->IsA( UTexture::StaticClass() ) );
			if( bIsMaterialOrTexture && DraggedAssets.Num() == 1 )
			{
				DraggingSingleMaterialOrTexture = AssetObj;;
			}

			// Check if the asset has an actor factory
			bool bHasActorFactory = FActorFactoryAssetProxy::GetFactoryForAsset( AssetData ) != NULL;

			if( !( AssetObj->IsA( AActor::StaticClass() ) || bHasActorFactory ) &&
				!AssetObj->IsA( UBrushBuilder::StaticClass() ) )
			{
				bCanPlace = false;
			}


			if( bCanPlace )
			{
				CA_SUPPRESS(6240);
				if( !bTransactionStarted && !bIsPreview )
				{
					bTransactionStarted = true;

					TrackingTransaction.TransCount++;
					TrackingTransaction.Begin( LOCTEXT( "PlacingActors", "Placing Actors" ) );

					// Suspend actor/component modification during each delta step to avoid recording unnecessary overhead into the transaction buffer
					GEditor->DisableDeltaModification( true );
				}

				GEditor->ClickLocation = PlaceAt;
				GEditor->ClickPlane = FPlane( PlaceAt, FVector::UpVector );

				// Attempt to create actors from the dropped object
				const bool bSelectNewActors = true;
				const EObjectFlags NewObjectFlags = bIsPreview ? RF_Transient : RF_Transactional;

				TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject( GetViewportWorld()->GetCurrentLevel(), AssetObj, bSelectNewActors, NewObjectFlags, FactoryToUse );

				if( NewActors.Num() > 0 )
				{
					DroppedObjects.Add( AssetObj );
					for( AActor* NewActor : NewActors )
					{
						AllNewActors.Add( NewActor );

						if( VREd::OverrideSizeOfPlacedActors->GetFloat() > KINDA_SMALL_NUMBER )
						{
							const FBox LocalSpaceBounds = NewActor->CalculateComponentsBoundingBoxInLocalSpace();
							const float LocalBoundsSize = LocalSpaceBounds.GetSize().GetAbsMax();

							const float DesiredSize = 
								( bShouldInterpolateFromDragLocation ? 
									VREd::SizeOfActorsOverContentBrowserThumbnail->GetFloat() :
									VREd::OverrideSizeOfPlacedActors->GetFloat() )
								* GetWorldScaleFactor();
							NewActor->SetActorScale3D( FVector( DesiredSize / LocalBoundsSize ) );
						}
					}
				}
			}
		}

		// Cancel the transaction if nothing was placed
		if( bTransactionStarted && AllNewActors.Num() == 0)
		{
			--TrackingTransaction.TransCount;
			TrackingTransaction.Cancel();
			GEditor->DisableDeltaModification( false );
		}

		if( AllNewActors.Num() > 0 )	// @todo vreditor: Should we do this for dragged materials too?
		{
			if( !bIsPreview )
			{
				if( IPlacementModeModule::IsAvailable() )
				{
					IPlacementModeModule::Get().AddToRecentlyPlaced( DroppedObjects, FactoryToUse );
				}

				FEditorDelegates::OnNewActorsDropped.Broadcast( DroppedObjects, AllNewActors );
			}
		}

		if( AllNewActors.Num() > 0 )
		{
			const FViewportActionKeyInput Action( ViewportWorldActionTypes::SelectAndMove_LightlyPressed );

			// Start dragging the new actor(s)
			const bool bIsPlacingActors = true;
			StartDraggingActors( PlacingWithInteractor, Action, GetTransformGizmoActor()->GetRootComponent(), PlaceAt, bIsPlacingActors );

			// If we're interpolating, update the target transform of the actors to use our overridden size.  When
			// we placed them we set their size to be 'thumbnail sized', and we want them to interpolate to
			// their actual size in the world
			if( bShouldInterpolateFromDragLocation )
			{
				for( TUniquePtr<FViewportTransformable>& TransformablePtr : Transformables )
				{
					FViewportTransformable& Transformable = *TransformablePtr;

					float ObjectSize = 1.0f;

					if( VREd::OverrideSizeOfPlacedActors->GetFloat() > KINDA_SMALL_NUMBER )
					{
						ObjectSize = 1.0f * GetWorldScaleFactor();

						// Got an actor?
						AActor* Actor = Cast<AActor>( Transformable.Object.Get() );
						if( Actor != nullptr )
						{
							const FBox LocalSpaceBounds = Actor->CalculateComponentsBoundingBoxInLocalSpace();
							const float LocalBoundsSize = LocalSpaceBounds.GetSize().GetAbsMax();

							const float DesiredSize = VREd::OverrideSizeOfPlacedActors->GetFloat() * GetWorldScaleFactor();
							const FVector NewScale( DesiredSize / LocalBoundsSize );

							Transformable.UnsnappedTargetTransform.SetScale3D( NewScale );
							Transformable.StartTransform.SetScale3D( NewScale );
							Transformable.LastTransform.SetScale3D( NewScale );
							Transformable.TargetTransform.SetScale3D( NewScale );
						}
					}
				}
			}
		}

		if( DraggingSingleMaterialOrTexture != nullptr )
		{
			const FViewportActionKeyInput Action( ViewportWorldActionTypes::SelectAndMove_LightlyPressed );

			// Start dragging the material
			StartDraggingMaterialOrTexture( PlacingWithInteractor,	Action, PlacingWithInteractor->GetHoverLocation(), DraggingSingleMaterialOrTexture );
		}
	}
}


void UVREditorWorldInteraction::PlaceDraggedMaterialOrTexture( UViewportInteractor* Interactor )
{
	if( ensure( Interactor->GetDraggingMode() == EViewportInteractionDraggingMode::Material ) &&
		PlacingMaterialOrTextureAsset != nullptr )
	{
		// Check to see if the laser pointer is over something we can drop on
		UPrimitiveComponent* HitComponent = nullptr;
		{
			const bool bIgnoreGizmos = true;	// Never place on top of gizmos, just ignore them
			const bool bEvenIfUIIsInFront = true;	// Don't let the UI block placement
			FHitResult HitResult = Interactor->GetHitResultFromLaserPointer( nullptr, bIgnoreGizmos, nullptr, bEvenIfUIIsInFront );
			if( HitResult.Actor.IsValid() )
			{
				if( IsInteractableComponent( HitResult.GetComponent() ) )	// @todo vreditor placement: We don't necessarily need to restrict to only VR-interactive components here
				{
					// Don't place materials on UI widget handles though!
					if( Cast<AVREditorFloatingUI>( HitResult.GetComponent()->GetOwner() ) == nullptr )
					{
						HitComponent = HitResult.GetComponent();
					}
				}
			}
		}

		if( HitComponent != nullptr )
		{
			UObject* ObjToUse = PlacingMaterialOrTextureAsset;

			// Dropping a texture?
			UTexture* DroppedObjAsTexture = Cast<UTexture>( ObjToUse );
			if( DroppedObjAsTexture != NULL )
			{
				// Turn dropped textures into materials
				ObjToUse = FLevelEditorViewportClient::GetOrCreateMaterialFromTexture( DroppedObjAsTexture );
			}

			// Dropping a material?
			UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>( ObjToUse );
			if( DroppedObjAsMaterial )
			{
				// @todo vreditor placement: How do we get the material ID that was dropped on?  Regular editor uses hit proxies.  We may need to augment FHitResult.
				// @todo vreditor placement: Support optionally dropping on all materials, not only the impacted material
				const int32 TargetMaterialSlot = -1;	// All materials
				const bool bPlaced = FComponentEditorUtils::AttemptApplyMaterialToComponent( HitComponent, DroppedObjAsMaterial, TargetMaterialSlot );
			}

			if( DroppedObjAsMaterial || DroppedObjAsTexture )
			{
				UGameplayStatics::PlaySound2D( GetViewportWorld(), DropMaterialOrMaterialSound );
			}
		}
	}

	this->PlacingMaterialOrTextureAsset = nullptr;
}

void UVREditorWorldInteraction::SnapSelectedActorsToGround()
{
	TSharedPtr<SLevelViewport> LevelEditorViewport = StaticCastSharedPtr<SLevelViewport>( GetViewportClient()->GetEditorViewportWidget() );
	if ( LevelEditorViewport.IsValid() )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT( "LevelEditor" ) );
		const FLevelEditorCommands& Commands = LevelEditorModule.GetLevelEditorCommands();
		const TSharedPtr< FUICommandList >& CommandList = LevelEditorViewport->GetParentLevelEditor().Pin()->GetLevelEditorActions(); //@todo vreditor: Cast on leveleditor

		CommandList->ExecuteAction( Commands.SnapBottomCenterBoundsToFloor.ToSharedRef() );

		// @todo vreditor: This should not be needed after to allow transformables to stop animating the transforms of actors after they come to rest
		SetupTransformablesForSelectedActors();
	}
}

#undef LOCTEXT_NAMESPACE