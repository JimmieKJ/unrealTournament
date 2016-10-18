// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimationEditorViewport.h"
#include "SAnimationScrubPanel.h"
#include "SAnimMontageScrubPanel.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "SAnimViewportToolBar.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "AnimGraphDefinitions.h"
#include "AnimPreviewInstance.h"
#include "AnimationEditorViewportClient.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/LODUtilities.h"
#include "DetailLayoutBuilder.h"
#include "STextComboBox.h"

#define LOCTEXT_NAMESPACE "PersonaViewportToolbar"

namespace EAnimationPlaybackSpeeds
{
	// Speed scales for animation playback, must match EAnimationPlaybackSpeeds::Type
	float Values[EAnimationPlaybackSpeeds::NumPlaybackSpeeds] = {0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewport

SAnimationEditorViewport::~SAnimationEditorViewport()
{
	if(PersonaPtr.IsValid())
	{		
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
	}
}

void SAnimationEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<class FPersona> InPersona, TSharedPtr<class SAnimationEditorViewportTabBody> InTabBody)
{
	PersonaPtr = InPersona;
	TabBodyPtr = InTabBody;

	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP(this, &SAnimationEditorViewport::OnUndoRedo));

	SEditorViewport::Construct(
		SEditorViewport::FArguments()
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.AddMetaData<FTagMetaData>(TEXT("Persona.Viewport"))
		);

	Client->VisibilityDelegate.BindSP(this, &SAnimationEditorViewport::IsVisible);
}

TSharedRef<FEditorViewportClient> SAnimationEditorViewport::MakeEditorViewportClient()
{
	// Create an animation viewport client
	LevelViewportClient = MakeShareable(new FAnimationViewportClient(PersonaPtr.Pin()->GetPreviewScene(), PersonaPtr, SharedThis(this)));

	LevelViewportClient->ViewportType = LVT_Perspective;
	LevelViewportClient->bSetListenerPosition = false;
	LevelViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	LevelViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	return LevelViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SAnimationEditorViewport::MakeViewportToolbar()
{
	return SNew(SAnimViewportToolBar, TabBodyPtr.Pin(), SharedThis(this))
		.Cursor(EMouseCursor::Default);
}

void SAnimationEditorViewport::OnUndoRedo()
{
	LevelViewportClient->Invalidate();
	LevelViewportClient->PostUndo();
}

//////////////////////////////////////////////////////////////////////////
// 
/** //@TODO MODES: Simple text entry popup used it to get 3 vector position*/

class STranslationInputWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( STranslationInputWindow )
		: _X_Value(0.0f)
		, _Y_Value(0.0f)
		, _Z_Value(0.0f)
	{}


	SLATE_ARGUMENT( float, X_Value )
	SLATE_ARGUMENT( float, Y_Value )
	SLATE_ARGUMENT( float, Z_Value )
	SLATE_EVENT( FOnTextCommitted, OnXModified )
	SLATE_EVENT( FOnTextCommitted, OnYModified )
	SLATE_EVENT( FOnTextCommitted, OnZModified )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		FString XString = FString::Printf(TEXT("%0.2f"), InArgs._X_Value);
		FString YString = FString::Printf(TEXT("%0.2f"), InArgs._Y_Value);
		FString ZString = FString::Printf(TEXT("%0.2f"), InArgs._Z_Value);

		this->ChildSlot
		[
			SNew(SBorder)
			. BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			. Padding(10)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("AnimationEditorViewport", "XValueLabel", "X:"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(10.0f)
						.Text( FText::FromString(XString) )
						.OnTextCommitted( InArgs._OnXModified )
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("AnimationEditorViewport", "YValueLabel", "Y:"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(10.0f)
						.Text( FText::FromString(YString) )
						.OnTextCommitted( InArgs._OnYModified )
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("AnimationEditorViewport", "ZValueLabel", "Z: "))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(10.0f)
						.Text( FText::FromString(ZString) )
						.OnTextCommitted( InArgs._OnZModified )
					]
				]
			]
		];
	}
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewportTabBody

SAnimationEditorViewportTabBody::SAnimationEditorViewportTabBody()
	: SelectedTurnTableSpeed(EAnimationPlaybackSpeeds::Normal)
	, SelectedTurnTableMode(EPersonaTurnTableMode::Stopped)
	, AnimationPlaybackSpeedMode(EAnimationPlaybackSpeeds::Normal)
{
}

SAnimationEditorViewportTabBody::~SAnimationEditorViewportTabBody()
{
	CleanupPersonaReferences();

	// Close viewport
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = NULL;
	}

	// Release our reference to the viewport client
	LevelViewportClient.Reset();
}

void SAnimationEditorViewportTabBody::CleanupPersonaReferences()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnAnimChanged(this);
		PersonaPtr.Reset();
	}
}

bool SAnimationEditorViewportTabBody::CanUseGizmos() const
{
	class UDebugSkelMeshComponent* Component = PersonaPtr.Pin()->PreviewComponent;

	if (Component != NULL)
	{
		if (Component->bForceRefpose)
		{
			return false;
		}
		else if (Component->IsPreviewOn())
		{
			return true;
		}
	}

	return false;
}

FText SAnimationEditorViewportTabBody::GetDisplayString() const
{
	class UDebugSkelMeshComponent* Component = PersonaPtr.Pin()->PreviewComponent;
	FString TargetSkeletonName = TargetSkeleton ? TargetSkeleton->GetName() : FName(NAME_None).ToString();

	if (Component != NULL)
	{
		if (Component->bForceRefpose)
		{
			return LOCTEXT("ReferencePose", "Reference pose");
		}
		else if (Component->IsPreviewOn())
		{
			return FText::Format(LOCTEXT("Previewing", "Previewing {0}"), FText::FromString(Component->GetPreviewText()));
		}
		else if (Component->AnimClass != NULL)
		{
			const bool bWarnAboutBoneManip = !PersonaPtr.Pin()->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode);
			if (bWarnAboutBoneManip)
			{
				return FText::Format(LOCTEXT("PreviewingAnimBP_WarnDisabled", "Previewing {0}. \nBone manipulation is disabled in this mode. "), FText::FromString(Component->AnimClass->GetName()));
			}
			else
			{
				return FText::Format(LOCTEXT("PreviewingAnimBP", "Previewing {0}"), FText::FromString(Component->AnimClass->GetName()));
			}
		}
		else if (Component->SkeletalMesh == NULL)
		{
			return FText::Format(LOCTEXT("NoMeshFound", "No skeletal mesh found for skeleton '{0}'"), FText::FromString(TargetSkeletonName));
		}
		else
		{
			return LOCTEXT("NothingToPlay", "Nothing to play");
		}
	}
	else
	{
		return FText::Format(LOCTEXT("NoMeshFound", "No skeletal mesh found for skeleton '{0}'"), FText::FromString(TargetSkeletonName));
	}
}

