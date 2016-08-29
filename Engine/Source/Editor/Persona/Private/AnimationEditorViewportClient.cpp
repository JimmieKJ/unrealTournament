// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "SAnimationEditorViewport.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "SAnimViewportToolBar.h"
#include "AnimViewportShowCommands.h"
#include "AnimGraphDefinitions.h"
#include "AnimPreviewInstance.h"
#include "AnimationEditorViewportClient.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Runtime/Engine/Classes/Components/ReflectionCaptureComponent.h"
#include "Runtime/Engine/Classes/Components/SphereReflectionCaptureComponent.h"
#include "UnrealWidget.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "CanvasTypes.h"
#include "Engine/TextureCube.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/StaticMesh.h"
#include "SAnimationEditorViewport.h"

#include "AssetViewerSettings.h"

namespace {
	// Value from UE3
	static const float AnimationEditorViewport_RotateSpeed = 0.02f;
	// Value from UE3
	static const float AnimationEditorViewport_TranslateSpeed = 0.25f;
	// follow camera feature
	static const float FollowCamera_InterpSpeed = 4.f;
	static const float FollowCamera_InterpSpeed_Z = 1.f;

	// @todo double define - fix it
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;
}

/**
 * A hit proxy class for sockets in the Persona viewport.
 */
struct HPersonaSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FSelectedSocketInfo SocketInfo;

	explicit HPersonaSocketProxy( FSelectedSocketInfo InSocketInfo )
		:	SocketInfo( InSocketInfo )
	{}
};
IMPLEMENT_HIT_PROXY( HPersonaSocketProxy, HHitProxy );

/**
 * A hit proxy class for sockets in the Persona viewport.
 */
struct HPersonaBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName BoneName;

	explicit HPersonaBoneProxy(const FName& InBoneName)
		:	BoneName( InBoneName )
	{}
};
IMPLEMENT_HIT_PROXY( HPersonaBoneProxy, HHitProxy );

#define LOCTEXT_NAMESPACE "FAnimationViewportClient"

/////////////////////////////////////////////////////////////////////////
// FAnimationViewportClient

FAnimationViewportClient::FAnimationViewportClient(FAdvancedPreviewScene& InPreviewScene, TWeakPtr<FPersona> InPersonaPtr, const TSharedRef<SAnimationEditorViewport>& InAnimationEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InAnimationEditorViewport))
	, PersonaPtr( InPersonaPtr )
	, bManipulating(false)
	, bInTransaction(false)
	, GravityScaleSliderValue(0.25f)
	, PrevWindStrength(0.0f)
	, SelectedWindActor(NULL)
	, bFocusOnDraw(false)
	, BodyTraceDistance(100000.0f)
	, bShouldUpdateDefaultValues(false)
{
	// load config
	ConfigOption = UPersonaOptions::StaticClass()->GetDefaultObject<UPersonaOptions>();
	check (ConfigOption);

	// DrawHelper set up
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
	DrawHelper.AxesLineThickness = ConfigOption->bHighlightOrigin ? 1.0f : 0.0f;
	DrawHelper.bDrawGrid = ConfigOption->bShowGrid;

	LocalAxesMode = static_cast<ELocalAxesMode::Type>(ConfigOption->DefaultLocalAxesSelection);
	BoneDrawMode = static_cast<EBoneDrawMode::Type>(ConfigOption->DefaultBoneDrawSelection);

	WidgetMode = FWidget::WM_Rotate;

	SetSelectedBackgroundColor(ConfigOption->ViewportBackgroundColor, false);

	EngineShowFlags.Game = 0;
	EngineShowFlags.ScreenSpaceReflections = 1;
	EngineShowFlags.AmbientOcclusion = 1;
	EngineShowFlags.SetSnap(0);

	SetRealtime(true);
	if(GEditor->PlayWorld)
	{
		SetRealtime(false,true); // We are PIE, don't start in realtime mode
	}
	
	ViewFOV = FMath::Clamp<float>(ConfigOption->ViewFOV, FOVMin, FOVMax);

	EngineShowFlags.SetSeparateTranslucency(true);
	EngineShowFlags.SetCompositeEditorPrimitives(true);

	// set camera mode
	bCameraFollow = false;

	bDrawUVs = false;
	UVChannelToDraw = 0;

	bAutoAlignFloor = ConfigOption->bAutoAlignFloorToMesh;

	// Set audio mute option
	UWorld* World = PreviewScene->GetWorld();
	if(World)
	{
		World->bAllowAudioPlayback = !ConfigOption->bMuteAudio;
	}

	// Grab a wind actor if it exists (could have been placed in the scene before a mode transition)
	TActorIterator<AWindDirectionalSource> WindIter(World);
	if(WindIter)
	{
		AWindDirectionalSource* WindActor = *WindIter;
		WindSourceActor = WindActor;

		PrevWindLocation = WindActor->GetActorLocation();
		PrevWindRotation = WindActor->GetActorRotation();
		PrevWindStrength = WindActor->GetComponent()->Strength;
	}
	else
	{
		//wind actor's initial position, rotation and strength
		PrevWindLocation = FVector(100, 100, 100);
		PrevWindRotation = FRotator(0, 0, 0); // roll, yaw, pitch
		PrevWindStrength = 0.2f;
	}

	// Store direct pointer to advanced preview scene
	AdvancedPreviewScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	// Register delegate to update the show flags when the post processing is turned on or off
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &FAnimationViewportClient::OnAssetViewerSettingsChanged);
	// Set correct flags according to current profile settings
	SetAdvancedShowFlagsForScene();
}

FAnimationViewportClient::~FAnimationViewportClient()
{
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().RemoveAll(this);
}

FLinearColor FAnimationViewportClient::GetBackgroundColor() const
{
	return SelectedHSVColor.HSVToLinearRGB();
}

void FAnimationViewportClient::SetSelectedBackgroundColor(const FLinearColor& RGBColor, bool bSave/*=true*/)
{
	SelectedHSVColor = RGBColor.LinearRGBToHSV(); 

	// save to config
	if (bSave)
	{
		ConfigOption->SetViewportBackgroundColor(RGBColor);
	}

	// only consider V from HSV
	// just inversing works against for mid range brightness
	// V being from 0-0.3 (almost white), 0.31-1 (almost black)
	if (SelectedHSVColor.B < 0.3f)
	{
		DrawHelper.GridColorAxis = FColor(230,230,230);
		DrawHelper.GridColorMajor = FColor(180,180,180);
		DrawHelper.GridColorMinor = FColor(128,128,128);
	}
	else
	{
		DrawHelper.GridColorAxis = FColor(20,20,20);
		DrawHelper.GridColorMajor = FColor(60,60,60);
		DrawHelper.GridColorMinor = FColor(128,128,128);
	}

	Invalidate();
}

void FAnimationViewportClient::SetBackgroundColor( FLinearColor InColor )
{
	SetSelectedBackgroundColor(InColor);
}

float FAnimationViewportClient::GetBrightnessValue() const
{
	return SelectedHSVColor.B;
}

void FAnimationViewportClient::SetBrightnessValue( float Value )
{
	SelectedHSVColor.B = Value;
	SetSelectedBackgroundColor(SelectedHSVColor.HSVToLinearRGB());
}

void FAnimationViewportClient::OnToggleShowGrid()
{
	FEditorViewportClient::SetShowGrid();

	ConfigOption->SetShowGrid(DrawHelper.bDrawGrid);
}

bool FAnimationViewportClient::IsShowingGrid() const
{
	return FEditorViewportClient::IsSetShowGridChecked();
}

void FAnimationViewportClient::OnToggleAutoAlignFloor()
{
	bAutoAlignFloor = !bAutoAlignFloor;
	UpdateCameraSetup();

	ConfigOption->SetAutoAlignFloorToMesh(bAutoAlignFloor);
}

bool FAnimationViewportClient::IsAutoAlignFloor() const
{
	return bAutoAlignFloor;
}

void FAnimationViewportClient::OnToggleMuteAudio()
{
	UWorld* World = PreviewScene->GetWorld();

	if(World)
	{
		bool bNewAllowAudioPlayback = !World->AllowAudioPlayback();
		World->bAllowAudioPlayback = bNewAllowAudioPlayback;

		ConfigOption->SetMuteAudio(!bNewAllowAudioPlayback);
	}
}

bool FAnimationViewportClient::IsAudioMuted() const
{
	UWorld* World = PreviewScene->GetWorld();

	return (World) ? !World->AllowAudioPlayback() : false;
}

void FAnimationViewportClient::SetCameraFollow()
{
	bCameraFollow = !bCameraFollow;
	
	if( bCameraFollow )
	{

		EnableCameraLock(false);

		if (PreviewSkelMeshComp.IsValid())
		{
			FBoxSphereBounds Bound = PreviewSkelMeshComp.Get()->CalcBounds(FTransform::Identity);
			SetViewLocationForOrbiting(Bound.Origin);
		}
	}
	else
	{
		FocusViewportOnPreviewMesh();
		Invalidate();
	}
}

bool FAnimationViewportClient::IsSetCameraFollowChecked() const
{
	return bCameraFollow;
}

