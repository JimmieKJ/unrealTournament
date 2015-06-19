// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "Animation/VertexAnim/VertexAnimation.h"
#include "SAnimationSequenceBrowser.h"
#include "Persona.h"
#include "AssetRegistryModule.h"
#include "SSkeletonWidget.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "FeedbackContextEditor.h"
#include "EditorAnimUtils.h"
#include "Editor/ContentBrowser/Public/FrontendFilterBase.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "SceneViewport.h"
#include "AnimPreviewInstance.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "SequenceBrowser"

/** A filter that displays animations that are additive */
class FFrontendFilter_AdditiveAnimAssets : public FFrontendFilter
{
public:
	FFrontendFilter_AdditiveAnimAssets(TSharedPtr<FFrontendFilterCategory> InCategory) : FFrontendFilter(InCategory) {}

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("AdditiveAnimAssets"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_AdditiveAnimAssets", "Additive Animations"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_AdditiveAnimAssetsToolTip", "Show only animations that are additive."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override
	{
		const FString TagValue = InItem.TagsAndValues.FindRef(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
		return !TagValue.IsEmpty() && !TagValue.Equals(TEXT("AAT_None"));
	}
};

////////////////////////////////////////////////////

const int32 SAnimationSequenceBrowser::MaxAssetsHistory = 10;

SAnimationSequenceBrowser::~SAnimationSequenceBrowser()
{
	if(PreviewComponent)
	{
		for(int32 ComponentIdx = PreviewComponent->AttachChildren.Num() - 1 ; ComponentIdx >= 0 ; --ComponentIdx)
		{
			USceneComponent* Component = PreviewComponent->AttachChildren[ComponentIdx];
			if(Component)
			{
				CleanupPreviewSceneComponent(Component);
			}
		}
		PreviewComponent->AttachChildren.Empty();
	}

	if(ViewportClient.IsValid())
	{
		ViewportClient->Viewport = NULL;
	}

	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->SetSequenceBrowser(NULL);
	}
}

void SAnimationSequenceBrowser::OnRequestOpenAsset(const FAssetData& AssetData, bool bFromHistory)
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid())
	{
		if (UObject* RawAsset = AssetData.GetAsset())
		{
			if (UAnimationAsset* Asset = Cast<UAnimationAsset>(RawAsset))
			{
				if(!bFromHistory)
				{
					AddAssetToHistory(AssetData);
				}
				Persona->OpenNewDocumentTab(Asset);
				Persona->SetPreviewAnimationAsset(Asset);
			}
			else if(UVertexAnimation* VertexAnim = Cast<UVertexAnimation>(RawAsset))
			{
				if(!bFromHistory)
				{
					AddAssetToHistory(AssetData);
				}
				Persona->SetPreviewVertexAnim(VertexAnim);
			}
		}
	}
}