void SAnimationEditorViewportTabBody::RefreshViewport()
{
	LevelViewportClient->Invalidate();
}

bool SAnimationEditorViewportTabBody::IsVisible() const
{
	return ViewportWidget.IsValid();
}


void SAnimationEditorViewportTabBody::Construct(const FArguments& InArgs)
{
	UICommandList = MakeShareable(new FUICommandList);

	PersonaPtr = InArgs._Persona;
	IsEditable = InArgs._IsEditable;
	TargetSkeleton = InArgs._Skeleton;
	bPreviewLockModeOn = 0;
	LODSelection = 0;

	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();
	check(SharedPersona.IsValid());

	// register delegate for anim change notification
	SharedPersona->RegisterOnAnimChanged(FPersona::FOnAnimChanged::CreateSP(this, &SAnimationEditorViewportTabBody::AnimChanged));

	const FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

	FAnimViewportMenuCommands::Register();
	FAnimViewportShowCommands::Register();
	FAnimViewportLODCommands::Register();
	FAnimViewportPlaybackCommands::Register();

	// Build toolbar widgets
	UVChannelCombo = SNew(STextComboBox)
		.OptionsSource(&UVChannels)
		.OnSelectionChanged(this, &SAnimationEditorViewportTabBody::ComboBoxSelectionChanged);

	ViewportWidget = SNew(SAnimationEditorViewport, InArgs._Persona, SharedThis(this));

	TSharedPtr<SVerticalBox> ViewportContainer = nullptr;
	this->ChildSlot
	[
		SAssignNew(ViewportContainer, SVerticalBox)

		// Build our toolbar level toolbar
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SOverlay)

			// The viewport
			+SOverlay::Slot()
			[
				ViewportWidget.ToSharedRef()
			]

			// The 'dirty/in-error' indicator text in the bottom-right corner
			+SOverlay::Slot()
			.Padding(8)
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Visibility(this, &SAnimationEditorViewportTabBody::GetViewportCornerTextVisibility)
				.OnClicked(this, &SAnimationEditorViewportTabBody::ClickedOnViewportCornerText)
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage)
						.Visibility(this, &SAnimationEditorViewportTabBody::GetViewportCornerImageVisibility)
						.Image(this, &SAnimationEditorViewportTabBody::GetViewportCornerImage)
					]
						
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "Persona.Viewport.BlueprintDirtyText")
						.Text(this, &SAnimationEditorViewportTabBody::GetViewportCornerText)
						.ToolTipText(this, &SAnimationEditorViewportTabBody::GetViewportCornerTooltip)
					]
				]
			]
		]
	];

	if(!SharedPersona->IsModeCurrent(FPersonaModes::AnimationEditMode) && ViewportContainer.IsValid())
	{
		ViewportContainer->AddSlot()
		.AutoHeight()
		[
			SAssignNew(ScrubPanelContainer, SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SAnimationScrubPanel)
				.Persona(PersonaPtr)
				.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
				.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
				.bAllowZoom(true)
			]
		];

		UpdateScrubPanel(Cast<UAnimationAsset>(SharedPersona->GetPreviewAnimationAsset()));
	}

	LevelViewportClient = ViewportWidget->GetViewportClient();

	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());

	// Load the view mode from config
	AnimViewportClient->SetViewMode(AnimViewportClient->ConfigOption->ViewModeIndex);
	UpdateShowFlagForMeshEdges();


	OnSetTurnTableMode(SelectedTurnTableMode);
	OnSetTurnTableSpeed(SelectedTurnTableSpeed);

	BindCommands();
}

void SAnimationEditorViewportTabBody::BindCommands()
{
	FUICommandList& CommandList = *UICommandList;

	//Bind menu commands
	const FAnimViewportMenuCommands& MenuActions = FAnimViewportMenuCommands::Get();

	CommandList.MapAction( 
		MenuActions.Lock,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::SetPreviewMode, 1),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewModeOn, 1));

	CommandList.MapAction( 
		MenuActions.Auto,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::SetPreviewMode, 0),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewModeOn, 0));

	CommandList.MapAction(
		MenuActions.CameraFollow,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ToggleCameraFollow),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanChangeCameraMode),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsCameraFollowEnabled));

	CommandList.MapAction(
		MenuActions.UseInGameBound,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::UseInGameBound),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanUseInGameBound),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsUsingInGameBound));

	TSharedRef<FAnimationViewportClient> EditorViewportClientRef = GetAnimationViewportClient();

	CommandList.MapAction(
		MenuActions.SetCPUSkinning,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FAnimationViewportClient::ToggleCPUSkinning),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FAnimationViewportClient::IsSetCPUSkinningChecked));

	CommandList.MapAction(
		MenuActions.SetShowNormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowNormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowNormalsChecked ) );

	CommandList.MapAction(
		MenuActions.SetShowTangents,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowTangents ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowTangentsChecked ) );

	CommandList.MapAction(
		MenuActions.SetShowBinormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleShowBinormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetShowBinormalsChecked ) );

	CommandList.MapAction(
		MenuActions.AnimSetDrawUVs,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::ToggleDrawUVOverlay ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FAnimationViewportClient::IsSetDrawUVOverlayChecked ) );

	//Bind Show commands
	const FAnimViewportShowCommands& ViewportShowMenuCommands = FAnimViewportShowCommands::Get();
	
	CommandList.MapAction(
		ViewportShowMenuCommands.ShowReferencePose,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ShowReferencePose),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowReferencePose),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowReferencePoseEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowRetargetBasePose,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ShowRetargetBasePose),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowRetargetBasePose),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowRetargetBasePoseEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBound,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ShowBound),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowBound),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowBoundEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowPreviewMesh,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::ToggleShowPreviewMesh),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::CanShowPreviewMesh),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowPreviewMeshEnabled));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowMorphTargets,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowMorphTargets),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMorphTargets));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowBoneNames,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowBoneNames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingBoneNames));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowRawAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowRawAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingRawAnimation));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowNonRetargetedAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowNonRetargetedAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingNonRetargetedPose));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowAdditiveBaseBones,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowAdditiveBase),
		FCanExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewingAnimation),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingAdditiveBase));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowSourceRawAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowSourceRawAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingSourceRawAnimation));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBakedAnimation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowBakedAnimation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingBakedAnimation));

	//Display info
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowDisplayInfoBasic,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::Basic),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::Basic));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowDisplayInfoDetailed,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::Detailed),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::Detailed));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowDisplayInfoSkelControls,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::SkeletalControls),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::SkeletalControls));

	CommandList.MapAction(
		ViewportShowMenuCommands.HideDisplayInfo,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowDisplayInfo, (int32)EDisplayInfoMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingMeshInfo, (int32)EDisplayInfoMode::None));

	//Bone weight
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowBoneWeight,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowBoneWeight),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingBoneWeight));

	// Show sockets
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowSockets,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowSockets),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingSockets));

	// Set bone local axes mode
	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawNone,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::None));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawSelected,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::Selected),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::Selected));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowBoneDrawAll,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetBoneDrawMode, (int32)EBoneDrawMode::All),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsBoneDrawModeSet, (int32)EBoneDrawMode::All));

	// Set bone local axes mode
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesNone,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::None));
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesSelected,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::Selected),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::Selected));
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowLocalAxesAll,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLocalAxesMode, (int32)ELocalAxesMode::All),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLocalAxesModeSet, (int32)ELocalAxesMode::All));