void FAnimationViewportClient::SetPreviewMeshComponent(UDebugSkelMeshComponent* InPreviewSkelMeshComp)
{
	PreviewSkelMeshComp = InPreviewSkelMeshComp;

	PreviewSkelMeshComp->BonesOfInterest.Empty();

	UpdateCameraSetup();

	// Setup physics data from physics assets if available, clearing any physics setup on the component
	UPhysicsAsset* PhysAsset = PreviewSkelMeshComp->GetPhysicsAsset();
	if(PhysAsset)
	{
		PhysAsset->InvalidateAllPhysicsMeshes();
		PreviewSkelMeshComp->TermArticulated();
		PreviewSkelMeshComp->InitArticulated(GetWorld()->GetPhysicsScene());

		// Set to block all to enable tracing.
		PreviewSkelMeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
}

void FAnimationViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	if (PreviewSkelMeshComp.IsValid() && PreviewSkelMeshComp->SkeletalMesh)
	{
		// Can't have both bones of interest and sockets of interest set
		check( !(PreviewSkelMeshComp->BonesOfInterest.Num() && PreviewSkelMeshComp->SocketsOfInterest.Num() ) )

		// if we have BonesOfInterest, draw sub set of the bones only
		if ( PreviewSkelMeshComp->BonesOfInterest.Num() > 0 )
		{
			DrawMeshSubsetBones(PreviewSkelMeshComp.Get(), PreviewSkelMeshComp->BonesOfInterest, PDI);
		}
		// otherwise, if we display bones, display
		if ( BoneDrawMode != EBoneDrawMode::None )
		{
			DrawMeshBones(PreviewSkelMeshComp.Get(), PDI);
		}
		if ( PreviewSkelMeshComp->bDisplayRawAnimation )
		{
			DrawMeshBonesUncompressedAnimation(PreviewSkelMeshComp.Get(), PDI);
		}
		if ( PreviewSkelMeshComp->NonRetargetedSpaceBases.Num() > 0 )
		{
			DrawMeshBonesNonRetargetedAnimation(PreviewSkelMeshComp.Get(), PDI);
		}
		if( PreviewSkelMeshComp->bDisplayAdditiveBasePose )
		{
			DrawMeshBonesAdditiveBasePose(PreviewSkelMeshComp.Get(), PDI);
		}
		if(PreviewSkelMeshComp->bDisplayBakedAnimation)
		{
			DrawMeshBonesBakedAnimation(PreviewSkelMeshComp.Get(), PDI);
		}
		if(PreviewSkelMeshComp->bDisplaySourceAnimation)
		{
			DrawMeshBonesSourceRawAnimation(PreviewSkelMeshComp.Get(), PDI);
		}
		
		DrawWatchedPoses(PreviewSkelMeshComp.Get(), PDI);

		// Display normal vectors of each simulation vertex
		if ( PreviewSkelMeshComp->bDisplayClothingNormals )
		{
			PreviewSkelMeshComp->DrawClothingNormals(PDI);
		}

		// Display tangent spaces of each graphical vertex
		if ( PreviewSkelMeshComp->bDisplayClothingTangents )
		{
			PreviewSkelMeshComp->DrawClothingTangents(PDI);
		}

		// Display collision volumes of current selected cloth
		if ( PreviewSkelMeshComp->bDisplayClothingCollisionVolumes )
		{
			PreviewSkelMeshComp->DrawClothingCollisionVolumes(PDI);
		}

		// Display collision volumes of current selected cloth
		if ( PreviewSkelMeshComp->bDisplayClothPhysicalMeshWire )
		{
			PreviewSkelMeshComp->DrawClothingPhysicalMeshWire(PDI);
		}

		// Display collision volumes of current selected cloth
		if ( PreviewSkelMeshComp->bDisplayClothMaxDistances )
		{
			PreviewSkelMeshComp->DrawClothingMaxDistances(PDI);
		}

		// Display collision volumes of current selected cloth
		if ( PreviewSkelMeshComp->bDisplayClothBackstops )
		{
			PreviewSkelMeshComp->DrawClothingBackstops(PDI);
		}

		if( PreviewSkelMeshComp->bDisplayClothFixedVertices )
		{
			PreviewSkelMeshComp->DrawClothingFixedVertices(PDI);
		}
		
		// Display socket hit points
		if ( PreviewSkelMeshComp->bDrawSockets )
		{
			if ( PreviewSkelMeshComp->bSkeletonSocketsVisible && PreviewSkelMeshComp->SkeletalMesh->Skeleton )
			{
				DrawSockets( PreviewSkelMeshComp.Get()->SkeletalMesh->Skeleton->Sockets, PDI, true );
			}

			if ( PreviewSkelMeshComp->bMeshSocketsVisible )
			{
				DrawSockets( PreviewSkelMeshComp.Get()->SkeletalMesh->GetMeshOnlySocketList(), PDI, false );
			}
		}

		// If we have a socket of interest, draw the widget
		if ( PreviewSkelMeshComp->SocketsOfInterest.Num() == 1 )
		{
			USkeletalMeshSocket* Socket = PreviewSkelMeshComp->SocketsOfInterest[0].Socket;

			bool bSocketIsOnSkeleton = PreviewSkelMeshComp->SocketsOfInterest[0].bSocketIsOnSkeleton;

			TArray<USkeletalMeshSocket*> SocketAsArray;
			SocketAsArray.Add( Socket );
			DrawSockets( SocketAsArray, PDI, false );
		}
	}

	if ( PersonaPtr.IsValid() && PreviewSkelMeshComp.IsValid() )
	{
		// Allow selected nodes to draw debug rendering if they support it
		const FGraphPanelSelectionSet SelectedNodes = PersonaPtr.Pin()->GetSelectedNodes();
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UAnimGraphNode_SkeletalControlBase* Node = Cast<UAnimGraphNode_SkeletalControlBase>(*NodeIt);

			if (Node)
			{
				Node->Draw(PDI, PreviewSkelMeshComp.Get());
			}
		}

		if(bFocusOnDraw)
		{
			bFocusOnDraw = false;
			FocusViewportOnPreviewMesh();
		}
	}
}

void FAnimationViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
	if (PreviewSkelMeshComp.IsValid())
	{

		// Display bone names
		if (PreviewSkelMeshComp->bShowBoneNames)
		{
			ShowBoneNames(&Canvas, &View);
		}

		// Allow nodes to draw with the canvas, and collect on screen strings to draw later
		TArray<FText> NodeDebugLines;
		if(PersonaPtr.IsValid())
		{
			// Allow selected nodes to draw debug rendering if they support it
			const FGraphPanelSelectionSet SelectedNodes = PersonaPtr.Pin()->GetSelectedNodes();
			for(FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
			{
				UAnimGraphNode_SkeletalControlBase* Node = Cast<UAnimGraphNode_SkeletalControlBase>(*NodeIt);

				if(Node)
				{
					Node->DrawCanvas(InViewport, View, Canvas, PreviewSkelMeshComp.Get());
					Node->GetOnScreenDebugInfo(NodeDebugLines, PreviewSkelMeshComp.Get());
				}
			}
		}

		// Display info
		if (IsShowingMeshStats())
		{
			DisplayInfo(&Canvas, &View, IsDetailedMeshStats());
		}
		else if(IsShowingSelectedNodeStats())
		{
			// Draw Node info instead of mesh info if we have entries
			DrawNodeDebugLines(NodeDebugLines, &Canvas, &View);
		}

		// Draw name of selected bone
		if (PreviewSkelMeshComp->BonesOfInterest.Num() == 1)
		{
			const FIntPoint ViewPortSize = Viewport->GetSizeXY();
			const int32 HalfX = ViewPortSize.X/2;
			const int32 HalfY = ViewPortSize.Y/2;

			int32 BoneIndex = PreviewSkelMeshComp->BonesOfInterest[0];
			const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);

			FMatrix BoneMatrix = PreviewSkelMeshComp->GetBoneMatrix(BoneIndex);
			const FPlane proj = View.Project(BoneMatrix.GetOrigin());
			if (proj.W > 0.f)
			{
				const int32 XPos = HalfX + ( HalfX * proj.X );
				const int32 YPos = HalfY + ( HalfY * (proj.Y * -1) );

				FQuat BoneQuat = PreviewSkelMeshComp->GetBoneQuaternion(BoneName);
				FVector Loc = PreviewSkelMeshComp->GetBoneLocation(BoneName);
				FCanvasTextItem TextItem( FVector2D( XPos, YPos), FText::FromString( BoneName.ToString() ), GEngine->GetSmallFont(), FLinearColor::White );
				Canvas.DrawItem( TextItem );				
			}
		}

		// Draw name of selected socket
		if (PreviewSkelMeshComp->SocketsOfInterest.Num() == 1)
		{
			USkeletalMeshSocket* Socket = PreviewSkelMeshComp->SocketsOfInterest[0].Socket;

			FMatrix SocketMatrix;
			Socket->GetSocketMatrix( SocketMatrix, PreviewSkelMeshComp.Get() );
			const FVector SocketPos	= SocketMatrix.GetOrigin();

			const FPlane proj = View.Project( SocketPos );
			if(proj.W > 0.f)
			{
				const FIntPoint ViewPortSize = Viewport->GetSizeXY();
				const int32 HalfX = ViewPortSize.X/2;
				const int32 HalfY = ViewPortSize.Y/2;

				const int32 XPos = HalfX + ( HalfX * proj.X );
				const int32 YPos = HalfY + ( HalfY * (proj.Y * -1) );
				FCanvasTextItem TextItem( FVector2D( XPos, YPos), FText::FromString( Socket->SocketName.ToString() ), GEngine->GetSmallFont(), FLinearColor::White );
				Canvas.DrawItem( TextItem );				
			}
		}

		if (bDrawUVs)
		{
			DrawUVsForMesh(Viewport, &Canvas, 1.0f);
		}
	}
}

void FAnimationViewportClient::DrawUVsForMesh(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos)
{
	//use the overridden LOD level
	const uint32 LODLevel = FMath::Clamp(PreviewSkelMeshComp->ForcedLodModel - 1, 0, PreviewSkelMeshComp->SkeletalMesh->LODInfo.Num() - 1);

	TArray<FVector2D> SelectedEdgeTexCoords; //No functionality in Persona for this (yet?)

	DrawUVs(InViewport, InCanvas, InTextYPos, LODLevel, UVChannelToDraw, SelectedEdgeTexCoords, NULL, &PreviewSkelMeshComp->GetSkeletalMeshResource()->LODModels[LODLevel] );
}

FAnimNode_SkeletalControlBase* FAnimationViewportClient::FindSkeletalControlAnimNode(TWeakObjectPtr<UAnimGraphNode_SkeletalControlBase> AnimGraphNode) const
{
	FAnimNode_SkeletalControlBase* AnimNode = NULL;

	if (PreviewSkelMeshComp.IsValid() && PreviewSkelMeshComp->GetAnimInstance() && AnimGraphNode.IsValid())
	{
		AnimNode = AnimGraphNode.Get()->FindDebugAnimNode(PreviewSkelMeshComp.Get());
	}

	return AnimNode;
}

void FAnimationViewportClient::FindSelectedAnimGraphNode()
{
	bool bSelected = false;
	// finding a selected node
	if (PersonaPtr.IsValid() && PreviewSkelMeshComp.IsValid())
	{
		USkeletalMeshComponent* PreviewComponent = PreviewSkelMeshComp.Get();
		check(PreviewComponent);

		const FGraphPanelSelectionSet SelectedNodes = PersonaPtr.Pin()->GetSelectedNodes();

		// don't support multi-selection
		if (SelectedNodes.Num() == 1)
		{
			FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes);
			// first element
			UAnimGraphNode_SkeletalControlBase* AnimGraphNode = Cast<UAnimGraphNode_SkeletalControlBase>(*NodeIt);

			if (AnimGraphNode)
			{
				FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(AnimGraphNode);

				if (AnimNode)
				{
					bSelected = true;

					// when selected first after AnimGraph is opened, assign previous data to current node
					if (SelectedSkelControlAnimGraph != AnimGraphNode)
					{

						if (SelectedSkelControlAnimGraph.IsValid())
						{
							SelectedSkelControlAnimGraph->DeselectActor(PreviewComponent);
						}

						// make same values to ensure data consistency
						AnimGraphNode->CopyNodeDataTo(AnimNode);

						WidgetMode = (FWidget::EWidgetMode)AnimGraphNode->GetWidgetMode(PreviewComponent);
						ECoordSystem DesiredCoordSystem = (ECoordSystem)AnimGraphNode->GetWidgetCoordinateSystem(PreviewComponent);
						SetWidgetCoordSystemSpace(DesiredCoordSystem);
					}
					else
					{
						if (bShouldUpdateDefaultValues)
						{
							// copy updated values into internal node to ensure data consistency
							AnimGraphNode->CopyNodeDataFrom(AnimNode);
							bShouldUpdateDefaultValues = false;
						}
					}

					AnimGraphNode->MoveSelectActorLocation(PreviewComponent, AnimNode);
					SelectedSkelControlAnimGraph = AnimGraphNode;
				}
			}
		}

		if (!bSelected)
		{
			if (SelectedSkelControlAnimGraph.IsValid())
			{
				SelectedSkelControlAnimGraph->DeselectActor(PreviewComponent);
			}
			SelectedSkelControlAnimGraph.Reset();
		}
	}
}

void FAnimationViewportClient::ClearSelectedAnimGraphNode()
{
	if (SelectedSkelControlAnimGraph.IsValid())
	{
		SelectedSkelControlAnimGraph->DeselectActor(PreviewSkelMeshComp.Get());
	}
}

