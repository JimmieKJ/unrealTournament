// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SSkeletonSmartNameManager.h"
#include "ScopedTransaction.h"
#include "AssetRegistryModule.h"
#include "AssetData.h"
#include "SSearchBox.h"
#include "SInlineEditableTextBlock.h"
#include "STextEntryPopup.h"

#define LOCTEXT_NAMESPACE "SkeletonSmartNameManager"

static const FName ColumnId_SmartNameLabel("SmartNameLabel");

//////////////////////////////////////////////////////////////////////////

typedef TSharedPtr<FDisplayedSmartNameInfo> FDisplayedSmartNameInfoPtr;

class SSmartNameListRow : public SMultiColumnTableRow < FDisplayedSmartNameInfoPtr >
{
public:
	SLATE_BEGIN_ARGS(SSmartNameListRow) {}

	SLATE_ARGUMENT(FDisplayedSmartNameInfoPtr, Item)							// The display object for this row
	SLATE_ARGUMENT(TSharedPtr<SSkeletonSmartNameManager>, NameListWidget)		// The list that we are in

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	// The editable text boxes in the rows consume clicks on the rows, this stops that behaviour
	// similar to SSkeletonAnimNotifies
	bool IsSelected() {return false;}

private:

	// Gets the name of the smartname we are displaying
	FText GetItemName() const;

	FDisplayedSmartNameInfoPtr Item;						// The display object for this row
	TSharedPtr<SSkeletonSmartNameManager> NameListWidget;	// The list we are in
};

void SSmartNameListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	NameListWidget = InArgs._NameListWidget;

	check(Item.IsValid());

	SMultiColumnTableRow< FDisplayedSmartNameInfoPtr >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SSmartNameListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	return SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SAssignNew(Item->EditableText, SInlineEditableTextBlock)
		.OnTextCommitted(NameListWidget.Get(), &SSkeletonSmartNameManager::OnNameCommitted, Item)
		.IsSelected(this, &SSmartNameListRow::IsSelected)
		.Text(this, &SSmartNameListRow::GetItemName)
	];
}

FText SSmartNameListRow::GetItemName() const
{
	const FSmartNameMapping* Mapping = Item->Skeleton->SmartNames.GetContainer(Item->ContainerName);
	check(Mapping);
	FName ItemName;
	Mapping->GetName(Item->NameUid, ItemName);
	
	return FText::FromName(ItemName);
}

//////////////////////////////////////////////////////////////////////////

FSkeletonCurveNameManagerSummoner::FSkeletonCurveNameManagerSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	:FWorkflowTabFactory(FPersonaTabs::CurveNameManagerID, InHostingApp)
{
	TabLabel = LOCTEXT("CurveNameTabTitle", "Skeleton Curves");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.SkeletonCurves");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("CurveNameTabMenuDesc", "Skeleton Curves");
	ViewMenuTooltip = LOCTEXT("CurveNameTabMenuToolTip", "Shows the list of curves for the current skeleton");
}

TSharedRef<SWidget> FSkeletonCurveNameManagerSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FAssetEditorToolkit> Toolkit = HostingApp.Pin();
	check(Toolkit.IsValid());

	return SNew(SCurveNameManager)
		.Persona(StaticCastSharedPtr<FPersona>(Toolkit))
		.ContainerName(FName(USkeleton::AnimCurveMappingName))
		.NameColumnDescription(LOCTEXT("CurveNameTabColumnDesc", "Name"));
}

//////////////////////////////////////////////////////////////////////////

SSkeletonSmartNameManager::~SSkeletonSmartNameManager()
{
	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	if(SharedPersona.IsValid())
	{
		SharedPersona->UnregisterOnPostUndo(this);
	}
}