TSharedPtr<SWidget> SAnimationSequenceBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/ true, NULL);

	bool bHasSelectedAnimSequence = false;
	if ( SelectedAssets.Num() )
	{
		for(auto Iter = SelectedAssets.CreateConstIterator(); Iter; ++Iter)
		{
			UObject* Asset =  Iter->GetAsset();
			if(Cast<UAnimSequence>(Asset))
			{
				bHasSelectedAnimSequence = true;
				break;
			}
		}
	}

	if(bHasSelectedAnimSequence)
	{
		MenuBuilder.BeginSection("AnimationSequenceOptions", NSLOCTEXT("Docking", "TabAnimationHeading", "Animation"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RunCompressionOnAnimations", "Apply Compression"),
				LOCTEXT("RunCompressionOnAnimations_ToolTip", "Apply a compression scheme from the options given to the selected animations"),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SAnimationSequenceBrowser::OnApplyCompression, SelectedAssets),
				FCanExecuteAction()
				)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ExportAnimationsToFBX", "Export to FBX"),
				LOCTEXT("ExportAnimationsToFBX_ToolTip", "Export Animation(s) To FBX"),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SAnimationSequenceBrowser::OnExportToFBX, SelectedAssets),
				FCanExecuteAction()
				)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddLoopingInterpolation", "Add Looping Interpolation"),
				LOCTEXT("AddLoopingInterpolation_ToolTip", "Add an extra frame at the end of the animation to create better looping"),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SAnimationSequenceBrowser::OnAddLoopingInterpolation, SelectedAssets),
				FCanExecuteAction()
				)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ReimportAnimation", "Reimport Animation"),
				LOCTEXT("ReimportAnimation_ToolTip", "Reimport current animaion."),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SAnimationSequenceBrowser::OnReimportAnimation, SelectedAssets),
				FCanExecuteAction()
				)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetCurrentPreviewMesh", "Set Current Preview Mesh"),
				LOCTEXT("SetCurrentPreviewMesh_ToolTip", "Set current preview mesh to be used when previewed by this asset. This only applies when you open Persona using this asset."),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &SAnimationSequenceBrowser::OnSetCurrentPreviewMesh, SelectedAssets),
				FCanExecuteAction()
				)
				);
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("AnimationSequenceOptions", NSLOCTEXT("Docking", "TabOptionsHeading", "Options") );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("GoToInContentBrowser", "Go To Content Browser"),
			LOCTEXT("GoToInContentBrowser_ToolTip", "Select the asset in the content browser"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SAnimationSequenceBrowser::OnGoToInContentBrowser, SelectedAssets ),
			FCanExecuteAction()
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SaveSelectedAssets", "Save"),
			LOCTEXT("SaveSelectedAssets_ToolTip", "Save the selected assets"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAnimationSequenceBrowser::SaveSelectedAssets, SelectedAssets),
				FCanExecuteAction::CreateSP( this, &SAnimationSequenceBrowser::CanSaveSelectedAssets, SelectedAssets)
				)
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimationSequenceAdvancedOptions", NSLOCTEXT("Docking", "TabAdvancedOptionsHeading", "Advanced") );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ChangeSkeleton", "Create a copy for another Skeleton..."),
			LOCTEXT("ChangeSkeleton_ToolTip", "Create a copy for different skeleton"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SAnimationSequenceBrowser::OnCreateCopy, SelectedAssets ),
			FCanExecuteAction()
			)
			);
	}

	return MenuBuilder.MakeWidget();
}

void SAnimationSequenceBrowser::OnGoToInContentBrowser(TArray<FAssetData> ObjectsToSync)
{
	if ( ObjectsToSync.Num() > 0 )
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets( ObjectsToSync );
	}
}

void SAnimationSequenceBrowser::GetSelectedPackages(const TArray<FAssetData>& Assets, TArray<UPackage*>& OutPackages) const
{
	for (int32 AssetIdx = 0; AssetIdx < Assets.Num(); ++AssetIdx)
	{
		UPackage* Package = FindPackage(NULL, *Assets[AssetIdx].PackageName.ToString());

		if ( Package )
		{
			OutPackages.Add(Package);
		}
	}
}

void SAnimationSequenceBrowser::SaveSelectedAssets(TArray<FAssetData> ObjectsToSave) const
{
	TArray<UPackage*> PackagesToSave;
	GetSelectedPackages(ObjectsToSave, PackagesToSave);

	const bool bCheckDirty = false;
	const bool bPromptToSave = false;
	const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
}

bool SAnimationSequenceBrowser::CanSaveSelectedAssets(TArray<FAssetData> ObjectsToSave) const
{
	TArray<UPackage*> Packages;
	GetSelectedPackages(ObjectsToSave, Packages);
	// Don't offer save option if none of the packages are loaded
	return Packages.Num() > 0;
}

void SAnimationSequenceBrowser::OnApplyCompression(TArray<FAssetData> SelectedAssets)
{
	if ( SelectedAssets.Num() > 0 )
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		for(auto Iter = SelectedAssets.CreateIterator(); Iter; ++Iter)
		{
			if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(Iter->GetAsset()) )
			{
				AnimSequences.Add( TWeakObjectPtr<UAnimSequence>(AnimSequence) );
			}
		}

		PersonaPtr.Pin()->ApplyCompression(AnimSequences);
	}
}