void FAnimationViewportClient::PostUndo()
{
	// undo for skeletal controllers
	// search all UAnimGraphNode_SkeletalControlBase nodes and apply data
	UEdGraph* FocusedGraph = PersonaPtr.Pin()->GetFocusedGraph();

	// @fixme : fix this to better way than looping all nodes
	if (FocusedGraph)
	{
		// find UAnimGraphNode_SkeletalControlBase
		for (UEdGraphNode* Node : FocusedGraph->Nodes)
		{
			UAnimGraphNode_SkeletalControlBase* AnimGraphNode = Cast<UAnimGraphNode_SkeletalControlBase>(Node);

			if (AnimGraphNode)
			{
				FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(AnimGraphNode);

				if (AnimNode)
				{
					// copy undo data
					AnimGraphNode->CopyNodeDataTo(AnimNode);
				}
			}
		}
	}
}

void FAnimationViewportClient::PostCompile()
{
	// reset the selected anim graph to copy 
	SelectedSkelControlAnimGraph.Reset();

	// if the user manipulated Pin values directly from the node, then should copy updated values to the internal node to retain data consistency
	UEdGraph* FocusedGraph = PersonaPtr.Pin()->GetFocusedGraph();

	// @fixme : fix this to better way than looping all nodes
	if (FocusedGraph)
	{
		// find UAnimGraphNode_SkeletalControlBase
		for (UEdGraphNode* Node : FocusedGraph->Nodes)
		{
			UAnimGraphNode_SkeletalControlBase* AnimGraphNode = Cast<UAnimGraphNode_SkeletalControlBase>(Node);

			if (AnimGraphNode)
			{
				FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(AnimGraphNode);

				if (AnimNode)
				{
					AnimGraphNode->CopyNodeDataFrom(AnimNode);
				}
			}
		}
	}
}

void FAnimationViewportClient::Tick(float DeltaSeconds) 
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// @todo fixme
	if (bCameraFollow && PreviewSkelMeshComp.IsValid())
	{
		// if camera isn't lock, make the mesh bounds to be center
		FSphere BoundSphere = GetCameraTarget();

		// need to interpolate from ViewLocation to Origin
		SetCameraTargetLocation(BoundSphere, DeltaSeconds);
	}

	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}

	UDebugSkelMeshComponent* PreviewComp = PreviewSkelMeshComp.Get();
	if (PreviewComp)
	{
		// Handle updating the preview component to represent the effects of root motion
		const UStaticMeshComponent* FloorMeshComponent = AdvancedPreviewScene->GetFloorMeshComponent();
		FBoxSphereBounds Bounds = FloorMeshComponent->CalcBounds(FloorMeshComponent->GetRelativeTransform());
		PreviewComp->ConsumeRootMotion(Bounds.GetBox().Min, Bounds.GetBox().Max);
	}	

	FindSelectedAnimGraphNode();

}

void FAnimationViewportClient::SetCameraTargetLocation(const FSphere &BoundSphere, float DeltaSeconds)
{
	FVector OldViewLoc = GetViewLocation();
	FMatrix EpicMat = FTranslationMatrix(-GetViewLocation());
	EpicMat = EpicMat * FInverseRotationMatrix(GetViewRotation());
	FMatrix CamRotMat = EpicMat.InverseFast();
	FVector CamDir = FVector(CamRotMat.M[0][0],CamRotMat.M[0][1],CamRotMat.M[0][2]);
	FVector NewViewLocation = BoundSphere.Center - BoundSphere.W * 2 * CamDir;

	NewViewLocation.X = FMath::FInterpTo(OldViewLoc.X, NewViewLocation.X, DeltaSeconds, FollowCamera_InterpSpeed);
	NewViewLocation.Y = FMath::FInterpTo(OldViewLoc.Y, NewViewLocation.Y, DeltaSeconds, FollowCamera_InterpSpeed);
	NewViewLocation.Z = FMath::FInterpTo(OldViewLoc.Z, NewViewLocation.Z, DeltaSeconds, FollowCamera_InterpSpeed_Z);

	SetViewLocation( NewViewLocation );
}

void FAnimationViewportClient::ShowBoneNames( FCanvas* Canvas, FSceneView* View )
{
	if (!PreviewSkelMeshComp.IsValid() || !PreviewSkelMeshComp->MeshObject)
	{
		return;
	}

	//Most of the code taken from FASVViewportClient::Draw() in AnimSetViewerMain.cpp
	FSkeletalMeshResource* SkelMeshResource = PreviewSkelMeshComp->GetSkeletalMeshResource();
	check(SkelMeshResource);
	const int32 LODIndex = FMath::Clamp(PreviewSkelMeshComp->PredictedLODLevel, 0, SkelMeshResource->LODModels.Num()-1);
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[ LODIndex ];

	const int32 HalfX = Viewport->GetSizeXY().X/2;
	const int32 HalfY = Viewport->GetSizeXY().Y/2;

	for (int32 i=0; i< LODModel.RequiredBones.Num(); i++)
	{
		const int32 BoneIndex = LODModel.RequiredBones[i];

		// If previewing a specific section, only show the bone names that belong to it
		if ((PreviewSkelMeshComp->SectionIndexPreview >= 0) && !LODModel.Sections[PreviewSkelMeshComp->SectionIndexPreview].BoneMap.Contains(BoneIndex))
		{
			continue;
		}

		const FColor BoneColor = FColor::White;
		if (BoneColor.A != 0)
		{
			const FVector BonePos = PreviewSkelMeshComp->ComponentToWorld.TransformPosition(PreviewSkelMeshComp->GetComponentSpaceTransforms()[BoneIndex].GetLocation());

			const FPlane proj = View->Project(BonePos);
			if (proj.W > 0.f)
			{
				const int32 XPos = HalfX + ( HalfX * proj.X );
				const int32 YPos = HalfY + ( HalfY * (proj.Y * -1) );

				const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
				const FString BoneString = FString::Printf( TEXT("%d: %s"), BoneIndex, *BoneName.ToString() );
				FCanvasTextItem TextItem( FVector2D( XPos, YPos), FText::FromString( BoneString ), GEngine->GetSmallFont(), BoneColor );
				TextItem.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem( TextItem );
			}
		}
	}
}