#if WITH_APEX_CLOTHING
	//Clothing show options
	CommandList.MapAction( 
		ViewportShowMenuCommands.DisableClothSimulation,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnDisableClothSimulation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsDisablingClothSimulation));

	//Apply wind
	CommandList.MapAction( 
		ViewportShowMenuCommands.ApplyClothWind,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnApplyClothWind),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsApplyingClothWind));
	
	//Cloth simulation normal
	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothSimulationNormals,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowClothSimulationNormals),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingClothSimulationNormals));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothGraphicalTangents,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowClothGraphicalTangents),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingClothGraphicalTangents));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothCollisionVolumes,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowClothCollisionVolumes),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingClothCollisionVolumes));

	CommandList.MapAction( 
		ViewportShowMenuCommands.EnableCollisionWithAttachedClothChildren,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnEnableCollisionWithAttachedClothChildren),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsEnablingCollisionWithAttachedClothChildren));

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothPhysicalMeshWire,
		FExecuteAction::CreateSP( this, &SAnimationEditorViewportTabBody::OnShowClothPhysicalMeshWire ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SAnimationEditorViewportTabBody::IsShowingClothPhysicalMeshWire ) );

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothMaxDistances,
		FExecuteAction::CreateSP( this, &SAnimationEditorViewportTabBody::OnShowClothMaxDistances ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SAnimationEditorViewportTabBody::IsShowingClothMaxDistances ) );

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothBackstop,
		FExecuteAction::CreateSP( this, &SAnimationEditorViewportTabBody::OnShowClothBackstops ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SAnimationEditorViewportTabBody::IsShowingClothBackstops ) );

	CommandList.MapAction( 
		ViewportShowMenuCommands.ShowClothFixedVertices,
		FExecuteAction::CreateSP( this, &SAnimationEditorViewportTabBody::OnShowClothFixedVertices ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SAnimationEditorViewportTabBody::IsShowingClothFixedVertices ) );

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowAllSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll));

	CommandList.MapAction(
		ViewportShowMenuCommands.ShowOnlyClothSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections));

	CommandList.MapAction(
		ViewportShowMenuCommands.HideOnlyClothSections,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsSectionsDisplayMode, (int32)UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections));

#endif// #if WITH_APEX_CLOTHING		

	//Bind LOD preview menu commands
	const FAnimViewportLODCommands& ViewportLODMenuCommands = FAnimViewportLODCommands::Get();

	//LOD Auto
	CommandList.MapAction( 
		ViewportLODMenuCommands.LODAuto,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLODModel, 0),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLODModelSelected, 0));

	// LOD 0
	CommandList.MapAction(
		ViewportLODMenuCommands.LOD0,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetLODModel, 1),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsLODModelSelected, 1));

	// all other LODs will be added dynamically 
	
	CommandList.MapAction( 
		ViewportShowMenuCommands.ToggleGrid,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsShowingGrid));

	CommandList.MapAction(
		ViewportShowMenuCommands.AutoAlignFloorToMesh,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnToggleAutoAlignFloor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsAutoAlignFloor));
	
	//Bind LOD preview menu commands
	const FAnimViewportPlaybackCommands& ViewportPlaybackCommands = FAnimViewportPlaybackCommands::Get();

	//Create a menu item for each playback speed in EAnimationPlaybackSpeeds
	for(int32 i = 0; i < int(EAnimationPlaybackSpeeds::NumPlaybackSpeeds); ++i)
	{
		CommandList.MapAction( 
			ViewportPlaybackCommands.PlaybackSpeedCommands[i],
			FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetPlaybackSpeed, i),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPlaybackSpeedSelected, i));
	}

	CommandList.MapAction(
		ViewportShowMenuCommands.MuteAudio,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnMuteAudio),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsAudioMuted));

	CommandList.MapAction(
		ViewportShowMenuCommands.ProcessRootMotion,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnTogglePreviewRootMotion),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsPreviewingRootMotion));

	// Turn Table Controls
	for (int32 i = 0; i < int(EAnimationPlaybackSpeeds::NumPlaybackSpeeds); ++i)
	{
		CommandList.MapAction(
			ViewportPlaybackCommands.TurnTableSpeeds[i],
			FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableSpeed, i),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableSpeedSelected, i));
	}

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTablePlay,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Playing)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Playing)));

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTablePause,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Paused)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Paused)));

	CommandList.MapAction(
		ViewportPlaybackCommands.PersonaTurnTableStop,
		FExecuteAction::CreateSP(this, &SAnimationEditorViewportTabBody::OnSetTurnTableMode, int32(EPersonaTurnTableMode::Stopped)),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SAnimationEditorViewportTabBody::IsTurnTableModeSelected, int32(EPersonaTurnTableMode::Stopped)));
}

