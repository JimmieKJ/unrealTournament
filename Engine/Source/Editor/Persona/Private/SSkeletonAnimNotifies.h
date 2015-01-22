// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorObjectsTracker.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "SkeletonAnimnotifies"
/////////////////////////////////////////////////////
// FSkeletonTreeSummoner
struct FSkeletonAnimNotifiesSummoner : public FWorkflowTabFactory
{
public:
	FSkeletonAnimNotifiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("AnimationNotifierTooltip", "This tab lets you modify custom animation notifies"), NULL, TEXT("Shared/Editors/Persona"), TEXT("AnimationNotifies_Window"));
	}
};

//////////////////////////////////////////////////////////////////////////
// FDisplayedAnimNotifyInfo

class FDisplayedAnimNotifyInfo
{
public:
	FName Name;

	/** Handle to editable text block for rename */
	TSharedPtr<SInlineEditableTextBlock> InlineEditableText;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedAnimNotifyInfo> Make(const FName& NotifyName)
	{
		return MakeShareable(new FDisplayedAnimNotifyInfo(NotifyName));
	}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedAnimNotifyInfo(const FName& InNotifyName)
		: Name( InNotifyName )
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedAnimNotifyInfo() {}
};

/** Widgets list type */
typedef SListView< TSharedPtr<FDisplayedAnimNotifyInfo> > SAnimNotifyListType;

class SSkeletonAnimNotifies : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SSkeletonAnimNotifies )
		: _Persona()
	{}

	SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )
		
		SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	~SSkeletonAnimNotifies();

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

	/** When user attempts to commit the name of a track*/
	bool OnVerifyNotifyNameCommit( const FText& NewName, FText& OutErrorMessage, TSharedPtr<FDisplayedAnimNotifyInfo> Item );

	/** When user commits the name of a track*/
	void OnNotifyNameCommitted( const FText& NewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimNotifyInfo> Item );

	/** Dummy handler to stop editable text boxes swallowing our list selected events */
	bool IsSelected(){return false;}
private:

	/** Called when the user changes the contents of the search box */
	void OnFilterTextChanged( const FText& SearchText );

	/** Called when the user changes the contents of the search box */
	void OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo );

	/** Delegate handler for generating rows in NotifiesListView */ 
	TSharedRef<ITableRow> GenerateNotifyRow( TSharedPtr<FDisplayedAnimNotifyInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable );

	/** Delegate handler called when the user right clicks in NotifiesListView */
	TSharedPtr<SWidget> OnGetContextMenuContent() const;

	/** Delegate handler for when the user selects something in NotifiesListView */
	void OnNotifySelectionChanged( TSharedPtr<FDisplayedAnimNotifyInfo> Selection, ESelectInfo::Type SelectInfo );

	/** Delegate handler for determining whether we can show the delete menu options */
	bool CanPerformDelete() const;

	/** Delegate handler for deleting anim notifies */
	void OnDeleteAnimNotify();

	/** Delegate handler for determining whether we can show the rename menu options */
	bool CanPerformRename() const;

	/** Delegate handler for renaming anim notifies */
	void OnRenameAnimNotify();

	/** Wrapper that populates NotifiesListView using current filter test */
	void RefreshNotifiesListWithFilter();

	/** Populates NotifiesListView based on the skeletons notifies and the supplied filter text */
	void CreateNotifiesList( const FString& SearchText = FString("") );

	/** handler for user selecting a Notify in NotifiesListView - populates the details panel */
	void ShowNotifyInDetailsView( FName NotifyName );

	/** Populates OutAssets with the AnimSequences that match Personas current skeleton */
	void GetCompatibleAnimSequences( TArray<class FAssetData>& OutAssets );

	/** Utility function to display notifications to the user */
	void NotifyUser( FNotificationInfo& NotificationInfo );

	/** The owning Persona instance */
	TWeakPtr<class FPersona> PersonaPtr;

	/** The skeleton we are currently editing */
	USkeleton* TargetSkeleton;

	/** SSearchBox to filter the notify list */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** Widget used to display the list of notifies */
	TSharedPtr<SAnimNotifyListType> NotifiesListView;

	/** A list of notifies. Used by the NotifiesListView. */
	TArray< TSharedPtr<FDisplayedAnimNotifyInfo> > NotifyList;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Tracks objects created for displaying in the details panel*/
	FEditorObjectTracker EditorObjectTracker;
};

#undef LOCTEXT_NAMESPACE