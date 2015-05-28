// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "AnimMontageSegmentDetails.h"
#include "AnimPreviewInstance.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "Editor/KismetWidgets/Public/SScrubControlPanel.h"
#define LOCTEXT_NAMESPACE "AnimMontageSegmentDetails"

/////////////////////////////////////////////////////////////////////////
FAnimationSegmentViewportClient::FAnimationSegmentViewportClient(FPreviewScene& InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(nullptr, &InPreviewScene, InEditorViewportWidget)
{
	SetViewMode(VMI_Lit);

	// Always composite editor objects after post processing in the editor
	EngineShowFlags.CompositeEditorPrimitives = true;
	EngineShowFlags.DisableAdvancedFeatures();
	
	UpdateLighting();

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(70, 70, 70);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor =  FColor(20, 20, 20);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
}


void FAnimationSegmentViewportClient::UpdateLighting()
{
	const UDestructableMeshEditorSettings* Options = GetDefault<UDestructableMeshEditorSettings>();

	PreviewScene->SetLightDirection(Options->AnimPreviewLightingDirection);
	PreviewScene->GetScene()->UpdateDynamicSkyLight(Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewSkyColor), Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewFloorColor));
	PreviewScene->SetLightColor(Options->AnimPreviewDirectionalColor);
	PreviewScene->SetLightBrightness(Options->AnimPreviewLightBrightness);
}


FSceneInterface* FAnimationSegmentViewportClient::GetScene() const
{
	return PreviewScene->GetScene();
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


TSharedRef<IDetailCustomization> FAnimMontageSegmentDetails::MakeInstance()
{
	return MakeShareable( new FAnimMontageSegmentDetails );
}

void FAnimMontageSegmentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& SegmentCategory = DetailBuilder.EditCategory("Animation Segment", LOCTEXT("AnimationSegmentCategoryTitle", "Animation Segment") );

	SegmentCategory.AddProperty("AnimSegment.AnimReference").DisplayName( LOCTEXT("AnimationReferenceLabel", "Animation Reference") );

	SegmentCategory.AddProperty("AnimSegment.AnimStartTime").DisplayName( LOCTEXT("StartTimeLabel", "Start Time") );
	SegmentCategory.AddProperty("AnimSegment.AnimEndTime").DisplayName( LOCTEXT("EndTimeLabel", "End Time") );

	SegmentCategory.AddProperty("AnimSegment.AnimPlayRate").DisplayName( LOCTEXT("PlayRateLabel", "Play Rate") );
	SegmentCategory.AddProperty("AnimSegment.LoopingCount").DisplayName( LOCTEXT("LoopCountLabel", "Loop Count") );

	TSharedPtr<IPropertyHandle> InPropertyHandle = DetailBuilder.GetProperty("AnimSegment.AnimReference");
	UObject *Object = NULL;
	InPropertyHandle->GetValue(Object);

	UAnimSequenceBase *AnimRef = Cast<UAnimSequenceBase>(Object);
	USkeleton *Skeleton = NULL;
	if(AnimRef != NULL)
	{
		Skeleton = AnimRef->GetSkeleton();
	}

	SegmentCategory.AddCustomRow(FText::GetEmpty(), false)
	[
		SNew(SAnimationSegmentViewport)
		.Skeleton(Skeleton)
		.AnimRef(AnimRef)
		.AnimRefPropertyHandle(DetailBuilder.GetProperty("AnimSegment.AnimReference"))
		.StartTimePropertyHandle(DetailBuilder.GetProperty("AnimSegment.AnimStartTime"))
		.EndTimePropertyHandle(DetailBuilder.GetProperty("AnimSegment.AnimEndTime"))
	];	
}


/////////////////////////////////////////////////
////////////////////////////////////////////////

SAnimationSegmentViewport::~SAnimationSegmentViewport()
{
	// clean up components
	if (PreviewComponent)
	{
		for (int32 I=PreviewComponent->AttachChildren.Num()-1; I >= 0; --I) // Iterate backwards because Cleancomponent will remove from AttachChildren
		{
			// PreviewComponet will be cleaned up by PreviewScene, 
			// but if anything is attached, it won't be cleaned up, 
			// so we'll need to clean them up manually
			CleanupComponent(PreviewComponent->AttachChildren[I]);
		}

		PreviewComponent->AttachChildren.Empty();
	}

	// Close viewport
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = NULL;
	}
}