void SAnimationSequenceBrowser::OnExportToFBX(TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() > 0)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		for(auto Iter = SelectedAssets.CreateIterator(); Iter; ++Iter)
		{
			if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(Iter->GetAsset()))
			{
				// we only shows anim sequence that belong to this skeleton
				AnimSequences.Add(TWeakObjectPtr<UAnimSequence>(AnimSequence));
			}
		}

		PersonaPtr.Pin()->ExportToFBX(AnimSequences);
	}
}

void SAnimationSequenceBrowser::OnSetCurrentPreviewMesh(TArray<FAssetData> SelectedAssets)
{
	if(SelectedAssets.Num() > 0)
	{
		USkeletalMesh * PreviewMesh = PersonaPtr.Pin()->GetMesh();

		if (PreviewMesh)
		{
			TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
			for(auto Iter = SelectedAssets.CreateIterator(); Iter; ++Iter)
			{
				UAnimationAsset * AnimAsset = Cast<UAnimationAsset>(Iter->GetAsset());
				if (AnimAsset)
				{
					AnimAsset->SetPreviewMesh(PreviewMesh);
				}
			}
		}
	}
}

void SAnimationSequenceBrowser::OnAddLoopingInterpolation(TArray<FAssetData> SelectedAssets)
{
	if(SelectedAssets.Num() > 0)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		for(auto Iter = SelectedAssets.CreateIterator(); Iter; ++Iter)
		{
			if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(Iter->GetAsset()))
			{
				// we only shows anim sequence that belong to this skeleton
				AnimSequences.Add(TWeakObjectPtr<UAnimSequence>(AnimSequence));
			}
		}

		PersonaPtr.Pin()->AddLoopingInterpolation(AnimSequences);
	}
}

void SAnimationSequenceBrowser::OnReimportAnimation(TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() > 0)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		for (auto Iter = SelectedAssets.CreateIterator(); Iter; ++Iter)
		{
			if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Iter->GetAsset()))
			{
				FReimportManager::Instance()->Reimport(AnimSequence, true);
			}
		}
	}
}

void SAnimationSequenceBrowser::RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, TArray<TWeakObjectPtr<UObject>> InAnimAssets)
{
	UObject* AssetToOpen = EditorAnimUtils::RetargetAnimations(OldSkeleton, NewSkeleton, InAnimAssets, bRemapReferencedAssets, true, bConvertSpaces);

	if(UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(AssetToOpen))
	{
		FAssetRegistryModule::AssetCreated(AssetToOpen);
		// once all success, attempt to open new persona module with new skeleton
		EToolkitMode::Type Mode = EToolkitMode::Standalone;
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		PersonaModule.CreatePersona(Mode, TSharedPtr<IToolkitHost>(), NewSkeleton, NULL, AnimAsset, NULL);
	}
}

void SAnimationSequenceBrowser::OnCreateCopy(TArray<FAssetData> Selected)
{
	if ( Selected.Num() > 0 )
	{
		// ask which skeleton users would like to choose
		USkeleton* OldSkeleton = PersonaPtr.Pin()->GetSkeleton();
		USkeleton* NewSkeleton = NULL;
		bool		bDuplicateAssets = true;

		const FText Message = LOCTEXT("RemapSkeleton_Warning", "This will duplicate the asset and convert to new skeleton.");

		TArray<UObject *> AnimAssets;
		for ( auto SelectedAsset : Selected )
		{
			UAnimationAsset* Asset = Cast<UAnimationAsset>(SelectedAsset.GetAsset());
			if (Asset)
			{
				AnimAssets.Add(Asset);
			}
		}

		if (AnimAssets.Num() > 0)
		{
			auto AnimAssetsToConvert = FObjectEditorUtils::GetTypedWeakObjectPtrs<UObject>(AnimAssets);
			// ask user what they'd like to change to 
			SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, FOnRetargetAnimation::CreateSP(this, &SAnimationSequenceBrowser::RetargetAnimationHandler, AnimAssetsToConvert));
		}
	}
}

bool SAnimationSequenceBrowser::CanShowColumnForAssetRegistryTag(FName AssetType, FName TagName) const
{
	return !AssetRegistryTagsToIgnore.Contains(TagName);
}