void FAnimationViewportClient::DisplayInfo(FCanvas* Canvas, FSceneView* View, bool bDisplayAllInfo)
{
	int32 CurXOffset = 5;
	int32 CurYOffset = 60;

	int32 XL, YL;
	StringSize( GEngine->GetSmallFont(),  XL, YL, TEXT("L") );
	FString InfoString;

	const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();	
	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	// it is weird, but unless it's completely black, it's too bright, so just making it white if only black
	const FLinearColor TextColor = ((SelectedHSVColor.B < 0.3f) || (Settings->Profiles[ProfileIndex].bShowEnvironment)) ? FLinearColor::White : FLinearColor::Black;
	const FColor HeadlineColour(255, 83, 0);
	const FColor SubHeadlineColour(202, 66, 0);

	// if not valid skeletalmesh
	if (!PreviewSkelMeshComp.IsValid() || !PreviewSkelMeshComp->SkeletalMesh)
	{
		return;
	}

	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();

	if (SharedPersona.IsValid() && SharedPersona->ShouldDisplayAdditiveScaleErrorMessage())
	{
		InfoString = TEXT("Additve ref pose contains scales of 0.0, this can cause additive animations to not give the desired results");
		Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour);
		CurYOffset += YL + 2;
	}

	if (PreviewSkelMeshComp->SkeletalMesh->MorphTargets.Num() > 0)
	{
		int32 SubHeadingIndent = CurXOffset + 10;

		TArray<UMaterial*> ProcessedMaterials;
		TArray<UMaterial*> MaterialsThatNeedMorphFlagOn;
		TArray<UMaterial*> MaterialsThatNeedSaving;

		for (int i = 0; i < PreviewSkelMeshComp->GetNumMaterials(); ++i)
		{
			if (UMaterialInterface* MaterialInterface = PreviewSkelMeshComp->GetMaterial(i))
			{
				UMaterial* Material = MaterialInterface->GetMaterial();
				if ((Material != nullptr) && !ProcessedMaterials.Contains(Material))
				{
					ProcessedMaterials.Add(Material);
					if (!Material->GetUsageByFlag(MATUSAGE_MorphTargets))
					{
						MaterialsThatNeedMorphFlagOn.Add(Material);
					}
					else if (Material->IsUsageFlagDirty(MATUSAGE_MorphTargets))
					{
						MaterialsThatNeedSaving.Add(Material);
					}
				}
			}
		}

		if (MaterialsThatNeedMorphFlagOn.Num() > 0)
		{
			InfoString = FString::Printf( *LOCTEXT("MorphSupportNeeded", "The following materials need morph support ('Used with Morph Targets' in material editor):").ToString() );
			Canvas->DrawShadowedString( CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), HeadlineColour );

			CurYOffset += YL + 2;

			for(auto Iter = MaterialsThatNeedMorphFlagOn.CreateIterator(); Iter; ++Iter)
			{
				UMaterial* Material = (*Iter);
				InfoString = FString::Printf(TEXT("%s"), *Material->GetPathName());
				Canvas->DrawShadowedString( SubHeadingIndent, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour );
				CurYOffset += YL + 2;
			}
			CurYOffset += 2;
		}

		if (MaterialsThatNeedSaving.Num() > 0)
		{
			InfoString = FString::Printf( *LOCTEXT("MaterialsNeedSaving", "The following materials need saving to fully support morph targets:").ToString() );
			Canvas->DrawShadowedString( CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), HeadlineColour );

			CurYOffset += YL + 2;

			for(auto Iter = MaterialsThatNeedSaving.CreateIterator(); Iter; ++Iter)
			{
				UMaterial* Material = (*Iter);
				InfoString = FString::Printf(TEXT("%s"), *Material->GetPathName());
				Canvas->DrawShadowedString( SubHeadingIndent, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour );
				CurYOffset += YL + 2;
			}
			CurYOffset += 2;
		}
	}

	UAnimPreviewInstance* PreviewInstance = PreviewSkelMeshComp->PreviewInstance;
	if( PreviewInstance )
	{
		// see if you have anim sequence that has transform curves
		UAnimSequence* Sequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if (Sequence)
		{
			if (Sequence->DoesNeedRebake())
			{
				InfoString = TEXT("Animation is being edited. To apply to raw animation data, click \"Apply\"");
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour);
				CurYOffset += YL + 2;
			}

			if (Sequence->DoesNeedRecompress())
			{
				InfoString = TEXT("Animation is being edited. To apply to compressed data (and recalculate baked additives), click \"Apply\"");
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour);
				CurYOffset += YL + 2;
			}
		}
	}

	if (PreviewSkelMeshComp->IsUsingInGameBounds())
	{
		if (!PreviewSkelMeshComp->CheckIfBoundsAreCorrrect())
		{
			if( PreviewSkelMeshComp->GetPhysicsAsset() == NULL )
			{
				InfoString = FString::Printf( *LOCTEXT("NeedToSetupPhysicsAssetForAccurateBounds", "You may need to setup Physics Asset to use more accurate bounds").ToString() );
			}
			else
			{
				InfoString = FString::Printf( *LOCTEXT("NeedToSetupBoundsInPhysicsAsset", "You need to setup bounds in Physics Asset to include whole mesh").ToString() );
			}
			Canvas->DrawShadowedString( CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor );
			CurYOffset += YL + 2;
		}
	}

	if (PreviewSkelMeshComp != NULL && PreviewSkelMeshComp->MeshObject != NULL)
	{
		if (bDisplayAllInfo)
		{
			FSkeletalMeshResource* SkelMeshResource = PreviewSkelMeshComp->GetSkeletalMeshResource();
			check(SkelMeshResource);

			// Draw stats about the mesh
			const FBoxSphereBounds& SkelBounds = PreviewSkelMeshComp->Bounds;
			const FPlane ScreenPosition = View->Project(SkelBounds.Origin);

			const int32 HalfX = Viewport->GetSizeXY().X / 2;
			const int32 HalfY = Viewport->GetSizeXY().Y / 2;

			const float ScreenRadius = FMath::Max((float)HalfX * View->ViewMatrices.ProjMatrix.M[0][0], (float)HalfY * View->ViewMatrices.ProjMatrix.M[1][1]) * SkelBounds.SphereRadius / FMath::Max(ScreenPosition.W, 1.0f);
			const float LODFactor = ScreenRadius / 320.0f;

			int32 NumBonesInUse;
			int32 NumBonesMappedToVerts;
			int32 NumSectionsInUse;
			FString WeightUsage;

			const int32 LODIndex = FMath::Clamp(PreviewSkelMeshComp->PredictedLODLevel, 0, SkelMeshResource->LODModels.Num() - 1);
			FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

			NumBonesInUse = LODModel.RequiredBones.Num();
			NumBonesMappedToVerts = LODModel.ActiveBoneIndices.Num();
			NumSectionsInUse = LODModel.Sections.Num();

			// Calculate polys based on non clothing sections so we don't duplicate the counts.
			uint32 NumTotalTriangles = 0;
			int32 NumSections = LODModel.NumNonClothingSections();
			for(int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
			{
				NumTotalTriangles += LODModel.Sections[SectionIndex].NumTriangles;
			}

			InfoString = FString::Printf(TEXT("LOD: %d, Bones: %d (Mapped to Vertices: %d), Polys: %d"),
				LODIndex,
				NumBonesInUse,
				NumBonesMappedToVerts,
				NumTotalTriangles);

			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			InfoString = FString::Printf(TEXT("Current Screen Size: %3.2f, FOV:%3.0f"), LODFactor, ViewFOV);
			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			CurYOffset += 1; // --

			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				int32 SectionVerts = LODModel.Sections[SectionIndex].GetNumVertices();

				InfoString = FString::Printf(TEXT(" [Section %d] Verts:%d, Bones:%d"),
					SectionIndex,
					SectionVerts,
					LODModel.Sections[SectionIndex].BoneMap.Num()
					);

				CurYOffset += YL + 2;
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor*0.8f);
			}

			InfoString = FString::Printf(TEXT("TOTAL Verts:%d"),
				LODModel.NumVertices);

			CurYOffset += 1; // --


			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			InfoString = FString::Printf(TEXT("Sections:%d %s"),
				NumSectionsInUse,
				*WeightUsage
				);

			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			if (PreviewSkelMeshComp->BonesOfInterest.Num() > 0)
			{
				int32 BoneIndex = PreviewSkelMeshComp->BonesOfInterest[0];
				FTransform ReferenceTransform = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
				FTransform LocalTransform = PreviewSkelMeshComp->BoneSpaceTransforms[BoneIndex];
				FTransform ComponentTransform = PreviewSkelMeshComp->GetComponentSpaceTransforms()[BoneIndex];

				CurYOffset += YL + 2;
				InfoString = FString::Printf(TEXT("Local :%s"), *LocalTransform.ToString());
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

				CurYOffset += YL + 2;
				InfoString = FString::Printf(TEXT("Component :%s"), *ComponentTransform.ToString());
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

				CurYOffset += YL + 2;
				InfoString = FString::Printf(TEXT("Reference :%s"), *ReferenceTransform.ToString());
				Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);
			}

			CurYOffset += YL + 2;
			InfoString = FString::Printf(TEXT("Approximate Size: %ix%ix%i"),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.X * 2.0f),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.Y * 2.0f),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.Z * 2.0f));
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			uint32 NumNotiesWithErrors = PreviewSkelMeshComp->AnimNotifyErrors.Num();
			for (uint32 i = 0; i < NumNotiesWithErrors; ++i)
			{
				uint32 NumErrors = PreviewSkelMeshComp->AnimNotifyErrors[i].Errors.Num();
				for (uint32 ErrorIdx = 0; ErrorIdx < NumErrors; ++ErrorIdx)
				{
					CurYOffset += YL + 2;
					Canvas->DrawShadowedString(CurXOffset, CurYOffset, *PreviewSkelMeshComp->AnimNotifyErrors[i].Errors[ErrorIdx], GEngine->GetSmallFont(), FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
				}
			}
		}
		else // simplified default display info to be same as static mesh editor
		{
			FSkeletalMeshResource* SkelMeshResource = PreviewSkelMeshComp->GetSkeletalMeshResource();
			check(SkelMeshResource);

			const int32 LODIndex = FMath::Clamp(PreviewSkelMeshComp->PredictedLODLevel, 0, SkelMeshResource->LODModels.Num() - 1);
			FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

			// Current LOD 
			InfoString = FString::Printf(TEXT("LOD: %d"), LODIndex);
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			// current screen size
			const FBoxSphereBounds& SkelBounds = PreviewSkelMeshComp->Bounds;
			const FPlane ScreenPosition = View->Project(SkelBounds.Origin);

			const int32 HalfX = Viewport->GetSizeXY().X / 2;
			const int32 HalfY = Viewport->GetSizeXY().Y / 2;
			const float ScreenRadius = FMath::Max((float)HalfX * View->ViewMatrices.ProjMatrix.M[0][0], (float)HalfY * View->ViewMatrices.ProjMatrix.M[1][1]) * SkelBounds.SphereRadius / FMath::Max(ScreenPosition.W, 1.0f);
			const float LODFactor = ScreenRadius / 320.0f;

			float ScreenSize = ComputeBoundsScreenSize(ScreenPosition, SkelBounds.SphereRadius, *View);

			InfoString = FString::Printf(TEXT("Current Screen Size: %3.2f"), LODFactor);
			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			// Triangles
			uint32 NumTotalTriangles = 0;
			int32 NumSections = LODModel.NumNonClothingSections();
			for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
			{
				NumTotalTriangles += LODModel.Sections[SectionIndex].NumTriangles;
			}
			InfoString = FString::Printf(TEXT("Triangles: %d"), NumTotalTriangles);
			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			// Vertices
			InfoString = FString::Printf(TEXT("Vertices: %d"), LODModel.NumVertices);
			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);

			// UV Channels
			InfoString = FString::Printf(TEXT("UV Channels: %d"), LODModel.NumTexCoords);
			CurYOffset += YL + 2;
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);
			
			// Approx Size 
			CurYOffset += YL + 2;
			InfoString = FString::Printf(TEXT("Approx Size: %ix%ix%i"),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.X * 2.0f),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.Y * 2.0f),
				FMath::RoundToInt(PreviewSkelMeshComp->Bounds.BoxExtent.Z * 2.0f));
			Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), TextColor);
		}
	}

	if (PreviewSkelMeshComp->SectionIndexPreview != INDEX_NONE)
	{
		// Notify the user if they are isolating a mesh section.
		CurYOffset += YL + 2;
		InfoString = FString::Printf(*LOCTEXT("MeshSectionsHiddenWarning", "Mesh Sections Hidden").ToString());
		Canvas->DrawShadowedString(CurXOffset, CurYOffset, *InfoString, GEngine->GetSmallFont(), SubHeadlineColour);
		
	}
}

void FAnimationViewportClient::DrawNodeDebugLines(TArray<FText>& Lines, FCanvas* Canvas, FSceneView* View)
{
	if(Lines.Num() > 0)
	{
		int32 CurrentXOffset = 5;
		int32 CurrentYOffset = 60;

		int32 CharWidth;
		int32 CharHeight;
		StringSize(GEngine->GetSmallFont(), CharWidth, CharHeight, TEXT("0"));

		const int32 LineHeight = CharHeight + 2;

		for(FText& Line : Lines)
		{
			FCanvasTextItem TextItem(FVector2D(CurrentXOffset, CurrentYOffset), Line, GEngine->GetSmallFont(), FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);

			Canvas->DrawItem(TextItem);

			CurrentYOffset += LineHeight;
		}
	}
}

int32 FAnimationViewportClient::FindSelectedBone() const
{
	int32 SelectedBone = INDEX_NONE;
	if (SelectedSkelControlAnimGraph.IsValid())
	{		
		FName BoneName = SelectedSkelControlAnimGraph->FindSelectedBone();
		SelectedBone = PreviewSkelMeshComp->GetBoneIndex(BoneName);
	}

	if (SelectedBone == INDEX_NONE && PreviewSkelMeshComp.IsValid() && (PreviewSkelMeshComp->BonesOfInterest.Num() == 1) )
	{
		SelectedBone = PreviewSkelMeshComp->BonesOfInterest[0];
	}
	return SelectedBone;
}

USkeletalMeshSocket* FAnimationViewportClient::FindSelectedSocket() const
{
	USkeletalMeshSocket* SelectedSocket = NULL;

	if (PreviewSkelMeshComp.IsValid() && (PreviewSkelMeshComp->SocketsOfInterest.Num() == 1) )
	{
		SelectedSocket = PreviewSkelMeshComp->SocketsOfInterest[0].Socket;
	}

	return SelectedSocket;
}

TWeakObjectPtr<AWindDirectionalSource> FAnimationViewportClient::FindSelectedWindActor() const
{
	return SelectedWindActor;
}

void FAnimationViewportClient::ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();

	if(!SharedPersona.IsValid())
	{
		// No reason to continue, cannot select elements
		return;
	}

	if ( HitProxy )
	{
		if ( HitProxy->IsA( HPersonaSocketProxy::StaticGetType() ) )
		{
			HPersonaSocketProxy* SocketProxy = ( HPersonaSocketProxy* )HitProxy;

			// Tell Persona that the socket has been selected - this will sort out the skeleton tree, etc.
			SharedPersona->SetSelectedSocket( SocketProxy->SocketInfo );
		}
		else if ( HitProxy->IsA( HPersonaBoneProxy::StaticGetType() ) )
		{
			HPersonaBoneProxy* BoneProxy = ( HPersonaBoneProxy* )HitProxy;
			// Tell Persona that the bone has been selected - this will sort out the skeleton tree, etc.
			SharedPersona->SetSelectedBone( PreviewSkelMeshComp.Get()->SkeletalMesh->Skeleton, BoneProxy->BoneName );
		}
		else if ( HitProxy->IsA( HActor::StaticGetType() ) )
		{
			HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
			AWindDirectionalSource* WindActor = Cast<AWindDirectionalSource>(ActorHitProxy->Actor);

			if( WindActor )
			{
				//clear previously selected things
				SharedPersona->DeselectAll();
				SelectedWindActor = WindActor;
			}
			else if (SelectedSkelControlAnimGraph.IsValid() && SelectedSkelControlAnimGraph->IsActorClicked(ActorHitProxy))
			{
				// do some actions when hit
				SelectedSkelControlAnimGraph->ProcessActorClick(ActorHitProxy);
			}
		}
	}
	else
	{
		// Cast for phys bodies if we didn't get any hit proxies
		FHitResult Result(1.0f);
		const FViewportClick Click(&View, this, EKeys::Invalid, IE_Released, Viewport->GetMouseX(), Viewport->GetMouseY());
		bool bHit = PreviewSkelMeshComp.Get()->LineTraceComponent(Result, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * BodyTraceDistance, FCollisionQueryParams(NAME_None,true));
		
		if(bHit)
		{
			SharedPersona->SetSelectedBone(PreviewSkelMeshComp->SkeletalMesh->Skeleton, Result.BoneName);
		}
		else
		{
			// We didn't hit a proxy or a physics object, so deselect all objects
			SharedPersona->DeselectAll();
		}
}
}

