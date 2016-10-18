// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionModule.h"
#include "ViewportInteractor.h"
#include "ViewportWorldInteraction.h"
#include "VIBaseTransformGizmo.h"
#include "ViewportInteractableInterface.h"

namespace VI
{
	static FAutoConsoleVariable LaserPointerMaxLength( TEXT( "VI.LaserPointerMaxLength" ), 10000.0f, TEXT( "Maximum length of the laser pointer line" ) );
	static FAutoConsoleVariable DragHapticFeedbackStrength( TEXT( "VI.DragHapticFeedbackStrength" ), 1.0f, TEXT( "Default strength for haptic feedback when starting to drag objects" ) ); //@todo ViewportInteraction: Duplicate from ViewportWorldInteraction
	static FAutoConsoleVariable SelectionHapticFeedbackStrength( TEXT( "VI.SelectionHapticFeedbackStrength" ), 0.5f, TEXT( "Default strength for haptic feedback when selecting objects" ) );
}

UViewportInteractor::UViewportInteractor( const FObjectInitializer& Initializer ) : 
	UObject( Initializer ),
	InteractorData(),
	KeyToActionMap(),
	WorldInteraction( nullptr ),
	OtherInteractor( nullptr )
{

}

FViewportInteractorData& UViewportInteractor::GetInteractorData()
{
	return InteractorData;
}

void UViewportInteractor::SetWorldInteraction( UViewportWorldInteraction* InWorldInteraction )
{
	WorldInteraction = InWorldInteraction;
}

void UViewportInteractor::SetOtherInteractor( UViewportInteractor* InOtherInteractor )
{
	OtherInteractor = InOtherInteractor;
}

void UViewportInteractor::RemoveOtherInteractor()
{
	OtherInteractor = nullptr;
}

UViewportInteractor* UViewportInteractor::GetOtherInteractor() const
{
	return OtherInteractor;
}

void UViewportInteractor::UpdateHoverResult( FHitResult& OutHitResult )
{
	FHitResult HitResult = GetHitResultFromLaserPointer();
	InteractorData.bIsHovering = HitResult.bBlockingHit;
	InteractorData.HoverLocation = HitResult.ImpactPoint;
	OutHitResult = HitResult;
}

void UViewportInteractor::Shutdown()
{
	KeyToActionMap.Empty();

	WorldInteraction = nullptr;
	OtherInteractor = nullptr;
}

class UActorComponent* UViewportInteractor::GetHoverComponent()
{
	return InteractorData.HoveredActorComponent.Get();
}

void UViewportInteractor::AddKeyAction( const FKey& Key, const FViewportActionKeyInput& Action )
{
	KeyToActionMap.Add( Key, Action );
}

void UViewportInteractor::RemoveKeyAction( const FKey& Key )
{
	KeyToActionMap.Remove( Key );
}