void SAnimationSequenceBrowser::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;

	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->SetSequenceBrowser(this);
	}

	CurrentAssetHistoryIndex = INDEX_NONE;
	bTriedToCacheOrginalAsset = false;

	bIsActiveTimerRegistered = false;
	bToolTipVisualizedThisFrame = false;
	bToolTipClosedThisFrame = false;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	CreateAssetTooltipResources();

	// Configure filter for asset picker
	FAssetPickerConfig Config;
	Config.Filter.bRecursiveClasses = true;
	Config.Filter.ClassNames.Add(UAnimationAsset::StaticClass()->GetFName());
	Config.Filter.ClassNames.Add(UVertexAnimation::StaticClass()->GetFName()); //@TODO: Is currently ignored due to the skeleton check
	Config.InitialAssetViewType = EAssetViewType::Column;
	Config.bAddFilterUI = true;

	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid())
	{
		USkeleton* DesiredSkeleton = Persona->GetSkeleton();
		if(DesiredSkeleton)
		{
			FString SkeletonString = FAssetData(DesiredSkeleton).GetExportTextName();
			Config.Filter.TagsAndValues.Add(TEXT("Skeleton"), SkeletonString);
		}
	}

	// Configure response to click and double-click
	Config.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SAnimationSequenceBrowser::OnRequestOpenAsset, false);
	Config.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SAnimationSequenceBrowser::OnGetAssetContextMenu);
	Config.OnAssetTagWantsToBeDisplayed = FOnShouldDisplayAssetTag::CreateSP(this, &SAnimationSequenceBrowser::CanShowColumnForAssetRegistryTag);
	Config.SyncToAssetsDelegates.Add(&SyncToAssetsDelegate);
	Config.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	Config.bFocusSearchBoxWhenOpened = false;
	Config.DefaultFilterMenuExpansion = EAssetTypeCategories::Animation;

	TSharedPtr<FFrontendFilterCategory> AnimCategory = MakeShareable( new FFrontendFilterCategory(LOCTEXT("ExtraAnimationFilters", "Anim Filters"), LOCTEXT("ExtraAnimationFiltersTooltip", "Filter assets by all filters in this category.")) );
	Config.ExtraFrontendFilters.Add( MakeShareable(new FFrontendFilter_AdditiveAnimAssets(AnimCategory)) );
	
	Config.OnGetCustomAssetToolTip = FOnGetCustomAssetToolTip::CreateSP(this, &SAnimationSequenceBrowser::CreateCustomAssetToolTip);
	Config.OnVisualizeAssetToolTip = FOnVisualizeAssetToolTip::CreateSP(this, &SAnimationSequenceBrowser::OnVisualizeAssetToolTip);
	Config.OnAssetToolTipClosing = FOnAssetToolTipClosing::CreateSP( this, &SAnimationSequenceBrowser::OnAssetToolTipClosing );

	TSharedRef< SMenuAnchor > BackMenuAnchorPtr = SNew(SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(this, &SAnimationSequenceBrowser::CreateHistoryMenu, true)
		[
			SNew(SButton)
			.OnClicked(this, &SAnimationSequenceBrowser::OnGoBackInHistory)
			.ButtonStyle(FEditorStyle::Get(), "GraphBreadcrumbButton")
			.IsEnabled(this, &SAnimationSequenceBrowser::CanStepBackwardInHistory)
			.ToolTipText(LOCTEXT("Backward_Tooltip", "Step backward in the asset history. Right click to see full history."))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("GraphBreadcrumb.BrowseBack"))
			]
		];

	TSharedRef< SMenuAnchor > FwdMenuAnchorPtr = SNew(SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(this, &SAnimationSequenceBrowser::CreateHistoryMenu, false)
		[
			SNew(SButton)
			.OnClicked(this, &SAnimationSequenceBrowser::OnGoForwardInHistory)
			.ButtonStyle(FEditorStyle::Get(), "GraphBreadcrumbButton")
			.IsEnabled(this, &SAnimationSequenceBrowser::CanStepForwardInHistory)
			.ToolTipText(LOCTEXT("Forward_Tooltip", "Step forward in the asset history. Right click to see full history."))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("GraphBreadcrumb.BrowseForward"))
			]
		];

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(Config)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Visibility(this, &SAnimationSequenceBrowser::GetNonBlueprintModeVisibility)
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SAnimationSequenceBrowser::GetNonBlueprintModeVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.OnMouseButtonDown(this, &SAnimationSequenceBrowser::OnMouseDownHisory, TWeakPtr<SMenuAnchor>(BackMenuAnchorPtr))
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				[
					BackMenuAnchorPtr
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.OnMouseButtonDown(this, &SAnimationSequenceBrowser::OnMouseDownHisory, TWeakPtr<SMenuAnchor>(FwdMenuAnchorPtr))
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				[
					FwdMenuAnchorPtr
				]
			]
		]
	];

	// Create the ignore set for asset registry tags
	// Making Skeleton to be private, and now GET_MEMBER_NAME_CHECKED doesn't work
	AssetRegistryTagsToIgnore.Add(TEXT("Skeleton"));
	AssetRegistryTagsToIgnore.Add(GET_MEMBER_NAME_CHECKED(UAnimSequenceBase, SequenceLength));
	AssetRegistryTagsToIgnore.Add(GET_MEMBER_NAME_CHECKED(UAnimSequenceBase, RateScale));
}