bool FAnimationViewportClient::InputWidgetDelta( FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale )
{
	// Get some useful info about buttons being held down
	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
	const bool bMouseButtonDown = InViewport->KeyState( EKeys::LeftMouseButton ) || InViewport->KeyState( EKeys::MiddleMouseButton ) || InViewport->KeyState( EKeys::RightMouseButton );

	bool bHandled = false;

	if ( bManipulating && CurrentAxis != EAxisList::None )
	{
		bHandled = true;

		int32 BoneIndex = FindSelectedBone();
		USkeletalMeshSocket* SelectedSocket = FindSelectedSocket();
		TWeakObjectPtr<AWindDirectionalSource> WindActor = FindSelectedWindActor();

		FAnimNode_ModifyBone* SkelControl = NULL;

		if ( BoneIndex >= 0 )
		{
			//Get the skeleton control manipulating this bone
			const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
			SkelControl = &(PreviewSkelMeshComp->PreviewInstance->ModifyBone(BoneName));
		}

		if ( SkelControl || SelectedSocket )
		{
			FTransform CurrentSkelControlTM(
				SelectedSocket ? SelectedSocket->RelativeRotation : SkelControl->Rotation,
				SelectedSocket ? SelectedSocket->RelativeLocation : SkelControl->Translation,
				SelectedSocket ? SelectedSocket->RelativeScale : SkelControl->Scale);

			FTransform BaseTM;

			if ( SelectedSocket )
			{
				BaseTM = SelectedSocket->GetSocketTransform( PreviewSkelMeshComp.Get() );
			}
			else
			{
				BaseTM = PreviewSkelMeshComp->GetBoneTransform( BoneIndex );
			}

			// Remove SkelControl's orientation from BoneMatrix, as we need to translate/rotate in the non-SkelControlled space
			BaseTM = BaseTM.GetRelativeTransformReverse(CurrentSkelControlTM);

			const bool bDoRotation    = WidgetMode == FWidget::WM_Rotate    || WidgetMode == FWidget::WM_TranslateRotateZ;
			const bool bDoTranslation = WidgetMode == FWidget::WM_Translate || WidgetMode == FWidget::WM_TranslateRotateZ;
			const bool bDoScale = WidgetMode == FWidget::WM_Scale;

			if (bDoRotation)
			{
				FVector RotAxis;
				float RotAngle;
				Rot.Quaternion().ToAxisAndAngle( RotAxis, RotAngle );

				FVector4 BoneSpaceAxis = BaseTM.TransformVector( RotAxis );

				//Calculate the new delta rotation
				FQuat DeltaQuat( BoneSpaceAxis, RotAngle );
				DeltaQuat.Normalize();

				FRotator NewRotation = ( CurrentSkelControlTM * FTransform( DeltaQuat )).Rotator();

				if (SelectedSkelControlAnimGraph.IsValid())
				{
					FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(SelectedSkelControlAnimGraph);
					if (AnimNode)
					{
						SelectedSkelControlAnimGraph->DoRotation(PreviewSkelMeshComp.Get(), Rot, AnimNode);
						bShouldUpdateDefaultValues = true;
					}
				}
				else if ( SelectedSocket )
				{
					SelectedSocket->RelativeRotation = NewRotation;
				}
				else
				{
					SkelControl->Rotation = NewRotation;
				}
			}

			if (bDoTranslation)
			{
				FVector4 BoneSpaceOffset = BaseTM.TransformVector(Drag);
				if (SelectedSkelControlAnimGraph.IsValid())
				{
					FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(SelectedSkelControlAnimGraph);
					if (AnimNode)
					{
						SelectedSkelControlAnimGraph->DoTranslation(PreviewSkelMeshComp.Get(), Drag, AnimNode);
						bShouldUpdateDefaultValues = true;
					}
				}
				else if (SelectedSocket)
				{
					SelectedSocket->RelativeLocation += BoneSpaceOffset;
				}
				else
				{
					SkelControl->Translation += BoneSpaceOffset;
				}
			}
			if(bDoScale)
			{
				FVector4 BoneSpaceScaleOffset;

				if (ModeTools->GetCoordSystem() == COORD_World)
				{
					BoneSpaceScaleOffset = BaseTM.TransformVector(Scale);
				}
				else
				{
					BoneSpaceScaleOffset = Scale;
				}

				if (SelectedSkelControlAnimGraph.IsValid())
				{
					FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(SelectedSkelControlAnimGraph);
					if (AnimNode)
					{
						SelectedSkelControlAnimGraph->DoScale(PreviewSkelMeshComp.Get(), Scale, AnimNode);
						bShouldUpdateDefaultValues = true;
					}
				}
				else if(SelectedSocket)
				{
					SelectedSocket->RelativeScale += BoneSpaceScaleOffset;
				}
				else
				{
					SkelControl->Scale += BoneSpaceScaleOffset;
				}
			}

			}
		else if( WindActor.IsValid() )
		{
			if (WidgetMode == FWidget::WM_Rotate)
			{
				FTransform WindTransform = WindActor->GetTransform();

				FRotator NewRotation = ( WindTransform * FTransform( Rot ) ).Rotator();

				WindActor->SetActorRotation( NewRotation );
			}
			else
			{
				FVector Location = WindActor->GetActorLocation();
				Location += Drag;
				WindActor->SetActorLocation(Location);
			}
		}

		InViewport->Invalidate();
	}

	return bHandled;
}


void FAnimationViewportClient::TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	int32 BoneIndex = FindSelectedBone();
	USkeletalMeshSocket* SelectedSocket = FindSelectedSocket();
	TWeakObjectPtr<AWindDirectionalSource> WindActor = FindSelectedWindActor();

	if( (BoneIndex >= 0 || SelectedSocket || WindActor.IsValid()) && bIsDraggingWidget )
	{
		bool bValidAxis = false;
		FVector WorldAxisDir;

		if(InInputState.IsLeftMouseButtonPressed() && (Widget->GetCurrentAxis() & EAxisList::XYZ) != 0)
		{
			if ( PreviewSkelMeshComp->SocketsOfInterest.Num() == 1 )
		{
			const bool bAltDown = InInputState.IsAltButtonPressed();

			if ( bAltDown && PersonaPtr.IsValid() )
			{
				// Rather than moving/rotating the selected socket, copy it and move the copy instead
				PersonaPtr.Pin()->DuplicateAndSelectSocket( PreviewSkelMeshComp->SocketsOfInterest[0] );
			}

			// Socket movement is transactional - we want undo/redo and saving of it
			USkeletalMeshSocket* Socket = PreviewSkelMeshComp->SocketsOfInterest[0].Socket;

			if ( Socket && bInTransaction == false )
			{
				if (WidgetMode == FWidget::WM_Rotate )
				{
					GEditor->BeginTransaction( LOCTEXT("AnimationEditorViewport_RotateSocket", "Rotate Socket" ) );
				}
				else
				{
					GEditor->BeginTransaction( LOCTEXT("AnimationEditorViewport_TranslateSocket", "Translate Socket" ) );
				}

				Socket->SetFlags( RF_Transactional );	// Undo doesn't work without this!
				Socket->Modify();
				bInTransaction = true;
			}
		}
			else if( BoneIndex >= 0 )
			{
				if ( bInTransaction == false )
				{
					// we also allow undo/redo of bone manipulations
					if (WidgetMode == FWidget::WM_Rotate )
					{
						GEditor->BeginTransaction( LOCTEXT("AnimationEditorViewport_RotateBone", "Rotate Bone" ) );
					}
					else
					{
						GEditor->BeginTransaction( LOCTEXT("AnimationEditorViewport_TranslateBone", "Translate Bone" ) );
					}

					PreviewSkelMeshComp->PreviewInstance->SetFlags( RF_Transactional );	// Undo doesn't work without this!
					PreviewSkelMeshComp->PreviewInstance->Modify();
					bInTransaction = true;

					// now modify the bone array
					const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
					PreviewSkelMeshComp->PreviewInstance->ModifyBone(BoneName);
				}
			}
		}


		bManipulating = true;
	}
}

void FAnimationViewportClient::TrackingStopped() 
{
	if (bManipulating)
	{
		// Socket movement is transactional - we want undo/redo and saving of it
		if ( bInTransaction )
		{
			GEditor->EndTransaction();
			bInTransaction = false;
		}

		bManipulating = false;
	}
	Invalidate();
}

FWidget::EWidgetMode FAnimationViewportClient::GetWidgetMode() const
{
	FWidget::EWidgetMode Mode = FWidget::WM_None;
	if (!PreviewSkelMeshComp->IsAnimBlueprintInstanced())
	{
	if ((PreviewSkelMeshComp != NULL) && ((PreviewSkelMeshComp->BonesOfInterest.Num() == 1) || (PreviewSkelMeshComp->SocketsOfInterest.Num() == 1)))
	{
			Mode = WidgetMode;
	}
	else if (SelectedWindActor.IsValid())
	{
			Mode = WidgetMode;
	}
	}

	if (SelectedSkelControlAnimGraph.IsValid())
	{
		Mode = (FWidget::EWidgetMode)SelectedSkelControlAnimGraph->GetWidgetMode(PreviewSkelMeshComp.Get());
	}

	return Mode;
}

FVector FAnimationViewportClient::GetWidgetLocation() const
{
	if (SelectedSkelControlAnimGraph.IsValid())
	{
		FAnimNode_SkeletalControlBase* AnimNode = FindSkeletalControlAnimNode(SelectedSkelControlAnimGraph.Get());
		if (AnimNode)
		{
			return SelectedSkelControlAnimGraph->GetWidgetLocation(PreviewSkelMeshComp.Get(), AnimNode);
		}
	}
	else if( PreviewSkelMeshComp->BonesOfInterest.Num() > 0 )
	{
		int32 BoneIndex = PreviewSkelMeshComp->BonesOfInterest.Last();
		const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);

		FMatrix BoneMatrix = PreviewSkelMeshComp->GetBoneMatrix(BoneIndex);

		return BoneMatrix.GetOrigin();
	}
	else if( PreviewSkelMeshComp->SocketsOfInterest.Num() > 0 )
	{
		USkeletalMeshSocket* Socket = PreviewSkelMeshComp->SocketsOfInterest.Last().Socket;

 		FMatrix SocketMatrix;
		Socket->GetSocketMatrix( SocketMatrix, PreviewSkelMeshComp.Get() );

		return SocketMatrix.GetOrigin();
	}
	else if( SelectedWindActor.IsValid() )
	{
		return SelectedWindActor.Get()->GetActorLocation();
	}

	return FVector::ZeroVector;
}