void SSkeletonSmartNameManager::Construct(const FArguments& InArgs)
{
	WeakPersona = InArgs._Persona;
	ContainerName = InArgs._ContainerName;

	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	check(SharedPersona.IsValid());

	CurrentSkeleton = SharedPersona->GetSkeleton();
	CurrentFilterText = FText::GetEmpty();

	SharedPersona->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP(this, &SSkeletonSmartNameManager::OnPostUndo));

	this->ChildSlot
	[
		SNew(SVerticalBox)

		// Search box
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSearchBox)
			.SelectAllTextWhenFocused(true)
			.OnTextChanged(this, &SSkeletonSmartNameManager::OnFilterTextChanged)
			.OnTextCommitted(this, &SSkeletonSmartNameManager::OnFilterTextCommitted)
			.HintText(LOCTEXT("SmartNameSearchHint", "Search Names . . . "))
		]

		// Name list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ListWidget, SSmartNameListType)
			.ListItemsSource(&DisplayedInfoList)
			.OnGenerateRow(this, &SSkeletonSmartNameManager::OnGenerateRow)
			.OnContextMenuOpening(this, &SSkeletonSmartNameManager::OnGetContextMenu)
			.ItemHeight(22.0f)
			.SelectionMode(ESelectionMode::Multi)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+SHeaderRow::Column(ColumnId_SmartNameLabel)
				.DefaultLabel(InArgs._NameColumnDescription)
			)
		]
	];

	GenerateDisplayedList(CurrentFilterText);
}

FText& SSkeletonSmartNameManager::GetNameFilterText()
{
	return CurrentFilterText;
}

void SSkeletonSmartNameManager::OnFilterTextChanged(const FText& FilterText)
{
	CurrentFilterText = FilterText;

	GenerateDisplayedList(CurrentFilterText);
}

void SSkeletonSmartNameManager::OnFilterTextCommitted(const FText& FilterText, ETextCommit::Type CommitType)
{
	OnFilterTextChanged(FilterText);
}

TSharedRef<ITableRow> SSkeletonSmartNameManager::OnGenerateRow(TSharedPtr<FDisplayedSmartNameInfo> InInfo, const TSharedRef<STableViewBase>& OwningTable)
{
	check(InInfo.IsValid());

	return SNew(SSmartNameListRow, OwningTable)
		.Item(InInfo)
		.NameListWidget(SharedThis(this));
}

TSharedPtr<SWidget> SSkeletonSmartNameManager::OnGetContextMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("SmartNameAction", LOCTEXT("SmartNameActions", "Actions"));
	{
		FUIAction Action;
		// Rename selection
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonSmartNameManager::OnRenameClicked);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SSkeletonSmartNameManager::CanRename);
			const FText Label = LOCTEXT("RenameSmartNameLabel", "Rename");
			const FText ToolTip = LOCTEXT("RenameSmartNameToolTip", "Rename the selected item");
			MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), Action);
		}
		// Delete a name
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonSmartNameManager::OnDeleteNameClicked);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SSkeletonSmartNameManager::CanDelete);
			const FText Label = LOCTEXT("DeleteSmartNameLabel", "Delete");
			const FText ToolTip = LOCTEXT("DeleteSmartNameToolTip", "Delete the selected item");
			MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), Action);
		}
		// Add a name
		MenuBuilder.AddMenuSeparator();
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonSmartNameManager::OnAddClicked);
			Action.CanExecuteAction = nullptr;
			const FText Label = LOCTEXT("AddSmartNameLabel", "Add...");
			const FText ToolTip = LOCTEXT("AddSmartNameToolTip", "Add an entry to the skeleton");
			MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonSmartNameManager::GenerateDisplayedList(const FText& FilterText)
{
	FSmartNameMapping* Mapping = CurrentSkeleton->SmartNames.GetContainer(ContainerName);
	if(Mapping)
	{
		DisplayedInfoList.Empty();
		TArray<FSmartNameMapping::UID> UidList;
		Mapping->FillUidArray(UidList);

		for(FSmartNameMapping::UID Uid : UidList)
		{
			bool bAddToList = true;

			FName CurrentName;
			Mapping->GetName(Uid, CurrentName);

			if(!FilterText.IsEmpty())
			{

				if(!CurrentName.ToString().Contains(*FilterText.ToString()))
				{
					bAddToList = false;
				}
			}
			
			if(bAddToList)
			{
				DisplayedInfoList.Add(FDisplayedSmartNameInfo::Make(CurrentSkeleton, ContainerName, Uid, CurrentName));
			}
		}

		struct FSortSmartNamesAlphabetically
		{
			bool operator()(const TSharedPtr<FDisplayedSmartNameInfo>& A, const TSharedPtr<FDisplayedSmartNameInfo>& B) const
			{
				return (A.Get()->SmartName.Compare(B.Get()->SmartName) < 0);
			}
		};

		DisplayedInfoList.Sort(FSortSmartNamesAlphabetically());
		ListWidget->RequestListRefresh();
	}
}

