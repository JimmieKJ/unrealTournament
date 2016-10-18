// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Persona.h"

//////////////////////////////////////////////////////////////////////////
// FDisplayedBoneMappingInfo

class FDisplayedBoneMappingInfo
{
public:
	FName Name;
	FString DisplayName;
	USkeleton* Skeleton;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedBoneMappingInfo> Make(const FName NodeName, const FString DisplayName, USkeleton* InSkeleton)
	{
		return MakeShareable(new FDisplayedBoneMappingInfo(NodeName, DisplayName, InSkeleton));
	}

	FName GetNodeName() const
	{
		return Name;
	}

	FString GetDisplayName() const
	{
		return DisplayName;
	}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedBoneMappingInfo(const FName InNodeName, const FString InDisplayName, USkeleton* InSkeleton)
		: Name( InNodeName )
		, DisplayName( InDisplayName )
		, Skeleton( InSkeleton )
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedBoneMappingInfo() {}
};

typedef SListView< TSharedPtr<FDisplayedBoneMappingInfo> > SBoneMappingListType;

//////////////////////////////////////////////////////////////////////////
// SRigWindow

class SRigWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRigWindow )
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
	virtual ~SRigWindow();

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
	TSharedRef<ITableRow> GenerateBoneMappingRow(TSharedPtr<FDisplayedBoneMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	* Return true if it can delete
	*
	*/
	bool CanPerformRefresh() const;

	/**
	* Handler for the delete of retarget source
	*/
	void OnRefreshBoneMappingList();

	/**
	* Accessor so our rows can grab the filtertext for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }
private:

	/**
	* Clears and rebuilds the table, according to an optional search string
	*
	* @param SearchText - Optional search string
	*
	*/
	void CreateBoneMappingList( const FString& SearchText = FString("") );

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;

	/** Box to filter to a specific morph target name */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** Widget used to display the list of retarget sources */
	TSharedPtr<SBoneMappingListType> BoneMappingListView;

	/** A list of retarget sources. Used by the BoneMappingListView. */
	TArray< TSharedPtr<FDisplayedBoneMappingInfo> > BoneMappingList;

	/** The skeletal mesh that we grab the morph targets from */
	USkeleton* Skeleton;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** show advanced? */
	bool bDisplayAdvanced;

	/** rig combo button */
	TSharedPtr< class SComboButton > AssetComboButton;

	/** Delegate for undo/redo transaction **/
	void PostUndo();

	/**
	 * Callback for asset picker
	 */
	/* Set rig set combo box*/
	void OnAssetSelected(UObject* Object);
	FText GetAssetName() const;
	void CloseComboButton();
	TSharedRef<SWidget> MakeRigPickerWithMenu();

	/** Returns true if the asset shouldn't show  */
	bool ShouldFilterAsset(const class FAssetData& AssetData);

	URig* GetRigObject() const;
	
	void OnBoneMappingChanged( FName NodeName, FName BoneName );
	FName GetBoneMapping( FName NodeName );

	FReply OnAutoMapping();
	FReply OnClearMapping();
	FReply OnToggleView();

	FReply OnToggleAdvanced();
	FText GetAdvancedButtonText() const;

	bool SelectSourceReferenceSkeleton(URig* Rig) const;
	bool OnTargetSkeletonSelected(USkeleton* SelectedSkeleton, URig*  Rig) const;
};