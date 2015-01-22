// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "Persona.h"
#include "SSkeletonSlotNames.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "SSearchBox.h"
#include "SInlineEditableTextBlock.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "STextEntryPopup.h"

#define LOCTEXT_NAMESPACE "SkeletonSlotNames"

static const FName ColumnId_SlotNameLabel( "SlotName" );

//////////////////////////////////////////////////////////////////////////
// SMorphTargetListRow

typedef TSharedPtr< FDisplayedSlotNameInfo > FDisplayedSlotNameInfoPtr;

class SSlotNameListRow
	: public SMultiColumnTableRow< FDisplayedSlotNameInfoPtr >
{
public:

	SLATE_BEGIN_ARGS( SSlotNameListRow ) {}

	/** The item for this row **/
	SLATE_ARGUMENT( FDisplayedSlotNameInfoPtr, Item )

	/* Widget used to display the list of morph targets */
	SLATE_ARGUMENT( TSharedPtr<SSkeletonSlotNames>, SlotNameListView )

	/* Persona used to update the viewport when a weight slider is dragged */
	SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:

	/** Widget used to display the list of slot name */
	TSharedPtr<SSkeletonSlotNames> SlotNameListView;

	/** The notify being displayed by this row */
	FDisplayedSlotNameInfoPtr	Item;

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;
};

void SSlotNameListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	SlotNameListView = InArgs._SlotNameListView;
	PersonaPtr = InArgs._Persona;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedSlotNameInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SSlotNameListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	check( ColumnName == ColumnId_SlotNameLabel );

	// Items can be either Slots or Groups.
	FText ItemText = Item->bIsGroupItem ? FText::Format(LOCTEXT("AnimSlotManagerGroupItem", "(Group) {0}"), FText::FromName(Item->Name))
		: FText::Format(LOCTEXT("AnimSlotManagerSlotItem", "(Slot) {0}"), FText::FromName(Item->Name));

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(ItemText)
		];
}

/////////////////////////////////////////////////////
// FSkeletonSlotNamesSummoner

FSkeletonSlotNamesSummoner::FSkeletonSlotNamesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonSlotNamesID, InHostingApp)
{
	TabLabel = LOCTEXT("AnimSlotManagerTabTitle", "Anim Slot Manager");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonSlotNamesMenu", "Anim Slots");
	ViewMenuTooltip = LOCTEXT("SkeletonSlotNames_ToolTip", "Manage Skeleton's Slots and Groups.");
}

TSharedRef<SWidget> FSkeletonSlotNamesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonSlotNames)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

/////////////////////////////////////////////////////
// SSkeletonSlotNames

void SSkeletonSlotNames::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;
	TargetSkeleton = PersonaPtr.Pin()->GetSkeleton();

	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SSkeletonSlotNames::PostUndo ) );

	// Toolbar
	FToolBarBuilder ToolbarBuilder(TSharedPtr< FUICommandList >(), FMultiBoxCustomization::None);

	// Save USkeleton
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SSkeletonSlotNames::OnSaveSkeleton))
		, NAME_None
		, LOCTEXT("AnimSlotManagerToolbarSaveLabel", "Save")
		, LOCTEXT("AnimSlotManagerToolbarSaveTooltip", "Saves changes into Skeleton asset")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "AnimSlotManager.SaveSkeleton")
		);

	ToolbarBuilder.AddSeparator();

	// Add Slot
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SSkeletonSlotNames::OnAddSlot))
		, NAME_None
		, LOCTEXT("AnimSlotManagerToolbarAddSlotLabel", "Add Slot")
		, LOCTEXT("AnimSlotManagerToolbarAddSlotTooltip", "Create a new unique Slot name")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "AnimSlotManager.AddSlot")
		);

	// Add Group
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SSkeletonSlotNames::OnAddGroup))
		, NAME_None
		, LOCTEXT("AnimSlotManagerToolbarAddGroupLabel", "Add Group")
		, LOCTEXT("AnimSlotManagerToolbarAddGroupTooltip", "Create a new unique Group name")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "AnimSlotManager.AddGroup")
		);

	this->ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ToolbarBuilder.MakeWidget()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonSlotNames::OnFilterTextChanged )
			.OnTextCommitted( this, &SSkeletonSlotNames::OnFilterTextCommitted )
			.HintText( LOCTEXT( "AnimSlotManagerSlotNameSearchBoxHint", "Slot name filter...") )
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( SlotNameListView, SSlotNameListType )
			.TreeItemsSource(&NotifyList)
			.OnGenerateRow( this, &SSkeletonSlotNames::GenerateNotifyRow )
			.OnGetChildren(this, &SSkeletonSlotNames::GetChildrenForInfo)
			.OnContextMenuOpening( this, &SSkeletonSlotNames::OnGetContextMenuContent )
			.SelectionMode(ESelectionMode::Single)
			.OnSelectionChanged( this, &SSkeletonSlotNames::OnNotifySelectionChanged )
			.ItemHeight( 22.0f )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_SlotNameLabel )
				.DefaultLabel( LOCTEXT( "SlotNameNameLabel", "Slot Name" ) )
			)
		]
	];

	CreateSlotNameList();
}

