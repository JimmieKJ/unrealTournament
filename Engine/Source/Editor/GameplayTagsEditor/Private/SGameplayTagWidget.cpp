// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModulePrivatePCH.h"
#include "SGameplayTagWidget.h"
#include "GameplayTagsModule.h"
#include "GameplayTags.h"
#include "ScopedTransaction.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "SSearchBox.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "GameplayTagWidget"

const FString SGameplayTagWidget::SettingsIniSection = TEXT("GameplayTagWidget");

void SGameplayTagWidget::Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagContainerDatum>& EditableTagContainers)
{
	ensure(EditableTagContainers.Num() > 0);
	TagContainers = EditableTagContainers;

	OnTagChanged = InArgs._OnTagChanged;
	bReadOnly = InArgs._ReadOnly;
	TagContainerName = InArgs._TagContainerName;
	bMultiSelect = InArgs._MultiSelect;
	PropertyHandle = InArgs._PropertyHandle;

	IGameplayTagsModule::Get().GetGameplayTagsManager().GetFilteredGameplayRootTags(InArgs._Filter, TagItems);

	// Tag the assets as transactional so they can support undo/redo
	for (int32 AssetIdx = 0; AssetIdx < TagContainers.Num(); ++AssetIdx)
	{
		UObject* TagContainerOwner = TagContainers[AssetIdx].TagContainerOwner.Get();
		if (TagContainerOwner)
		{
			TagContainerOwner->SetFlags(RF_Transactional);
		}
	}

	ChildSlot
	[
		SNew(SScaleBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Top)
		.StretchDirection(EStretchDirection::DownOnly)
		.Stretch(EStretch::ScaleToFit)
		.Content()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SGameplayTagWidget::OnExpandAllClicked)
						.Text(LOCTEXT("GameplayTagWidget_ExpandAll", "Expand All"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SGameplayTagWidget::OnCollapseAllClicked)
						.Text(LOCTEXT("GameplayTagWidget_CollapseAll", "Collapse All"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.IsEnabled(!bReadOnly)
						.OnClicked(this, &SGameplayTagWidget::OnClearAllClicked)
						.Text(LOCTEXT("GameplayTagWidget_ClearAll", "Clear All"))
					]
					+SHorizontalBox::Slot()
					.VAlign( VAlign_Center )
					.FillWidth(1.f)
					.Padding(5,1,5,1)
					[
						SNew(SSearchBox)
						.HintText(LOCTEXT("GameplayTagWidget_SearchBoxHint", "Search Gameplay Tags"))
						.OnTextChanged( this, &SGameplayTagWidget::OnFilterTextChanged )
					]
				]
				+SVerticalBox::Slot()
				[
					SNew(SBorder)
					.Padding(FMargin(4.f))
					[
						SAssignNew(TagTreeWidget, STreeView< TSharedPtr<FGameplayTagNode> >)
						.TreeItemsSource(&TagItems)
						.OnGenerateRow(this, &SGameplayTagWidget::OnGenerateRow)
						.OnGetChildren(this, &SGameplayTagWidget::OnGetChildren)
						.OnExpansionChanged( this, &SGameplayTagWidget::OnExpansionChanged)
						.SelectionMode(ESelectionMode::Multi)
					]
				]
			]
		]
	];

	// Force the entire tree collapsed to start
	SetTagTreeItemExpansion(false);
	 
	LoadSettings();

	// Strip any invalid tags from the assets being edited
	VerifyAssetTagValidity();
}

void SGameplayTagWidget::OnFilterTextChanged( const FText& InFilterText )
{
	FilterString = InFilterText.ToString();	

	if( FilterString.IsEmpty() )
	{
		TagTreeWidget->SetTreeItemsSource( &TagItems );

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			SetDefaultTagNodeItemExpansion( TagItems[iItem] );
		}
	}
	else
	{
		FilteredTagItems.Empty();

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			if( FilterChildrenCheck( TagItems[iItem] ) )
			{
				FilteredTagItems.Add( TagItems[iItem] );
				SetTagNodeItemExpansion( TagItems[iItem], true );
			}
			else
			{
				SetTagNodeItemExpansion( TagItems[iItem], false );
			}
		}

		TagTreeWidget->SetTreeItemsSource( &FilteredTagItems );	
	}
		
	TagTreeWidget->RequestTreeRefresh();	
}

