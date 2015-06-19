// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Animation/AnimSet.h"
#include "SSkeletonWidget.h"
#include "AssetData.h"
#include "MainFrame.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "IDocumentation.h"
#include "Animation/Rig.h"
#include "Editor/AnimGraph/Classes/AnimPreviewInstance.h"
#include "SceneViewport.h"
#include "NotificationManager.h"
#include "SNotificationList.h"

#define LOCTEXT_NAMESPACE "SkeletonWidget"

void SSkeletonListWidget::Construct(const FArguments& InArgs)
{
	CurSelectedSkeleton = NULL;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonListWidget::SkeletonSelectionChanged);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectSkeletonLabel", "Select Skeleton: "))
			]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				[
					SNew(SBorder)
					.Content()
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					]
				]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				.Expose(BoneListSlot)

		];

	// Construct the BoneListSlot by clearing the skeleton selection. 
	SkeletonSelectionChanged(FAssetData());
}

void SSkeletonListWidget::SkeletonSelectionChanged(const FAssetData& AssetData)
{
	BoneList.Empty();
	CurSelectedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

	if (CurSelectedSkeleton != NULL)
	{
		const FReferenceSkeleton& RefSkeleton = CurSelectedSkeleton->GetReferenceSkeleton();

		for (int32 I=0; I<RefSkeleton.GetNum(); ++I)
		{
			BoneList.Add( MakeShareable(new FName(RefSkeleton.GetBoneName(I))) ) ;
		}

		(*BoneListSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(SListView< TSharedPtr<FName> >)
					.OnGenerateRow(this, &SSkeletonListWidget::GenerateSkeletonBoneRow)
					.ListItemsSource(&BoneList)
					.HeaderRow
					(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("Bone Name"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "BoneName", "Bone Name"))
					)
				]
			];
	}
	else
	{
		(*BoneListSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SkeletonWidget", "NoSkeletonIsSelected", "No skeleton is selected!"))
				]
			];
	}
}

void SSkeletonCompareWidget::Construct(const FArguments& InArgs)
{
	const UObject* Object = InArgs._Object;

	CurSelectedSkeleton = NULL;
	BoneNames = *InArgs._BoneNames;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonCompareWidget::SkeletonSelectionChanged);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;

	TSharedPtr<SToolTip> SkeletonTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Pick a skeleton for this mesh"), NULL, FString("Shared/Editors/Persona"), FString("Skeleton"));

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
				 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CurrentlySelectedSkeletonLabel_SelectSkeleton", "Select Skeleton"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
						.ToolTip(SkeletonTooltip)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						IDocumentation::Get()->CreateAnchor(FString("Engine/Animation/Skeleton"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2, 10)
				.HAlign(HAlign_Fill)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CurrentlySelectedSkeletonLabel", "Currently Selected : "))
				]
				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Object->GetFullName()))
				]
			]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				[
					SNew(SBorder)
					.Content()
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					]
				]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				.Expose(BonePairSlot)
		];

	// Construct the BonePairSlot by clearing the skeleton selection. 
	SkeletonSelectionChanged(FAssetData());
}

void SSkeletonCompareWidget::SkeletonSelectionChanged(const FAssetData& AssetData)
{
	BonePairList.Empty();
	CurSelectedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

	if (CurSelectedSkeleton != NULL)
	{
		for (int32 I=0; I<BoneNames.Num(); ++I)
		{
			if ( CurSelectedSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneNames[I]) != INDEX_NONE )
			{
				BonePairList.Add( MakeShareable(new FBoneTrackPair(BoneNames[I], BoneNames[I])) );
			}
			else
			{
				BonePairList.Add( MakeShareable(new FBoneTrackPair(BoneNames[I], TEXT(""))) );
			}
		}

		(*BonePairSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(SListView< TSharedPtr<FBoneTrackPair> >)
					.OnGenerateRow(this, &SSkeletonCompareWidget::GenerateBonePairRow)
					.ListItemsSource(&BonePairList)
					.HeaderRow
					(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("Curretly Selected"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "CurrentlySelected", "Currently Selected"))
					+SHeaderRow::Column(TEXT("Target Skeleton Bone"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "TargetSkeletonBone", "Target Skeleton Bone"))
					)
				]
			];
	}
	else
	{
		(*BonePairSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoSkeletonSelectedLabel", "No skeleton is selected!"))
				]
			];
	}
}