void SAnimationSequenceBrowser::AddAssetToHistory(const FAssetData& AssetData)
{
	CacheOriginalAnimAssetHistory();

	if (CurrentAssetHistoryIndex == AssetHistory.Num() - 1)
	{
		// History added to the end
		if (AssetHistory.Num() == MaxAssetsHistory)
		{
			// If max history entries has been reached
			// remove the oldest history
			AssetHistory.RemoveAt(0);
		}
	}
	else
	{
		// Clear out any history that is in front of the current location in the history list
		AssetHistory.RemoveAt(CurrentAssetHistoryIndex + 1, AssetHistory.Num() - (CurrentAssetHistoryIndex + 1), true);
	}

	AssetHistory.Add(AssetData);
	CurrentAssetHistoryIndex = AssetHistory.Num() - 1;
}

FReply SAnimationSequenceBrowser::OnMouseDownHisory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor )
{
	if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		InMenuAnchor.Pin()->SetIsOpen(true);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SAnimationSequenceBrowser::CreateHistoryMenu(bool bInBackHistory) const
{
	FMenuBuilder MenuBuilder(true, NULL);
	if(bInBackHistory)
	{
		int32 HistoryIdx = CurrentAssetHistoryIndex - 1;
		while( HistoryIdx >= 0 )
		{
			const FAssetData& AssetData = AssetHistory[ HistoryIdx ];

			if(AssetData.IsValid())
			{
				const FText DisplayName = FText::FromName(AssetData.AssetName);
				const FText Tooltip = FText::FromString( AssetData.ObjectPath.ToString() );

				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon(), 
					FUIAction(
					FExecuteAction::CreateRaw(this, &SAnimationSequenceBrowser::GoToHistoryIndex, HistoryIdx)
					), 
					NAME_None, EUserInterfaceActionType::Button);
			}

			--HistoryIdx;
		}
	}
	else
	{
		int32 HistoryIdx = CurrentAssetHistoryIndex + 1;
		while( HistoryIdx < AssetHistory.Num() )
		{
			const FAssetData& AssetData = AssetHistory[ HistoryIdx ];

			if(AssetData.IsValid())
			{
				const FText DisplayName = FText::FromName(AssetData.AssetName);
				const FText Tooltip = FText::FromString( AssetData.ObjectPath.ToString() );

				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon(), 
					FUIAction(
					FExecuteAction::CreateRaw(this, &SAnimationSequenceBrowser::GoToHistoryIndex, HistoryIdx)
					), 
					NAME_None, EUserInterfaceActionType::Button);
			}

			++HistoryIdx;
		}
	}

	return MenuBuilder.MakeWidget();
}