void SSkeletonSmartNameManager::OnNameCommitted(const FText& InNewName, ETextCommit::Type, TSharedPtr<FDisplayedSmartNameInfo> Item)
{
	FSmartNameMapping* Mapping = CurrentSkeleton->SmartNames.GetContainer(ContainerName);
	if(Mapping)
	{
		FName NewName(*InNewName.ToString());
		if(!Mapping->Exists(NewName))
		{
			FScopedTransaction Transaction(LOCTEXT("TransactionRename", "Rename Element"));
			CurrentSkeleton->RenameSmartnameAndModify(ContainerName, Item->NameUid, NewName);
		}
	}
}

void SSkeletonSmartNameManager::OnDeleteNameClicked()
{
	TArray<FDisplayedSmartNameInfoPtr> SelectedItems = ListWidget->GetSelectedItems();

	bool bModifiedList = false;

	for(FDisplayedSmartNameInfoPtr Item : SelectedItems)
	{
		CurrentSkeleton->RemoveSmartnameAndModify(Item->ContainerName, Item->NameUid);
	}

	GenerateDisplayedList(CurrentFilterText);
	ListWidget->RequestListRefresh();
}

void SSkeletonSmartNameManager::OnRenameClicked()
{
	TArray<FDisplayedSmartNameInfoPtr> SelectedItems = ListWidget->GetSelectedItems();

	SelectedItems[0]->EditableText->EnterEditingMode();
}