SSkeletonSlotNames::~SSkeletonSlotNames()
{
	if ( PersonaPtr.IsValid() )
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo( this );
	}
}

void SSkeletonSlotNames::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshSlotNameListWithFilter();
}

void SSkeletonSlotNames::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SSkeletonSlotNames::GenerateNotifyRow(TSharedPtr<FDisplayedSlotNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SSlotNameListRow, OwnerTable )
		.Persona( PersonaPtr )
		.Item( InInfo )
		.SlotNameListView( SharedThis( this ) );
}

void SSkeletonSlotNames::GetChildrenForInfo(TSharedPtr<FDisplayedSlotNameInfo> InInfo, TArray< TSharedPtr<FDisplayedSlotNameInfo> >& OutChildren)
{
	check(InInfo.IsValid());
	OutChildren = InInfo->Children;
}

TSharedPtr<SWidget> SSkeletonSlotNames::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	TArray< TSharedPtr< FDisplayedSlotNameInfo > > SelectedItems = SlotNameListView.Get()->GetSelectedItems();

	bool bHasSelectedItem = (SelectedItems.Num() > 0);
	bool bShowGroupItem = bHasSelectedItem && SelectedItems[0].Get()->bIsGroupItem;
	bool bShowSlotItem = bHasSelectedItem && !SelectedItems[0].Get()->bIsGroupItem;

	if (bShowGroupItem)
	{

	}
	else if (bShowSlotItem)
	{
		MenuBuilder.BeginSection("SlotManagerSlotActions", LOCTEXT("SlotManagerSlotActions", "Slot Actions"));
		// Set Slot Group
		{
			MenuBuilder.AddSubMenu(
				FText::Format(LOCTEXT("ContextMenuSetSlotGroupLabel", "Set Slot {0} Group to"), FText::FromName(SelectedItems[0].Get()->Name)),
				FText::Format(LOCTEXT("ContextMenuSetSlotGroupToolTip", "Set Slot {0} Group"), FText::FromName(SelectedItems[0].Get()->Name)),
				FNewMenuDelegate::CreateRaw(this, &SSkeletonSlotNames::FillSetSlotGroupSubMenu));
			MenuBuilder.EndSection();
		}
	}

	MenuBuilder.BeginSection("SlotManagerGeneralActions", LOCTEXT("SlotManagerGeneralActions", "Slot Manager Actions"));
	// Add Slot
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonSlotNames::OnAddSlot));
		const FText Label = LOCTEXT("AnimSlotManagerContextMenuAddSlotLabel", "Add Slot");
		const FText ToolTip = LOCTEXT("AnimSlotManagerContextMenuAddSlotTooltip", "Adds a new Slot");
		MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), Action);
	}
	// Add Group
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonSlotNames::OnAddGroup));
		const FText Label = LOCTEXT("AnimSlotManagerContextMenuAddGroupLabel", "Add Group");
		const FText ToolTip = LOCTEXT("AnimSlotManagerContextMenuAddGroupTooltip", "Adds a new Group");
		MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonSlotNames::FillSetSlotGroupSubMenu(FMenuBuilder& MenuBuilder)
{
	const TArray<FAnimSlotGroup>& SlotGroups = TargetSkeleton->GetSlotGroups();
	for (auto SlotGroup : SlotGroups)
	{
		const FName& GroupName = SlotGroup.GroupName;

		const FText ToolTip = FText::Format(LOCTEXT("ContextMenuSetSlotSubMenuToolTip", "Changes slot's group to {0}"), FText::FromName(GroupName));
/*		FString Label = Class->GetDisplayNameText().ToString();*/
		const FText Label = FText::FromName(GroupName);

		FUIAction UIAction;
		UIAction.ExecuteAction.BindRaw(this, &SSkeletonSlotNames::ContextMenuOnSetSlot,	GroupName);
		MenuBuilder.AddMenuEntry(Label, ToolTip, FSlateIcon(), UIAction);
	}
}