void SAnimationEditorViewportTabBody::OnSetTurnTableSpeed(int32 SpeedIndex)
{
	SelectedTurnTableSpeed = (EAnimationPlaybackSpeeds::Type)SpeedIndex;

	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if (PreviewComponent)
	{
		PreviewComponent->TurnTableSpeedScaling = EAnimationPlaybackSpeeds::Values[SelectedTurnTableSpeed];
	}
}

bool SAnimationEditorViewportTabBody::IsTurnTableSpeedSelected(int32 SpeedIndex) const
{
	return (SelectedTurnTableSpeed == SpeedIndex);
}

void SAnimationEditorViewportTabBody::OnSetTurnTableMode(int32 ModeIndex)
{
	SelectedTurnTableMode = (EPersonaTurnTableMode::Type)ModeIndex;

	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if (PreviewComponent)
	{
		PreviewComponent->TurnTableMode = SelectedTurnTableMode;

		if (SelectedTurnTableMode == EPersonaTurnTableMode::Stopped)
		{
			PreviewComponent->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

bool SAnimationEditorViewportTabBody::IsTurnTableModeSelected(int32 ModeIndex) const
{
	return (SelectedTurnTableMode == ModeIndex);
}

bool SAnimationEditorViewportTabBody::IsPreviewModeOn(int32 PreviewMode) const
{
	if (bPreviewLockModeOn && PreviewMode)
	{
		return true;
	}
	else if (!bPreviewLockModeOn && !PreviewMode)
	{
		return true;
	}

	return false;

}
void SAnimationEditorViewportTabBody::SetPreviewMode(int32 PreviewMode)
{
	if (PreviewMode)
	{
		bPreviewLockModeOn = true; 
	}
	else
	{
		bPreviewLockModeOn = false; 
	}
}

int32 SAnimationEditorViewportTabBody::GetLODModelCount() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if( PreviewComponent && PreviewComponent->SkeletalMesh )
	{
		return PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num();
	}
	return 0;
}

void SAnimationEditorViewportTabBody::OnShowMorphTargets()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisableMorphTarget = !PreviewComponent->bDisableMorphTarget;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

void SAnimationEditorViewportTabBody::OnShowBoneNames()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bShowBoneNames = !PreviewComponent->bShowBoneNames;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

void SAnimationEditorViewportTabBody::OnShowRawAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplayRawAnimation = !PreviewComponent->bDisplayRawAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowNonRetargetedAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplayNonRetargetedPose = !PreviewComponent->bDisplayNonRetargetedPose;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowSourceRawAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplaySourceAnimation = !PreviewComponent->bDisplaySourceAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowBakedAnimation()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplayBakedAnimation = !PreviewComponent->bDisplayBakedAnimation;
		PreviewComponent->MarkRenderStateDirty();
	}
}

void SAnimationEditorViewportTabBody::OnShowAdditiveBase()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplayAdditiveBasePose = !PreviewComponent->bDisplayAdditiveBasePose;
		PreviewComponent->MarkRenderStateDirty();
	}
}

bool SAnimationEditorViewportTabBody::IsPreviewingAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return (PreviewComponent && PreviewComponent->PreviewInstance && (PreviewComponent->PreviewInstance == PreviewComponent->GetAnimInstance()));
}

bool SAnimationEditorViewportTabBody::IsShowingMorphTargets() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisableMorphTarget == false;
}

bool SAnimationEditorViewportTabBody::IsShowingBoneNames() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bShowBoneNames;
}

bool SAnimationEditorViewportTabBody::IsShowingRawAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisplayRawAnimation;
}

bool SAnimationEditorViewportTabBody::IsShowingNonRetargetedPose() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisplayNonRetargetedPose;
}

bool SAnimationEditorViewportTabBody::IsShowingAdditiveBase() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisplayAdditiveBasePose;
}

bool SAnimationEditorViewportTabBody::IsShowingSourceRawAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisplaySourceAnimation;
}

bool SAnimationEditorViewportTabBody::IsShowingBakedAnimation() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDisplayBakedAnimation;
}

void SAnimationEditorViewportTabBody::OnShowDisplayInfo(int32 DisplayInfoMode)
{
	GetAnimationViewportClient()->OnSetShowMeshStats(DisplayInfoMode);
}

bool SAnimationEditorViewportTabBody::IsShowingMeshInfo(int32 DisplayInfoMode) const
{
	return GetAnimationViewportClient()->GetShowMeshStats() == DisplayInfoMode;
}

void SAnimationEditorViewportTabBody::OnShowBoneWeight()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if( PreviewComponent )
	{
		PreviewComponent->SetShowBoneWeight( !PreviewComponent->bDrawBoneInfluences );
		UpdateShowFlagForMeshEdges();
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingBoneWeight() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDrawBoneInfluences;
}

void SAnimationEditorViewportTabBody::OnSetBoneDrawMode(int32 BoneDrawMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetBoneDrawMode((EBoneDrawMode::Type)BoneDrawMode);
}

bool SAnimationEditorViewportTabBody::IsBoneDrawModeSet(int32 BoneDrawMode) const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsBoneDrawModeSet((EBoneDrawMode::Type)BoneDrawMode);
}

void SAnimationEditorViewportTabBody::OnSetLocalAxesMode(int32 LocalAxesMode)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetLocalAxesMode((ELocalAxesMode::Type)LocalAxesMode);
}

bool SAnimationEditorViewportTabBody::IsLocalAxesModeSet(int32 LocalAxesMode) const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsLocalAxesModeSet((ELocalAxesMode::Type)LocalAxesMode);
}

void SAnimationEditorViewportTabBody::OnShowSockets()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if( PreviewComponent )
	{
		PreviewComponent->bDrawSockets = !PreviewComponent->bDrawSockets;
		PreviewComponent->MarkRenderStateDirty();
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingSockets() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->bDrawSockets;
}


void SAnimationEditorViewportTabBody::OnShowGrid()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->OnToggleShowGrid();
}

bool SAnimationEditorViewportTabBody::IsShowingGrid() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsShowingGrid();
}

