// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "PreviewScene.h"
#include "SSCSEditorViewport.h"
#include "SCSEditorViewportClient.h"
#include "SSCSEditor.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorUtils.h"
#include "SKismetInspector.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "SoundDefinitions.h"
#include "Editor/UnrealEd/Public/Kismet2/ComponentEditorUtils.h"
#include "ISCSEditorCustomization.h"
#include "ComponentVisualizer.h"
#include "InstancedFoliage.h"
#include "Engine/SimpleConstructionScript.h"
#include "CanvasTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/SCS_Node.h"
#include "Engine/TextureCube.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogSCSEditorViewport, Log, All);

namespace
{
	/** Automatic translation applied to the camera in the default editor viewport logic when orbit mode is enabled. */
	const float AutoViewportOrbitCameraTranslate = 256.0f;

	void DrawAngles(FCanvas* Canvas, int32 XPos, int32 YPos, EAxisList::Type ManipAxis, FWidget::EWidgetMode MoveMode, const FRotator& Rotation, const FVector& Translation)
	{
		FString OutputString(TEXT(""));
		if(MoveMode == FWidget::WM_Rotate && Rotation.IsZero() == false)
		{
			//Only one value moves at a time
			const FVector EulerAngles = Rotation.Euler();
			if(ManipAxis == EAxisList::X)
			{
				OutputString += FString::Printf(TEXT("Roll: %0.2f"), EulerAngles.X);
			}
			else if(ManipAxis == EAxisList::Y)
			{
				OutputString += FString::Printf(TEXT("Pitch: %0.2f"), EulerAngles.Y);
			}
			else if(ManipAxis == EAxisList::Z)
			{
				OutputString += FString::Printf(TEXT("Yaw: %0.2f"), EulerAngles.Z);
			}
		}
		else if(MoveMode == FWidget::WM_Translate && Translation.IsZero() == false)
		{
			//Only one value moves at a time
			if(ManipAxis == EAxisList::X)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.X);
			}
			else if(ManipAxis == EAxisList::Y)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Y);
			}
			else if(ManipAxis == EAxisList::Z)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Z);
			}
		}

		if(OutputString.Len() > 0)
		{
			FCanvasTextItem TextItem( FVector2D(XPos, YPos), FText::FromString( OutputString ), GEngine->GetSmallFont(), FLinearColor::White );
			Canvas->DrawItem( TextItem );
		} 
	}

	// Determine whether or not the given node has a parent node that is not the root node, is movable and is selected
	bool IsMovableParentNodeSelected(const FSCSEditorTreeNodePtrType& NodePtr, const TArray<FSCSEditorTreeNodePtrType>& SelectedNodes)
	{
		if(NodePtr.IsValid())
		{
			// Check for a valid parent node
			FSCSEditorTreeNodePtrType ParentNodePtr = NodePtr->GetParent();
			if(ParentNodePtr.IsValid() && !ParentNodePtr->IsRoot())
			{
				if(SelectedNodes.Contains(ParentNodePtr))
				{
					// The parent node is not the root node and is also selected; success
					return true;
				}
				else
				{
					// Recursively search for any other parent nodes farther up the tree that might be selected
					return IsMovableParentNodeSelected(ParentNodePtr, SelectedNodes);
				}
			}
		}

		return false;
	}
}

/////////////////////////////////////////////////////////////////////////
// FSCSEditorViewportClient

