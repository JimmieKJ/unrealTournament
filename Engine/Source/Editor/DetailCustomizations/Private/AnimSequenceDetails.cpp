// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "AnimSequenceDetails.h"
#include "AnimMontageSegmentDetails.h"
#include "AnimPreviewInstance.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "Editor/KismetWidgets/Public/SScrubControlPanel.h"

#define LOCTEXT_NAMESPACE	"AnimSequenceDetails"

// default name for retarget source
#define DEFAULT_RETARGET_SOURCE_NAME	TEXT("Default")

TSharedRef<IDetailCustomization> FAnimSequenceDetails::MakeInstance()
{
	return MakeShareable(new FAnimSequenceDetails);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FAnimSequenceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// retarget source handler
	IDetailCategoryBuilder& AnimationCategory = DetailBuilder.EditCategory("Animation");
	RetargetSourceNameHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimSequence, RetargetSource));

	// first create profile combo list
	RetargetSourceComboList.Empty();
	// first one is default one
	RetargetSourceComboList.Add( MakeShareable( new FString (DEFAULT_RETARGET_SOURCE_NAME) ));

	// find skeleton
	TSharedPtr<IPropertyHandle> SkeletonHanlder = DetailBuilder.GetProperty(TEXT("Skeleton"));
	FName CurrentPoseName;
	ensure (RetargetSourceNameHandler->GetValue(CurrentPoseName) != FPropertyAccess::Fail);
	
	// Check if we use only one skeleton
	USkeleton* Skeleton = NULL;
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList = DetailBuilder.GetDetailsView().GetSelectedObjects();
	for (auto SelectionIt = SelectedObjectsList.CreateIterator(); SelectionIt; ++SelectionIt)
	{
		if (UAnimSequence* TestAnimSequence = Cast<UAnimSequence>(SelectionIt->Get()))
		{
			// we should only have one selected anim sequence
			if(Skeleton && Skeleton != TestAnimSequence->GetSkeleton())
			{
				// Multiple different skeletons
				Skeleton = NULL;
				break;
			}
			Skeleton = TestAnimSequence->GetSkeleton();
		}
	}

	// set target skeleton. It can be null
	TargetSkeleton = Skeleton;

	// find what is initial selection is
	TSharedPtr<FString> InitialSelected;
	if (TargetSkeleton.IsValid())
	{
		RegisterRetargetSourceChanged();
		// go through profile and see if it has mine
		for (auto Iter = TargetSkeleton->AnimRetargetSources.CreateConstIterator(); Iter; ++Iter)
		{
			RetargetSourceComboList.Add( MakeShareable( new FString ( Iter.Key().ToString() )));

			if (Iter.Key() == CurrentPoseName) 
			{
				InitialSelected = RetargetSourceComboList.Last();
			}
		}
	}

	// add widget for editing retarget source
	AnimationCategory.AddCustomRow(RetargetSourceNameHandler->GetPropertyDisplayName())
	.NameContent()
	[
		RetargetSourceNameHandler->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SAssignNew(RetargetSourceComboBox, SComboBox< TSharedPtr<FString> >)
		.OptionsSource(&RetargetSourceComboList)
		.OnGenerateWidget(this, &FAnimSequenceDetails::MakeRetargetSourceComboWidget)
		.OnSelectionChanged(this, &FAnimSequenceDetails::OnRetargetSourceChanged)
		.OnComboBoxOpening(this, &FAnimSequenceDetails::OnRetargetSourceComboOpening)
		.InitiallySelectedItem(InitialSelected)
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.ContentPadding(0)
		.Content()
		[
			SNew( STextBlock )
			.Text(this, &FAnimSequenceDetails::GetRetargetSourceComboBoxContent)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.ToolTipText(this, &FAnimSequenceDetails::GetRetargetSourceComboBoxToolTip)
		]	
	];

	DetailBuilder.HideProperty(RetargetSourceNameHandler);

	// now customize to combo box
	IDetailCategoryBuilder& AdditiveSettingsCategory = DetailBuilder.EditCategory("AdditiveSettings");

	// hide all properties for additive anim and replace them with custom additive settings setup
	AdditiveAnimTypeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
	RefPoseTypeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimSequence, RefPoseType));
	RefPoseSeqHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimSequence, RefPoseSeq));
	RefFrameIndexHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimSequence, RefFrameIndex));

	CreateOverridenProperty(DetailBuilder, AdditiveSettingsCategory, AdditiveAnimTypeHandle, TAttribute<EVisibility>( EVisibility::Visible ));
	CreateOverridenProperty(DetailBuilder, AdditiveSettingsCategory, RefPoseTypeHandle, TAttribute<EVisibility>( this, &FAnimSequenceDetails::ShouldShowRefPoseType ));
	
	DetailBuilder.HideProperty(RefPoseSeqHandle);

	AdditiveSettingsCategory.AddCustomRow(RefPoseSeqHandle->GetPropertyDisplayName())
	.Visibility(TAttribute<EVisibility>( this, &FAnimSequenceDetails::ShouldShowRefAnimInfo ))
	.NameContent()
	[
		RefPoseSeqHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			RefPoseSeqHandle->CreatePropertyValueWidget()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SAnimationRefPoseViewport)
			.Skeleton(TargetSkeleton.Get())
			.AnimRefPropertyHandle(RefPoseSeqHandle)
			.RefPoseTypeHandle(RefPoseTypeHandle)
			.RefFrameIndexPropertyHandle(RefFrameIndexHandle)
		]
	];

	CreateOverridenProperty(DetailBuilder, AdditiveSettingsCategory, RefFrameIndexHandle, TAttribute<EVisibility>( this, &FAnimSequenceDetails::ShouldShowRefFrameIndex ));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FAnimSequenceDetails::CreateOverridenProperty(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& AdditiveSettingsCategory, TSharedPtr<IPropertyHandle> PropertyHandle, TAttribute<EVisibility> VisibilityAttribute)
{
	DetailBuilder.HideProperty(PropertyHandle);
	
	AdditiveSettingsCategory.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	.Visibility(VisibilityAttribute)
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		PropertyHandle->CreatePropertyValueWidget()
	];
}

