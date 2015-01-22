// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorObjectsTracker.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "SkeletonSlotNames"
/////////////////////////////////////////////////////
// FSkeletonTreeSummoner
struct FSkeletonSlotNamesSummoner : public FWorkflowTabFactory
{
public:
	FSkeletonSlotNamesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("WindowTooltip", "This tab lets you modify custom animation SlotName"), NULL, TEXT("Shared/Editors/Persona"), TEXT("AnimationSlotName_Window"));
	}
};

//////////////////////////////////////////////////////////////////////////
// FDisplayedSlotNameInfo

class FDisplayedSlotNameInfo
{
public:
	FName Name;

	bool bIsGroupItem;

	TArray< TSharedPtr<FDisplayedSlotNameInfo> > Children;

	/** Handle to editable text block for rename */
	TSharedPtr<SInlineEditableTextBlock> InlineEditableText;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedSlotNameInfo> Make(const FName& InItemName, bool InbIsGroupItem)
	{
		return MakeShareable(new FDisplayedSlotNameInfo(InItemName, InbIsGroupItem));
	}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedSlotNameInfo(const FName& InItemName, bool InbIsGroupItem)
		: Name(InItemName)
		, bIsGroupItem(InbIsGroupItem)
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedSlotNameInfo() {}
};

/** Widgets list type */
typedef STreeView< TSharedPtr<FDisplayedSlotNameInfo> > SSlotNameListType;

class SSkeletonSlotNames : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SSkeletonSlotNames )
		: _Persona()
	{}

	SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )
		
		SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	~SSkeletonSlotNames();

	/**
	* Accessor so our rows can grab the filter text for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }

	/** Creates an editor object from the given type to be used in a details panel */
	UObject* ShowInDetailsView( UClass* EdClass );

	/** Clears the detail view of whatever we displayed last */
	void ClearDetailsView();

	/** This triggers a UI repopulation after undo has been called */
	void PostUndo();

	// FGCObject interface start
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// FGCObject interface end

private:

	/** Called when the user changes the contents of the search box */
	void OnFilterTextChanged( const FText& SearchText );

	/** Called when the user changes the contents of the search box */
	void OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo );

	/** Delegate handler for generating rows in SlotNameListView */ 
	TSharedRef<ITableRow> GenerateNotifyRow( TSharedPtr<FDisplayedSlotNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable );

	/** Get all children for a given entry in the list */
	void GetChildrenForInfo(TSharedPtr<FDisplayedSlotNameInfo> InInfo, TArray< TSharedPtr<FDisplayedSlotNameInfo> >& OutChildren);

	/** Delegate handler called when the user right clicks in SlotNameListView */
	TSharedPtr<SWidget> OnGetContextMenuContent() const;

	/** Delegate handler for when the user selects something in SlotNameListView */
	void OnNotifySelectionChanged( TSharedPtr<FDisplayedSlotNameInfo> Selection, ESelectInfo::Type SelectInfo );

	// Save Skeleton
	void OnSaveSkeleton();

	// Add a new Slot
	void OnAddSlot();
	void AddSlotPopUpOnCommit(const FText& InNewSlotName, ETextCommit::Type CommitInfo);

	// Add a new Group
	void OnAddGroup();
	void AddGroupPopUpOnCommit(const FText& InNewGroupName, ETextCommit::Type CommitInfo);

	// Set Slot Group
	void FillSetSlotGroupSubMenu(FMenuBuilder& MenuBuilder);
	void ContextMenuOnSetSlot(FName InNewGroupName);

	/** Wrapper that populates SlotNameListView using current filter test */
	void RefreshSlotNameListWithFilter();

	TSharedPtr< FDisplayedSlotNameInfo > FindItemNamed(FName ItemName) const;

	/** Populates SlotNameListView based on the skeletons SlotName and the supplied filter text */
	void CreateSlotNameList( const FString& SearchText = FString("") );

	/** handler for user selecting a Notify in SlotNameListView - populates the details panel */
	void ShowNotifyInDetailsView( FName NotifyName );

	/** Populates OutAssets with the AnimSequences that match Personas current skeleton */
	void GetCompatibleAnimMontage( TArray<class FAssetData>& OutAssets );

//	void GetCompatibleAnimBlueprints( TMultiMap<class UAnimBlueprint * , class UAnimGraphNode_Slot *>& OutAssets );

	/** Utility function to display notifications to the user */
	void NotifyUser( FNotificationInfo& NotificationInfo );

	/** The owning Persona instance */
	TWeakPtr<class FPersona> PersonaPtr;

	/** The skeleton we are currently editing */
	USkeleton* TargetSkeleton;

	/** SSearchBox to filter the notify list */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** Widget used to display the list of SlotName */
	TSharedPtr<SSlotNameListType> SlotNameListView;

	/** A list of SlotName. Used by the SlotNameListView. */
	TArray< TSharedPtr<FDisplayedSlotNameInfo> > NotifyList;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Tracks objects created for displaying in the details panel*/
	FEditorObjectTracker EditorObjectTracker;
};

#undef LOCTEXT_NAMESPACE