bool UViewportInteractor::HandleInputKey( const FKey Key, const EInputEvent Event )
{
	bool bHandled = false;
	FViewportActionKeyInput* Action = KeyToActionMap.Find( Key );
	if ( Action != nullptr )	// Ignore key repeats
	{
		Action->Event = Event;
		
		// Give subsystems a chance to handle actions for this interactor
		WorldInteraction->OnViewportInteractionInputAction().Broadcast( *WorldInteraction->GetViewportClient(), this, *Action, Action->bIsInputCaptured, bHandled );

		if ( !bHandled )
		{
			// Give the derived classes a chance to update according to the input
			HandleInputKey( *Action, Key, Event, bHandled );
		}

		// Start checking on default action implementation
		if ( !bHandled )
		{
			// Selection/Movement
			if ( ( Action->ActionType == ViewportWorldActionTypes::SelectAndMove ||
				   Action->ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed ) )
			{
				const bool bIsDraggingWorld = InteractorData.DraggingMode == EViewportInteractionDraggingMode::World;
				const bool bIsDraggingWorldWithTwoHands =
					OtherInteractor != nullptr &&
					( ( InteractorData.DraggingMode == EViewportInteractionDraggingMode::World && GetOtherInteractor()->GetInteractorData().DraggingMode == EViewportInteractionDraggingMode::AssistingDrag ) ||
					  ( GetOtherInteractor()->GetInteractorData().DraggingMode == EViewportInteractionDraggingMode::World && InteractorData.DraggingMode == EViewportInteractionDraggingMode::AssistingDrag ) );

				if ( Event == IE_Pressed )
				{
					// No clicking while we're dragging the world.  (No laser pointers are visible, anyway.)
					FHitResult HitResult = GetHitResultFromLaserPointer();
					if ( !bIsDraggingWorldWithTwoHands && !bHandled && HitResult.Actor.IsValid() )
					{
						if ( WorldInteraction->IsInteractableComponent( HitResult.GetComponent() ) )
						{
							InteractorData.ClickingOnComponent = HitResult.GetComponent();

							AActor* Actor = HitResult.Actor.Get();

							FVector LaserPointerStart, LaserPointerEnd;
							const bool bHaveLaserPointer = GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd );
							check( bHaveLaserPointer );

							if ( IViewportInteractableInterface* ActorInteractable = Cast<IViewportInteractableInterface>( Actor ) )
							{
								if ( Action->ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed )
								{
									bHandled = true;
									SetAllowTriggerFullPress( false );
									bool bResultedInInteractableDrag = false;
									ActorInteractable->OnPressed( this, HitResult, bResultedInInteractableDrag );

									if ( bResultedInInteractableDrag )
									{
										InteractorData.DraggingMode = EViewportInteractionDraggingMode::Interactable;
										WorldInteraction->SetDraggedInteractable( ActorInteractable );
									}
								}
							}
							else
							{
								bHandled = true;

								FViewportInteractorData* OtherInteractorData = nullptr;
								if( OtherInteractor != nullptr )
								{
									OtherInteractorData = &OtherInteractor->GetInteractorData();
								}

								// Is the other hand already dragging this stuff?
								if ( OtherInteractorData != nullptr &&
									 ( OtherInteractorData->DraggingMode == EViewportInteractionDraggingMode::ActorsWithGizmo ||
									   OtherInteractorData->DraggingMode == EViewportInteractionDraggingMode::ActorsFreely ) )
								{
									// Only if they clicked on one of the objects we're moving
									if ( WorldInteraction->IsTransformingActor( Actor ) )
									{
										// If we were dragging with a gizmo, we'll need to stop doing that and instead drag freely.
										if ( OtherInteractorData->DraggingMode == EViewportInteractionDraggingMode::ActorsWithGizmo )
										{
											WorldInteraction->StopDragging( this );

											FViewportActionKeyInput OtherInteractorAction( ViewportWorldActionTypes::SelectAndMove );
											const bool bIsPlacingActors = false;
											WorldInteraction->StartDraggingActors( OtherInteractor, OtherInteractorAction, HitResult.GetComponent(), OtherInteractorData->HoverLocation, bIsPlacingActors ); //@todo ViewportInteraction
										}

										InteractorData.DraggingMode = InteractorData.LastDraggingMode = EViewportInteractionDraggingMode::AssistingDrag;

										InteractorData.bIsFirstDragUpdate = true;
										InteractorData.bWasAssistingDrag = true;
										InteractorData.DragRayLength = ( HitResult.ImpactPoint - LaserPointerStart ).Size();
										InteractorData.LastLaserPointerStart = LaserPointerStart;
										InteractorData.LastDragToLocation = HitResult.ImpactPoint;
										InteractorData.LaserPointerImpactAtDragStart = HitResult.ImpactPoint;
										InteractorData.DragTranslationVelocity = FVector::ZeroVector;
										InteractorData.DragRayLengthVelocity = 0.0f;
										InteractorData.DraggingTransformGizmoComponent = nullptr;
										InteractorData.TransformGizmoInteractionType = ETransformGizmoInteractionType::None;
										InteractorData.bIsDrivingVelocityOfSimulatedTransformables = false;

										InteractorData.GizmoStartTransform = OtherInteractorData->GizmoStartTransform;
										InteractorData.GizmoStartLocalBounds = OtherInteractorData->GizmoStartLocalBounds;
										InteractorData.GizmoSpaceFirstDragUpdateOffsetAlongAxis = FVector::ZeroVector;	// Will be determined on first update
										InteractorData.GizmoSpaceDragDeltaFromStartOffset = FVector::ZeroVector;	// Set every frame while dragging

										WorldInteraction->SetDraggedSinceLastSelection( true );
										WorldInteraction->SetLastDragGizmoStartTransform( InteractorData.GizmoStartTransform );

										Action->bIsInputCaptured = true; 

										// Play a haptic effect when objects are picked up
										PlayHapticEffect( VI::DragHapticFeedbackStrength->GetFloat() ); //@todo ViewportInteraction
									}
									else
									{
										// @todo vreditor: We don't currently support selection/dragging separate objects with either hand
									}
								}
								else if ( OtherInteractorData != nullptr && OtherInteractorData->DraggingMode == EViewportInteractionDraggingMode::ActorsWithGizmo )
								{
									// We don't support dragging objects with the gizmo using two hands.  Just absorb it.
								}
								else if ( OtherInteractorData != nullptr && OtherInteractorData->DraggingMode == EViewportInteractionDraggingMode::ActorsAtLaserImpact )
								{
									// Doesn't work with two hands.  Just absorb it.
								}
								else
								{
									// Default to replacing our selection with whatever the user clicked on
									enum ESelectionModification
									{
										Replace,
										AddTo,
										Toggle
									} SelectionModification = ESelectionModification::Replace;

									// If the other hand is holding down the button after selecting an object, we'll allow this hand to toggle selection
									// of additional objects (multi select)
									{
										if( OtherInteractorData != nullptr && 
											OtherInteractorData->ClickingOnComponent.IsValid() &&		// Other hand is clicking on something
											Actor != WorldInteraction->GetTransformGizmoActor() &&					// Not clicking on a gizmo
											OtherInteractorData->ClickingOnComponent.Get()->GetOwner() != Actor )	// Not clicking on same actor
										{
											// OK, the other hand is holding down the "select and move" button.
											SelectionModification = ESelectionModification::Toggle;
										}
									}

									// Only start dragging if the actor was already selected (and it's a full press), or if we clicked on a gizmo.  It's too easy to 
									// accidentally move actors around the scene when you only want to select them otherwise.
									bool bShouldDragSelected =
										( ( Actor->IsSelected() && Action->ActionType == ViewportWorldActionTypes::SelectAndMove ) || Actor == WorldInteraction->GetTransformGizmoActor() ) &&
										( SelectionModification == ESelectionModification::Replace );

									bool bSelectionChanged = false;
									if ( Actor != WorldInteraction->GetTransformGizmoActor() )  // Don't change any actor selection state if the user clicked on a gizmo
									{
										const bool bWasSelected = Actor->IsSelected();

										// Clicked on a normal actor.  So update selection!
										if ( SelectionModification == ESelectionModification::Replace || SelectionModification == ESelectionModification::AddTo )
										{
											if ( !bWasSelected )
											{
												if ( SelectionModification == ESelectionModification::Replace )
												{
													GEditor->SelectNone( true, true );
												}

												GEditor->SelectActor( Actor, true, true );
												bSelectionChanged = true;
											}
										}
										else if ( SelectionModification == ESelectionModification::Toggle )
										{
											GEditor->SelectActor( Actor, !Actor->IsSelected(), true );
											bSelectionChanged = true;
										}

										// If we've selected an actor with a light press, don't prevent the user from moving the actor if they
										// press down all the way.  We only want locking when interacting with gizmos
										SetAllowTriggerLightPressLocking( false );

										// If the user did a full press to select an actor that wasn't selected, allow them to drag it right away
										const bool bOtherHandTryingToDrag =
											OtherInteractorData != nullptr &&
											OtherInteractorData->ClickingOnComponent.IsValid() &&
											OtherInteractorData->ClickingOnComponent.Get()->GetOwner()->IsSelected() &&
											OtherInteractorData->ClickingOnComponent.Get()->GetOwner() == HitResult.GetComponent()->GetOwner();	// Trying to drag same actor
										if ( ( !bWasSelected || bOtherHandTryingToDrag ) &&
											Actor->IsSelected() &&
											( Action->ActionType == ViewportWorldActionTypes::SelectAndMove || bOtherHandTryingToDrag ) )
										{
											bShouldDragSelected = true;
										}
									}

									if ( bShouldDragSelected && InteractorData.DraggingMode != EViewportInteractionDraggingMode::Interactable )
									{
										const bool bIsPlacingActors = false;
										WorldInteraction->StartDraggingActors( this, *Action, HitResult.GetComponent(), HitResult.ImpactPoint, bIsPlacingActors ); //@todo ViewportInteraction
									}
									else if ( bSelectionChanged )
									{
										// User didn't drag but did select something, so play a haptic effect.
										PlayHapticEffect( VI::SelectionHapticFeedbackStrength->GetFloat() );
									}
								}
							}
						}
					}
				}
				else if ( Event == IE_Released )
				{
					if ( InteractorData.ClickingOnComponent.IsValid() )
					{
						bHandled = true;
						InteractorData.ClickingOnComponent = nullptr;
					}

					// Don't allow the trigger to cancel our drag on release if we're dragging the world. 
					if ( InteractorData.DraggingMode != EViewportInteractionDraggingMode::Nothing &&
						!bIsDraggingWorld &&
						!bIsDraggingWorldWithTwoHands )
					{
						WorldInteraction->StopDragging( this );
						Action->bIsInputCaptured = false;
						bHandled = true;
					}
				}
			}
			// World Movement
			else if ( Action->ActionType == ViewportWorldActionTypes::WorldMovement )
			{
				if ( Event == IE_Pressed )
				{
					// Is our other hand already dragging the world around?
					if ( OtherInteractor && OtherInteractor->GetInteractorData().DraggingMode == EViewportInteractionDraggingMode::World )
					{
						InteractorData.DraggingMode = InteractorData.LastDraggingMode = EViewportInteractionDraggingMode::AssistingDrag;
						InteractorData.bWasAssistingDrag = true;
					}
					else
					{
						// Start dragging the world
						InteractorData.DraggingMode = InteractorData.LastDraggingMode = EViewportInteractionDraggingMode::World;
						InteractorData.bWasAssistingDrag = false;

						if( OtherInteractor != nullptr )
						{
							// Starting a new drag, so make sure the other hand doesn't think it's assisting us
							OtherInteractor->GetInteractorData().bWasAssistingDrag = false;

							// Stop any interia from the other hand's previous movements -- we've grabbed the world and it needs to stick!
							OtherInteractor->GetInteractorData().DragTranslationVelocity = FVector::ZeroVector;
						}
					}

					InteractorData.bIsFirstDragUpdate = true;
					InteractorData.DragRayLength = 0.0f;
					InteractorData.LastLaserPointerStart = InteractorData.Transform.GetLocation();
					InteractorData.LastDragToLocation = InteractorData.Transform.GetLocation();
					InteractorData.DragTranslationVelocity = FVector::ZeroVector;
					InteractorData.DragRayLengthVelocity = 0.0f;
					InteractorData.bIsDrivingVelocityOfSimulatedTransformables = false;

					// We won't use gizmo features for world movement
					InteractorData.DraggingTransformGizmoComponent = nullptr;
					InteractorData.TransformGizmoInteractionType = ETransformGizmoInteractionType::None;
					InteractorData.OptionalHandlePlacement.Reset();
					InteractorData.GizmoStartTransform = FTransform::Identity;
					InteractorData.GizmoStartLocalBounds = FBox( 0 );
					InteractorData.GizmoSpaceFirstDragUpdateOffsetAlongAxis = FVector::ZeroVector;
					InteractorData.GizmoSpaceDragDeltaFromStartOffset = FVector::ZeroVector;

					Action->bIsInputCaptured = true;
				}
				else if ( Event == IE_Released )
				{
					WorldInteraction->StopDragging( this );

					Action->bIsInputCaptured = false;
				}
			}
			else if ( Action->ActionType == ViewportWorldActionTypes::Delete )
			{
				if ( Event == IE_Pressed )
				{
					WorldInteraction->DeleteSelectedObjects();
				}
				bHandled = true;
			}
			else if ( Action->ActionType == ViewportWorldActionTypes::Redo )
			{
				if ( Event == IE_Pressed || Event == IE_Repeat )
				{
					WorldInteraction->Redo();
				}
				bHandled = true;
			}
			else if ( Action->ActionType == ViewportWorldActionTypes::Undo )
			{
				if ( Event == IE_Pressed || Event == IE_Repeat )
				{
					WorldInteraction->Undo();
				}
				bHandled = true;
			}
		}

		if ( !bHandled )
		{
			// If "select and move" was pressed but not handled, go ahead and deselect everything
			if ( Action->ActionType == ViewportWorldActionTypes::SelectAndMove && Event == IE_Pressed )
			{
				WorldInteraction->Deselect();
			}
		}
	}

	return bHandled;
}