FSCSEditorViewportClient::FSCSEditorViewportClient(TWeakPtr<FBlueprintEditor>& InBlueprintEditorPtr, FPreviewScene& InPreviewScene)
	: FEditorViewportClient(nullptr, &InPreviewScene)
	,BlueprintEditorPtr(InBlueprintEditorPtr)
	,PreviewBlueprint(NULL)
	,PreviewActorBounds(ForceInitToZero)
	,bIsManipulating(false)
	,ScopedTransaction(NULL)
	,bIsSimulateEnabled(false)
{
	WidgetMode = FWidget::WM_Translate;
	WidgetCoordSystem = COORD_Local;
	EngineShowFlags.DisableAdvancedFeatures();

	check(Widget);
	Widget->SetSnapEnabled(true);

	// Selectively set particular show flags that we need
	EngineShowFlags.TextRender = 1;
	EngineShowFlags.SelectionOutline = GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline;

	// Set if the grid will be drawn
	DrawHelper.bDrawGrid = GEditor->GetEditorUserSettings().bSCSEditorShowGrid;

	// now add floor
	EditorFloorComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());

	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PhAT_FloorBox.PhAT_FloorBox"), NULL, LOAD_None, NULL);
	if (ensure(FloorMesh))
	{
		EditorFloorComp->SetStaticMesh(FloorMesh);
	}

	UMaterial* Material = LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMaterials/PersonaFloorMat.PersonaFloorMat"), NULL, LOAD_None, NULL);
	if (ensure(Material))
	{
		EditorFloorComp->SetMaterial(0, Material);
	}

	EditorFloorComp->SetRelativeScale3D(FVector(3.f, 3.f, 1.f));
	EditorFloorComp->SetVisibility(GEditor->GetEditorUserSettings().bSCSEditorShowFloor);
	EditorFloorComp->SetCollisionEnabled(GEditor->GetEditorUserSettings().bSCSEditorShowFloor? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	PreviewScene->AddComponent(EditorFloorComp, FTransform::Identity);

	// Turn off so that actors added to the world do not have a lifespan (so they will not auto-destroy themselves).
	PreviewScene->GetWorld()->bBegunPlay = false;
}

FSCSEditorViewportClient::~FSCSEditorViewportClient()
{
	// Ensure that an in-progress transaction is ended
	EndTransaction();

	// Clean up the preview
	DestroyPreview();
}

void FSCSEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Register the selection override delegate for the preview actor's components
	TSharedRef<SSCSEditor> SCSEditor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	AActor* PreviewActor = GetPreviewActor();
	if (PreviewActor != NULL)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		PreviewActor->GetComponents(PrimitiveComponents);

		for (int32 CompIdx = 0; CompIdx < PrimitiveComponents.Num(); ++CompIdx)
		{
			UPrimitiveComponent* PrimComponent = PrimitiveComponents[CompIdx];
			if (!PrimComponent->SelectionOverrideDelegate.IsBound())
			{
				SCSEditor->SetSelectionOverride(PrimComponent);
			}
		}
	}

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		// Ensure that the preview actor instance is up-to-date for component editing (e.g. after compiling the Blueprint, the actor may be reinstanced outside of this class)
		if(PreviewActor != BlueprintEditorPtr.Pin()->GetBlueprintObj()->SimpleConstructionScript->GetComponentEditorActorInstance())
		{
			BlueprintEditorPtr.Pin()->GetBlueprintObj()->SimpleConstructionScript->SetComponentEditorActorInstance(PreviewActor);
		}

		// Allow full tick only if preview simulation is enabled and we're not currently in an active SIE or PIE session
		if(bIsSimulateEnabled && GEditor->PlayWorld == NULL && !GEditor->bIsSimulatingInEditor)
		{
			PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_All : LEVELTICK_TimeOnly, DeltaSeconds);
		}
		else
		{
			PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_ViewportsOnly : LEVELTICK_TimeOnly, DeltaSeconds);
		}
	}
}

FSceneView* FSCSEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);

	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;

	return SceneView;
}

void FSCSEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	bool bHitTesting = PDI->IsHitTesting();
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		if(GUnrealEd != NULL)
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			for (int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); ++SelectionIndex)
			{
				FSCSEditorTreeNodePtrType SelectedNode = SelectedNodes[SelectionIndex];

				UActorComponent* Comp = SelectedNode->FindComponentInstanceInActor(PreviewActor, true);
				if(Comp != NULL && Comp->IsRegistered())
				{
					// Try and find a visualizer
					TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Comp->GetClass());
					if (Visualizer.IsValid())
					{
						Visualizer->DrawVisualization(Comp, View, PDI);
					}
				}
			}
		}
	}
}

void FSCSEditorViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		if (GUnrealEd != NULL)
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			for (int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); ++SelectionIndex)
			{
				FSCSEditorTreeNodePtrType SelectedNode = SelectedNodes[SelectionIndex];

				UActorComponent* Comp = Cast<USceneComponent>(SelectedNode->FindComponentInstanceInActor(PreviewActor, true));
				if (Comp != NULL && Comp->IsRegistered())
				{
					// Try and find a visualizer
					TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Comp->GetClass());
					if (Visualizer.IsValid())
					{
						Visualizer->DrawVisualizationHUD(Comp, &InViewport, &View, &Canvas);
					}
				}
			}
		}

		TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

		const int32 HalfX = 0.5f * Viewport->GetSizeXY().X;
		const int32 HalfY = 0.5f * Viewport->GetSizeXY().Y;

		auto SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(bIsManipulating && SelectedNodes.Num() > 0)
		{
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes[0]->FindComponentInstanceInActor(PreviewActor, true));
			if(SceneComp)
			{
				const FVector WidgetLocation = GetWidgetLocation();
				const FPlane Proj = View.Project(WidgetLocation);
				if(Proj.W > 0.0f)
				{
					const int32 XPos = HalfX + (HalfX * Proj.X);
					const int32 YPos = HalfY + (HalfY * (Proj.Y * -1));
					DrawAngles(&Canvas, XPos, YPos, GetCurrentWidgetAxis(), GetWidgetMode(), GetWidgetCoordSystem().Rotator(), WidgetLocation);
				}
			}
		}
	}
}

