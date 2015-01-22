// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Persona.h"

//////////////////////////////////////////////////////////////////////////
// FDisplayedRetargetSourceInfo

class FDisplayedRetargetSourceInfo
{
public:
	FName Name;
	USkeletalMesh * ReferenceMesh;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedRetargetSourceInfo> Make(const FName& SourcePose, USkeletalMesh * ReferenceMesh)
	{
		return MakeShareable(new FDisplayedRetargetSourceInfo(SourcePose, ReferenceMesh));
	}

	FString GetReferenceMeshName()
	{
		return (ReferenceMesh)? ReferenceMesh->GetPathName() : TEXT("None");
	}

	/** Requests a rename on the socket item */
	void RequestRename()
	{
		OnEnterEditingMode.ExecuteIfBound();
	}

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnEnterEditingMode);
	FOnEnterEditingMode OnEnterEditingMode;

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedRetargetSourceInfo(const FName& InSource, USkeletalMesh * RefMesh)
		: Name( InSource )
		, ReferenceMesh( RefMesh )
		, bRenamePending( false )
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedRetargetSourceInfo() {}

	bool bRenamePending;
};

typedef SListView< TSharedPtr<FDisplayedRetargetSourceInfo> > SRetargetSourceListType;

//////////////////////////////////////////////////////////////////////////
// SRetargetSourceWindow

class SRetargetSourceWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRetargetSourceWindow )
		: _Persona()
	{}
		
		/* The Persona that owns this table */
		SLATE_ARGUMENT( TWeakPtr< FPersona >, Persona )

	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct( const FArguments& InArgs );

	/**
	* Destructor - resets the morph targets
	*
	*/
	virtual ~SRetargetSourceWindow();

	/**
	* Filters the SListView when the user changes the search text box (NameFilterBox)
	*
	* @param SearchText - The text the user has typed
	*
	*/
	void OnFilterTextChanged( const FText& SearchText );

	/**
	* Filters the SListView when the user hits enter or clears the search box
	* Simply calls OnFilterTextChanged
	*
	* @param SearchText - The text the user has typed
	* @param CommitInfo - Not used
	*
	*/
	void OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo );

	/**
	* Create a widget for an entry in the tree from an info
	*
	* @param InInfo - Shared pointer to the morph target we're generating a row for
	* @param OwnerTable - The table that owns this row
	*
	* @return A new Slate widget, containing the UI for this row
	*/
	TSharedRef<ITableRow> GenerateRetargetSourceRow(TSharedPtr<FDisplayedRetargetSourceInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	* Handler for adding new retarget source. It displays the asset picker
	*/
	void OnAddRetargetSource();

	/**
	 * Callback for asset picker
	 */
	void OnAssetSelectedFromMeshPicker(const FAssetData& AssetData);

	/**
	* Adds a new retarget source with given name
	*
	* @param Name - Name of retarget source
	* @param ReferenceMesh	- Reference Mesh this pose is based on
	*
	*/
	void AddRetargetSource( const FName Name, USkeletalMesh * ReferenceMesh  );

	/**
	* Return true if it can delete
	*
	*/
	bool CanPerformDelete() const;

	/**
	* Handler for the delete of retarget source
	*/
	void OnDeleteRetargetSource();

	/**
	* Return true if it can perform rename
	*
	*/
	bool CanPerformRename() const;

	/**
	* Handler for the rename of retarget source
	*/
	void OnRenameRetargetSource();

	/**
	* Return true if it can delete
	*
	*/
	bool CanPerformRefresh() const;

	/**
	* Handler for the delete of retarget source
	*/
	void OnRefreshRetargetSource();

	/**
	* Accessor so our rows can grab the filtertext for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }

	// SWidget interface
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	// end of SWidget interface
private:

	/** Handler for context menus */
	TSharedPtr<SWidget> OnGetContextMenuContent() const;

	/**
	* Clears and rebuilds the table, according to an optional search string
	*
	* @param SearchText - Optional search string
	*
	*/
	void CreateRetargetSourceList( const FString& SearchText = FString(""), const FName  NewName = NAME_None );

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;

	/** Box to filter to a specific morph target name */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** Widget used to display the list of retarget sources */
	TSharedPtr<SRetargetSourceListType> RetargetSourceListView;

	/** A list of retarget sources. Used by the RetargetSourceListView. */
	TArray< TSharedPtr<FDisplayedRetargetSourceInfo> > RetargetSourceList;

	/** The skeletal mesh that we grab the morph targets from */
	USkeleton* Skeleton;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Item to rename. Only valid for adding **/
	mutable TSharedPtr<FDisplayedRetargetSourceInfo> ItemToRename;

	/** data to handle renamed pose when undo action is executed */
	struct FPoseNameForUndo
	{
		FName OldPoseName;
		FName NewPoseName;

		FPoseNameForUndo(FName OldName, FName NewName)
			: OldPoseName(OldName), NewPoseName(NewName)
		{}
	};

	/** stack for renamed poses for multiple undo actions **/
	TArray<FPoseNameForUndo> RenamedPoseNameStack;

	/** Rename committed. Called by Inline Widget **/
	void OnRenameCommit( const FName& OldName,  const FString& NewName );
	/** Verify Rename is legit or not **/
	bool OnVerifyRenameCommit( const FName& OldName, const FString& NewName, FText& OutErrorMessage);

	/** Delegate for undo/redo transaction **/
	void PostUndo();

	/** Button handler **/
	FReply OnAddRetargetSourceButtonClicked();
};