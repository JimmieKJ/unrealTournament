// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "BoneSelectionWidget.h"
#include "SSearchBox.h"
#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "SBoneSelectionWidget"

/////////////////////////////////////////////////////
void SBoneTreeMenu::Construct(const FArguments& InArgs, TWeakObjectPtr<const USkeleton> Skeleton)
{
	TargetSkeleton = Skeleton;
	OnSelectionChangedDelegate = InArgs._OnBoneSelectionChanged;

	FText TitleToUse = !InArgs._Title.IsEmpty() ? InArgs._Title  : LOCTEXT("BonePickerTitle", "Pick Bone...");

	SAssignNew(TreeView, STreeView<TSharedPtr<FBoneNameInfo>>)
		.TreeItemsSource(&SkeletonTreeInfo)
		.OnGenerateRow(this, &SBoneTreeMenu::MakeTreeRowWidget)
		.OnGetChildren(this, &SBoneTreeMenu::GetChildrenForInfo)
		.OnSelectionChanged(this, &SBoneTreeMenu::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Single);

	RebuildBoneList();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(6)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.Content()
		[
			SNew(SBox)
			.WidthOverride(300)
			.HeightOverride(512)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(TitleToUse)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(FEditorStyle::GetBrush("Menu.Separator"))
					.Orientation(Orient_Horizontal)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(FilterTextWidget, SSearchBox)
					.SelectAllTextWhenFocused(true)
					.OnTextChanged(this, &SBoneTreeMenu::OnFilterTextChanged)
					.HintText(NSLOCTEXT("BonePicker", "Search", "Search..."))
				]
				+ SVerticalBox::Slot()
				[
					TreeView->AsShared()
				]
			]
		]
	];
}

TSharedRef<ITableRow> SBoneTreeMenu::MakeTreeRowWidget(TSharedPtr<FBoneNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FBoneNameInfo>>, OwnerTable)
		.Content()
		[
			SNew(STextBlock)
			.HighlightText(FilterText)
			.Text(FText::FromName(InInfo->BoneName))
		];
}

void SBoneTreeMenu::GetChildrenForInfo(TSharedPtr<FBoneNameInfo> InInfo, TArray< TSharedPtr<FBoneNameInfo> >& OutChildren)
{
	OutChildren = InInfo->Children;
}

void SBoneTreeMenu::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;

	RebuildBoneList();
}

void SBoneTreeMenu::OnSelectionChanged(TSharedPtr<SBoneTreeMenu::FBoneNameInfo> BoneInfo, ESelectInfo::Type SelectInfo)
{
	//Because we recreate all our items on tree refresh we will get a spurious null selection event initially.
	if (BoneInfo.IsValid())
	{
		OnSelectionChangedDelegate.ExecuteIfBound(BoneInfo->BoneName);
	}
}

void SBoneTreeMenu::RebuildBoneList()
{
	SkeletonTreeInfo.Empty();
	SkeletonTreeInfoFlat.Empty();
	if (!TargetSkeleton.IsValid())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = TargetSkeleton->GetReferenceSkeleton();
	for(int32 BoneIdx = 0; BoneIdx < RefSkeleton.GetNum(); ++BoneIdx)
	{
		TSharedRef<FBoneNameInfo> BoneInfo = MakeShareable(new FBoneNameInfo(RefSkeleton.GetBoneName(BoneIdx)));

		// Filter if Necessary
		if(!FilterText.IsEmpty() && !BoneInfo->BoneName.ToString().Contains(FilterText.ToString()))
		{
			continue;
		}

		int32 ParentIdx = RefSkeleton.GetParentIndex(BoneIdx);
		bool bAddToParent = false;

		if(ParentIdx != INDEX_NONE && FilterText.IsEmpty())
		{
			// We have a parent, search for it in the flat list
			FName ParentName = RefSkeleton.GetBoneName(ParentIdx);

			for(int32 FlatListIdx = 0; FlatListIdx < SkeletonTreeInfoFlat.Num(); ++FlatListIdx)
			{
				TSharedPtr<FBoneNameInfo> InfoEntry = SkeletonTreeInfoFlat[FlatListIdx];
				if(InfoEntry->BoneName == ParentName)
				{
					bAddToParent = true;
					ParentIdx = FlatListIdx;
					break;
				}
			}

			if(bAddToParent)
			{
				SkeletonTreeInfoFlat[ParentIdx]->Children.Add(BoneInfo);
			}
			else
			{
				SkeletonTreeInfo.Add(BoneInfo);
			}
		}
		else
		{
			SkeletonTreeInfo.Add(BoneInfo);
		}

		SkeletonTreeInfoFlat.Add(BoneInfo);
		TreeView->SetItemExpansion(BoneInfo, true);
	}

	TreeView->RequestTreeRefresh();
}

/////////////////////////////////////////////////////

void SBoneSelectionWidget::Construct(const FArguments& InArgs, TWeakObjectPtr<const USkeleton> Skeleton)
{
	TargetSkeleton = Skeleton;

	check(TargetSkeleton.IsValid());

	OnBoneSelectionChanged = InArgs._OnBoneSelectionChanged;
	OnGetSelectedBone = InArgs._OnGetSelectedBone;

	FText FinalTooltip = FText::Format(LOCTEXT("BoneClickToolTip", "{0}\nClick to choose a different bone"), InArgs._Tooltip);

	this->ChildSlot
	[
		SAssignNew(BonePickerButton, SComboButton)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SBoneSelectionWidget::CreateSkeletonWidgetMenu))
		.ContentPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SBoneSelectionWidget::GetCurrentBoneName)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(FinalTooltip)
		]
	];
}

TSharedRef<SWidget> SBoneSelectionWidget::CreateSkeletonWidgetMenu()
{
	TSharedRef<SBoneTreeMenu> MenuWidget = SNew(SBoneTreeMenu, TargetSkeleton)
									.OnBoneSelectionChanged(this, &SBoneSelectionWidget::OnSelectionChanged);

	BonePickerButton->SetMenuContentWidgetToFocus(MenuWidget->FilterTextWidget);

	return MenuWidget;
}

void SBoneSelectionWidget::OnSelectionChanged(FName BoneName)
{
	//Because we recreate all our items on tree refresh we will get a spurious null selection event initially.
	if (OnBoneSelectionChanged.IsBound())
	{
		OnBoneSelectionChanged.Execute(BoneName);
	}

	BonePickerButton->SetIsOpen(false);
}

FText SBoneSelectionWidget::GetCurrentBoneName() const
{
	if(OnGetSelectedBone.IsBound())
	{
		FName Name = OnGetSelectedBone.Execute();

		return FText::FromName(Name);
	}

	// @todo implement default solution?
	return FText::GetEmpty();
}
#undef LOCTEXT_NAMESPACE