void SAnimationEditorViewportTabBody::OnToggleAutoAlignFloor()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->OnToggleAutoAlignFloor();
}

bool SAnimationEditorViewportTabBody::IsAutoAlignFloor() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->IsAutoAlignFloor();
}

/** Function to set the current playback speed*/
void SAnimationEditorViewportTabBody::OnSetPlaybackSpeed(int32 PlaybackSpeedMode)
{
	AnimationPlaybackSpeedMode = (EAnimationPlaybackSpeeds::Type)PlaybackSpeedMode;
	
	float PlaySpeed = EAnimationPlaybackSpeeds::Values[AnimationPlaybackSpeedMode];
	if(LevelViewportClient->GetWorld())
	{
		LevelViewportClient->GetWorld()->GetWorldSettings()->TimeDilation = PlaySpeed;
	}
}

bool SAnimationEditorViewportTabBody::IsPlaybackSpeedSelected(int32 PlaybackSpeedMode)
{
	return PlaybackSpeedMode == AnimationPlaybackSpeedMode;
}

void SAnimationEditorViewportTabBody::ShowReferencePose()
{
	PersonaPtr.Pin()->ShowReferencePose(IsShowReferencePoseEnabled() == false);
}

bool SAnimationEditorViewportTabBody::CanShowReferencePose() const
{
	return PersonaPtr.Pin()->CanShowReferencePose();
}

bool SAnimationEditorViewportTabBody::IsShowReferencePoseEnabled() const
{
	return PersonaPtr.Pin()->IsShowReferencePoseEnabled();
}

void SAnimationEditorViewportTabBody::ShowRetargetBasePose()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if(PreviewComponent && PreviewComponent->PreviewInstance)
	{
		PreviewComponent->PreviewInstance->SetForceRetargetBasePose(!PreviewComponent->PreviewInstance->GetForceRetargetBasePose());
	}
}

bool SAnimationEditorViewportTabBody::CanShowRetargetBasePose() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->PreviewInstance;
}

bool SAnimationEditorViewportTabBody::IsShowRetargetBasePoseEnabled() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if(PreviewComponent && PreviewComponent->PreviewInstance)
	{
		return PreviewComponent->PreviewInstance->GetForceRetargetBasePose();
	}
	return false;
}

void SAnimationEditorViewportTabBody::ShowBound()
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());	
	AnimViewportClient->ToggleShowBounds();

	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if(PreviewComponent)
	{
		PreviewComponent->bDisplayBound = AnimViewportClient->EngineShowFlags.Bounds;
		PreviewComponent->RecreateRenderState_Concurrent();
	}
}

bool SAnimationEditorViewportTabBody::CanShowBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL;
}

bool SAnimationEditorViewportTabBody::IsShowBoundEnabled() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());	
	return AnimViewportClient->IsSetShowBoundsChecked();
}

void SAnimationEditorViewportTabBody::ToggleShowPreviewMesh()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		bool bCurrentlyVisible = IsShowPreviewMeshEnabled();
		PreviewComponent->SetVisibility(!bCurrentlyVisible);
	}
}

bool SAnimationEditorViewportTabBody::CanShowPreviewMesh() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL;
}

bool SAnimationEditorViewportTabBody::IsShowPreviewMeshEnabled() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return (PreviewComponent != NULL) && PreviewComponent->IsVisible();
}

void SAnimationEditorViewportTabBody::UseInGameBound()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if (PreviewComponent != NULL)
	{
		PreviewComponent->UseInGameBounds(! PreviewComponent->IsUsingInGameBounds());
	}
}

bool SAnimationEditorViewportTabBody::CanUseInGameBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL;
}

bool SAnimationEditorViewportTabBody::IsUsingInGameBound() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	return PreviewComponent != NULL && PreviewComponent->IsUsingInGameBounds();
}

void SAnimationEditorViewportTabBody::SetPreviewComponent(class UDebugSkelMeshComponent* PreviewComponent)
{
	PreviewComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewComponent->bCanHighlightSelectedSections = true;

	//Set preview skeleton mesh component
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetPreviewMeshComponent(PreviewComponent);

	AnimViewportClient->FocusViewportOnPreviewMesh();

	// Adding the component to the PreviewScene can change AnimScriptInstance so we must re register it
	// with the AnimBlueprint
	UAnimBlueprint* SourceBlueprint = Cast<UAnimBlueprint>(PersonaPtr.Pin()->GetBlueprintObj());
	if ( SourceBlueprint && PreviewComponent->IsAnimBlueprintInstanced() )
	{
		SourceBlueprint->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
	}

	PopulateNumUVChannels();
}

void SAnimationEditorViewportTabBody::AnimChanged(UAnimationAsset* AnimAsset)
{
	UpdateScrubPanel(AnimAsset);
}

void SAnimationEditorViewportTabBody::ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 NewUVSelection = UVChannels.Find(NewSelection);

	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetUVChannelToDraw(NewUVSelection);

	RefreshViewport();
}

void SAnimationEditorViewportTabBody::PopulateNumUVChannels()
{
	NumUVChannels.Empty();

	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		if (FSkeletalMeshResource* MeshResource = PreviewComponent->GetSkeletalMeshResource())
		{
			int32 NumLods = MeshResource->LODModels.Num();
			NumUVChannels.AddZeroed(NumLods);
			for(int32 LOD = 0; LOD < NumLods; ++LOD)
			{
				NumUVChannels[LOD] = MeshResource->LODModels[LOD].VertexBufferGPUSkin.GetNumTexCoords();
			}
		}
	}

	PopulateUVChoices();
}

void SAnimationEditorViewportTabBody::PopulateUVChoices()
{
	// Fill out the UV channels combo.
	UVChannels.Empty();
	
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		int32 CurrentLOD = FMath::Clamp(PreviewComponent->ForcedLodModel - 1, 0, NumUVChannels.Num() - 1);

		if (NumUVChannels.IsValidIndex(CurrentLOD))
		{
			for (int32 UVChannelID = 0; UVChannelID < NumUVChannels[CurrentLOD]; ++UVChannelID)
			{
				UVChannels.Add( MakeShareable( new FString( FText::Format( NSLOCTEXT("AnimationEditorViewport", "UVChannel_ID", "UV Channel {0}"), FText::AsNumber( UVChannelID ) ).ToString() ) ) );
			}

			TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
			int32 CurrentUVChannel = AnimViewportClient->GetUVChannelToDraw();
			if (!UVChannels.IsValidIndex(CurrentUVChannel))
			{
				CurrentUVChannel = 0;
			}

			AnimViewportClient->SetUVChannelToDraw(CurrentUVChannel);

			if (UVChannelCombo.IsValid() && UVChannels.IsValidIndex(CurrentUVChannel))
			{
				UVChannelCombo->SetSelectedItem(UVChannels[CurrentUVChannel]);
			}
		}
	}
}