bool SAnimationSequenceBrowser::CanStepBackwardInHistory() const
{
	int32 HistoryIdx = CurrentAssetHistoryIndex - 1;
	while( HistoryIdx >= 0 )
	{
		if(AssetHistory[HistoryIdx].IsValid())
		{
			return true;
		}

		--HistoryIdx;
	}
	return false;
}

bool SAnimationSequenceBrowser::CanStepForwardInHistory() const
{
	int32 HistoryIdx = CurrentAssetHistoryIndex + 1;
	while( HistoryIdx < AssetHistory.Num() )
	{
		if(AssetHistory[HistoryIdx].IsValid())
		{
			return true;
		}

		++HistoryIdx;
	}
	return false;
}

FReply SAnimationSequenceBrowser::OnGoForwardInHistory()
{
	while( CurrentAssetHistoryIndex < AssetHistory.Num() - 1)
	{
		++CurrentAssetHistoryIndex;

		if( AssetHistory[CurrentAssetHistoryIndex].IsValid() )
		{
			GoToHistoryIndex(CurrentAssetHistoryIndex);
			break;
		}
	}
	return FReply::Handled();
}

FReply SAnimationSequenceBrowser::OnGoBackInHistory()
{
	while( CurrentAssetHistoryIndex > 0 )
	{
		--CurrentAssetHistoryIndex;

		if( AssetHistory[CurrentAssetHistoryIndex].IsValid() )
		{
			GoToHistoryIndex(CurrentAssetHistoryIndex);
			break;
		}
	}
	return FReply::Handled();
}

void SAnimationSequenceBrowser::GoToHistoryIndex(int32 InHistoryIdx)
{
	if(AssetHistory[InHistoryIdx].IsValid())
	{
		CurrentAssetHistoryIndex = InHistoryIdx;
		OnRequestOpenAsset(AssetHistory[InHistoryIdx], /**bFromHistory=*/true);
	}
}

void SAnimationSequenceBrowser::CacheOriginalAnimAssetHistory()
{
	/** If we have nothing in the AssetHistory see if we can store 
	anything for where we currently are as we can't do this on construction */
	if (!bTriedToCacheOrginalAsset)
	{
		bTriedToCacheOrginalAsset = true;

		if(AssetHistory.Num() == 0 && PersonaPtr.IsValid())
		{
			USkeleton* DesiredSkeleton = PersonaPtr.Pin()->GetSkeleton();

			if(UObject* PreviewAsset = PersonaPtr.Pin()->GetPreviewAnimationAsset())
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*PreviewAsset->GetPathName()));
				AssetHistory.Add(AssetData);
				CurrentAssetHistoryIndex = AssetHistory.Num() - 1;
			}
		}
	}
}

void SAnimationSequenceBrowser::SelectAsset(UAnimationAsset * AnimAsset)
{
	FAssetData AssetData(AnimAsset);

	if (AssetData.IsValid())
	{
		TArray<FAssetData> CurrentSelection = GetCurrentSelectionDelegate.Execute();

		if ( !CurrentSelection.Contains(AssetData) )
		{
			TArray<FAssetData> AssetsToSelect;
			AssetsToSelect.Add(AssetData);

			SyncToAssetsDelegate.Execute(AssetsToSelect);
		}
	}
}