EVisibility FAnimSequenceDetails::ShouldShowRefPoseType() const
{
	uint8 AdditiveAnimType = AAT_None; 
	AdditiveAnimTypeHandle->GetValue(AdditiveAnimType);
	return AdditiveAnimType != AAT_None? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility FAnimSequenceDetails::ShouldShowRefAnimInfo() const
{
	uint8 AdditiveAnimType = AAT_None; 
	uint8 RefPoseType = ABPT_None; 
	AdditiveAnimTypeHandle->GetValue(AdditiveAnimType);
	RefPoseTypeHandle->GetValue(RefPoseType);
	return TargetSkeleton.IsValid() && AdditiveAnimType != AAT_None && (RefPoseType == ABPT_AnimScaled || RefPoseType == ABPT_AnimFrame)? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FAnimSequenceDetails::ShouldShowRefFrameIndex() const
{
	uint8 AdditiveAnimType = AAT_None; 
	uint8 RefPoseType = ABPT_None; 
	AdditiveAnimTypeHandle->GetValue(AdditiveAnimType);
	RefPoseTypeHandle->GetValue(RefPoseType);
	return TargetSkeleton.IsValid() && AdditiveAnimType != AAT_None && RefPoseType == ABPT_AnimFrame? EVisibility::Visible : EVisibility::Collapsed;
}


TSharedRef<SWidget> FAnimSequenceDetails::MakeRetargetSourceComboWidget( TSharedPtr<FString> InItem )
{
	return SNew(STextBlock) .Text( FText::FromString(*InItem) ) .Font( IDetailLayoutBuilder::GetDetailFont() );
}

void FAnimSequenceDetails::DelegateRetargetSourceChanged()
{
	if (TargetSkeleton.IsValid())
	{
		// first create profile combo list
		RetargetSourceComboList.Empty();
		// first one is default one
		RetargetSourceComboList.Add( MakeShareable( new FString (DEFAULT_RETARGET_SOURCE_NAME) ));	

		// go through profile and see if it has mine
		for (auto Iter = TargetSkeleton->AnimRetargetSources.CreateConstIterator(); Iter; ++Iter)
		{
			RetargetSourceComboList.Add( MakeShareable( new FString ( Iter.Key().ToString() )));
		}

		RetargetSourceComboBox->RefreshOptions();
	}
}

void FAnimSequenceDetails::RegisterRetargetSourceChanged()
{
	if (TargetSkeleton.IsValid() && !OnDelegateRetargetSourceChanged.IsBound())
	{
		OnDelegateRetargetSourceChanged = USkeleton::FOnRetargetSourceChanged::CreateSP( this, &FAnimSequenceDetails::DelegateRetargetSourceChanged );
		OnDelegateRetargetSourceChangedDelegateHandle = TargetSkeleton->RegisterOnRetargetSourceChanged(OnDelegateRetargetSourceChanged);
	}
}

FAnimSequenceDetails::~FAnimSequenceDetails()
{
	if (TargetSkeleton.IsValid() && OnDelegateRetargetSourceChanged.IsBound())
	{
		TargetSkeleton->UnregisterOnRetargetSourceChanged(OnDelegateRetargetSourceChangedDelegateHandle);
	}
}

void FAnimSequenceDetails::OnRetargetSourceComboOpening()
{
	FName RetargetSourceName;
	if (RetargetSourceNameHandler->GetValue(RetargetSourceName) != FPropertyAccess::Result::MultipleValues)
	{
		TSharedPtr<FString> ComboStringPtr = GetRetargetSourceString(RetargetSourceName);
		if( ComboStringPtr.IsValid() )
		{
			RetargetSourceComboBox->SetSelectedItem(ComboStringPtr);
		}
	}
}

void FAnimSequenceDetails::OnRetargetSourceChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  )
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();

		if (NewValue == DEFAULT_RETARGET_SOURCE_NAME)
		{
			NewValue = TEXT("");
		}
		// set profile set up
		ensure ( RetargetSourceNameHandler->SetValue(NewValue) ==  FPropertyAccess::Result::Success );
	}
}