void SSkeletonSelectorWindow::Construct(const FArguments& InArgs)
{
	UObject* Object = InArgs._Object;
	WidgetWindow = InArgs._WidgetWindow;
	SelectedSkeleton = NULL;
	if (Object == NULL)
	{
		ConstructWindow();
	}
	else if (Object->IsA(USkeletalMesh::StaticClass()))
	{
		ConstructWindowFromMesh(CastChecked<USkeletalMesh>(Object));
	}
	else if (Object->IsA(UAnimSet::StaticClass()))
	{
		ConstructWindowFromAnimSet(CastChecked<UAnimSet>(Object));
	}
}

void SSkeletonSelectorWindow::ConstructWindowFromAnimSet(UAnimSet* InAnimSet)
{
	TArray<FName>  *TrackNames = &InAnimSet->TrackBoneNames;

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() 
		.FillHeight(1) 
		.Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonCompareWidget)
			.Object(InAnimSet)
			.BoneNames(TrackNames)	
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

void SSkeletonSelectorWindow::ConstructWindowFromMesh(USkeletalMesh* InSkeletalMesh)
{
	TArray<FName>  BoneNames;

	for (int32 I=0; I<InSkeletalMesh->RefSkeleton.GetNum(); ++I)
	{
		BoneNames.Add(InSkeletalMesh->RefSkeleton.GetBoneName(I));
	}

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonCompareWidget)
			.Object(InSkeletalMesh)
			.BoneNames(&BoneNames)
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

void SSkeletonSelectorWindow::ConstructWindow()
{
	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonListWidget)
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

TSharedPtr<SWindow> SAnimationRemapSkeleton::DialogWindow;

bool SAnimationRemapSkeleton::OnShouldFilterAsset(const class FAssetData& AssetData)
{
	USkeleton* AssetSkeleton = NULL;
	if (AssetData.IsAssetLoaded())
	{
		AssetSkeleton = Cast<USkeleton>(AssetData.GetAsset());
	}

	// do not show same skeleton
	if (OldSkeleton && OldSkeleton == AssetSkeleton)
	{
		return true;
	}

	if (bShowOnlyCompatibleSkeletons)
	{
		if(OldSkeleton && OldSkeleton->GetRig())
		{
			URig * Rig = OldSkeleton->GetRig();

			const FString* Value = AssetData.TagsAndValues.Find(USkeleton::RigTag);

			if(Value && Rig->GetFullName() == *Value)
			{
				return false;
			}

			// if loaded, check to see if it has same rig
			if (AssetData.IsAssetLoaded())
			{
				USkeleton* LoadedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

				if (LoadedSkeleton && LoadedSkeleton->GetRig() == Rig)
				{
					return false;
				}
			}
		}

		return true;
	}

	return false;
}

void SAnimationRemapSkeleton::UpdateAssetPicker()
{
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAnimationRemapSkeleton::OnAssetSelectedFromPicker);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAnimationRemapSkeleton::OnShouldFilterAsset);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	if (AssetPickerBox.IsValid())
	{
		AssetPickerBox->SetContent(
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		);
	}
}