bool SGameplayTagWidget::FilterChildrenCheck( TSharedPtr<FGameplayTagNode> InItem )
{
	if( !InItem.IsValid() )
	{
		return false;
	}

	if( InItem->GetCompleteTag().ToString().Contains( FilterString ) || FilterString.IsEmpty() )
	{
		return true;
	}

	TArray< TSharedPtr<FGameplayTagNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			return true;
		}
	}

	return false;
}

TSharedRef<ITableRow> SGameplayTagWidget::OnGenerateRow(TSharedPtr<FGameplayTagNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText TooltipText;
	if (InItem.IsValid())
	{
		TooltipText = FText::FromName(InItem.Get()->GetCompleteTag());
	}

	return SNew(STableRow< TSharedPtr<FGameplayTagNode> >, OwnerTable)
		.Style(FEditorStyle::Get(), "GameplayTagTreeView")
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SGameplayTagWidget::OnTagCheckStatusChanged, InItem)
			.IsChecked(this, &SGameplayTagWidget::IsTagChecked, InItem)
			.ToolTipText(TooltipText)
			.IsEnabled(!bReadOnly)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem->GetSimpleTag()))
			]
		];
}

void SGameplayTagWidget::OnGetChildren(TSharedPtr<FGameplayTagNode> InItem, TArray< TSharedPtr<FGameplayTagNode> >& OutChildren)
{
	TArray< TSharedPtr<FGameplayTagNode> > FilteredChildren;
	TArray< TSharedPtr<FGameplayTagNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			FilteredChildren.Add( Children[iChild] );
		}
	}
	OutChildren += FilteredChildren;
}

void SGameplayTagWidget::OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FGameplayTagNode> NodeChanged)
{
	if (NewCheckState == ECheckBoxState::Checked)
	{
		OnTagChecked(NodeChanged);
	}
	else if (NewCheckState == ECheckBoxState::Unchecked)
	{
		OnTagUnchecked(NodeChanged);
	}
}

void SGameplayTagWidget::OnTagChecked(TSharedPtr<FGameplayTagNode> NodeChecked)
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_AddTags", "Add Gameplay Tags") );

	bool bRemoveParents = false;
	
	UGameplayTagsManager& TagsManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		TWeakPtr<FGameplayTagNode> CurNode(NodeChecked);
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EditableContainer = *Container;

			while (CurNode.IsValid())
			{
				FGameplayTag Tag = TagsManager.RequestGameplayTag(CurNode.Pin()->GetCompleteTag());

				if (bRemoveParents == false)
				{
					bRemoveParents = true;
					if (bMultiSelect == false)
					{
						EditableContainer.RemoveAllTags();
					}
					EditableContainer.AddTag(Tag);
				}
				else
				{
					EditableContainer.RemoveTag(Tag);
				}

				CurNode = CurNode.Pin()->GetParentTagNode();
			}
			SetContainer(Container, &EditableContainer, OwnerObj);
		}
	}
}

void SGameplayTagWidget::OnTagUnchecked(TSharedPtr<FGameplayTagNode> NodeUnchecked)
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_RemoveTags", "Remove Gameplay Tags"));
	if (NodeUnchecked.IsValid())
	{
		UGameplayTagsManager& TagsManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
			FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			FGameplayTag Tag = TagsManager.RequestGameplayTag(NodeUnchecked->GetCompleteTag());

			if (Container)
			{
				FGameplayTagContainer EditableContainer = *Container;
				EditableContainer.RemoveTag(Tag);

				TWeakPtr<FGameplayTagNode> ParentNode = NodeUnchecked->GetParentTagNode();
				if (ParentNode.IsValid())
				{
					// Check if there are other siblings before adding parent
					bool bOtherSiblings = false;
					for (auto It = ParentNode.Pin()->GetChildTagNodes().CreateConstIterator(); It; ++It)
					{
						Tag = TagsManager.RequestGameplayTag(It->Get()->GetCompleteTag());
						if (EditableContainer.HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
						{
							bOtherSiblings = true;
							break;
						}
					}
					// Add Parent
					if (!bOtherSiblings)
					{
						Tag = TagsManager.RequestGameplayTag(ParentNode.Pin()->GetCompleteTag());
						EditableContainer.AddTag(Tag);
					}
				}

				// Uncheck Children
				for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
				{
					UncheckChildren(ChildNode, EditableContainer);
				}

				SetContainer(Container, &EditableContainer, OwnerObj);
			}
			
		}
	}
}