void SSkeletonSmartNameManager::OnAddClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSmartnameLabel", "New Name"))
		.OnTextCommitted(this, &SSkeletonSmartNameManager::CreateNewNameEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

void SSkeletonSmartNameManager::OnPostUndo()
{
	GenerateDisplayedList(CurrentFilterText);
}

bool SSkeletonSmartNameManager::CanDelete()
{
	return ListWidget->GetNumItemsSelected() > 0;
}

bool SSkeletonSmartNameManager::CanRename()
{
	return ListWidget->GetNumItemsSelected() == 1;
}

void SSkeletonSmartNameManager::CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if(!CommittedText.IsEmpty() && CommitType == ETextCommit::OnEnter)
	{
		if(FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(ContainerName))
		{
			FName NewName = FName(*CommittedText.ToString());
			FSmartNameMapping::UID NewUid;
			if(CurrentSkeleton->AddSmartnameAndModify(ContainerName, NewName, NewUid))
			{
				// Successfully added
				GenerateDisplayedList(CurrentFilterText);
				ListWidget->RequestListRefresh();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SCurveNameManager::OnDeleteNameClicked()
{
	TArray<FDisplayedSmartNameInfoPtr> SelectedItems = ListWidget->GetSelectedItems();
	TArray<FSmartNameMapping::UID> SelectedUids;

	for(FDisplayedSmartNameInfoPtr Item : SelectedItems)
	{
		SelectedUids.Add(Item->NameUid);
	}

	FAssetRegistryModule& AssetModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AnimationAssets;
	AssetModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AnimationAssets, true);
	FAssetData SkeletonData(CurrentSkeleton);
	const FString CurrentSkeletonName = SkeletonData.GetExportTextName();

	GWarn->BeginSlowTask(LOCTEXT("CollectAnimationsTaskDesc", "Collecting assets..."), true);

	for(int32 Idx = AnimationAssets.Num() - 1 ; Idx >= 0 ; --Idx)
	{
		FAssetData& Data = AnimationAssets[Idx];
		bool bRemove = true;

		const FString* SkeletonDataTag = Data.TagsAndValues.Find("Skeleton");
		if(SkeletonDataTag && *SkeletonDataTag == CurrentSkeletonName)
		{
			const FString* CurveData = Data.TagsAndValues.Find(USkeleton::CurveTag);
			FString CurveDataCopy;

			if(!CurveData)
			{
				// This asset is old; it hasn't been loaded since before smartnames were added for curves.
				// unfortunately the only way to delete curves safely is to load old assets to see if they're
				// using the selected name. We only load what we have to here.
				UObject* Asset = Data.GetAsset();
				check(Asset);
				TArray<UObject::FAssetRegistryTag> Tags;
				Asset->GetAssetRegistryTags(Tags);
				
				UObject::FAssetRegistryTag* CurveTag = Tags.FindByPredicate([](const UObject::FAssetRegistryTag& Tag)
				{
					return Tag.Name == USkeleton::CurveTag;
				});
				CurveDataCopy = CurveTag->Value;
			}
			else
			{
				CurveDataCopy = *CurveData;
			}

			TArray<FString> ParsedStringUids;
			CurveDataCopy.ParseIntoArray(ParsedStringUids, *USkeleton::CurveTagDelimiter, true);

			for(const FString& UidString : ParsedStringUids)
			{
				FSmartNameMapping::UID Uid = (FSmartNameMapping::UID)TCString<TCHAR>::Strtoui64(*UidString, NULL, 10);
				if(SelectedUids.Contains(Uid))
				{
					bRemove = false;
					break;
				}
			}
		}

		if(bRemove)
		{
			AnimationAssets.RemoveAt(Idx);
		}
	}

	GWarn->EndSlowTask();

	// AnimationAssets now only contains assets that are using the selected curve(s)
	if(AnimationAssets.Num() > 0)
	{
		FString ConfirmMessage = LOCTEXT("DeleteCurveMessage", "The following assets use the selected curve(s), deleting will remove the curves from these assets. Continue?\n\n").ToString();
		
		for(FAssetData& Data : AnimationAssets)
		{
			ConfirmMessage += Data.AssetName.ToString() + " (" + Data.AssetClass.ToString() + ")\n";
		}

		FText Title = LOCTEXT("DeleteCurveDialogTitle", "Confirm Deletion");
		FText Message = FText::FromString(ConfirmMessage);
		if(FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) == EAppReturnType::Yes)
		{
			// Proceed to delete the curves
			GWarn->BeginSlowTask(FText::Format(LOCTEXT("DeleteCurvesTaskDesc", "Deleting curve from skeleton {1}"), FText::FromString(CurrentSkeleton->GetName())), true);
			FScopedTransaction Transaction(LOCTEXT("DeleteCurvesTransactionName", "Delete skeleton curve"));

			// Remove curves from animation assets
			for(FAssetData& Data : AnimationAssets)
			{
				UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Data.GetAsset());	
				Sequence->Modify(true);
				for(FSmartNameMapping::UID Uid : SelectedUids)
				{
					Sequence->RawCurveData.DeleteCurveData(Uid);
				}
			}
			GWarn->EndSlowTask();
		}
	}

	if(SelectedUids.Num() > 0)
	{
		// Remove names from skeleton
		CurrentSkeleton->Modify(true);
		FSmartNameMapping* Mapping = CurrentSkeleton->SmartNames.GetContainer(ContainerName);
		for(FSmartNameMapping::UID Uid : SelectedUids)
		{
			Mapping->Remove(Uid);
		}
	}

	GenerateDisplayedList(CurrentFilterText);

	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	if(SharedPersona.IsValid())
	{
		if(SharedPersona->GetCurrentMode() == FPersonaModes::AnimationEditMode)
		{
			// If we're in animation mode then we need to reinitialise the mode to
			// refresh the sequencer which will now have a widget it shouldn't have
			SharedPersona->ReinitMode();
		}
	}
}


#undef LOCTEXT_NAMESPACE