FText FAnimSequenceDetails::GetRetargetSourceComboBoxContent() const
{
	FName RetargetSourceName;
	if (RetargetSourceNameHandler->GetValue(RetargetSourceName) == FPropertyAccess::Result::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*GetRetargetSourceString(RetargetSourceName).Get());
}

FText FAnimSequenceDetails::GetRetargetSourceComboBoxToolTip() const
{
	return LOCTEXT("RetargetSourceComboToolTip", "When retargeting, this pose will be used as a base of animation");
}

TSharedPtr<FString> FAnimSequenceDetails::GetRetargetSourceString(FName RetargetSourceName) const
{
	FString RetargetSourceString = RetargetSourceName.ToString();

	// go through profile and see if it has mine
	for (int32 Index=1; Index<RetargetSourceComboList.Num(); ++Index)
	{
		if (RetargetSourceString == *RetargetSourceComboList[Index])
		{
			return RetargetSourceComboList[Index];
		}
	}

	return RetargetSourceComboList[0];
}


////////////////////////////////////////////////
// based on SAnimationSegmentViewport
SAnimationRefPoseViewport::~SAnimationRefPoseViewport()
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

void SAnimationRefPoseViewport::CleanupComponent(USceneComponent* Component)
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

SAnimationRefPoseViewport::SAnimationRefPoseViewport()
	: PreviewScene(FPreviewScene::ConstructionValues())
	, PreviewComponent(nullptr)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAnimationRefPoseViewport::Construct(const FArguments& InArgs)
{
	TargetSkeleton = InArgs._Skeleton;
	AnimRefPropertyHandle = InArgs._AnimRefPropertyHandle;
	RefPoseTypeHandle = InArgs._RefPoseTypeHandle;
	RefFrameIndexPropertyHandle = InArgs._RefFrameIndexPropertyHandle;

	// Create the preview component
	PreviewComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent( PreviewComponent, FTransform::Identity );

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(Description, STextBlock)
			.Text(LOCTEXT("DefaultViewportLabel", "Default View"))
			.AutoWrapText(true)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.HAlign(HAlign_Center)
			[
				SAssignNew(ViewportWidget, SViewport)
				.EnableGammaCorrection(false)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SAnimationSegmentScrubPanel)
			.ViewInputMin(this, &SAnimationRefPoseViewport::GetViewMinInput)
			.ViewInputMax(this, &SAnimationRefPoseViewport::GetViewMaxInput)
			.PreviewInstance(this, &SAnimationRefPoseViewport::GetPreviewInstance)
			.DraggableBars(this, &SAnimationRefPoseViewport::GetBars)
			.OnBarDrag(this, &SAnimationRefPoseViewport::OnBarDrag)
			.OnTickPlayback(this, &SAnimationRefPoseViewport::OnTickPreview)
			.bAllowZoom(true)
		]
	];

	// Create the viewport
	LevelViewportClient = MakeShareable( new FAnimationSegmentViewportClient( PreviewScene ) );

	LevelViewportClient->ViewportType = LVT_Perspective;
	LevelViewportClient->bSetListenerPosition = false;
	LevelViewportClient->SetViewLocation( EditorViewportDefs::DefaultPerspectiveViewLocation );
	LevelViewportClient->SetViewRotation( EditorViewportDefs::DefaultPerspectiveViewRotation );

	SceneViewport = MakeShareable( new FSceneViewport( LevelViewportClient.Get(), ViewportWidget ) );
	LevelViewportClient->Viewport = SceneViewport.Get();
	LevelViewportClient->SetRealtime( true );
	LevelViewportClient->VisibilityDelegate.BindSP( this, &SAnimationRefPoseViewport::IsVisible );
	LevelViewportClient->SetViewMode( VMI_Lit );

	ViewportWidget->SetViewportInterface( SceneViewport.ToSharedRef() );

	InitSkeleton();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAnimationRefPoseViewport::InitSkeleton()
{
	UObject *Object = NULL;
	AnimRefPropertyHandle->GetValue(Object);
	AnimRef = Cast<UAnimSequence>(Object);
	USkeleton *Skeleton = NULL;
	if(AnimRef != NULL)
	{
		Skeleton = AnimRef->GetSkeleton();
	}
	else
	{
		Skeleton = TargetSkeleton;
	}

	// if skeleton doesn't match with target skeleton, this is error, we can't support it
	if ( Skeleton == TargetSkeleton)
	{
		if(PreviewComponent != NULL && Skeleton != NULL)
		{
			UAnimSingleNodeInstance * Preview = PreviewComponent->PreviewInstance;
			USkeletalMesh* PreviewSkeletalMesh = Skeleton->GetPreviewMesh();
			if((Preview == NULL || Preview->CurrentAsset != AnimRef) || PreviewComponent->SkeletalMesh != PreviewSkeletalMesh)
			{
				PreviewComponent->SetSkeletalMesh(PreviewSkeletalMesh);
				PreviewComponent->EnablePreview(true, AnimRef, NULL);
				PreviewComponent->PreviewInstance->SetLooping(true);

				//Place the camera at a good viewer position
				FVector NewPosition = LevelViewportClient->GetViewLocation();
				NewPosition.Normalize();
				if(PreviewSkeletalMesh)
				{
					NewPosition *= (PreviewSkeletalMesh->Bounds.SphereRadius*1.5f);
				}
				LevelViewportClient->SetViewLocation( NewPosition );
			}
		}
	}
}

