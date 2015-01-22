// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PhysicsEngine/PhysicsSettings.h"

DECLARE_DELEGATE(FOnCommitChange)

// Class containing the friend information - used to build the list view
class FPhysicalSurfaceListItem
{
public:

	/**
	* Constructor takes the required details
	*/
	FPhysicalSurfaceListItem(TSharedPtr<FPhysicalSurfaceName> InPhysicalSurface)
		: PhysicalSurface(InPhysicalSurface)
	{}

	TSharedPtr<FPhysicalSurfaceName> PhysicalSurface;
};

/**
* Implements the FriendsList
*/
class SPhysicalSurfaceListItem
	: public SMultiColumnTableRow< TSharedPtr<class FPhysicalSurfaceListItem> >
{
public:

	SLATE_BEGIN_ARGS(SPhysicalSurfaceListItem) { }
		SLATE_ARGUMENT(TSharedPtr<FPhysicalSurfaceName>, PhysicalSurface)
		SLATE_ARGUMENT(UEnum*, PhysicalSurfaceEnum)
		SLATE_EVENT(FOnCommitChange, OnCommitChange)
	SLATE_END_ARGS()

public:

	/**
	* Constructs the application.
	*
	* @param InArgs - The Slate argument list.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	FString GetTypeString() const;

	FText GetName() const;
	void NewNameEntered(const FText& NewText, ETextCommit::Type CommitInfo);
	void OnTextChanged(const FText& NewText);

private:
	TSharedPtr<FPhysicalSurfaceName>	PhysicalSurface;
	UEnum *								PhysicalSurfaceEnum;
	FOnCommitChange						OnCommitChange;
	TSharedPtr<SEditableTextBox>		NameBox;
};

typedef  SListView< TSharedPtr< FPhysicalSurfaceListItem > > SPhysicalSurfaceListView;

class FPhysicsSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:

	/**
	* Generates a widget for a channel item.
	* @param InItem - the ChannelListItem
	* @param OwnerTable - the owning table
	* @return The table row widget
	*/
	TSharedRef<ITableRow> HandleGenerateListWidget(TSharedPtr< FPhysicalSurfaceListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);


private:
	TSharedPtr<SPhysicalSurfaceListView>				PhysicalSurfaceListView;
	TArray< TSharedPtr< FPhysicalSurfaceListItem > >	PhysicalSurfaceList;

	UPhysicsSettings *							PhysicsSettings;
	UEnum*										PhysicalSurfaceEnum;
	// functions
	void UpdatePhysicalSurfaceList();
	void RefreshPhysicalSurfaceList();
	void OnCommitChange();
};