void SAnimationRemapSkeleton::Construct( const FArguments& InArgs )
{
	OldSkeleton = InArgs._CurrentSkeleton;
	NewSkeleton = NULL;
	WidgetWindow = InArgs._WidgetWindow;
	bRemapReferencedAssets = true;
	bConvertSpaces = false;
	bShowOnlyCompatibleSkeletons = false;
	OnRetargetAnimationDelegate = InArgs._OnRetargetDelegate;

	TSharedRef<SVerticalBox> Widget = SNew(SVerticalBox);
	if(InArgs._ShowRemapOption)
	{
		Widget->AddSlot()
		[
			SNew(SCheckBox)
			.IsChecked(this, &SAnimationRemapSkeleton::IsRemappingReferencedAssets)
			.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnRemappingReferencedAssetsChanged)
			[
				SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_RemapAssets", "Remap referenced assets "))
			]
		];

		bRemapReferencedAssets = true;
	}

	if (InArgs._ShowConvertSpacesOption)
	{
		TSharedPtr<SToolTip> ConvertSpaceTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Check if you'd like to convert animation data to new skeleton space. If this is false, it won't convert any animation data to new space."),
			NULL, FString("Shared/Editors/Persona"), FString("AnimRemapSkeleton_ConvertSpace"));
		Widget->AddSlot()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SAnimationRemapSkeleton::IsConvertSpacesChecked)
				.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnConvertSpacesCheckChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_ConvertSpaces", "Convert Spaces to new Skeleton"))
					.ToolTip(ConvertSpaceTooltip)
				]
			];

		bConvertSpaces = true;
	}

	if(InArgs._ShowCompatibleDisplayOption)
	{
		TSharedPtr<SToolTip> ConvertSpaceTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Check if you'd like to show only the skeleton that uses the same rig."),
			NULL, FString("Shared/Editors/Persona"), FString("AnimRemapSkeleton_ShowCompatbielSkeletons")); // @todo add tooltip
		Widget->AddSlot()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsChecked)
				.IsEnabled(this, &SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsEnabled)
				.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnShowOnlyCompatibleSkeletonsCheckChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_ShowCompatible", "Show Only Compatible Skeletons"))
					.ToolTip(ConvertSpaceTooltip)
				]
			];

		bShowOnlyCompatibleSkeletons = true;
	}

	TSharedPtr<SToolTip> SkeletonTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Pick a skeleton for this mesh"), NULL, FString("Shared/Editors/Persona"), FString("Skeleton"));

	this->ChildSlot
		[
			SNew (SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CurrentlySelectedSkeletonLabel_SelectSkeleton", "Select Skeleton"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
						.ToolTip(SkeletonTooltip)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						IDocumentation::Get()->CreateAnchor(FString("Engine/Animation/Skeleton"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 10)
				.HAlign(HAlign_Fill)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(5)
				[
					SNew(STextBlock)
					.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10))
					.ColorAndOpacity(FLinearColor::Red)
					.Text(LOCTEXT("RemapSkeleton_Warning_Title", "[WARNING]"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(STextBlock)
					.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10))
					.ColorAndOpacity(FLinearColor::Red)
					.Text(InArgs._WarningMessage)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSeparator)
				]

				+SVerticalBox::Slot()
				.MaxHeight(500)
				[
					SAssignNew(AssetPickerBox, SBox)
					.WidthOverride(400)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					Widget
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSeparator)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("RemapSkeleton_Apply", "Select"))
						.IsEnabled(this, &SAnimationRemapSkeleton::CanApply)
						.OnClicked(this, &SAnimationRemapSkeleton::OnApply)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("RemapSkeleton_Cancel", "Cancel"))
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SAnimationRemapSkeleton::OnCancel)
					]
				]
			]

			+SHorizontalBox::Slot()
			.Padding(2)
			.AutoWidth()
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5, 5)
				[
					// put nice message here
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
					.Text(LOCTEXT("RetargetBasePose_WarningMessage", "Make sure you have the similar retarget base pose. If they don't look alike here, you can edit your base pose in the Retarget Manager window to look alike."))
				]

				+SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SourceSkeleteonTitle", "[Source]"))
							.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.FilterFont"))
							.AutoWrapText(true)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(SourceViewport, SBasePoseViewport)
							.Skeleton(OldSkeleton)
						]
						/*SAssignNew(SourceViewport, SBasePoseViewport)
						.Title(TEXT("[Source]"))
						.Skeleton(OldSkeleton)*/
					]

					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TargetSkeleteonTitle", "[Target]"))
							.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.FilterFont"))
							.AutoWrapText(true)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(TargetViewport, SBasePoseViewport)
							.Skeleton(nullptr)
						]
						/*SAssignNew(TargetViewport, SBasePoseViewport)
						.Title(TEXT("[Target]"))
						.Skeleton(NULL)*/
					]
				]
			]
		];

	UpdateAssetPicker();
}