void SAnimationSegmentViewport::CleanupComponent(USceneComponent* Component)
{
	if (Component)
	{
		for (int32 I=0; I<Component->AttachChildren.Num(); ++I)
		{
			CleanupComponent(Component->AttachChildren[I]);
		}

		Component->AttachChildren.Empty();
		Component->DestroyComponent();
	}
}

SAnimationSegmentViewport::SAnimationSegmentViewport()
	: PreviewScene(FPreviewScene::ConstructionValues())
{
}

void SAnimationSegmentViewport::Construct(const FArguments& InArgs)
{
	TargetSkeleton = InArgs._Skeleton;
	AnimRef = InArgs._AnimRef;
	
	AnimRefPropertyHandle = InArgs._AnimRefPropertyHandle;
	StartTimePropertyHandle = InArgs._StartTimePropertyHandle;
	EndTimePropertyHandle = InArgs._EndTimePropertyHandle;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SAssignNew(Description, STextBlock)
			.Text(LOCTEXT("DefaultViewportLabel", "Default View"))
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]

		+SVerticalBox::Slot()
		.FillHeight(1)
		.HAlign(HAlign_Center)
		[
			SNew( SBorder )
			.HAlign(HAlign_Center)
			[
				SAssignNew( ViewportWidget, SViewport )
				.EnableGammaCorrection( false )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[			
			SNew(SAnimationSegmentScrubPanel)
			.ViewInputMin(this, &SAnimationSegmentViewport::GetViewMinInput)
			.ViewInputMax(this, &SAnimationSegmentViewport::GetViewMaxInput)
			.PreviewInstance(this, &SAnimationSegmentViewport::GetPreviewInstance)
			.DraggableBars(this, &SAnimationSegmentViewport::GetBars)
			.OnBarDrag(this, &SAnimationSegmentViewport::OnBarDrag)
			.OnTickPlayback(this, &SAnimationSegmentViewport::OnTickPreview)
			.bAllowZoom(true)
		]
	];
	

	// Create a viewport client
	LevelViewportClient	= MakeShareable( new FAnimationSegmentViewportClient(PreviewScene) );

	LevelViewportClient->ViewportType = LVT_Perspective;
	LevelViewportClient->bSetListenerPosition = false;
	LevelViewportClient->SetViewLocation( EditorViewportDefs::DefaultPerspectiveViewLocation );
	LevelViewportClient->SetViewRotation( EditorViewportDefs::DefaultPerspectiveViewRotation );	

	SceneViewport = MakeShareable( new FSceneViewport( LevelViewportClient.Get(), ViewportWidget) );
	LevelViewportClient->Viewport = SceneViewport.Get();
	LevelViewportClient->SetRealtime( true );
	LevelViewportClient->VisibilityDelegate.BindSP( this, &SAnimationSegmentViewport::IsVisible );
	LevelViewportClient->SetViewMode( VMI_Lit );

	ViewportWidget->SetViewportInterface( SceneViewport.ToSharedRef() );
	
	PreviewComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewComponent->bEnablePhysicsOnDedicatedServer = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);

	InitSkeleton();
}

void SAnimationSegmentViewport::InitSkeleton()
{
	UObject *Object = NULL;
	AnimRefPropertyHandle->GetValue(Object);
	UAnimSequenceBase *AnimSequence = Cast<UAnimSequenceBase>(Object);
	USkeleton *Skeleton = NULL;
	if(AnimSequence != NULL)
	{
		Skeleton = AnimSequence->GetSkeleton();
	}

	if( PreviewComponent != NULL && Skeleton != NULL )
	{
		USkeletalMesh* PreviewMesh = Skeleton->GetAssetPreviewMesh(AnimSequence);
		if (PreviewMesh)
		{
			UAnimSingleNodeInstance * Preview = PreviewComponent->PreviewInstance;
			if((Preview == NULL || Preview->CurrentAsset != AnimSequence) ||
				(PreviewComponent->SkeletalMesh != PreviewMesh))
			{
				PreviewComponent->SetSkeletalMesh(PreviewMesh);
				PreviewComponent->EnablePreview(true, AnimSequence, NULL);
				PreviewComponent->PreviewInstance->SetLooping(true);

				//Place the camera at a good viewer position
				FVector NewPosition = LevelViewportClient->GetViewLocation();
				NewPosition.Normalize();
				LevelViewportClient->SetViewLocation(NewPosition * (PreviewMesh->Bounds.SphereRadius*1.5f));
			}
		}
	}

	TargetSkeleton = Skeleton;
}