bool FSCSEditorViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	bool bHandled = false;

	if( !bHandled )
	{
		bHandled = FEditorViewportClient::InputKey(Viewport, ControllerId, Key, Event, AmountDepressed, bGamepad);
	}

	return bHandled;
}

void FSCSEditorViewportClient::ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	if(HitProxy)
	{
		if(HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			HInstancedStaticMeshInstance* InstancedStaticMeshInstanceProxy = ( ( HInstancedStaticMeshInstance* )HitProxy );

			TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(InstancedStaticMeshInstanceProxy->Component);
			if(Customization.IsValid() && Customization->HandleViewportClick(AsShared(), View, HitProxy, Key, Event, HitX, HitY))
			{
				Invalidate();
			}
		}
		else if(HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorProxy = (HActor*)HitProxy;
			AActor* PreviewActor = GetPreviewActor();
			if(ActorProxy && ActorProxy->Actor && ActorProxy->Actor == PreviewActor && ActorProxy->PrimComponent != NULL)
			{
				TArray<USceneComponent*> SceneComponents;
				ActorProxy->Actor->GetComponents(SceneComponents);
	
				for(auto CompIt = SceneComponents.CreateConstIterator(); CompIt; ++CompIt)
				{
					USceneComponent* CompInstance = *CompIt;
					TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(CompInstance);
					if (Customization.IsValid() && Customization->HandleViewportClick(AsShared(), View, HitProxy, Key, Event, HitX, HitY))
					{
						break;
					}

					if (CompInstance == ActorProxy->PrimComponent)
					{
						const bool bIsCtrlKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
						if (BlueprintEditorPtr.IsValid())
						{
							// Note: This will find and select any node associated with the component instance that's attached to the proxy (including visualizers)
							BlueprintEditorPtr.Pin()->FindAndSelectSCSEditorTreeNode(CompInstance, bIsCtrlKeyDown);
						}
						break;
					}
				}
			}

			Invalidate();
		}

		// Pass to component vis manager
		//GUnrealEd->ComponentVisManager.HandleProxyForComponentVis(HitProxy);
	}
}