TSharedRef<SToolTip> SAnimationSequenceBrowser::CreateCustomAssetToolTip(FAssetData& AssetData)
{
	// Make a list of tags to show
	TArray<UObject::FAssetRegistryTag> Tags;
	UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
	check(AssetClass);
	AssetClass->GetDefaultObject()->GetAssetRegistryTags(Tags);

	TArray<FName> TagsToShow;
	for(UObject::FAssetRegistryTag& TagEntry : Tags)
	{
		if(TagEntry.Name != FName(TEXT("Skeleton")) && TagEntry.Type != UObject::FAssetRegistryTag::TT_Hidden)
		{
			TagsToShow.Add(TagEntry.Name);
		}
	}

	// Add asset registry tags to a text list; except skeleton as that is implied in Persona
	TSharedRef<SVerticalBox> DescriptionBox = SNew(SVerticalBox);
	bool bDescriptionCreated = false;
	for(TPair<FName, FString> TagPair : AssetData.TagsAndValues)
	{
		if(TagsToShow.Contains(TagPair.Key))
		{
			if(!bDescriptionCreated)
			{
				bDescriptionCreated = true;
			}

			DescriptionBox->AddSlot()
			.AutoHeight()
			.Padding(0,0,5,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("AssetTagKey", "{0} :"), FText::FromName(TagPair.Key)))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TagPair.Value))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
		}
	}

	TSharedPtr<SHorizontalBox> ContentBox = nullptr;
	TSharedRef<SToolTip> ToolTip = SNew(SToolTip)
	.TextMargin(1)
	.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ToolTipBorder"))
	[
		SNew(SBorder)
		.Padding(6)
		.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,0,0,4)
			[
				SNew(SBorder)
				.Padding(6)
				.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromName(AssetData.AssetName))
						.Font(FEditorStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
					]
				]
			]
		
			+ SVerticalBox::Slot()
			[
				SAssignNew(ContentBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.Padding(6)
					.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NoPreviewMesh", "No Preview Mesh"))
						]

						+ SOverlay::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							ViewportWidget.ToSharedRef()
						]
					]
				]
			]
		]
	];

	// If we have a description, add an extra section to the tooltip for it.
	if(bDescriptionCreated)
	{
		ContentBox->AddSlot()
		.Padding(4, 0, 0, 0)
		[
			SNew(SBorder)
			.Padding(6)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
			[
				DescriptionBox
			]
		];
	}

	return ToolTip;
}

void SAnimationSequenceBrowser::CreateAssetTooltipResources()
{
	SAssignNew(ViewportWidget, SViewport)
		.EnableGammaCorrection(false)
		.ViewportSize(FVector2D(128, 128));

	ViewportClient = MakeShareable(new FAnimationAssetViewportClient(PreviewScene));
	SceneViewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));
	PreviewComponent = NewObject<UDebugSkelMeshComponent>();

	// Client options
	ViewportClient->ViewportType = LVT_Perspective;
	ViewportClient->bSetListenerPosition = false;
	// Default view until we need to show the viewport
	ViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	ViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	ViewportClient->Viewport = SceneViewport.Get();
	ViewportClient->SetRealtime(true);
	ViewportClient->SetViewMode(VMI_Lit);
	ViewportClient->ToggleOrbitCamera(true);
	ViewportClient->VisibilityDelegate.BindSP(this, &SAnimationSequenceBrowser::IsToolTipPreviewVisible);

	// Add the scene viewport
	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());

	// Setup the preview component to ensure an animation will update when requested
	PreviewComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);

	const UDestructableMeshEditorSettings* Options = GetDefault<UDestructableMeshEditorSettings>();

	PreviewScene.SetLightDirection(Options->AnimPreviewLightingDirection);
	PreviewScene.GetScene()->UpdateDynamicSkyLight(Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewSkyColor), Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewFloorColor));
	PreviewScene.SetLightColor(Options->AnimPreviewDirectionalColor);
	PreviewScene.SetLightBrightness(Options->AnimPreviewLightBrightness);
}