ECheckBoxState SAnimationRemapSkeleton::IsRemappingReferencedAssets() const
{
	return bRemapReferencedAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnRemappingReferencedAssetsChanged(ECheckBoxState InNewRadioState)
{
	bRemapReferencedAssets = (InNewRadioState == ECheckBoxState::Checked);
}

ECheckBoxState SAnimationRemapSkeleton::IsConvertSpacesChecked() const
{
	return bConvertSpaces ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnConvertSpacesCheckChanged(ECheckBoxState InNewRadioState)
{
	bConvertSpaces = (InNewRadioState == ECheckBoxState::Checked);
}

bool SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsEnabled() const
{
	// if convert space is false, compatible skeletons won't matter either. 
	return (bConvertSpaces == true);
}

ECheckBoxState SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsChecked() const
{
	return bShowOnlyCompatibleSkeletons ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnShowOnlyCompatibleSkeletonsCheckChanged(ECheckBoxState InNewRadioState)
{
	bShowOnlyCompatibleSkeletons = (InNewRadioState == ECheckBoxState::Checked);

	UpdateAssetPicker();
}

bool SAnimationRemapSkeleton::CanApply() const
{
	return (NewSkeleton!=NULL && NewSkeleton!=OldSkeleton);
}

void SAnimationRemapSkeleton::OnAssetSelectedFromPicker(const FAssetData& AssetData)
{
	if (AssetData.GetAsset())
	{
		NewSkeleton = Cast<USkeleton>(AssetData.GetAsset());

		TargetViewport->SetSkeleton(NewSkeleton);
	}
}

FReply SAnimationRemapSkeleton::OnApply()
{
	if (OnRetargetAnimationDelegate.IsBound())
	{
		// verify if old skeleton and new skeleton has preview mesh, otherwise, it won't work. 
		if ( (!OldSkeleton || OldSkeleton->GetPreviewMesh(true)) && (!NewSkeleton || NewSkeleton->GetPreviewMesh(true)))
		{
			OnRetargetAnimationDelegate.Execute(OldSkeleton, NewSkeleton, bRemapReferencedAssets, bConvertSpaces);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("OldSkeletonName"), FText::FromString(GetNameSafe(OldSkeleton)));
			Args.Add(TEXT("NewSkeletonName"), FText::FromString(GetNameSafe(NewSkeleton)));
			FNotificationInfo Info(FText::Format(LOCTEXT("Retarget Failed", "Old Skeleton {OldSkeletonName} and New Skeleton {NewSkeletonName} need to have Preview Mesh set up to convert animation"), Args));
			Info.ExpireDuration = 5.0f;
			Info.bUseLargeFont = false;
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if(Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}

	CloseWindow();
	return FReply::Handled();
}

FReply SAnimationRemapSkeleton::OnCancel()
{
	NewSkeleton = NULL;
	CloseWindow();
	return FReply::Handled();
}

void SAnimationRemapSkeleton::OnRemapDialogClosed(const TSharedRef<SWindow>& Window)
{
	NewSkeleton = NULL;
	DialogWindow = nullptr;
}

void SAnimationRemapSkeleton::CloseWindow()
{
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
}

void SAnimationRemapSkeleton::ShowWindow(USkeleton* OldSkeleton, const FText& WarningMessage, FOnRetargetAnimation RetargetDelegate)
{
	if(DialogWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(DialogWindow.ToSharedRef());
	}

	DialogWindow = SNew(SWindow)
		.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
		.SupportsMinimize(false) .SupportsMaximize(false)
		.SizingRule( ESizingRule::Autosized );

	TSharedPtr<class SAnimationRemapSkeleton> DialogWidget;

	TSharedPtr<SBorder> DialogWrapper = 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SAnimationRemapSkeleton)
			.CurrentSkeleton(OldSkeleton)
			.WidgetWindow(DialogWindow)
			.WarningMessage(WarningMessage)
			.ShowRemapOption(true)
			.ShowConvertSpacesOption(OldSkeleton != NULL)
			.ShowCompatibleDisplayOption(OldSkeleton != NULL)
			.OnRetargetDelegate(RetargetDelegate)
		];

	DialogWindow->SetOnWindowClosed(FRequestDestroyWindowOverride::CreateSP(DialogWidget.Get(), &SAnimationRemapSkeleton::OnRemapDialogClosed));
	DialogWindow->SetContent(DialogWrapper.ToSharedRef());

	FSlateApplication::Get().AddWindow(DialogWindow.ToSharedRef());
}

////////////////////////////////////////////////////

FDlgRemapSkeleton::FDlgRemapSkeleton( USkeleton* Skeleton )
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.SizingRule( ESizingRule::Autosized );

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SAnimationRemapSkeleton)
				.CurrentSkeleton(Skeleton)
				.WidgetWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

bool FDlgRemapSkeleton::ShowModal()
{
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());

	NewSkeleton = DialogWidget.Get()->NewSkeleton;

	return (NewSkeleton != NULL && NewSkeleton != DialogWidget.Get()->OldSkeleton);
}

