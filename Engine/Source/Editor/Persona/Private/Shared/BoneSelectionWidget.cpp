// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "BoneSelectionWidget.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "SBoneSelectionWidget"

/////////////////////////////////////////////////////

void SBoneSelectionWidget::Construct(const FArguments& InArgs)
{
	if(InArgs._Skeleton.IsValid())
	{
		TargetSkeleton = InArgs._Skeleton.Get();
	}

	check(TargetSkeleton);

	OnBoneSelectionChanged = InArgs._OnBoneSelectionChanged;
	OnGetSelectedBone = InArgs._OnGetSelectedBone;

	FText FinalTooltip = FText::Format(LOCTEXT("BoneClickToolTip", "{0}\nClick to choose a different bone"), InArgs._Tooltip);

	SAssignNew(TreeView, STreeView<TSharedPtr<FBoneNameInfo>>)
		.TreeItemsSource(&SkeletonTreeInfo)
		.OnGenerateRow(this, &SBoneSelectionWidget::MakeTreeRowWidget)
		.OnGetChildren(this, &SBoneSelectionWidget::GetChildrenForInfo)
		.OnSelectionChanged(this, &SBoneSelectionWidget::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Single);

	this->ChildSlot
	[
		SAssignNew(BonePickerButton, SComboButton)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SBoneSelectionWidget::CreateSkeletonWidgetMenu))
		.ContentPadding(0)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SBoneSelectionWidget::GetCurrentBoneName)
			.ToolTipText(FinalTooltip)
		]
	];
}

TSharedRef<SWidget> SBoneSelectionWidget::CreateSkeletonWidgetMenu()
{
	RebuildBoneList();

	TSharedPtr<SSearchBox> SearchWidgetToFocus = NULL;
	TSharedRef<SBorder> MenuWidget = SNew(SBorder)
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
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
						.Text(LOCTEXT("BonePickerTitle", "Pick Bone..."))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(FEditorStyle::GetBrush("Menu.Separator"))
						.Orientation(Orient_Horizontal)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SearchWidgetToFocus, SSearchBox)
					.SelectAllTextWhenFocused(true)
					.OnTextChanged(this, &SBoneSelectionWidget::OnFilterTextChanged)
					.OnTextCommitted(this, &SBoneSelectionWidget::OnFilterTextCommitted)
					.HintText(NSLOCTEXT("BonePicker", "Search", "Search..."))
				]
				+SVerticalBox::Slot()
				[
					TreeView->AsShared()
				]
			]
		];

	BonePickerButton->SetMenuContentWidgetToFocus(SearchWidgetToFocus);

	return MenuWidget;
}

TSharedRef<ITableRow> SBoneSelectionWidget::MakeTreeRowWidget(TSharedPtr<FBoneNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FBoneNameInfo>>, OwnerTable)
		.Content()
		[
			SNew(STextBlock)
			.HighlightText(FilterText)
			.Text(FText::FromName(InInfo->BoneName))
		];
}

void SBoneSelectionWidget::GetChildrenForInfo(TSharedPtr<FBoneNameInfo> InInfo, TArray< TSharedPtr<FBoneNameInfo> >& OutChildren)
{
	OutChildren = InInfo->Children;
}

void SBoneSelectionWidget::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;

	RebuildBoneList();
}

void SBoneSelectionWidget::OnFilterTextCommitted(const FText& SearchText, ETextCommit::Type CommitInfo)
{
	// Already committed as the text was typed
}

void SBoneSelectionWidget::RebuildBoneList()
{
	SkeletonTreeInfo.Empty();
	SkeletonTreeInfoFlat.Empty();
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

void SBoneSelectionWidget::OnSelectionChanged(TSharedPtr<FBoneNameInfo> BoneInfo, ESelectInfo::Type SelectInfo)
{
	FilterText = FText::FromString("");

	//Because we recreate all our items on tree refresh we will get a spurious null selection event initially.
	if (BoneInfo.IsValid())
	{
		if (OnBoneSelectionChanged.IsBound())
		{
			OnBoneSelectionChanged.Execute(BoneInfo->BoneName);
		}

		BonePickerButton->SetIsOpen(false);
	}
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