bool FSCSEditorViewportClient::InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale )
{
	bool bHandled = false;
	if(bIsManipulating && CurrentAxis != EAxisList::None)
	{
		bHandled = true;
		AActor* PreviewActor = GetPreviewActor();
		if(PreviewActor)
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			if(SelectedNodes.Num() > 0)
			{
				FVector ModifiedScale = Scale;
				if( GEditor->UsePercentageBasedScaling() )
				{
					ModifiedScale = Scale * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
				}

				TSet<USceneComponent*> UpdatedComponents;

				for(auto It(SelectedNodes.CreateIterator());It;++It)
				{
					FSCSEditorTreeNodePtrType SelectedNodePtr = *It;
					// Don't allow editing of a root node, inherited SCS node or child node that also has a movable (non-root) parent node selected
					const bool bCanEdit =  !SelectedNodePtr->IsRoot() && !SelectedNodePtr->IsInherited()
						&& !IsMovableParentNodeSelected(SelectedNodePtr, SelectedNodes);

					if(bCanEdit)
					{
						USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodePtr->FindComponentInstanceInActor(PreviewActor, true));
						USceneComponent* SelectedTemplate = Cast<USceneComponent>(SelectedNodePtr->GetComponentTemplate());
						if(SceneComp != NULL && SelectedTemplate != NULL)
						{
							// Cache the current default values for propagation
							FVector OldRelativeLocation = SelectedTemplate->RelativeLocation;
							FRotator OldRelativeRotation = SelectedTemplate->RelativeRotation;
							FVector OldRelativeScale3D = SelectedTemplate->RelativeScale3D;

							USceneComponent* ParentSceneComp = SceneComp->GetAttachParent();
							if( ParentSceneComp )
							{
								const FTransform ParentToWorldSpace = ParentSceneComp->GetSocketTransform(SceneComp->AttachSocketName);

								if(!SceneComp->bAbsoluteLocation)
								{
									Drag = ParentToWorldSpace.Inverse().TransformVector(Drag);
								}
								
								if(!SceneComp->bAbsoluteRotation)
								{
									Rot = (ParentToWorldSpace.Inverse().GetRotation() * Rot.Quaternion() * ParentToWorldSpace.GetRotation()).Rotator();
								}
							}

							FComponentEditorUtils::FTransformData OldDefaultTransform(*SelectedTemplate);

							TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(SceneComp);
							if(Customization.IsValid() && Customization->HandleViewportDrag(SceneComp, SelectedTemplate, Drag, Rot, ModifiedScale, GetWidgetLocation()))
							{
								UpdatedComponents.Add(SceneComp);
								UpdatedComponents.Add(SelectedTemplate);
							}
							else
							{
								// Apply delta to the preview actor's scene component
								GEditor->ApplyDeltaToComponent(
										SceneComp,
										true,
										&Drag,
										&Rot,
										&ModifiedScale,
										SceneComp->RelativeLocation );
								UpdatedComponents.Add(SceneComp);

							// Apply delta to the template component object
								GEditor->ApplyDeltaToComponent(
										SelectedTemplate,
										true,
										&Drag,
										&Rot,
										&ModifiedScale,
										SelectedTemplate->RelativeLocation );
								UpdatedComponents.Add(SelectedTemplate);
							}

							FComponentEditorUtils::FTransformData NewDefaultTransform(*SelectedTemplate);

							if (SelectedNodePtr->IsNative())
							{
								if (ensure(SelectedTemplate->HasAnyFlags(RF_DefaultSubObject)))
								{
									FComponentEditorUtils::PropagateTransformPropertyChange(SelectedTemplate, OldDefaultTransform, NewDefaultTransform, UpdatedComponents);
								}
							}

							if(PreviewBlueprint != NULL)
							{
								// Like PostEditMove(), but we only need to re-run construction scripts
								if(PreviewBlueprint && PreviewBlueprint->bRunConstructionScriptOnDrag)
								{
									PreviewActor->RerunConstructionScripts();
								}

								SceneComp->PostEditComponentMove(true); // @TODO HACK passing 'finished' every frame...

								// If a constraint, copy back updated constraint frames to template
								UPhysicsConstraintComponent* ConstraintComp = Cast<UPhysicsConstraintComponent>(SceneComp);
								UPhysicsConstraintComponent* TemplateComp = Cast<UPhysicsConstraintComponent>(SelectedTemplate);
								if(ConstraintComp && TemplateComp)
								{
									TemplateComp->ConstraintInstance.CopyConstraintGeometryFrom(&ConstraintComp->ConstraintInstance);
								}

								// Get the Blueprint class default object
								AActor* CDO = NULL;
								if(PreviewBlueprint->GeneratedClass)
								{
									CDO = Cast<AActor>(PreviewBlueprint->GeneratedClass->ClassDefaultObject);
								}
								else if(PreviewBlueprint->ParentClass)
								{
									CDO = Cast<AActor>(PreviewBlueprint->ParentClass->ClassDefaultObject);
								}

								if(CDO != NULL)
								{
									// Iterate over all the active archetype instances and propagate the change(s) to the matching component instance
									TArray<UObject*> ArchetypeInstances;
									CDO->GetArchetypeInstances(ArchetypeInstances);
									for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
									{
										AActor* ArchetypeInstance = Cast<AActor>(ArchetypeInstances[InstanceIndex]);
										if(ArchetypeInstance != NULL)
										{
											const bool bIsProcessingPreviewActor = (ArchetypeInstance == PreviewActor);
											SceneComp = Cast<USceneComponent>(SelectedNodePtr->FindComponentInstanceInActor(ArchetypeInstance, bIsProcessingPreviewActor));
											if(!bIsProcessingPreviewActor && SceneComp != nullptr && !UpdatedComponents.Contains(SceneComp))
											{
												FComponentEditorUtils::PropagateTransformPropertyChange(SceneComp, SceneComp->RelativeLocation, OldRelativeLocation, SelectedTemplate->RelativeLocation, UpdatedComponents);
												FComponentEditorUtils::PropagateTransformPropertyChange(SceneComp, SceneComp->RelativeRotation, OldRelativeRotation, SelectedTemplate->RelativeRotation, UpdatedComponents);
												FComponentEditorUtils::PropagateTransformPropertyChange(SceneComp, SceneComp->RelativeScale3D,  OldRelativeScale3D,  SelectedTemplate->RelativeScale3D,  UpdatedComponents);
											}
										}
									}
								}
							}
						}
					}
				}
				GUnrealEd->RedrawLevelEditingViewports();
			}
		}

		Invalidate();
	}

	return bHandled;
}