void SAnimationEditorViewportTabBody::UpdateScrubPanel(UAnimationAsset* AnimAsset)
{
	// We might not have a scrub panel if we're in animation mode.
	if (ScrubPanelContainer.IsValid())
	{
		ScrubPanelContainer->ClearChildren();
		bool bUseDefaultScrubPanel = true;
		if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimAsset))
		{
			ScrubPanelContainer->AddSlot()
				.AutoHeight()
				[
					SNew(SAnimMontageScrubPanel)
					.Persona(PersonaPtr)
					.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
					.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
					.bAllowZoom(true)
				];
			bUseDefaultScrubPanel = false;
		}
		if(bUseDefaultScrubPanel)
		{
			ScrubPanelContainer->AddSlot()
				.AutoHeight()
				[
					SNew(SAnimationScrubPanel)
					.Persona(PersonaPtr)
					.ViewInputMin(this, &SAnimationEditorViewportTabBody::GetViewMinInput)
					.ViewInputMax(this, &SAnimationEditorViewportTabBody::GetViewMaxInput)
					.bAllowZoom(true)
				];
		}
	}
}

float SAnimationEditorViewportTabBody::GetViewMinInput() const
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		UObject* PreviewAsset = PersonaPtr.Pin()->GetPreviewAnimationAsset();
		if (PreviewAsset != NULL)
		{
			return 0.0f;
		}
		else if (PreviewComponent->GetAnimInstance() != NULL)
		{
			return FMath::Max<float>((float)(PreviewComponent->GetAnimInstance()->LifeTimer - 30.0), 0.0f);
		}
	}

	return 0.f; 
}

float SAnimationEditorViewportTabBody::GetViewMaxInput() const
{ 
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if (PreviewComponent != NULL)
	{
		UObject* PreviewAsset = PersonaPtr.Pin()->GetPreviewAnimationAsset();
		if ((PreviewAsset != NULL) && (PreviewComponent->PreviewInstance != NULL))
		{
			return PreviewComponent->PreviewInstance->GetLength();
		}
		else if (PreviewComponent->GetAnimInstance() != NULL)
		{
			return PreviewComponent->GetAnimInstance()->LifeTimer;
		}
	}

	return 0.f;
}

void SAnimationEditorViewportTabBody::UpdateShowFlagForMeshEdges()
{
	bool bDrawBonesInfluence = false;
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		bDrawBonesInfluence = PreviewComponent->bDrawBoneInfluences;
	}

	//@TODO: SNOWPOCALYPSE: broke UnlitWithMeshEdges
	bool bShowMeshEdgesViewMode = false;
#if 0
	bShowMeshEdgesViewMode = (CurrentViewMode == EAnimationEditorViewportMode::UnlitWithMeshEdges);
#endif

	LevelViewportClient->EngineShowFlags.SetMeshEdges(bDrawBonesInfluence || bShowMeshEdgesViewMode);
}

bool SAnimationEditorViewportTabBody::IsLODModelSelected(int32 LODSelectionType) const
{
	return (LODSelection == LODSelectionType) ? true : false;
}

void SAnimationEditorViewportTabBody::OnSetLODModel(int32 LODSelectionType)
{
	LODSelection = LODSelectionType;

	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;
	if( PreviewComponent )
	{
		PreviewComponent->ForcedLodModel = LODSelection;
		PopulateUVChoices();
	}
}

FLinearColor SAnimationEditorViewportTabBody::GetViewportBackgroundColor() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->GetBackgroundColor();
}

void SAnimationEditorViewportTabBody::SetViewportBackgroundColor(FLinearColor InColor)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetBackgroundColor( InColor );
}

float SAnimationEditorViewportTabBody::GetBackgroundBrightness() const
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return AnimViewportClient->GetBrightnessValue();
}

void SAnimationEditorViewportTabBody::SetBackgroundBrightness(float Value)
{
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetBrightnessValue(Value);
	RefreshViewport();
}

TSharedRef<FAnimationViewportClient> SAnimationEditorViewportTabBody::GetAnimationViewportClient() const
{
	return StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
}

void SAnimationEditorViewportTabBody::ToggleCameraFollow()
{
	// Switch to rotation mode
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	AnimViewportClient->SetCameraFollow();
}

bool SAnimationEditorViewportTabBody::IsCameraFollowEnabled() const
{
	// need a single selected bone in the skeletal tree
	TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());
	return (AnimViewportClient->IsSetCameraFollowChecked());
}


bool SAnimationEditorViewportTabBody::CanChangeCameraMode() const
{
	//Not allowed to change camera type when we are in an ortho camera
	return !LevelViewportClient->IsOrtho();
}

void SAnimationEditorViewportTabBody::SaveData(class SAnimationEditorViewportTabBody* OldViewport)
{
	if ( PersonaPtr.IsValid() && OldViewport )
	{
		FPersonaModeSharedData& SharedData = PersonaPtr.Pin()->ModeSharedData;

		TSharedRef<FAnimationViewportClient> OldAnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(OldViewport->LevelViewportClient.ToSharedRef());
		// set camera set up
		SharedData.ViewLocation = OldAnimViewportClient.Get().GetViewLocation();
		SharedData.ViewRotation = OldAnimViewportClient.Get().GetViewRotation();
		SharedData.LookAtLocation = OldAnimViewportClient.Get().GetLookAtLocation();
		SharedData.OrthoZoom = OldAnimViewportClient.Get().GetOrthoZoom();
		SharedData.bCameraLock = OldAnimViewportClient.Get().IsCameraLocked();
		SharedData.bCameraFollow = OldAnimViewportClient.Get().IsSetCameraFollowChecked();
		SharedData.bShowBound = OldAnimViewportClient.Get().IsSetShowBoundsChecked();
		SharedData.LocalAxesMode = OldAnimViewportClient.Get().GetLocalAxesMode();
		SharedData.ViewportType = OldAnimViewportClient.Get().ViewportType;
		SharedData.PlaybackSpeedMode = OldViewport->AnimationPlaybackSpeedMode;
	}
}