//////////////////////////////////////////////////////////////
void SRemapFailures::Construct( const FArguments& InArgs )
{
	for ( auto RemapIt = InArgs._FailedRemaps.CreateConstIterator(); RemapIt; ++RemapIt )
	{
		FailedRemaps.Add( MakeShareable( new FText(*RemapIt) ) );
	}

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
			.Padding(FMargin(4, 8, 4, 4))
			[
				SNew(SVerticalBox)

				// Title text
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock) .Text( LOCTEXT("RemapFailureTitle", "The following assets could not be Remaped.") )
				]

				// Failure list
				+SVerticalBox::Slot()
				.Padding(0, 8)
				.FillHeight(1.f)
				[
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					[
						SNew(SListView<TSharedRef<FText>>)
						.ListItemsSource(&FailedRemaps)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &SRemapFailures::MakeListViewWidget)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton) 
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("RemapFailuresCloseButton", "Close"))
						.OnClicked(this, &SRemapFailures::CloseClicked)
					]
				]
			]
		];
}

void SRemapFailures::OpenRemapFailuresDialog(const TArray<FText>& InFailedRemaps)
{
	TSharedRef<SWindow> RemapWindow = SNew(SWindow)
		.Title(LOCTEXT("FailedRemapsDialog", "Failed Remaps"))
		.ClientSize(FVector2D(800,400))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SRemapFailures).FailedRemaps(InFailedRemaps)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

	if ( MainFrameModule.GetParentWindow().IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild(RemapWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(RemapWindow);
	}
}

TSharedRef<ITableRow> SRemapFailures::MakeListViewWidget(TSharedRef<FText> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow< TSharedRef<FText> >, OwnerTable)
		[
			SNew(STextBlock) .Text( Item.Get() )
		];
}

FReply SRemapFailures::CloseClicked()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}

	return FReply::Handled();
}

void SSkeletonBoneRemoval::Construct( const FArguments& InArgs )
{
	bShouldContinue = false;
	WidgetWindow = InArgs._WidgetWindow;

	for (int32 I=0; I<InArgs._BonesToRemove.Num(); ++I)
	{
		BoneNames.Add( MakeShareable(new FName(InArgs._BonesToRemove[I])) ) ;
	}

	this->ChildSlot
	[
		SNew (SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(2) 
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoneRemovalLabel", "Bone Removal"))
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(2, 10)
		.HAlign(HAlign_Fill)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.WrapTextAt(400.f)
			.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 ) )
			.Text( InArgs._WarningMessage )
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(5)
		[
			SNew(SSeparator)
		]

		+SVerticalBox::Slot()
		.MaxHeight(300)
		.Padding(5)
		[
			SNew(SListView< TSharedPtr<FName> >)
			.OnGenerateRow(this, &SSkeletonBoneRemoval::GenerateSkeletonBoneRow)
			.ListItemsSource(&BoneNames)
			.HeaderRow
			(
			SNew(SHeaderRow)
			+SHeaderRow::Column(TEXT("Bone Name"))
			.DefaultLabel(NSLOCTEXT("SkeletonWidget", "BoneName", "Bone Name"))
			)
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(5)
		[
			SNew(SSeparator)
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0,0)
			[
				SNew(SButton) .HAlign(HAlign_Center)
				.Text(LOCTEXT("BoneRemoval_Ok", "Ok"))
				.OnClicked(this, &SSkeletonBoneRemoval::OnOk)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			]
			+SUniformGridPanel::Slot(1,0)
			[
				SNew(SButton) .HAlign(HAlign_Center)
				.Text(LOCTEXT("BoneRemoval_Cancel", "Cancel"))
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SSkeletonBoneRemoval::OnCancel)
			]
		]
	];
}

FReply SSkeletonBoneRemoval::OnOk()
{
	bShouldContinue = true;
	CloseWindow();
	return FReply::Handled();
}

FReply SSkeletonBoneRemoval::OnCancel()
{
	CloseWindow();
	return FReply::Handled();
}

void SSkeletonBoneRemoval::CloseWindow()
{
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
}