void SSkeletonSlotNames::ContextMenuOnSetSlot(FName InNewGroupName)
{
	TArray< TSharedPtr< FDisplayedSlotNameInfo > > SelectedItems = SlotNameListView.Get()->GetSelectedItems();

	bool bHasSelectedItem = (SelectedItems.Num() > 0);
	bool bShowSlotItem = bHasSelectedItem && !SelectedItems[0].Get()->bIsGroupItem;

	if (bShowSlotItem)
	{
		const FName SlotName = SelectedItems[0].Get()->Name;
		if (TargetSkeleton->ContainsSlotName(SlotName))
		{
			const FScopedTransaction Transaction(LOCTEXT("AnimSlotManager_ContextMenuOnSetSlot", "Set Slot Group Name"));
			TargetSkeleton->SetSlotGroupName(SlotName, InNewGroupName);
			TargetSkeleton->Modify(true);

			RefreshSlotNameListWithFilter();
		}

		// Highlight newly created item.
		TSharedPtr< FDisplayedSlotNameInfo > Item = FindItemNamed(SlotName);
		if (Item.IsValid())
		{
			SlotNameListView->SetSelection(Item);
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

void SSkeletonSlotNames::OnNotifySelectionChanged(TSharedPtr<FDisplayedSlotNameInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if(Selection.IsValid())
	{
		ShowNotifyInDetailsView(Selection->Name);
	}
}

void SSkeletonSlotNames::OnSaveSkeleton()
{
	if (TargetSkeleton)
	{
		TArray< UPackage* > PackagesToSave;
		PackagesToSave.Add(TargetSkeleton->GetOutermost());

		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty=*/ false, /*bPromptToSave=*/ false);
	}
}

void SSkeletonSlotNames::OnAddSlot()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSlotName_AskSlotName", "New Slot Name"))
		.OnTextCommitted(this, &SSkeletonSlotNames::AddSlotPopUpOnCommit);

	// Show dialog to enter new track name
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);

	TextEntry->FocusDefaultWidget();
}

void SSkeletonSlotNames::OnAddGroup()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewGroupName_AskGroupName", "New Group Name"))
		.OnTextCommitted(this, &SSkeletonSlotNames::AddGroupPopUpOnCommit);

	// Show dialog to enter new track name
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);

	TextEntry->FocusDefaultWidget();
}