void SGameplayTagWidget::UncheckChildren(TSharedPtr<FGameplayTagNode> NodeUnchecked, FGameplayTagContainer& EditableContainer)
{
	UGameplayTagsManager& TagsManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

	FGameplayTag Tag = TagsManager.RequestGameplayTag(NodeUnchecked->GetCompleteTag());
	EditableContainer.RemoveTag(Tag);

	// Uncheck Children
	for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
	{
		UncheckChildren(ChildNode, EditableContainer);
	}
}

ECheckBoxState SGameplayTagWidget::IsTagChecked(TSharedPtr<FGameplayTagNode> Node) const
{
	int32 NumValidAssets = 0;
	int32 NumAssetsTagIsAppliedTo = 0;

	if (Node.IsValid())
	{
		UGameplayTagsManager& TagsManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			if (Container)
			{
				NumValidAssets++;
				FGameplayTag Tag = TagsManager.RequestGameplayTag(Node->GetCompleteTag());
				if (Container->HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
				{
					++NumAssetsTagIsAppliedTo;
				}
			}
		}
	}

	if (NumAssetsTagIsAppliedTo == 0)
	{
		// Check if any children are tagged
		for (auto It = Node->GetChildTagNodes().CreateConstIterator(); It; ++It)
		{
			if (IsTagChecked(*It) == ECheckBoxState::Checked)
			{
				return ECheckBoxState::Checked;
			}
		}

		return ECheckBoxState::Unchecked;
	}
	else if (NumAssetsTagIsAppliedTo == NumValidAssets)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Undetermined;
	}
}

FReply SGameplayTagWidget::OnClearAllClicked()
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_RemoveAllTags", "Remove All Gameplay Tags") );

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EmptyContainer;
			SetContainer(Container, &EmptyContainer, OwnerObj);
		}
	}
	return FReply::Handled();
}

FReply SGameplayTagWidget::OnExpandAllClicked()
{
	SetTagTreeItemExpansion(true);
	return FReply::Handled();
}

FReply SGameplayTagWidget::OnCollapseAllClicked()
{
	SetTagTreeItemExpansion(false);
	return FReply::Handled();
}

void SGameplayTagWidget::SetTagTreeItemExpansion(bool bExpand)
{
	TArray< TSharedPtr<FGameplayTagNode> > TagArray;
	IGameplayTagsModule::Get().GetGameplayTagsManager().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		SetTagNodeItemExpansion(TagArray[TagIdx], bExpand);
	}
	
}

void SGameplayTagWidget::SetTagNodeItemExpansion(TSharedPtr<FGameplayTagNode> Node, bool bExpand)
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		TagTreeWidget->SetItemExpansion(Node, bExpand);

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			SetTagNodeItemExpansion(ChildTags[ChildIdx], bExpand);
		}
	}
}