void FSCSEditorViewportClient::TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	if( !bIsManipulating && bIsDraggingWidget )
	{
		// Suspend component modification during each delta step to avoid recording unnecessary overhead into the transaction buffer
		GEditor->DisableDeltaModification(true);

		// Begin transaction
		BeginTransaction( NSLOCTEXT("UnrealEd", "ModifyComponents", "Modify Component(s)") );
		bIsManipulating = true;
	}
}

void FSCSEditorViewportClient::TrackingStopped() 
{
	if( bIsManipulating )
	{
		// Re-run construction scripts if we haven't done so yet (so that the components in the preview actor can update their transforms)
		AActor* PreviewActor = GetPreviewActor();
		if(PreviewActor != NULL && PreviewBlueprint != NULL && !PreviewBlueprint->bRunConstructionScriptOnDrag)
		{
			PreviewActor->RerunConstructionScripts();
		}

		// End transaction
		bIsManipulating = false;
		EndTransaction();

		// Restore component delta modification
		GEditor->DisableDeltaModification(false);
	}
}

FWidget::EWidgetMode FSCSEditorViewportClient::GetWidgetMode() const
{
	// Default to not drawing the widget
	FWidget::EWidgetMode ReturnWidgetMode = FWidget::WM_None;

	AActor* PreviewActor = GetPreviewActor();
	if(!bIsSimulateEnabled && PreviewActor)
	{
		const TSharedPtr<FBlueprintEditor> BluePrintEditor = BlueprintEditorPtr.Pin();
		if ( BluePrintEditor.IsValid() )
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			const TArray<FSCSEditorTreeNodePtrType>& RootNodes = BluePrintEditor->GetSCSEditor()->RootNodes;

			// if the selected nodes array is empty, or only contains entries from the
			// root nodes array, or isn't visible in the preview actor, then don't display a transform widget
			for ( int32 CurrentNodeIndex=0; CurrentNodeIndex < SelectedNodes.Num(); CurrentNodeIndex++ )
			{
				FSCSEditorTreeNodePtrType CurrentNodePtr = SelectedNodes[CurrentNodeIndex];
				if ( CurrentNodePtr.IsValid() && !RootNodes.Contains( CurrentNodePtr ) && !CurrentNodePtr->IsRoot() && CurrentNodePtr->CanEditDefaults() && CurrentNodePtr->FindComponentInstanceInActor(PreviewActor, true))
				{
					// a non-NULL, non-root item is selected, draw the widget
					ReturnWidgetMode = WidgetMode;
					break;
				}
			}
		}
	}

	return ReturnWidgetMode;
}


void FSCSEditorViewportClient::SetWidgetMode( FWidget::EWidgetMode NewMode )
{
	WidgetMode = NewMode;
}

void FSCSEditorViewportClient::SetWidgetCoordSystemSpace( ECoordSystem NewCoordSystem ) 
{
	WidgetCoordSystem = NewCoordSystem;
}

FVector FSCSEditorViewportClient::GetWidgetLocation() const
{
	FVector Location = FVector::ZeroVector;

	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(SelectedNodes.Num() > 0)
		{
			// Use the last selected item for the widget location
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes.Last().Get()->FindComponentInstanceInActor(PreviewActor, true));
			if( SceneComp )
			{
				TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(SceneComp);
				FVector CustomLocation;
				if(Customization.IsValid() && Customization->HandleGetWidgetLocation(SceneComp, CustomLocation))
				{
					Location = CustomLocation;
				}
				else
				{
					Location = SceneComp->GetComponentLocation();
				}
			}
		}
	}

	return Location;
}