FMatrix FAnimationViewportClient::GetWidgetCoordSystem() const
{
	const bool bIsLocal = GetWidgetCoordSystemSpace() == COORD_Local;

	if( bIsLocal )
	{
		if (SelectedSkelControlAnimGraph.IsValid())
		{
			FName BoneName = SelectedSkelControlAnimGraph->FindSelectedBone();
			int32 BoneIndex = PreviewSkelMeshComp->GetBoneIndex(BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				FTransform BoneMatrix = PreviewSkelMeshComp->GetBoneTransform(BoneIndex);
				return BoneMatrix.ToMatrixNoScale().RemoveTranslation();
			}
		}
		else if ( PreviewSkelMeshComp->BonesOfInterest.Num() > 0 )
		{
			int32 BoneIndex = PreviewSkelMeshComp->BonesOfInterest.Last();

			FTransform BoneMatrix = PreviewSkelMeshComp->GetBoneTransform(BoneIndex);

			return BoneMatrix.ToMatrixNoScale().RemoveTranslation();
		}
		else if( PreviewSkelMeshComp->SocketsOfInterest.Num() > 0 )
		{
			USkeletalMeshSocket* Socket = PreviewSkelMeshComp->SocketsOfInterest.Last().Socket;

			FTransform SocketMatrix = Socket->GetSocketTransform( PreviewSkelMeshComp.Get() );
			
			return SocketMatrix.ToMatrixNoScale().RemoveTranslation();
		}
		else if ( SelectedWindActor.IsValid() )
		{
			return SelectedWindActor->GetTransform().ToMatrixNoScale().RemoveTranslation();
		}
	}

	return FMatrix::Identity;
}

ECoordSystem FAnimationViewportClient::GetWidgetCoordSystemSpace() const
{ 
	return ModeTools->GetCoordSystem();
}

void FAnimationViewportClient::SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem)
{
	ModeTools->SetCoordSystem(NewCoordSystem);
	Invalidate();
}

void FAnimationViewportClient::SetViewMode(EViewModeIndex InViewModeIndex)
{
	FEditorViewportClient::SetViewMode(InViewModeIndex);

	ConfigOption->SetViewModeIndex(InViewModeIndex);
}

void FAnimationViewportClient::SetViewportType(ELevelViewportType InViewportType)
{
	FEditorViewportClient::SetViewportType(InViewportType);
	FocusViewportOnPreviewMesh();
}

void FAnimationViewportClient::RotateViewportType()
{
	FEditorViewportClient::RotateViewportType();
	FocusViewportOnPreviewMesh();
}

bool FAnimationViewportClient::InputKey( FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad )
{
	const int32 HitX = InViewport->GetMouseX();
	const int32 HitY = InViewport->GetMouseY();

	bool bHandled = false;

	
	// Handle switching modes - only allowed when not already manipulating
	if ( (Event == IE_Pressed) && (Key == EKeys::SpaceBar) && !bManipulating )
	{
		int32 SelectedBone = FindSelectedBone();
		USkeletalMeshSocket* SelectedSocket = FindSelectedSocket();
		TWeakObjectPtr<AWindDirectionalSource> SelectedWind = FindSelectedWindActor();

		if (SelectedBone >= 0 || SelectedSocket || SelectedWind.IsValid())
		{
			if (SelectedSkelControlAnimGraph.IsValid())
			{
				WidgetMode = (FWidget::EWidgetMode)SelectedSkelControlAnimGraph->ChangeToNextWidgetMode(PreviewSkelMeshComp.Get(), WidgetMode);
				if (WidgetMode == FWidget::WM_Scale)
				{
					SetWidgetCoordSystemSpace(COORD_Local);
				}
				else
				{
					SetWidgetCoordSystemSpace(COORD_World);
				}
			}
			else
			if (WidgetMode == FWidget::WM_Rotate)
			{
				WidgetMode = FWidget::WM_Scale;
			}
			else if (WidgetMode == FWidget::WM_Scale)
			{
				WidgetMode = FWidget::WM_Translate;
			}
			else
			{
				WidgetMode = FWidget::WM_Rotate;
			}

			//@TODO: ANIMATION: Add scaling support
		}
		bHandled = true;
		Invalidate();
	}

	if( (Event == IE_Pressed) && (Key == EKeys::F) )
	{
		bHandled = true;
		FocusViewportOnPreviewMesh();
	}

	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	bHandled |= AdvancedScene->HandleInputKey(InViewport, ControllerId, Key, Event, AmountDepressed, bGamepad);

	// Pass keys to standard controls, if we didn't consume input
	return (bHandled)
		? true
		: FEditorViewportClient::InputKey(InViewport,  ControllerId,  Key,  Event,  AmountDepressed,  bGamepad);
}

bool FAnimationViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples /*= 1*/, bool bGamepad /*= false*/)
{
	bool bResult = true;

	if (!bDisableInput)
	{
		FAdvancedPreviewScene* AdvancedScene = (FAdvancedPreviewScene*)PreviewScene;
		bResult = AdvancedScene->HandleViewportInput(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
	}

	return bResult;
}

void FAnimationViewportClient::SetWidgetMode(FWidget::EWidgetMode InMode)
{
	bool bAnimBPMode = PersonaPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode);

	// support WER keys to change gizmo mode for Bone Controller preivew in anim blueprint
	if (bAnimBPMode)
	{
		if (!bManipulating)
		{
			int32 SelectedBone = FindSelectedBone();
			if (SelectedBone >= 0 && SelectedSkelControlAnimGraph.IsValid())
			{
				if (SelectedSkelControlAnimGraph->SetWidgetMode(PreviewSkelMeshComp.Get(), InMode))
				{
					WidgetMode = InMode;
				}
			}
		}
	}
	else
	{
		WidgetMode = InMode;
	}

	// Force viewport to redraw
	Viewport->Invalidate();
}

bool FAnimationViewportClient::CanSetWidgetMode(FWidget::EWidgetMode NewMode) const
{
	return true;
}

void FAnimationViewportClient::SetLocalAxesMode(ELocalAxesMode::Type AxesMode)
{
	LocalAxesMode = AxesMode;
	ConfigOption->SetDefaultLocalAxesSelection( AxesMode );
}

bool FAnimationViewportClient::IsLocalAxesModeSet( ELocalAxesMode::Type AxesMode ) const
{
	return LocalAxesMode == AxesMode;
}

void FAnimationViewportClient::SetBoneDrawMode(EBoneDrawMode::Type AxesMode)
{
	BoneDrawMode = AxesMode;
	ConfigOption->SetDefaultBoneDrawSelection(AxesMode);
}

bool FAnimationViewportClient::IsBoneDrawModeSet(EBoneDrawMode::Type AxesMode) const
{
	return BoneDrawMode == AxesMode;
}

void FAnimationViewportClient::DrawBonesFromTransforms(TArray<FTransform>& Transforms, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, FLinearColor BoneColour, FLinearColor RootBoneColour) const
{
	if ( Transforms.Num() > 0 )
	{
		TArray<FTransform> WorldTransforms;
		WorldTransforms.AddUninitialized(Transforms.Num());

		TArray<FLinearColor> BoneColours;
		BoneColours.AddUninitialized(Transforms.Num());

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for ( int32 Index=0; Index < MeshComponent->RequiredBones.Num(); ++Index )
		{
			const int32 BoneIndex = MeshComponent->RequiredBones[Index];
			const int32 ParentIndex = MeshComponent->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);

			WorldTransforms[BoneIndex] = Transforms[BoneIndex] * MeshComponent->ComponentToWorld;
			BoneColours[BoneIndex] = (ParentIndex >= 0) ? BoneColour : RootBoneColour;
		}

		DrawBones(MeshComponent, MeshComponent->RequiredBones, WorldTransforms, PDI, BoneColours);
	}
}

void FAnimationViewportClient::DrawBonesFromCompactPose(const FCompactHeapPose& Pose, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, const FLinearColor& DrawColour) const
{
	if (Pose.GetNumBones() > 0)
	{
		TArray<FTransform> WorldTransforms;
		WorldTransforms.AddUninitialized(Pose.GetBoneContainer().GetNumBones());

		TArray<FLinearColor> BoneColours;
		BoneColours.AddUninitialized(Pose.GetBoneContainer().GetNumBones());

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for (FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
		{
			FMeshPoseBoneIndex MeshBoneIndex = Pose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex);

			int32 ParentIndex = Pose.GetBoneContainer().GetParentBoneIndex(MeshBoneIndex.GetInt());

			if (ParentIndex == INDEX_NONE)
			{
				WorldTransforms[MeshBoneIndex.GetInt()] = Pose[BoneIndex] * MeshComponent->ComponentToWorld;
			}
			else
			{
				WorldTransforms[MeshBoneIndex.GetInt()] = Pose[BoneIndex] * WorldTransforms[ParentIndex];
			}
			BoneColours[MeshBoneIndex.GetInt()] = DrawColour;
		}

		DrawBones(MeshComponent, MeshComponent->RequiredBones, WorldTransforms, PDI, BoneColours, 1.0f, true);
	}
}

void FAnimationViewportClient::DrawMeshBonesUncompressedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if ( MeshComponent && MeshComponent->SkeletalMesh )
	{
		DrawBonesFromTransforms(MeshComponent->UncompressedSpaceBases, MeshComponent, PDI, FColor(255, 127, 39, 255), FColor(255, 127, 39, 255));
	}
}

void FAnimationViewportClient::DrawMeshBonesNonRetargetedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if ( MeshComponent && MeshComponent->SkeletalMesh )
	{
		DrawBonesFromTransforms(MeshComponent->NonRetargetedSpaceBases, MeshComponent, PDI, FColor(159, 159, 39, 255), FColor(159, 159, 39, 255));
	}
}

void FAnimationViewportClient::DrawMeshBonesAdditiveBasePose(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if ( MeshComponent && MeshComponent->SkeletalMesh )
	{
		DrawBonesFromTransforms(MeshComponent->AdditiveBasePoses, MeshComponent, PDI, FColor(0, 159, 0, 255), FColor(0, 159, 0, 255));
	}
}

void FAnimationViewportClient::DrawMeshBonesSourceRawAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if(MeshComponent && MeshComponent->SkeletalMesh)
	{
		DrawBonesFromTransforms(MeshComponent->SourceAnimationPoses, MeshComponent, PDI, FColor(195, 195, 195, 255), FColor(195, 159, 195, 255));
	}
}

void FAnimationViewportClient::DrawWatchedPoses(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI)
{
	if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
	{
		if (UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintGeneratedClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == AnimInstance)
				{
					for (const FAnimNodePoseWatch& AnimNodePoseWatch : AnimBlueprintGeneratedClass->GetAnimBlueprintDebugData().AnimNodePoseWatch)
					{
						DrawBonesFromCompactPose(*AnimNodePoseWatch.PoseInfo.Get(), MeshComponent, PDI, AnimNodePoseWatch.PoseDrawColour);
					}
				}
			}
		}
	}
}

void FAnimationViewportClient::DrawMeshBonesBakedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if(MeshComponent && MeshComponent->SkeletalMesh)
	{
		DrawBonesFromTransforms(MeshComponent->BakedAnimationPoses, MeshComponent, PDI, FColor(0, 128, 192, 255), FColor(0, 128, 192, 255));
	}
}