void SAnimationEditorViewportTabBody::RestoreData()
{
	if ( PersonaPtr.IsValid() )
	{
		FPersonaModeSharedData& SharedData = PersonaPtr.Pin()->ModeSharedData;
		TSharedRef<FAnimationViewportClient> AnimViewportClient = StaticCastSharedRef<FAnimationViewportClient>(LevelViewportClient.ToSharedRef());


		AnimViewportClient.Get().SetViewportType((ELevelViewportType)SharedData.ViewportType);
		AnimViewportClient.Get().SetViewLocation( SharedData.ViewLocation );
		AnimViewportClient.Get().SetViewRotation( SharedData.ViewRotation );
		AnimViewportClient.Get().SetShowBounds(SharedData.bShowBound);
		AnimViewportClient.Get().SetLocalAxesMode((ELocalAxesMode::Type)SharedData.LocalAxesMode);
		AnimViewportClient.Get().SetOrthoZoom(SharedData.OrthoZoom);

		OnSetPlaybackSpeed(SharedData.PlaybackSpeedMode);

		if (SharedData.bCameraLock)
		{
			AnimViewportClient.Get().SetLookAtLocation( SharedData.LookAtLocation );
		}
		else if(SharedData.bCameraFollow)
		{
			AnimViewportClient.Get().SetCameraFollow();
		}
	}
}

void SAnimationEditorViewportTabBody::OnLODChanged()
{
	int32 LodCount = 0;

	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent && PreviewComponent->SkeletalMesh )
	{
		LodCount = PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num();
	}

	// If the currently selected LoD is invalid, revert to LOD_Auto
	if ( LodCount < LODSelection )
	{
		OnSetLODModel( 0 );
	}

	if (PersonaPtr.Pin()->PersonaMeshDetailLayout)
	{
		PersonaPtr.Pin()->PersonaMeshDetailLayout->ForceRefreshDetails();
	}
}

void SAnimationEditorViewportTabBody::OnMuteAudio()
{
	GetAnimationViewportClient()->OnToggleMuteAudio();
}

bool SAnimationEditorViewportTabBody::IsAudioMuted()
{
	return GetAnimationViewportClient()->IsAudioMuted();
}

void SAnimationEditorViewportTabBody::OnTogglePreviewRootMotion()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if (PreviewComponent)
	{
		PreviewComponent->bPreviewRootMotion = !PreviewComponent->bPreviewRootMotion;
	}
}

bool SAnimationEditorViewportTabBody::IsPreviewingRootMotion() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if (PreviewComponent)
	{
		return PreviewComponent->bPreviewRootMotion;
	}
	return false;
}

#if WITH_APEX_CLOTHING
bool SAnimationEditorViewportTabBody::IsDisablingClothSimulation() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisableClothSimulation;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnDisableClothSimulation()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisableClothSimulation = !PreviewComponent->bDisableClothSimulation;

		// if the user turns on cloth simulation option while displaying max distances, then turns off max distance option
		if(!PreviewComponent->bDisableClothSimulation && PreviewComponent->bDisplayClothMaxDistances)
		{
			PreviewComponent->bDisplayClothMaxDistances = false;
		}

		// if the user turns on cloth simulation option while displaying back stops, then turns off back stops option
		if (!PreviewComponent->bDisableClothSimulation && PreviewComponent->bDisplayClothBackstops)
		{
			PreviewComponent->bDisplayClothBackstops = false;
		}

		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsApplyingClothWind() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->IsWindEnabled();
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnApplyClothWind()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bEnableWind = !PreviewComponent->IsWindEnabled();
		GetAnimationViewportClient()->EnableWindActor(PreviewComponent->IsWindEnabled());
		RefreshViewport();
	}
}

void SAnimationEditorViewportTabBody::SetWindStrength(float SliderPos)
{
	GetAnimationViewportClient()->SetWindStrength( SliderPos );
	RefreshViewport();
}

float SAnimationEditorViewportTabBody::GetWindStrengthSliderValue() const
{
	return GetAnimationViewportClient()->GetWindStrengthSliderValue();
}

FText SAnimationEditorViewportTabBody::GetWindStrengthLabel() const
{
	return GetAnimationViewportClient()->GetWindStrengthLabel();
}

void SAnimationEditorViewportTabBody::OnShowClothSimulationNormals()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothingNormals = !PreviewComponent->bDisplayClothingNormals;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothSimulationNormals() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisplayClothingNormals;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothGraphicalTangents()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothingTangents = !PreviewComponent->bDisplayClothingTangents;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothGraphicalTangents() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisplayClothingTangents;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothCollisionVolumes()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothingCollisionVolumes = !PreviewComponent->bDisplayClothingCollisionVolumes;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothCollisionVolumes() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisplayClothingCollisionVolumes;
	}

	return false;
}

void SAnimationEditorViewportTabBody::SetGravityScale(float SliderPos)
{
	GetAnimationViewportClient()->SetGravityScale(SliderPos);
	RefreshViewport();
}

float SAnimationEditorViewportTabBody::GetGravityScaleSliderValue() const
{
	return GetAnimationViewportClient()->GetGravityScaleSliderValue();
}

FText SAnimationEditorViewportTabBody::GetGravityScaleLabel() const
{
	return GetAnimationViewportClient()->GetGravityScaleLabel();
}