void SAnimationSegmentViewport::OnTickPreview( double InCurrentTime, float InDeltaTime )
{
	// Clamp the sequence playing to the start/end values of the anim segment
	float Start, End;
	StartTimePropertyHandle->GetValue( Start );
	EndTimePropertyHandle->GetValue( End );
	if ( PreviewComponent->PreviewInstance->CurrentTime > End || PreviewComponent->PreviewInstance->CurrentTime < Start )
	{
		PreviewComponent->PreviewInstance->SetPosition( Start, false );
	}

	LevelViewportClient->Invalidate();
}

void SAnimationSegmentViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	class UDebugSkelMeshComponent* Component = PreviewComponent;

	FString TargetSkeletonName = TargetSkeleton ? TargetSkeleton->GetName() : FName(NAME_None).ToString();

	if (Component != NULL)
	{
		// Reinit the skeleton if the anim ref has changed
		InitSkeleton();

		if (Component->IsPreviewOn())
		{
			Description->SetText(FText::Format( LOCTEXT("Previewing", "Previewing {0}"), FText::FromString(Component->GetPreviewText()) ));
		}
		else if (Component->AnimBlueprintGeneratedClass)
		{
			Description->SetText(FText::Format( LOCTEXT("Previewing", "Previewing {0}"), FText::FromString(Component->AnimBlueprintGeneratedClass->GetName()) ));
		}
		else if (Component->SkeletalMesh == NULL)
		{
			Description->SetText(FText::Format( LOCTEXT("NoMeshFound", "No skeletal mesh found for skeleton '{0}'"), FText::FromString(TargetSkeletonName) ));
		}
		else
		{
			Description->SetText(LOCTEXT("Default", "Default"));
		}

		Component->GetScene()->GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
	}
	else
	{
		Description->SetText(FText::Format( LOCTEXT("NoMeshFound", "No skeletal mesh found for skeleton '{0}'"), FText::FromString(TargetSkeletonName) ));
	}
}

void SAnimationSegmentViewport::RefreshViewport()
{
}

bool SAnimationSegmentViewport::IsVisible() const
{
	return ViewportWidget.IsValid();
}

float SAnimationSegmentViewport::GetViewMinInput() const 
{ 
	if (PreviewComponent != NULL)
	{
		if (PreviewComponent->PreviewInstance != NULL)
		{
			return 0.0f;
		}
		else if (PreviewComponent->AnimScriptInstance != NULL)
		{
			return FMath::Max<float>((float)(PreviewComponent->AnimScriptInstance->LifeTimer - 30.0), 0.0f);
		}
	}

	return 0.f; 
}

float SAnimationSegmentViewport::GetViewMaxInput() const 
{ 
	if (PreviewComponent != NULL)
	{
		if (PreviewComponent->PreviewInstance != NULL)
		{
			return PreviewComponent->PreviewInstance->GetLength();
		}
		else if (PreviewComponent->AnimScriptInstance != NULL)
		{
			return PreviewComponent->AnimScriptInstance->LifeTimer;
		}
	}

	return 0.f;
}

TArray<float> SAnimationSegmentViewport::GetBars() const
{
	float Start, End;
	StartTimePropertyHandle->GetValue(Start);
	EndTimePropertyHandle->GetValue(End);

	TArray<float> Bars;
	Bars.Add(Start);
	Bars.Add(End);

	return Bars;
}

void SAnimationSegmentViewport::OnBarDrag(int32 Index, float Position)
{
	if(Index==0)
	{
		StartTimePropertyHandle->SetValue(Position);
	}
	else if(Index==1)
	{
		EndTimePropertyHandle->SetValue(Position);
	}
}