bool SSkeletonBoneRemoval::ShowModal(const TArray<FName> BonesToRemove, const FText& WarningMessage)
{
	TSharedPtr<class SSkeletonBoneRemoval> DialogWidget;

	TSharedPtr<SWindow> DialogWindow = SNew(SWindow)
		.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
		.SupportsMinimize(false) .SupportsMaximize(false)
		.SizingRule( ESizingRule::Autosized );

	TSharedPtr<SBorder> DialogWrapper = 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SSkeletonBoneRemoval)
			.WidgetWindow(DialogWindow)
			.BonesToRemove(BonesToRemove)
			.WarningMessage(WarningMessage)
		];

	DialogWindow->SetContent(DialogWrapper.ToSharedRef());

	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());

	return DialogWidget.Get()->bShouldContinue;
}

////////////////////////////////////////

class FBasePoseViewportClient: public FEditorViewportClient
{
public:
	FBasePoseViewportClient(FPreviewScene& InPreviewScene, const TSharedRef<SBasePoseViewport>& InBasePoseViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InBasePoseViewport))
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

		bDisableInput = true;
	}


	// FEditorViewportClient interface
	virtual void Tick(float DeltaTime) override
	{
		if (PreviewScene)
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaTime);
		}
	}

	virtual FSceneInterface* GetScene() const override
	{
		return PreviewScene->GetScene();
	}

	virtual FLinearColor GetBackgroundColor() const override 
	{ 
		return FLinearColor::White; 
	}

	// End of FEditorViewportClient

	void UpdateLighting()
	{
		const UDestructableMeshEditorSettings* Options = GetDefault<UDestructableMeshEditorSettings>();

		PreviewScene->SetLightDirection(Options->AnimPreviewLightingDirection);
		PreviewScene->GetScene()->UpdateDynamicSkyLight(Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewSkyColor), Options->AnimPreviewSkyBrightness * FLinearColor(Options->AnimPreviewFloorColor));
		PreviewScene->SetLightColor(Options->AnimPreviewDirectionalColor);
		PreviewScene->SetLightBrightness(Options->AnimPreviewLightBrightness);
	}
};

////////////////////////////////
// SBasePoseViewport
void SBasePoseViewport::Construct(const FArguments& InArgs)
{
	SEditorViewport::Construct(SEditorViewport::FArguments());

	PreviewComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);

	SetSkeleton(InArgs._Skeleton);
}

void SBasePoseViewport::SetSkeleton(USkeleton* Skeleton)
{
	if(Skeleton != TargetSkeleton)
	{
		TargetSkeleton = Skeleton;

		if(TargetSkeleton)
		{
			USkeletalMesh* PreviewSkeletalMesh = Skeleton->GetPreviewMesh();
			if(PreviewSkeletalMesh)
			{
				PreviewComponent->SetSkeletalMesh(PreviewSkeletalMesh);
				PreviewComponent->EnablePreview(true, NULL, NULL);
//				PreviewComponent->AnimScriptInstance = PreviewComponent->PreviewInstance;
				PreviewComponent->PreviewInstance->bForceRetargetBasePose = true;
				PreviewComponent->RefreshBoneTransforms(NULL);

				//Place the camera at a good viewer position
				FVector NewPosition = Client->GetViewLocation();
				NewPosition.Normalize();
				if(PreviewSkeletalMesh)
				{
					NewPosition *= (PreviewSkeletalMesh->Bounds.SphereRadius*1.5f);
				}
				Client->SetViewLocation(NewPosition);
			}
			else
			{
				PreviewComponent->SetSkeletalMesh(NULL);
			}
		}
		else
		{
			PreviewComponent->SetSkeletalMesh(NULL);
		}

		Client->Invalidate();
	}
}

SBasePoseViewport::SBasePoseViewport()
: PreviewScene(FPreviewScene::ConstructionValues())
{
}

bool SBasePoseViewport::IsVisible() const
{
	return true;
}

TSharedRef<FEditorViewportClient> SBasePoseViewport::MakeEditorViewportClient()
{
	TSharedPtr<FEditorViewportClient> EditorViewportClient = MakeShareable(new FBasePoseViewportClient(PreviewScene, SharedThis(this)));

	EditorViewportClient->ViewportType = LVT_Perspective;
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	EditorViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	EditorViewportClient->SetRealtime(false);
	EditorViewportClient->VisibilityDelegate.BindSP(this, &SBasePoseViewport::IsVisible);
	EditorViewportClient->SetViewMode(VMI_Lit);

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SBasePoseViewport::MakeViewportToolbar()
{
	return nullptr;
}
#undef LOCTEXT_NAMESPACE 