void SAnimationEditorViewportTabBody::OnEnableCollisionWithAttachedClothChildren()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bCollideWithAttachedChildren = !PreviewComponent->bCollideWithAttachedChildren;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsEnablingCollisionWithAttachedClothChildren() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bCollideWithAttachedChildren;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothPhysicalMeshWire()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothPhysicalMeshWire = !PreviewComponent->bDisplayClothPhysicalMeshWire;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothPhysicalMeshWire() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisplayClothPhysicalMeshWire;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothMaxDistances()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothMaxDistances = !PreviewComponent->bDisplayClothMaxDistances;

		// disables cloth simulation and stop animation because showing max distances is only useful when the cloth is not moving
		if(PreviewComponent->bDisplayClothMaxDistances)
		{
			PreviewComponent->bPrevDisableClothSimulation = PreviewComponent->bDisableClothSimulation;
			PreviewComponent->bDisableClothSimulation = true;
			PreviewComponent->InitAnim(false);
		}
		else
		{
			// restore previous state
			PreviewComponent->bDisableClothSimulation = PreviewComponent->bPrevDisableClothSimulation;
		}

		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothMaxDistances() const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		return PreviewComponent->bDisplayClothMaxDistances;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothBackstops()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if( PreviewComponent )
	{
		PreviewComponent->bDisplayClothBackstops = !PreviewComponent->bDisplayClothBackstops;
		// disables cloth simulation and stop animation because showing back stops is only useful when the cloth is not moving
		if (PreviewComponent->bDisplayClothBackstops)
		{
			PreviewComponent->bPrevDisableClothSimulation = PreviewComponent->bDisableClothSimulation;
			PreviewComponent->bDisableClothSimulation = true;
			PreviewComponent->InitAnim(false);
		}
		else
		{
			// restore previous state
			PreviewComponent->bDisableClothSimulation = PreviewComponent->bPrevDisableClothSimulation;
		}
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothBackstops() const
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		return PreviewComponent->bDisplayClothBackstops;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnShowClothFixedVertices()
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		PreviewComponent->bDisplayClothFixedVertices = !PreviewComponent->bDisplayClothFixedVertices;
		RefreshViewport();
	}
}

bool SAnimationEditorViewportTabBody::IsShowingClothFixedVertices() const
{
	if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent)
	{
		return PreviewComponent->bDisplayClothFixedVertices;
	}

	return false;
}

void SAnimationEditorViewportTabBody::OnSetSectionsDisplayMode(int32 DisplayMode)
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if (!PreviewComponent)
	{
		return;
	}

	PreviewComponent->SectionsDisplayMode = DisplayMode;

	switch (DisplayMode)
	{
	case UDebugSkelMeshComponent::ESectionDisplayMode::ShowAll:
		// restore to the original states
		PreviewComponent->RestoreClothSectionsVisibility();
		break;
	case UDebugSkelMeshComponent::ESectionDisplayMode::ShowOnlyClothSections:
		// disable all except clothing sections and shows only cloth sections
		PreviewComponent->ToggleClothSectionsVisibility(true);
		break;
	case UDebugSkelMeshComponent::ESectionDisplayMode::HideOnlyClothSections:
		// disables only clothing sections
		PreviewComponent->ToggleClothSectionsVisibility(false);
		break;
	}

	RefreshViewport();
}

bool SAnimationEditorViewportTabBody::IsSectionsDisplayMode(int32 DisplayMode) const
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->PreviewComponent;

	if (ensure(PreviewComponent))
	{
		return (PreviewComponent->SectionsDisplayMode == DisplayMode);
	}

	return false;
}
#endif // #if WITH_APEX_CLOTHING

EVisibility SAnimationEditorViewportTabBody::GetViewportCornerImageVisibility() const
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();	
	return Persona->IsRecording() ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SAnimationEditorViewportTabBody::GetViewportCornerImage() const
{
	static int32 Count=0;

	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
//	if(Persona->Recorder.InRecording())
	{
		if(Count++ < 5)
		{
			return FEditorStyle::GetBrush("Persona.StopRecordAnimation");
		}
		else
		{
			if(Count == 10)
			{
				Count = 0;
			}

			return FEditorStyle::GetBrush("Persona.StopRecordAnimation_Alt");
		}
	}

//	return NULL;
}

EVisibility SAnimationEditorViewportTabBody::GetViewportCornerTextVisibility() const
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona->IsRecording())
	{
		return EVisibility::Visible;
	}
	else if (Persona->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		if (UBlueprint* Blueprint = Persona->GetBlueprintObj())
		{
			const bool bUpToDate = (Blueprint->Status == BS_UpToDate) || (Blueprint->Status == BS_UpToDateWithWarnings);
			return bUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
		}		
	}

	return EVisibility::Collapsed;
}

FText SAnimationEditorViewportTabBody::GetViewportCornerText() const
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if(Persona->IsRecording())
	{
		UAnimSequence* Recording = Persona->GetCurrentRecording();
		const FString& Name = Recording ? Recording->GetName() : TEXT("None");
		float TimeRecorded = Persona->GetCurrentRecordingTime();
		FNumberFormattingOptions NumberOption;
		NumberOption.MaximumFractionalDigits = 2;
		NumberOption.MinimumFractionalDigits = 2;
		return FText::Format(LOCTEXT("AnimRecorder", "Recording '{0}' - Time {1} sec(s)]\nTo stop, click here. "),
			FText::FromString(Name), FText::AsNumber(TimeRecorded, &NumberOption));
	}
	if (Persona->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		if (UBlueprint* Blueprint = Persona->GetBlueprintObj())
		{
			switch (Blueprint->Status)
			{
			case BS_UpToDate:
			case BS_UpToDateWithWarnings:
				// Fall thru and return empty string
				break;
			case BS_Dirty:
				return LOCTEXT("AnimBP_Dirty", "Preview out of date\nClick to recompile");
			case BS_Error:
				return LOCTEXT("AnimBP_CompileError", "Compile Error");
			default:
				return LOCTEXT("AnimBP_UnknownStatus", "Unknown Status");
			}
		}		
	}

	return FText::GetEmpty();
}

FText SAnimationEditorViewportTabBody::GetViewportCornerTooltip() const
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if(Persona->IsRecording())
	{
		return LOCTEXT("RecordingStatusTooltip", "Shows the status of animation recording.\nClick to stop the recording.");
	}
	if(Persona->IsModeCurrent(FPersonaModes::AnimBlueprintEditMode))
	{
		return LOCTEXT("BlueprintStatusTooltip", "Shows the status of the animation blueprint.\nClick to recompile a dirty blueprint");
	}

return FText::GetEmpty();
}

FReply SAnimationEditorViewportTabBody::ClickedOnViewportCornerText()
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	// if it's recording, it won't be able to see the message
	// so disable it
	if (Persona->IsRecording())
	{
		Persona->StopRecording();
	}
	else 
	{
		Persona->RecompileAnimBlueprintIfDirty();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