void FAnimationViewportClient::DrawMeshBones(USkeletalMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const
{
	if ( MeshComponent && MeshComponent->SkeletalMesh )
	{
		TArray<FTransform> WorldTransforms;
		WorldTransforms.AddUninitialized(MeshComponent->GetNumComponentSpaceTransforms());

		TArray<FLinearColor> BoneColours;
		BoneColours.AddUninitialized(MeshComponent->GetNumComponentSpaceTransforms());

		TArray<int32> SelectedBones;
		if(UDebugSkelMeshComponent* DebugMeshComponent = Cast<UDebugSkelMeshComponent>(MeshComponent))
		{
			SelectedBones = DebugMeshComponent->BonesOfInterest;
		}

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for ( int32 Index=0; Index<MeshComponent->RequiredBones.Num(); ++Index )
		{
			const int32 BoneIndex = MeshComponent->RequiredBones[Index];
			const int32 ParentIndex = MeshComponent->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);

			WorldTransforms[BoneIndex] = MeshComponent->GetComponentSpaceTransforms()[BoneIndex] * MeshComponent->ComponentToWorld;
			
			if(SelectedBones.Contains(BoneIndex))
			{
				BoneColours[BoneIndex] = FLinearColor(1.0f, 0.34f, 0.0f, 1.0f);
			}
			else
			{
				BoneColours[BoneIndex] = (ParentIndex >= 0) ? FLinearColor::White : FLinearColor::Red;
			}
		}

		DrawBones(MeshComponent, MeshComponent->RequiredBones, WorldTransforms, PDI, BoneColours);
	}
}

void FAnimationViewportClient::DrawBones(const USkeletalMeshComponent* MeshComponent, const TArray<FBoneIndexType> & RequiredBones, const TArray<FTransform> & WorldTransforms, FPrimitiveDrawInterface* PDI, const TArray<FLinearColor>& BoneColours, float LineThickness/*=0.f*/, bool bForceDraw/*=false*/) const
{
	check ( MeshComponent && MeshComponent->SkeletalMesh );

	TArray<int32> SelectedBones;
	if(const UDebugSkelMeshComponent* DebugMeshComponent = Cast<const UDebugSkelMeshComponent>(MeshComponent))
	{
		SelectedBones = DebugMeshComponent->BonesOfInterest;

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for ( int32 Index=0; Index<RequiredBones.Num(); ++Index )
		{
			const int32 BoneIndex = RequiredBones[Index];

			if (bForceDraw ||
				(BoneDrawMode == EBoneDrawMode::All) ||
				((BoneDrawMode == EBoneDrawMode::Selected) && SelectedBones.Contains(BoneIndex) )
				)
			{
				const int32 ParentIndex = MeshComponent->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
				FVector Start, End;
				FLinearColor LineColor = BoneColours[BoneIndex];

				if (ParentIndex >= 0)
				{
					Start = WorldTransforms[ParentIndex].GetLocation();
					End = WorldTransforms[BoneIndex].GetLocation();
				}
				else
				{
					Start = FVector::ZeroVector;
					End = WorldTransforms[BoneIndex].GetLocation();
				}

				static const float SphereRadius = 1.0f;
				TArray<FVector> Verts;

				//Calc cone size 
				FVector EndToStart = (Start - End);
				float ConeLength = EndToStart.Size();
				float Angle = FMath::RadiansToDegrees(FMath::Atan(SphereRadius / ConeLength));

				//Render Sphere for bone end point and a cone between it and its parent.
				PDI->SetHitProxy(new HPersonaBoneProxy(MeshComponent->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex)));
				DrawWireSphere(PDI, End, LineColor, SphereRadius, 10, SDPG_Foreground);
				DrawWireCone(PDI, Verts, FRotationMatrix::MakeFromX(EndToStart)*FTranslationMatrix(End), ConeLength, Angle, 4, LineColor, SDPG_Foreground);
				PDI->SetHitProxy(NULL);

				// draw gizmo
				if ((LocalAxesMode == ELocalAxesMode::All) ||
					((LocalAxesMode == ELocalAxesMode::Selected) && SelectedBones.Contains(BoneIndex))
					)
				{
					RenderGizmo(WorldTransforms[BoneIndex], PDI);
				}
			}
		}
	}
}

void FAnimationViewportClient::RenderGizmo(const FTransform& Transform, FPrimitiveDrawInterface* PDI) const
{
	// Display colored coordinate system axes for this joint.
	const float AxisLength = 3.75f;
	const float LineThickness = 0.3f;
	const FVector Origin = Transform.GetLocation();

	// Red = X
	FVector XAxis = Transform.TransformVector( FVector(1.0f,0.0f,0.0f) );
	XAxis.Normalize();
	PDI->DrawLine( Origin, Origin + XAxis * AxisLength, FColor( 255, 80, 80),SDPG_Foreground, LineThickness);			

	// Green = Y
	FVector YAxis = Transform.TransformVector( FVector(0.0f,1.0f,0.0f) );
	YAxis.Normalize();
	PDI->DrawLine( Origin, Origin + YAxis * AxisLength, FColor( 80, 255, 80),SDPG_Foreground, LineThickness); 

	// Blue = Z
	FVector ZAxis = Transform.TransformVector( FVector(0.0f,0.0f,1.0f) );
	ZAxis.Normalize();
	PDI->DrawLine( Origin, Origin + ZAxis * AxisLength, FColor( 80, 80, 255),SDPG_Foreground, LineThickness); 
}

void FAnimationViewportClient::DrawMeshSubsetBones(const USkeletalMeshComponent* MeshComponent, const TArray<int32>& BonesOfInterest, FPrimitiveDrawInterface* PDI) const
{
	// this BonesOfInterest has to be in MeshComponent base, not Skeleton 
	if ( MeshComponent && MeshComponent->SkeletalMesh && BonesOfInterest.Num() > 0 )
	{
		TArray<FTransform> WorldTransforms;
		WorldTransforms.AddUninitialized(MeshComponent->GetNumComponentSpaceTransforms());

		TArray<FLinearColor> BoneColours;
		BoneColours.AddUninitialized(MeshComponent->GetNumComponentSpaceTransforms());

		TArray<FBoneIndexType> RequiredBones;

		const FReferenceSkeleton& RefSkeleton = MeshComponent->SkeletalMesh->RefSkeleton;

		static const FName SelectionColorName("SelectionColor");

		const FSlateColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName);
		const FLinearColor LinearSelectionColor( SelectionColor.IsColorSpecified() ? SelectionColor.GetSpecifiedColor() : FLinearColor::White );

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for ( auto Iter = MeshComponent->RequiredBones.CreateConstIterator(); Iter; ++Iter)
		{
			const int32 BoneIndex = *Iter;
			bool bDrawBone = false;

			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);

			// need to see if it's child of any of Bones of interest
			for (auto SubIter=BonesOfInterest.CreateConstIterator(); SubIter; ++SubIter )
			{
				const int32 SubBoneIndex = *SubIter;
				// if I'm child of the BonesOfInterest
				if(BoneIndex == SubBoneIndex)
				{
					//found a bone we are interested in
					if(ParentIndex >= 0)
					{
						WorldTransforms[ParentIndex] = MeshComponent->GetComponentSpaceTransforms()[ParentIndex]*MeshComponent->ComponentToWorld;
					}
					BoneColours[BoneIndex] = LinearSelectionColor;
					bDrawBone = true;
					break;
				}
				else if ( RefSkeleton.BoneIsChildOf(BoneIndex, SubBoneIndex) )
				{
					BoneColours[BoneIndex] = FLinearColor::White;
					bDrawBone = true;
					break;
				}
			}

			if (bDrawBone)
			{
				//add to the list
				RequiredBones.AddUnique(BoneIndex);
				WorldTransforms[BoneIndex] = MeshComponent->GetComponentSpaceTransforms()[BoneIndex] * MeshComponent->ComponentToWorld;
			}
		}

		DrawBones(MeshComponent, RequiredBones, WorldTransforms, PDI, BoneColours, 0.3f);
	}
}

void FAnimationViewportClient::DrawSockets( TArray<USkeletalMeshSocket*>& Sockets, FPrimitiveDrawInterface* PDI, bool bUseSkeletonSocketColor ) const
{
	if ( const UDebugSkelMeshComponent* DebugMeshComponent = PreviewSkelMeshComp.Get() )
	{
		TArray<FSelectedSocketInfo> SelectedSockets;
		SelectedSockets = DebugMeshComponent->SocketsOfInterest;

		for ( auto SocketIt = Sockets.CreateConstIterator(); SocketIt; ++SocketIt )
		{
			USkeletalMeshSocket* Socket = *(SocketIt);
			FReferenceSkeleton& RefSkeleton = DebugMeshComponent->SkeletalMesh->RefSkeleton;

			const int32 ParentIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);

			FTransform WorldTransformSocket = Socket->GetSocketTransform(DebugMeshComponent);

			FLinearColor SocketColor;
			FVector Start, End;
			if (ParentIndex >=0)
			{
				FTransform WorldTransformParent = DebugMeshComponent->GetComponentSpaceTransforms()[ParentIndex]*DebugMeshComponent->ComponentToWorld;
				Start = WorldTransformParent.GetLocation();
				End = WorldTransformSocket.GetLocation();
			}
			else
			{
				Start = FVector::ZeroVector;
				End = WorldTransformSocket.GetLocation();
			}

			const bool bSelectedSocket = SelectedSockets.ContainsByPredicate([Socket](const FSelectedSocketInfo& Info){ return Info.Socket == Socket; });

			if( bSelectedSocket )
			{
				SocketColor = FLinearColor(1.0f, 0.34f, 0.0f, 1.0f);
			}
			else
			{
				SocketColor = (ParentIndex >= 0) ? FLinearColor::White : FLinearColor::Red;
			}

			static const float SphereRadius = 1.0f;
			TArray<FVector> Verts;

			//Calc cone size 
			FVector EndToStart = (Start-End);
			float ConeLength = EndToStart.Size();
			float Angle = FMath::RadiansToDegrees(FMath::Atan(SphereRadius / ConeLength));

			//Render Sphere for bone end point and a cone between it and its parent.
			PDI->SetHitProxy( new HPersonaBoneProxy( Socket->BoneName ) );
			PDI->DrawLine( Start, End, SocketColor, SDPG_Foreground );
			PDI->SetHitProxy( NULL );
			
			// draw gizmo
			if( (LocalAxesMode == ELocalAxesMode::All) || bSelectedSocket )
			{
				FMatrix SocketMatrix;
				Socket->GetSocketMatrix( SocketMatrix, PreviewSkelMeshComp.Get() );

				PDI->SetHitProxy( new HPersonaSocketProxy( FSelectedSocketInfo( Socket, bUseSkeletonSocketColor ) ) );
				DrawWireDiamond( PDI, SocketMatrix, 2.f, SocketColor, SDPG_Foreground );
				PDI->SetHitProxy( NULL );
				
				RenderGizmo(FTransform(SocketMatrix), PDI);
			}
		}
	}
}