void SGameplayTagWidget::VerifyAssetTagValidity()
{
	FGameplayTagContainer LibraryTags;

	// Create a set that is the library of all valid tags
	TArray< TSharedPtr<FGameplayTagNode> > NodeStack;

	UGameplayTagsManager& TagsManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
	
	TagsManager.GetFilteredGameplayRootTags(TEXT(""), NodeStack);

	while (NodeStack.Num() > 0)
	{
		TSharedPtr<FGameplayTagNode> CurNode = NodeStack.Pop();
		if (CurNode.IsValid())
		{
			LibraryTags.AddTag(TagsManager.RequestGameplayTag(CurNode->GetCompleteTag()));
			NodeStack.Append(CurNode->GetChildTagNodes());
		}
	}

	// Find and remove any tags on the asset that are no longer in the library
	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EditableContainer = *Container;
			FGameplayTagContainer InvalidTags;
			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				if (!LibraryTags.HasTag(*It, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
				{
					InvalidTags.AddTag(*It);
				}
			}
			if (InvalidTags.Num() > 0)
			{
				FString InvalidTagNames;

				for (auto InvalidIter = InvalidTags.CreateConstIterator(); InvalidIter; ++InvalidIter)
				{
					EditableContainer.RemoveTag(*InvalidIter);
					InvalidTagNames += InvalidIter->ToString() + TEXT("\n");
				}
				SetContainer(Container, &EditableContainer, OwnerObj);

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( InvalidTagNames ));
				FText DialogText = FText::Format( LOCTEXT("GameplayTagWidget_InvalidTags", "Invalid Tags that have been removed: \n\n{Objects}"), Arguments );
				OpenMsgDlgInt( EAppMsgType::Ok, DialogText, LOCTEXT("GameplayTagWidget_Warning", "Warning") );
			}
		}
	}
}

void SGameplayTagWidget::LoadSettings()
{
	TArray< TSharedPtr<FGameplayTagNode> > TagArray;
	IGameplayTagsModule::Get().GetGameplayTagsManager().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		LoadTagNodeItemExpansion(TagArray[TagIdx] );
	}
}

void SGameplayTagWidget::SetDefaultTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node )
{
	if ( Node.IsValid() && TagTreeWidget.IsValid() )
	{
		bool bExpanded = false;

		if ( IsTagChecked(Node) == ECheckBoxState::Checked )
		{
			bExpanded = true;
		}
		TagTreeWidget->SetItemExpansion(Node, bExpanded);

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for ( int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx )
		{
			SetDefaultTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SGameplayTagWidget::LoadTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node )
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		bool bExpanded = false;

		if( GConfig->GetBool(*SettingsIniSection, *(TagContainerName + Node->GetCompleteTag().ToString() + TEXT(".Expanded")), bExpanded, GEditorPerProjectIni) )
		{
			TagTreeWidget->SetItemExpansion( Node, bExpanded );
		}
		else if( IsTagChecked( Node ) == ECheckBoxState::Checked ) // If we have no save data but its ticked then we probably lost our settings so we shall expand it
		{
			TagTreeWidget->SetItemExpansion( Node, true );
		}

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			LoadTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SGameplayTagWidget::OnExpansionChanged( TSharedPtr<FGameplayTagNode> InItem, bool bIsExpanded )
{
	// Save the new expansion setting to ini file
	GConfig->SetBool(*SettingsIniSection, *(TagContainerName + InItem->GetCompleteTag().ToString() + TEXT(".Expanded")), bIsExpanded, GEditorPerProjectIni);
}

void SGameplayTagWidget::SetContainer(FGameplayTagContainer* OriginalContainer, FGameplayTagContainer* EditedContainer, UObject* OwnerObj)
{
	if (OwnerObj)
	{
		OwnerObj->PreEditChange(PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : NULL);
	}

	if (PropertyHandle.IsValid() && bMultiSelect)
	{
		// Case for a tag container 
		PropertyHandle->SetValueFromFormattedString(EditedContainer->ToString());
	}
	else if (PropertyHandle.IsValid() && !bMultiSelect)
	{
		// Case for a single Tag		
		FString FormattedString = TEXT("(TagName=\"");
		FormattedString += EditedContainer->First().GetTagName().ToString();
		FormattedString += TEXT("\")");
		PropertyHandle->SetValueFromFormattedString(FormattedString);
	}
	else
	{
		// Not sure if we should get here, means the property handle hasnt been setup which could be right or wrong.
		*OriginalContainer = *EditedContainer;
	}	

	if (OwnerObj)
	{
		OwnerObj->PostEditChange();
	}

	if (!PropertyHandle.IsValid())
	{
		OnTagChanged.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