FMatrix FSCSEditorViewportClient::GetWidgetCoordSystem() const
{
	FMatrix Matrix = FMatrix::Identity;
	if( GetWidgetCoordSystemSpace() == COORD_Local )
	{
		AActor* PreviewActor = GetPreviewActor();
		auto BlueprintEditor = BlueprintEditorPtr.Pin();
		if (PreviewActor && BlueprintEditor.IsValid())
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditor->GetSelectedSCSEditorTreeNodes();
			if(SelectedNodes.Num() > 0)
			{
				const auto SelectedNode = SelectedNodes.Last();
				USceneComponent* SceneComp = SelectedNode.IsValid() ? Cast<USceneComponent>(SelectedNode->FindComponentInstanceInActor(PreviewActor, true)) : NULL;
				if( SceneComp )
				{
					TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditor->CustomizeSCSEditor(SceneComp);
					FMatrix CustomTransform;
					if(Customization.IsValid() && Customization->HandleGetWidgetTransform(SceneComp, CustomTransform))
					{
						Matrix = CustomTransform;
					}					
					else
					{
						Matrix = FRotationMatrix( SceneComp->GetComponentRotation() );
					}
				}
			}
		}
	}

	if(!Matrix.Equals(FMatrix::Identity))
	{
		Matrix.RemoveScaling();
	}

	return Matrix;
}

void FSCSEditorViewportClient::InvalidatePreview(bool bResetCamera)
{
	// Ensure that the editor is valid before continuing
	if(!BlueprintEditorPtr.IsValid())
	{
		return;
	}

	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	// Create or update the Blueprint actor instance in the preview scene
	UpdatePreviewActorForBlueprint(Blueprint, !IsPreviewSceneValid());

	if( bResetCamera )
	{
		ResetCamera();
	}
}

void FSCSEditorViewportClient::ResetCamera()
{
	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();

	// For now, loosely base default camera positioning on thumbnail preview settings
	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(Blueprint->ThumbnailInfo);
	if(ThumbnailInfo)
	{
		if(PreviewActorBounds.SphereRadius + ThumbnailInfo->OrbitZoom < 0)
		{
			ThumbnailInfo->OrbitZoom = -PreviewActorBounds.SphereRadius;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	ToggleOrbitCamera(true);
	{
		float TargetDistance = PreviewActorBounds.SphereRadius;
		if(TargetDistance <= 0.0f)
		{
			TargetDistance = AutoViewportOrbitCameraTranslate;
		}

		FRotator ThumbnailAngle(ThumbnailInfo->OrbitPitch, ThumbnailInfo->OrbitYaw, 0.0f);

		SetViewLocationForOrbiting(PreviewActorBounds.Origin);
		SetViewLocation( GetViewLocation() + FVector(0.0f, TargetDistance * 1.5f + ThumbnailInfo->OrbitZoom - AutoViewportOrbitCameraTranslate, 0.0f) );
		SetViewRotation( ThumbnailAngle );
	
	}

	Invalidate();
}

void FSCSEditorViewportClient::ToggleRealtimePreview()
{
	SetRealtime(!IsRealtime());

	Invalidate();
}

bool FSCSEditorViewportClient::IsPreviewSceneValid() const
{
	return GetPreviewActor() != NULL;
}

void FSCSEditorViewportClient::FocusViewportToSelection()
{
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(SelectedNodes.Num() > 0)
		{
			// Use the last selected item for the widget location
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes.Last()->FindComponentInstanceInActor(PreviewActor, true));
			if( SceneComp )
			{
				FocusViewportOnBox( SceneComp->Bounds.GetBox() );
			}
		}
		else
		{
			FocusViewportOnBox( PreviewActor->GetComponentsBoundingBox( true ) );
		}
	}
}

bool FSCSEditorViewportClient::GetIsSimulateEnabled() 
{ 
	return bIsSimulateEnabled;
}

void FSCSEditorViewportClient::ToggleIsSimulateEnabled() 
{
	// Must destroy existing actors before we toggle the world state
	DestroyPreview();

	bIsSimulateEnabled = !bIsSimulateEnabled;
	PreviewScene->GetWorld()->bBegunPlay = bIsSimulateEnabled;
	PreviewScene->GetWorld()->bShouldSimulatePhysics = bIsSimulateEnabled;

	AActor* PreviewActor = GetPreviewActor();
	TSharedRef<SWidget> SCSEditor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	TSharedRef<SWidget> Inspector = BlueprintEditorPtr.Pin()->GetInspector();

	// When simulate is enabled, we don't want to allow the user to modify the components
	UpdatePreviewActorForBlueprint(PreviewBlueprint, true);
	SCSEditor->SetEnabled(!bIsSimulateEnabled);
	Inspector->SetEnabled(!bIsSimulateEnabled);

	if(!IsRealtime())
	{
		ToggleRealtimePreview();
	}
}