void SAnimationRefPoseViewport::OnTickPreview( double InCurrentTime, float InDeltaTime )
{
	LevelViewportClient->Invalidate();
}

void SAnimationRefPoseViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	class UDebugSkelMeshComponent* Component = PreviewComponent;

	FString TargetSkeletonName = TargetSkeleton ? TargetSkeleton->GetName() : FName( NAME_None ).ToString();

	if ( Component != NULL )
	{
		// Reinit the skeleton if the anim ref has changed
		InitSkeleton();

		if ( Component->IsPreviewOn() && AnimRef != NULL )
		{
			if ( PreviewComponent != NULL && PreviewComponent->PreviewInstance != NULL )
			{
				uint8 RefPoseType;
				RefPoseTypeHandle->GetValue( RefPoseType );
				if ( RefPoseType == ABPT_AnimFrame )
				{
					int RefFrameIndex;
					RefFrameIndexPropertyHandle->GetValue( RefFrameIndex );
					float Fraction = ( AnimRef->NumFrames > 0 ) ? FMath::Clamp<float>( (float)RefFrameIndex / (float)AnimRef->NumFrames, 0.f, 1.f ) : 0.f;
					float RefTime = AnimRef->SequenceLength * Fraction;
					PreviewComponent->PreviewInstance->SetPosition( RefTime, false );
					PreviewComponent->PreviewInstance->SetPlaying( false );
					LevelViewportClient->Invalidate();
				}
			}

			Description->SetText( FText::Format( LOCTEXT( "Previewing", "Previewing {0}" ), FText::FromString( Component->GetPreviewText() ) ) );
		}
		else if ( Component->AnimBlueprintGeneratedClass )
		{
			Description->SetText( FText::Format( LOCTEXT( "Previewing", "Previewing {0}" ), FText::FromString( Component->AnimBlueprintGeneratedClass->GetName() ) ) );
		}
		else if ( AnimRef && AnimRef->GetSkeleton() != TargetSkeleton )
		{
			Description->SetText( FText::Format( LOCTEXT( "IncorrectSkeleton", "The preview asset doesn't work for the skeleton '{0}'" ), FText::FromString( TargetSkeletonName ) ) );
		}
		else if ( Component->SkeletalMesh == NULL )
		{
			Description->SetText( FText::Format( LOCTEXT( "NoMeshFound", "No skeletal mesh found for skeleton '{0}'" ), FText::FromString( TargetSkeletonName ) ) );
		}
		else
		{
			Description->SetText( FText::Format( LOCTEXT( "SelectAnimation", "Select animation that works for skeleton '{0}'" ), FText::FromString( TargetSkeletonName ) ) );
		}

		Component->GetScene()->GetWorld()->Tick( LEVELTICK_All, InDeltaTime );
	}
	else
	{
		Description->SetText( FText::Format( LOCTEXT( "NoMeshFound", "No skeletal mesh found for skeleton '{0}'" ), FText::FromString( TargetSkeletonName ) ) );
	}
}