FSphere FAnimationViewportClient::GetCameraTarget()
{
	bool bFoundTarget = false;
	FSphere Sphere(FVector(0,0,0), 100.0f); // default
	if( PreviewSkelMeshComp.IsValid() )
	{
		FTransform ComponentToWorld = PreviewSkelMeshComp->ComponentToWorld;
		PreviewSkelMeshComp.Get()->CalcBounds(ComponentToWorld);

		if( PreviewSkelMeshComp->BonesOfInterest.Num() > 0 )
		{
			const int32 FocusBoneIndex = PreviewSkelMeshComp->BonesOfInterest[0];
			if( FocusBoneIndex != INDEX_NONE )
			{
				const FName BoneName = PreviewSkelMeshComp->SkeletalMesh->RefSkeleton.GetBoneName(FocusBoneIndex);
				Sphere.Center = PreviewSkelMeshComp->GetBoneLocation(BoneName);
				Sphere.W = 30.0f;
				bFoundTarget = true;
			}
		}


		if( !bFoundTarget && (PreviewSkelMeshComp->SocketsOfInterest.Num() > 0) )
		{
			USkeletalMeshSocket * Socket = PreviewSkelMeshComp->SocketsOfInterest[0].Socket;
			if( Socket )
			{
				Sphere.Center = Socket->GetSocketLocation( PreviewSkelMeshComp.Get() );
				Sphere.W = 30.0f;
				bFoundTarget = true;
			}
		}

		if( !bFoundTarget )
		{
			FBoxSphereBounds Bounds = PreviewSkelMeshComp.Get()->CalcBounds(FTransform::Identity);
			Sphere = Bounds.GetSphere();
		}
	}
	return Sphere;
}

void FAnimationViewportClient::UpdateCameraSetup()
{
	static FRotator CustomOrbitRotation(-33.75, -135, 0);
	if (PreviewSkelMeshComp.IsValid() && PreviewSkelMeshComp->SkeletalMesh)
	{
		FSphere BoundSphere = GetCameraTarget();
		FVector CustomOrbitZoom(0, BoundSphere.W / (75.0f * (float)PI / 360.0f), 0);
		FVector CustomOrbitLookAt = BoundSphere.Center;

		SetCameraSetup(CustomOrbitLookAt, CustomOrbitRotation, CustomOrbitZoom, CustomOrbitLookAt, GetViewLocation(), GetViewRotation() );
		
		// Move the floor to the bottom of the bounding box of the mesh, rather than on the origin
		FVector Bottom = PreviewSkelMeshComp->Bounds.GetBoxExtrema(0);
		const float FloorOffset = GetFloorOffset() + (bAutoAlignFloor ? -Bottom.Z : 0.0f);
		AdvancedPreviewScene->SetFloorOffset(FloorOffset);
	}
}

void FAnimationViewportClient::FocusViewportOnSphere( FSphere& Sphere )
{
	FBox Box( Sphere.Center - FVector(Sphere.W, 0.0f, 0.0f), Sphere.Center + FVector(Sphere.W, 0.0f, 0.0f) );

	bool bInstant = false;
	FocusViewportOnBox( Box, bInstant );

	Invalidate();
}

void FAnimationViewportClient::FocusViewportOnPreviewMesh()
{
	FIntPoint ViewportSize = Viewport->GetSizeXY();

	if(!(ViewportSize.SizeSquared() > 0))
	{
		// We cannot focus fully right now as the viewport does not know its size
		// and we must have the aspect to correctly focus on the component,
		bFocusOnDraw = true;
	}
	FSphere Sphere = GetCameraTarget();
	FocusViewportOnSphere(Sphere);
}

float FAnimationViewportClient::GetFloorOffset() const
{
	if ( PersonaPtr.IsValid() )
	{
		USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

		if ( Mesh )
		{
			return Mesh->FloorOffset;
		}
	}

	return 0.0f;
}

void FAnimationViewportClient::SetFloorOffset( float NewValue )
{
	if ( PersonaPtr.IsValid() )
	{
		USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

		if ( Mesh )
		{
			// This value is saved in a UPROPERTY for the mesh, so changes are transactional
			FScopedTransaction Transaction( LOCTEXT( "SetFloorOffset", "Set Floor Offset" ) );
			Mesh->Modify();

			Mesh->FloorOffset = NewValue;
			UpdateCameraSetup(); // This does the actual moving of the floor mesh
			Invalidate();
		}
	}
}

TWeakObjectPtr<AWindDirectionalSource> FAnimationViewportClient::CreateWindActor(UWorld* World)
{
	TWeakObjectPtr<AWindDirectionalSource> Wind = World->SpawnActor<AWindDirectionalSource>(PrevWindLocation, PrevWindRotation);

	check(Wind.IsValid());
	//initial wind strength value 
	Wind->GetComponent()->Strength = PrevWindStrength;
	return Wind;
}

void FAnimationViewportClient::EnableWindActor(bool bEnableWind)
{
	UWorld* World = PersonaPtr.Pin()->PreviewComponent->GetWorld();
	check(World);

	if(bEnableWind)
	{
		if(!WindSourceActor.IsValid())
		{
			WindSourceActor = CreateWindActor(World);
		}
	}
	else if(WindSourceActor.IsValid())
	{
		PrevWindLocation = WindSourceActor->GetActorLocation();
		PrevWindRotation = WindSourceActor->GetActorRotation();
		PrevWindStrength = WindSourceActor->GetComponent()->Strength;

		if(World->DestroyActor(WindSourceActor.Get()))
		{
			//clear the gizmo (translation or rotation)
			PersonaPtr.Pin()->DeselectAll();
		}
	}
}

void FAnimationViewportClient::SetWindStrength( float SliderPos )
{
	if(WindSourceActor.IsValid())
	{
		//Clamp grid size slider value between 0 - 1
		WindSourceActor->GetComponent()->Strength = FMath::Clamp<float>(SliderPos, 0.0f, 1.0f);
		//to apply this new wind strength
		WindSourceActor->UpdateComponentTransforms();
	}
}

float FAnimationViewportClient::GetWindStrengthSliderValue() const
{
	if(WindSourceActor.IsValid())
	{
		return WindSourceActor->GetComponent()->Strength;
	}

	return 0;
}

FText FAnimationViewportClient::GetWindStrengthLabel() const
{
	//Clamp slide value so that minimum value displayed is 0.00 and maximum is 1.0
	float SliderValue = FMath::Clamp<float>(GetWindStrengthSliderValue(), 0.0f, 1.0f);

	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::AsNumber(SliderValue, &FormatOptions);
}

void FAnimationViewportClient::SetGravityScale( float SliderPos )
{
	GravityScaleSliderValue = SliderPos;
	float RealGravityScale = SliderPos * 4;

	check(PersonaPtr.Pin()->PreviewComponent);

	UWorld* World = PersonaPtr.Pin()->PreviewComponent->GetWorld();
	AWorldSettings* Setting = World->GetWorldSettings();
	Setting->WorldGravityZ = UPhysicsSettings::Get()->DefaultGravityZ*RealGravityScale;
	Setting->bWorldGravitySet = true;
}

float FAnimationViewportClient::GetGravityScaleSliderValue() const
{
	return GravityScaleSliderValue;
}

FText FAnimationViewportClient::GetGravityScaleLabel() const
{
	//Clamp slide value so that minimum value displayed is 0.00 and maximum is 4.0
	float SliderValue = FMath::Clamp<float>(GetGravityScaleSliderValue()*4, 0.0f, 4.0f);

	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::AsNumber(SliderValue, &FormatOptions);
}

void FAnimationViewportClient::ToggleCPUSkinning()
{
	if (PreviewSkelMeshComp.IsValid())
	{
		PreviewSkelMeshComp->bCPUSkinning = !PreviewSkelMeshComp->bCPUSkinning;
		PreviewSkelMeshComp->MarkRenderStateDirty();
		Invalidate();
	}
}

bool FAnimationViewportClient::IsSetCPUSkinningChecked() const
{
	if (PreviewSkelMeshComp.IsValid())
	{
		return PreviewSkelMeshComp->bCPUSkinning;
	}
	return false;
}

void FAnimationViewportClient::ToggleShowNormals()
{
	if(PreviewSkelMeshComp.IsValid())
	{
		PreviewSkelMeshComp->bDrawNormals = !PreviewSkelMeshComp->bDrawNormals;
		PreviewSkelMeshComp->MarkRenderStateDirty();
		Invalidate();
	}
}

bool FAnimationViewportClient::IsSetShowNormalsChecked() const
{
	if (PreviewSkelMeshComp.IsValid())
	{
		return PreviewSkelMeshComp->bDrawNormals;
	}
	return false;
}

void FAnimationViewportClient::ToggleShowTangents()
{
	if(PreviewSkelMeshComp.IsValid())
	{
		PreviewSkelMeshComp->bDrawTangents = !PreviewSkelMeshComp->bDrawTangents;
		PreviewSkelMeshComp->MarkRenderStateDirty();
		Invalidate();
	}
}

bool FAnimationViewportClient::IsSetShowTangentsChecked() const
{
	if (PreviewSkelMeshComp.IsValid())
	{
		return PreviewSkelMeshComp->bDrawTangents;
	}
	return false;
}

void FAnimationViewportClient::ToggleShowBinormals()
{
	if(PreviewSkelMeshComp.IsValid())
	{
		PreviewSkelMeshComp->bDrawBinormals = !PreviewSkelMeshComp->bDrawBinormals;
		PreviewSkelMeshComp->MarkRenderStateDirty();
		Invalidate();
	}
}

bool FAnimationViewportClient::IsSetShowBinormalsChecked() const
{
	if (PreviewSkelMeshComp.IsValid())
	{
		return PreviewSkelMeshComp->bDrawBinormals;
	}
	return false;
}

void FAnimationViewportClient::ToggleDrawUVOverlay()
{
	bDrawUVs = !bDrawUVs;
	Invalidate();
}

bool FAnimationViewportClient::IsSetDrawUVOverlayChecked() const
{
	return bDrawUVs;
}

void FAnimationViewportClient::OnSetShowMeshStats(int32 ShowMode)
{
	ConfigOption->SetShowMeshStats(ShowMode);
}

bool FAnimationViewportClient::IsShowingMeshStats() const
{
	const bool bShouldBeEnabled = ConfigOption->ShowMeshStats != EDisplayInfoMode::None;
	const bool bCanBeEnabled = !PersonaPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode);

	return bShouldBeEnabled && bCanBeEnabled;
}

bool FAnimationViewportClient::IsShowingSelectedNodeStats() const
{
	return PersonaPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode) && ConfigOption->ShowMeshStats == EDisplayInfoMode::SkeletalControls;
}

bool FAnimationViewportClient::IsDetailedMeshStats() const
{
	return ConfigOption->ShowMeshStats == EDisplayInfoMode::Detailed;
}

int32 FAnimationViewportClient::GetShowMeshStats() const
{
	return ConfigOption->ShowMeshStats;
}

void FAnimationViewportClient::OnAssetViewerSettingsChanged(const FName& InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bPostProcessingEnabled))
	{
		SetAdvancedShowFlagsForScene();
	}
}

void FAnimationViewportClient::SetAdvancedShowFlagsForScene()
{
	const bool bAdvancedShowFlags = UAssetViewerSettings::Get()->Profiles[AdvancedPreviewScene->GetCurrentProfileIndex()].bPostProcessingEnabled;
	if (bAdvancedShowFlags)
	{
		EngineShowFlags.EnableAdvancedFeatures();
	}
	else
	{
		EngineShowFlags.DisableAdvancedFeatures();
	}
}

#undef LOCTEXT_NAMESPACE