bool FSCSEditorViewportClient::GetShowFloor() 
{
	return GEditor->GetEditorUserSettings().bSCSEditorShowFloor;
}

void FSCSEditorViewportClient::ToggleShowFloor() 
{
	bool bShowFloor = GEditor->AccessEditorUserSettings().bSCSEditorShowFloor;
	bShowFloor = !bShowFloor;
	
	EditorFloorComp->SetVisibility(bShowFloor);
	EditorFloorComp->SetCollisionEnabled(bShowFloor? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	GetWorld()->SendAllEndOfFrameUpdates();

	GEditor->AccessEditorUserSettings().bSCSEditorShowFloor = bShowFloor;

	Invalidate();
}

bool FSCSEditorViewportClient::GetShowGrid() 
{
	return GEditor->GetEditorUserSettings().bSCSEditorShowGrid;
}

void FSCSEditorViewportClient::ToggleShowGrid() 
{
	bool bShowGrid = GEditor->AccessEditorUserSettings().bSCSEditorShowGrid;
	bShowGrid = !bShowGrid;

	DrawHelper.bDrawGrid = bShowGrid;

	GEditor->AccessEditorUserSettings().bSCSEditorShowGrid = bShowGrid;

	Invalidate();
}

void FSCSEditorViewportClient::BeginTransaction(const FText& Description)
{
	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::BeginTransaction() pre: %s %08x"), SessionName, *((uint32*)&ScopedTransaction));

	if(!ScopedTransaction)
	{
		ScopedTransaction = new FScopedTransaction(Description);

		if(PreviewBlueprint != NULL)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(PreviewBlueprint);
		}

		TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(SelectedNodes.Num() > 0)
		{
			for(auto SelectedSCSNodeIter(SelectedNodes.CreateIterator()); SelectedSCSNodeIter; ++SelectedSCSNodeIter)
			{
				FSCSEditorTreeNodePtrType Node = *SelectedSCSNodeIter;
				if(Node.IsValid())
				{
					if(USCS_Node* SCS_Node = Node->GetSCSNode())
					{
						SCS_Node->Modify();
					}

					UActorComponent* ComponentTemplate = Node->GetComponentTemplate();
					if(ComponentTemplate != NULL)
					{
						ComponentTemplate->SetFlags(RF_Transactional);
						ComponentTemplate->Modify();
					}
				}
			}
		}
	}

	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::BeginTransaction() post: %s %08x"), SessionName, *((uint32*)&ScopedTransaction));
}

void FSCSEditorViewportClient::EndTransaction()
{
	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::EndTransaction(): %08x"), *((uint32*)&ScopedTransaction));

	if(ScopedTransaction)
	{
		delete ScopedTransaction;
		ScopedTransaction = NULL;
	}
}

AActor* FSCSEditorViewportClient::GetPreviewActor() const
{
	// Note: The weak ptr can become stale if the actor is reinstanced due to a Blueprint change, etc. In that case we look to see if we can find the new instance in the preview world and then update the weak ptr.
	if(PreviewActorPtr.IsStale(true) && PreviewBlueprint)
	{
		UWorld* PreviewWorld = PreviewScene->GetWorld();
		for(TActorIterator<AActor> It(PreviewWorld); It; ++It)
		{
			AActor* Actor = *It;
			if(!Actor->IsPendingKillPending()
				&& Actor->GetClass()->ClassGeneratedBy == PreviewBlueprint)
			{
				PreviewActorPtr = Actor;
				break;
			}
		}
	}

	return PreviewActorPtr.Get();
}

