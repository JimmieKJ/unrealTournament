// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotData.h: Declares the FScreenShotBaseNode class.
=============================================================================*/

#pragma once


/**
 * Base class for screenshot nodes.
 */
class FScreenShotBaseNode
	: public TSharedFromThis<FScreenShotBaseNode>
	, public IScreenShotData
{
public:

	/**
	* Construct test data with the given name.
	*
	* @param InName  The Name of this test data item.
	*/
	FScreenShotBaseNode( const FString& InName, const FString& InAssetName = "" )
		: AssetName(InAssetName)
		, ItemName(InName)
	{ }

public:

	// Begin IScreenShotData interface

	virtual void AddScreenShotData( const FScreenShotDataItem& InScreenDataItem ) override;
	virtual const FString& GetAssetName() const override;
	virtual TArray< TSharedPtr< IScreenShotData> >& GetChildren() override;
	virtual TArray< TSharedPtr< IScreenShotData> >& GetFilteredChildren() override;
	virtual const FString& GetName() const override;
	virtual EScreenShotDataType::Type GetScreenNodeType() override;
	virtual bool SetFilter( TSharedPtr< ScreenShotFilterCollection > ScreenFilter ) override;
	virtual void SetDisplayEveryNthScreenshot( int32 NewNthValue ) override;

	// End IScreenShotData interface

protected:

	/**
	 * Add a child to the tree.
	 *
	 * @param ChildName - The name of the node.
	 * @return The new node.
	 */
	IScreenShotDataRef AddChild( const FString& ChildName );

	/**
	 * Create a node to add to the tree.
	 *
	 * @param ChildName - The name of the node.
	 * @return The new node.
	 */

	virtual IScreenShotDataRef CreateNode( const FString& ChildName );

public:

	// The asset name
	FString AssetName;

	// Holds the array of child nodes
	TArray<IScreenShotDataPtr> Children;

	// Holds the array of filtered nodes
	TArray<IScreenShotDataPtr> FilteredChildren;

	// The item name
	FString ItemName;
};