void SSkeletonSlotNames::AddSlotPopUpOnCommit(const FText & InNewSlotText, ETextCommit::Type CommitInfo)
{
	if (!InNewSlotText.IsEmpty())
	{
		const FScopedTransaction Transaction(LOCTEXT("NewSlotName_AddSlotName", "Add New Slot Node Name"));

		FName NewSlotName = FName(*InNewSlotText.ToString());
		// Keep slot and group names unique
		if (!TargetSkeleton->ContainsSlotName(NewSlotName) && (TargetSkeleton->FindAnimSlotGroup(NewSlotName) == NULL))
		{
			TArray< TSharedPtr< FDisplayedSlotNameInfo > > SelectedItems = SlotNameListView->GetSelectedItems();
			bool bHasSelectedItem = (SelectedItems.Num() > 0);
			bool bShowGroupItem = bHasSelectedItem && SelectedItems[0].Get()->bIsGroupItem;

			TargetSkeleton->SetSlotGroupName(NewSlotName, bShowGroupItem ? SelectedItems[0].Get()->Name : FAnimSlotGroup::DefaultGroupName);
			TargetSkeleton->Modify(true);

			RefreshSlotNameListWithFilter();
		}

		// Highlight newly created item.
		TSharedPtr< FDisplayedSlotNameInfo > Item = FindItemNamed(NewSlotName);
		if (Item.IsValid())
		{
			SlotNameListView->SetSelection(Item);
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

void SSkeletonSlotNames::AddGroupPopUpOnCommit(const FText & InNewGroupText, ETextCommit::Type CommitInfo)
{
	if (!InNewGroupText.IsEmpty())
	{
		const FScopedTransaction Transaction(LOCTEXT("NewGroupName_AddGroupName", "Add New Slot Group Name"));

		FName NewGroupName = FName(*InNewGroupText.ToString());
		// Keep slot and group names unique
		if (!TargetSkeleton->ContainsSlotName(NewGroupName) && TargetSkeleton->AddSlotGroupName(NewGroupName))
		{
			TargetSkeleton->Modify(true);
			RefreshSlotNameListWithFilter();
		}

		// Highlight newly created item.
		TSharedPtr< FDisplayedSlotNameInfo > Item = FindItemNamed(NewGroupName);
		if (Item.IsValid())
		{
			SlotNameListView->SetSelection(Item);
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

// void SSkeletonSlotNames::GetCompatibleAnimBlueprints( TMultiMap<class UAnimBlueprint *, class UAnimGraphNode_Slot *>& OutAssets )
// {
// 	// second, go through all animgraphnode_slot
// 	for(FObjectIterator Iter(UAnimGraphNode_Slot::StaticClass()); Iter; ++Iter)
// 	{
// 		// if I find one, add the BP and slot nodes to the list
// 		UAnimGraphNode_Slot * SlotNode = CastChecked<UAnimGraphNode_Slot>(Iter);
// 		UAnimBlueprint * AnimBlueprint = Cast<UAnimBlueprint>(SlotNode->GetBlueprint());
// 		if (AnimBlueprint)
// 		{
// 			OutAssets.AddUnique(AnimBlueprint, SlotNode);
// 		}
// 	}
// }

void SSkeletonSlotNames::RefreshSlotNameListWithFilter()
{
	CreateSlotNameList( NameFilterBox->GetText().ToString() );
}

void SSkeletonSlotNames::CreateSlotNameList(const FString& SearchText)
{
	NotifyList.Empty();

	if ( TargetSkeleton )
	{
		const TArray<FAnimSlotGroup>& SlotGroups = TargetSkeleton->GetSlotGroups();
		for (auto SlotGroup : SlotGroups)
		{
			const FName& GroupName = SlotGroup.GroupName;
			
			TSharedRef<FDisplayedSlotNameInfo> GroupItem = FDisplayedSlotNameInfo::Make(GroupName, true);
			SlotNameListView->SetItemExpansion(GroupItem, true);
			NotifyList.Add(GroupItem);

			for (auto SlotName : SlotGroup.SlotNames)
			{
				if (SearchText.IsEmpty() || GroupName.ToString().Contains(SearchText) || SlotName.ToString().Contains(SearchText))
				{
					TSharedRef<FDisplayedSlotNameInfo> SlotItem = FDisplayedSlotNameInfo::Make(SlotName, false);
					SlotNameListView->SetItemExpansion(SlotItem, true);
					NotifyList[NotifyList.Num() - 1]->Children.Add(SlotItem);
				}
			}
		}
	}

	SlotNameListView->RequestTreeRefresh();
}

TSharedPtr< FDisplayedSlotNameInfo > SSkeletonSlotNames::FindItemNamed(FName ItemName) const
{
	for (auto SlotGroupItem : NotifyList)
	{
		if (SlotGroupItem->Name == ItemName)
		{
			return SlotGroupItem;
		}
		for (auto SlotItem : SlotGroupItem->Children)
		{
			if (SlotItem->Name == ItemName)
			{
				return SlotItem;
			}
		}
	}

	return NULL;
}

void SSkeletonSlotNames::ShowNotifyInDetailsView(FName NotifyName)
{
	// @todo nothing to show now, but in the future
	// we can show the list of montage that are used by this slot node?
}

void SSkeletonSlotNames::GetCompatibleAnimMontage(TArray<class FAssetData>& OutAssets)
{
	//Get the skeleton tag to search for
	FString SkeletonExportName = FAssetData(TargetSkeleton).GetExportTextName();

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UAnimMontage::StaticClass()->GetFName(), AssetDataList, true);

	OutAssets.Empty(AssetDataList.Num());

	for( int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex )
	{
		const FAssetData& PossibleAnimMontage = AssetDataList[AssetIndex];
		if( SkeletonExportName == PossibleAnimMontage.TagsAndValues.FindRef("Skeleton") )
		{
			OutAssets.Add( PossibleAnimMontage );
		}
	}
}

UObject* SSkeletonSlotNames::ShowInDetailsView( UClass* EdClass )
{
	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		PersonaPtr.Pin()->SetDetailObject(Obj);
	}
	return Obj;
}

void SSkeletonSlotNames::ClearDetailsView()
{
	PersonaPtr.Pin()->SetDetailObject(NULL);
}

void SSkeletonSlotNames::PostUndo()
{
	RefreshSlotNameListWithFilter();
}

void SSkeletonSlotNames::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void SSkeletonSlotNames::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}
#undef LOCTEXT_NAMESPACE