void SAnimationRefPoseViewport::RefreshViewport()
{
}

bool SAnimationRefPoseViewport::IsVisible() const
{
	return ViewportWidget.IsValid();
}

float SAnimationRefPoseViewport::GetViewMinInput() const 
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

float SAnimationRefPoseViewport::GetViewMaxInput() const 
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

TArray<float> SAnimationRefPoseViewport::GetBars() const
{
	TArray<float> Bars;
	if (AnimRef)
	{
		int RefFrameIndex;
		RefFrameIndexPropertyHandle->GetValue(RefFrameIndex);
		float Fraction = (AnimRef->NumFrames > 0)? FMath::Clamp<float>((float)RefFrameIndex/(float)AnimRef->NumFrames, 0.f, 1.f) : 0.f;
		Bars.Add(AnimRef->SequenceLength * Fraction);
	}
	else
	{
		Bars.Add(0.0f);
	}
	return Bars;
}

void SAnimationRefPoseViewport::OnBarDrag(int32 Index, float Position)
{
	if (AnimRef)
	{
		int RefFrameIndex = FMath::Clamp<int>(AnimRef->SequenceLength > 0.0f? (int)(Position * (float)AnimRef->NumFrames / AnimRef->SequenceLength + 0.5f) : 0.0f, 0, AnimRef->NumFrames - 1);
		RefFrameIndexPropertyHandle->SetValue(RefFrameIndex);
	}
}

UAnimSingleNodeInstance* SAnimationRefPoseViewport::GetPreviewInstance() const
{
	return PreviewComponent ? PreviewComponent->PreviewInstance : NULL;
}
#undef LOCTEXT_NAMESPACE