bool UViewportInteractor::HandleInputAxis( const FKey Key, const float Delta, const float DeltaTime )
{
	bool bHandled = false;
	FViewportActionKeyInput* KnownAction = KeyToActionMap.Find( Key );
	if ( KnownAction != nullptr )	// Ignore key repeats
	{
		FViewportActionKeyInput Action( KnownAction->ActionType );

		// Give the derived classes a chance to update according to the input
		HandleInputAxis( Action, Key, Delta, DeltaTime, bHandled );
	}

	return bHandled;
}

FTransform UViewportInteractor::GetTransform() const
{
	return InteractorData.Transform;
}

EViewportInteractionDraggingMode UViewportInteractor::GetDraggingMode() const
{
	return InteractorData.DraggingMode;
}

EViewportInteractionDraggingMode UViewportInteractor::GetLastDraggingMode() const
{
	return InteractorData.LastDraggingMode;
}

FVector UViewportInteractor::GetDragTranslationVelocity() const
{
	return InteractorData.DragTranslationVelocity;
}

bool UViewportInteractor::GetLaserPointer( FVector& LaserPointerStart, FVector& LaserPointerEnd, const bool bEvenIfBlocked, const float LaserLengthOverride )
{
	// If we're currently grabbing the world with both hands, then the laser pointer is not available
	if ( /*bHaveMotionController &&*/ //@todo ViewportInteraction
		 OtherInteractor == nullptr ||
		 ( !( InteractorData.DraggingMode == EViewportInteractionDraggingMode::World && GetOtherInteractor()->GetInteractorData().DraggingMode == EViewportInteractionDraggingMode::AssistingDrag ) &&
		   !( InteractorData.DraggingMode == EViewportInteractionDraggingMode::AssistingDrag && GetOtherInteractor()->GetInteractorData().DraggingMode == EViewportInteractionDraggingMode::World ) ) )
	{
		// If we have UI attached to us, don't allow a laser pointer
		if ( bEvenIfBlocked || !GetIsLaserBlocked() )
		{
			FTransform HandTransform;
			FVector HandForwardVector;
			if ( GetTransformAndForwardVector( HandTransform, HandForwardVector ) )
			{
				LaserPointerStart = HandTransform.GetLocation();

				float LaserLength = LaserLengthOverride == 0.0f ? GetLaserPointerMaxLength() : LaserLengthOverride;
				LaserPointerEnd = LaserPointerStart + HandForwardVector * LaserLength;

				return true;
			}
		}
	}
	return false;
}