UAnimSingleNodeInstance* SAnimationSegmentViewport::GetPreviewInstance() const
{
	return PreviewComponent ? PreviewComponent->PreviewInstance : NULL;
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
void SAnimationSegmentScrubPanel::Construct( const SAnimationSegmentScrubPanel::FArguments& InArgs )
{
	bSliderBeingDragged = false;
	PreviewInstance = InArgs._PreviewInstance;
	LockedSequence = InArgs._LockedSequence;

	this->ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill) 
			.VAlign(VAlign_Center)
			.FillWidth(1)
			.Padding( FMargin( 3.0f, 0.0f) )
			[
				SAssignNew(ScrubControlPanel, SScrubControlPanel)
				.IsEnabled(true)
				.Value(this, &SAnimationSegmentScrubPanel::GetScrubValue)
				.NumOfKeys(this, &SAnimationSegmentScrubPanel::GetNumOfFrames)
				.SequenceLength(this, &SAnimationSegmentScrubPanel::GetSequenceLength)
				.OnValueChanged(this, &SAnimationSegmentScrubPanel::OnValueChanged)
				.OnBeginSliderMovement(this, &SAnimationSegmentScrubPanel::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SAnimationSegmentScrubPanel::OnEndSliderMovement)
				.OnClickedForwardPlay(this, &SAnimationSegmentScrubPanel::OnClick_Forward)
				.OnGetPlaybackMode(this, &SAnimationSegmentScrubPanel::GetPlaybackMode)
				.ViewInputMin(InArgs._ViewInputMin)
				.ViewInputMax(InArgs._ViewInputMax)
				.bAllowZoom(InArgs._bAllowZoom)
				.IsRealtimeStreamingMode(this, &SAnimationSegmentScrubPanel::IsRealtimeStreamingMode)
				.DraggableBars(InArgs._DraggableBars)
				.OnBarDrag(InArgs._OnBarDrag)
				.OnTickPlayback(InArgs._OnTickPlayback)
			]
		];

}

SAnimationSegmentScrubPanel::~SAnimationSegmentScrubPanel()
{
	
}

FReply SAnimationSegmentScrubPanel::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance();
	if (PreviewInst)
	{
		bool bIsReverse = PreviewInst->bReverse;
		bool bIsPlaying = PreviewInst->bPlaying;
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInst->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying) 
		{
			PreviewInst->SetPlaying(false);
		}
		// if not playing, play forward
		else 
		{
			PreviewInst->SetReverse(false);
			PreviewInst->SetPlaying(true);
		}
	}

	return FReply::Handled();
}

EPlaybackMode::Type SAnimationSegmentScrubPanel::GetPlaybackMode() const
{
	UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance();
	if (PreviewInst && PreviewInst->bPlaying)
	{
		return PreviewInst->bReverse ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
	}
	return EPlaybackMode::Stopped;
}

bool SAnimationSegmentScrubPanel::IsRealtimeStreamingMode() const
{
	return GetPreviewInstance() == NULL;
}

void SAnimationSegmentScrubPanel::OnValueChanged(float NewValue)
{
	if (UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance())
	{
		PreviewInst->SetPosition(NewValue);
	}
}

// make sure viewport is freshes
void SAnimationSegmentScrubPanel::OnBeginSliderMovement()
{
	bSliderBeingDragged = true;

	if (UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance())
	{
		PreviewInst->SetPlaying(false);
	}
}

void SAnimationSegmentScrubPanel::OnEndSliderMovement(float NewValue)
{
	bSliderBeingDragged = false;
}

void SAnimationSegmentScrubPanel::AnimChanged(UAnimationAsset* AnimAsset)
{
}

uint32 SAnimationSegmentScrubPanel::GetNumOfFrames() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance();
		float Length = PreviewInst->GetLength();
		// if anim sequence, use correct num frames
		int32 NumFrames = (int32) (Length/0.0333f); 
		if (PreviewInst->CurrentAsset && PreviewInst->CurrentAsset->IsA(UAnimSequenceBase::StaticClass()))
		{
			NumFrames = CastChecked<UAnimSequenceBase>(PreviewInst->CurrentAsset)->GetNumberOfFrames();
		}
		return NumFrames;
	}
	else if (LockedSequence)
	{
		return LockedSequence->GetNumberOfFrames();
	}
	return 1;
}

float SAnimationSegmentScrubPanel::GetSequenceLength() const
{
	if (DoesSyncViewport())
	{
		return GetPreviewInstance()->GetLength();
	}
	else if (LockedSequence)
	{
		return LockedSequence->SequenceLength;
	}
	return 0.f;
}

bool SAnimationSegmentScrubPanel::DoesSyncViewport() const
{
	UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance();

	return (( LockedSequence==NULL && PreviewInst ) || ( LockedSequence && PreviewInst && PreviewInst->CurrentAsset == LockedSequence ));
}

class UAnimSingleNodeInstance* SAnimationSegmentScrubPanel::GetPreviewInstance() const
{
	return PreviewInstance.Get();
}

float SAnimationSegmentScrubPanel::GetScrubValue() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInst = GetPreviewInstance();
		if (PreviewInst)
		{
			return PreviewInst->CurrentTime; 
		}
	}
	return 0.f;
}

void SAnimationSegmentScrubPanel::ReplaceLockedSequence(class UAnimSequenceBase* NewLockedSequence)
{
	LockedSequence = NewLockedSequence;
}

#undef LOCTEXT_NAMESPACE