void FSCSEditorViewportClient::UpdatePreviewActorForBlueprint(UBlueprint* InBlueprint, bool bInForceFullUpdate/* = false*/)
{
	AActor* PreviewActor = GetPreviewActor();

	// Signal that we're going to be constructing editor components
	if(InBlueprint != NULL && InBlueprint->SimpleConstructionScript != NULL)
	{
		InBlueprint->SimpleConstructionScript->BeginEditorComponentConstruction();
	}

	// If the Blueprint is changing
	if(InBlueprint != PreviewBlueprint || bInForceFullUpdate)
	{
		// Destroy the previous actor instance
		DestroyPreview();

		// Save the Blueprint we're creating a preview for
		PreviewBlueprint = InBlueprint;

		// Spawn a new preview actor based on the Blueprint's generated class if it's Actor-based
		if(PreviewBlueprint && PreviewBlueprint->GeneratedClass && PreviewBlueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			FVector SpawnLocation = FVector::ZeroVector;
			FRotator SpawnRotation = FRotator::ZeroRotator;

			// Spawn an Actor based on the Blueprint's generated class
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.bNoCollisionFail = true;
			SpawnInfo.bNoFail = true;
			SpawnInfo.ObjectFlags = RF_Transient;

			// Temporarily remove the deprecated flag so we can respawn the Blueprint in the viewport
			bool bIsClassDeprecated = PreviewBlueprint->GeneratedClass->HasAnyClassFlags(CLASS_Deprecated);
			PreviewBlueprint->GeneratedClass->ClassFlags &= ~CLASS_Deprecated;

			PreviewActorPtr = PreviewActor = PreviewScene->GetWorld()->SpawnActor( PreviewBlueprint->GeneratedClass, &SpawnLocation, &SpawnRotation, SpawnInfo );

			// Reassign the deprecated flag if it was previously assigned
			if(bIsClassDeprecated)
			{
				PreviewBlueprint->GeneratedClass->ClassFlags |= CLASS_Deprecated;
			}

			check(PreviewActor);

			// Ensure that the actor is visible
			if(PreviewActor->bHidden)
			{
				PreviewActor->bHidden = false;
				PreviewActor->MarkComponentsRenderStateDirty();
				GetWorld()->SendAllEndOfFrameUpdates();
			}

			// Prevent any audio from playing as a result of spawning
			if(GEngine->AudioDevice)
			{
				GEngine->AudioDevice->Flush(GetWorld());
			}

			// Set the reference to the preview actor for component editing purposes
			if(PreviewBlueprint->SimpleConstructionScript != NULL)
			{
				PreviewBlueprint->SimpleConstructionScript->SetComponentEditorActorInstance(PreviewActor);
			}

			// Run the construction scripts again, otherwise the actor will appear as though it's had a script pass first, rather than the default properties as shown in the details panel
			PreviewActor->RerunConstructionScripts();
		}
	}
	else if(PreviewActor)
	{
		PreviewActor->RerunConstructionScripts();
	}

	// Signal that we're done constructing editor components
	if(InBlueprint != NULL && InBlueprint->SimpleConstructionScript != NULL)
	{
		InBlueprint->SimpleConstructionScript->EndEditorComponentConstruction();
	}

	Invalidate();
	RefreshPreviewBounds();
}

void FSCSEditorViewportClient::RefreshPreviewBounds()
{
	AActor* PreviewActor = GetPreviewActor();

	if(PreviewActor)
	{
		// Compute actor bounds as the sum of its visible parts
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		PreviewActor->GetComponents(PrimitiveComponents);

		PreviewActorBounds = FBoxSphereBounds(ForceInitToZero);
		for(auto CompIt = PrimitiveComponents.CreateConstIterator(); CompIt; ++CompIt)
		{
			// Aggregate primitive components that either have collision enabled or are otherwise visible components in-game
			UPrimitiveComponent* PrimComp = *CompIt;
			if(PrimComp->IsRegistered() && (!PrimComp->bHiddenInGame || PrimComp->IsCollisionEnabled()) && PrimComp->Bounds.SphereRadius < HALF_WORLD_MAX)
			{
				PreviewActorBounds = PreviewActorBounds + PrimComp->Bounds;
			}
		}
	}
}

void FSCSEditorViewportClient::DestroyPreview()
{
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor != NULL)
	{
		check(PreviewScene);
		check(PreviewScene->GetWorld());
		PreviewScene->GetWorld()->EditorDestroyActor(PreviewActor, false);
	}

	if(PreviewBlueprint != NULL)
	{
		if(PreviewBlueprint->SimpleConstructionScript != NULL
			&& PreviewActor == PreviewBlueprint->SimpleConstructionScript->GetComponentEditorActorInstance())
		{
			// Ensure that all editable component references are cleared
			PreviewBlueprint->SimpleConstructionScript->ClearEditorComponentReferences();

			// Clear the reference to the preview actor instance
			PreviewBlueprint->SimpleConstructionScript->SetComponentEditorActorInstance(NULL);
		}

		PreviewBlueprint = NULL;
	}
}