bool SAnimationSequenceBrowser::OnVisualizeAssetToolTip(const TSharedPtr<SWidget>& TooltipContent, FAssetData& AssetData)
{
	// Resolve the asset
	USkeletalMesh* MeshToUse = nullptr;
	UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
	if(AssetClass->IsChildOf(UAnimationAsset::StaticClass()) && AssetData.GetAsset())
	{
		// Set up the viewport to show the asset. Catching the visualize allows us to use
		// one viewport between all of the assets in the sequence browser.
		UAnimationAsset* Asset = StaticCast<UAnimationAsset*>(AssetData.GetAsset());
		USkeleton* Skeleton = Asset->GetSkeleton();
		
		MeshToUse = Skeleton->GetAssetPreviewMesh(Asset);
		
		if(MeshToUse)
		{
			if(PreviewComponent->SkeletalMesh != MeshToUse)
			{
				PreviewComponent->SetSkeletalMesh(MeshToUse);
			}

			PreviewComponent->EnablePreview(true, Asset, nullptr);
			PreviewComponent->PreviewInstance->PlayAnim(true);

			float HalfFov = FMath::DegreesToRadians(ViewportClient->ViewFOV) / 2.0f;
			float TargetDist = MeshToUse->Bounds.SphereRadius / FMath::Tan(HalfFov);

			ViewportClient->SetViewRotation(FRotator(0.0f, -45.0f, 0.0f));
			ViewportClient->SetViewLocationForOrbiting(FVector(0.0f, 0.0f, MeshToUse->Bounds.BoxExtent.Z / 2.0f), TargetDist);

			ViewportWidget->SetVisibility(EVisibility::Visible);
			
			// Update the preview as long as the tooltip is visible
			if ( !bIsActiveTimerRegistered )
			{
				bIsActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAnimationSequenceBrowser::UpdateTootipPreview));
			}
			bToolTipVisualizedThisFrame = true;
		}
		else
		{
			ViewportWidget->SetVisibility(EVisibility::Hidden);
		}
	}

	// We return false here as we aren't visualizing the tooltip - just detecting when it is about to be shown.
	// We still want slate to draw it.
	return false;
}

void SAnimationSequenceBrowser::OnAssetToolTipClosing()
{
	// Make sure that the tooltip isn't about to preview another animation
	if (!bToolTipVisualizedThisFrame)
	{
		ViewportWidget->SetVisibility(EVisibility::Hidden);
	}
}

void SAnimationSequenceBrowser::CleanupPreviewSceneComponent(USceneComponent* Component)
{
	if(Component)
	{
		for(int32 ComponentIdx = Component->AttachChildren.Num() - 1 ; ComponentIdx >= 0 ; --ComponentIdx)
		{
			USceneComponent* ChildComponent = Component->AttachChildren[ComponentIdx];
			CleanupPreviewSceneComponent(ChildComponent);
		}
		Component->AttachChildren.Empty();
		Component->DestroyComponent();
	}
}

EActiveTimerReturnType SAnimationSequenceBrowser::UpdateTootipPreview( double InCurrentTime, float InDeltaTime )
{
	bToolTipVisualizedThisFrame = false;
	if ( PreviewComponent && IsToolTipPreviewVisible() )
	{
		// Tick the world to update preview viewport for tooltips
		PreviewComponent->GetScene()->GetWorld()->Tick( LEVELTICK_All, InDeltaTime );
	}
	else
	{
		bIsActiveTimerRegistered = false;
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

bool SAnimationSequenceBrowser::IsToolTipPreviewVisible()
{
	bool bVisible = false;
	// during persona recording, disable this
	if( PersonaPtr.IsValid() && PersonaPtr.Pin()->Recorder.InRecording() == false 
		&& ViewportWidget.IsValid())
	{
		bVisible = ViewportWidget->GetVisibility() == EVisibility::Visible;
	}
	return bVisible;
}

EVisibility SAnimationSequenceBrowser::GetNonBlueprintModeVisibility() const
{
	if (PersonaPtr.IsValid())
	{
		if (PersonaPtr.Pin()->GetCurrentMode() == FPersonaModes::AnimBlueprintEditMode)
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}



FAnimationAssetViewportClient::FAnimationAssetViewportClient(FPreviewScene& InPreviewScene)
	: FEditorViewportClient(nullptr, &InPreviewScene)
{
	SetViewMode(VMI_Lit);

	// Always composite editor objects after post processing in the editor
	EngineShowFlags.CompositeEditorPrimitives = true;
	EngineShowFlags.DisableAdvancedFeatures();

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(70, 70, 70);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor = FColor(20, 20, 20);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
	bDrawAxes = false;
}

FSceneInterface* FAnimationAssetViewportClient::GetScene() const
{
	return PreviewScene->GetScene();
}

FLinearColor FAnimationAssetViewportClient::GetBackgroundColor() const
{
	return FLinearColor(0.8f, 0.85f, 0.85f);
}


#undef LOCTEXT_NAMESPACE