float UViewportInteractor::GetLaserPointerMaxLength() const
{
	return VI::LaserPointerMaxLength->GetFloat() * WorldInteraction->GetWorldScaleFactor();
}

FHitResult UViewportInteractor::GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors /*= nullptr*/, const bool bIgnoreGizmos /*= false*/, 
	TArray<UClass*>* ObjectsInFrontOfGizmo /*= nullptr */, const bool bEvenIfBlocked /*= false */, const float LaserLengthOverride /*= 0.0f */ )
{
	FHitResult BestHitResult;

	FVector LaserPointerStart, LaserPointerEnd;
	if ( GetLaserPointer( LaserPointerStart, LaserPointerEnd, bEvenIfBlocked, LaserLengthOverride ) )
	{
		// Twice twice.  Once for editor gizmos which are "on top" and always take precedence, then a second time
		// for all of the scene objects
		for ( int32 PassIndex = bIgnoreGizmos ? 1 : 0; PassIndex < 2; ++PassIndex )
		{
			const bool bOnlyEditorGizmos = ( PassIndex == 0 );

			const bool bTraceComplex = true;
			FCollisionQueryParams TraceParams( NAME_None, bTraceComplex, nullptr );

			if ( OptionalListOfIgnoredActors != nullptr )
			{
				TraceParams.AddIgnoredActors( *OptionalListOfIgnoredActors );
			}

			bool bHit = false;
			FHitResult HitResult;
			if ( bOnlyEditorGizmos )
			{
				const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam;
				const ECollisionChannel CollisionChannel = bOnlyEditorGizmos ? COLLISION_GIZMO : ECC_Visibility;

				bHit = WorldInteraction->GetViewportWorld()->LineTraceSingleByChannel( HitResult, LaserPointerStart, LaserPointerEnd, CollisionChannel, TraceParams, ResponseParam );
				if ( bHit )
				{
					BestHitResult = HitResult;
				}
			}
			else
			{
				FCollisionObjectQueryParams EverythingButGizmos( FCollisionObjectQueryParams::AllObjects );
				EverythingButGizmos.RemoveObjectTypesToQuery( COLLISION_GIZMO );
				bHit = WorldInteraction->GetViewportWorld()->LineTraceSingleByObjectType( HitResult, LaserPointerStart, LaserPointerEnd, EverythingButGizmos, TraceParams );
				
				if ( bHit )
				{
					bool bHitResultIsPriorityType = false;
					if ( !bOnlyEditorGizmos && ObjectsInFrontOfGizmo )
					{
						for ( UClass* CurrentClass : *ObjectsInFrontOfGizmo )
						{
							bool bClassHasPriority = false;
							bClassHasPriority =
								( HitResult.GetComponent() != nullptr && HitResult.GetComponent()->IsA( CurrentClass ) ) ||
								( HitResult.GetActor() != nullptr && HitResult.GetActor()->IsA( CurrentClass ) );

							if ( bClassHasPriority )
							{
								bHitResultIsPriorityType = bClassHasPriority;
								break;
							}
						}
					}

					const bool bHitResultIsGizmo = HitResult.GetActor() != nullptr && HitResult.GetActor() == WorldInteraction->GetTransformGizmoActor();
					if ( BestHitResult.GetActor() == nullptr ||
						 bHitResultIsPriorityType ||
						 bHitResultIsGizmo )
					{
						BestHitResult = HitResult;
					}
				}
			}
		}
	}

	return BestHitResult;
}

bool UViewportInteractor::GetTransformAndForwardVector( FTransform& OutHandTransform, FVector& OutForwardVector )
{
	OutHandTransform = InteractorData.Transform;
	OutForwardVector = OutHandTransform.GetRotation().Vector();

	return true;
}

FVector UViewportInteractor::GetHoverLocation() const 
{
	return InteractorData.HoverLocation;
}

bool UViewportInteractor::IsHovering() const
{
	return InteractorData.bIsHovering;
}

bool UViewportInteractor::IsHoveringOverGizmo()
{
	return InteractorData.HoveringOverTransformGizmoComponent.IsValid();
}

void UViewportInteractor::SetDraggingMode( const EViewportInteractionDraggingMode NewDraggingMode )
{
	InteractorData.DraggingMode = NewDraggingMode;
}

FViewportActionKeyInput* UViewportInteractor::GetActionWithName( const FName InActionName )
{
	FViewportActionKeyInput* ResultedAction = nullptr;
	for ( auto It = KeyToActionMap.CreateIterator(); It; ++It )
	{
		if ( It.Value().ActionType == InActionName )
		{
			ResultedAction = &(It->Value);
			break;
		}
	}
	return ResultedAction;
}

float UViewportInteractor::GetDragHapticFeedbackStrength() const
{
	return VI::DragHapticFeedbackStrength->GetFloat();
}

void UViewportInteractor::SetAllowTriggerLightPressLocking( const bool bInAllow )
{
	InteractorData.bAllowTriggerLightPressLocking = bInAllow;
}

bool UViewportInteractor::AllowTriggerLightPressLocking() const
{
	return InteractorData.bAllowTriggerLightPressLocking;
}

void UViewportInteractor::SetAllowTriggerFullPress( const bool bInAllow )
{
	InteractorData.bAllowTriggerFullPress = bInAllow;
}

bool UViewportInteractor::AllowTriggerFullPress() const
{
	return InteractorData.bAllowTriggerFullPress;
}