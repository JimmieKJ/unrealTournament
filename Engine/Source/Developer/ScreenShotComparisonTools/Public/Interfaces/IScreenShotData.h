// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotData.h: Declares the IScreenShotData interface.
=============================================================================*/

#pragma once


/** Flags for specifying the screen shot data type */
namespace EScreenShotDataType
{
	enum Type
	{
		SSDT_Base,
		SSDT_Platform,
		SSDT_ScreenView,
	};
};


/**
 * Type definition for shared pointers to instances of IScreenShotData.
 */
typedef TSharedPtr<class IScreenShotData> IScreenShotDataPtr;

/**
 * Type definition for shared references to instances of IScreenShotData.
 */
typedef TSharedRef<class IScreenShotData> IScreenShotDataRef;


/**
 * Interface for Screen shot data.
 */
class IScreenShotData
{
public:
	virtual ~IScreenShotData(){ }

	/**
	* Add screen shot data to the tree
	*
	* @param InScreenDataItem - the screen shot data
	*/
	virtual void AddScreenShotData( const FScreenShotDataItem& InScreenDataItem ) = 0;

	/**
	* Get the screen shot asset name. This will be a dynamic brush at some point
	*
	* @return The asset name
	*/
	virtual const FString& GetAssetName() const = 0;

	/**
	* Get children
	*
	* @return The child list
	*/
	virtual TArray<IScreenShotDataPtr>& GetChildren() = 0;

	/**
	* Get the filtered child list
	*
	* @return The filtered child list
	*/
	virtual TArray<IScreenShotDataPtr>& GetFilteredChildren() = 0;

	/**
	* Get the node type - used in filtering
	*
	* @return The node type
	*/
	virtual EScreenShotDataType::Type GetScreenNodeType() = 0;

	/**
	* Get the screen shot data name e.g. change list number
	*
	* @return The screen shot data name
	*/
	virtual const FString& GetName() const = 0;

	/**
	* Set the platform filter
	*
	* @param ScreenFilter - the screen name to filter
	* @return True if passes the filter
	*/
	virtual bool SetFilter( TSharedPtr< ScreenShotFilterCollection > ScreenFilter ) = 0;

	/**
	* Tells the node that we should only display every Nth screenshot
	*
	* @param NewDisplayEveryNth - The new N.
	*/
	virtual void SetDisplayEveryNthScreenshot( int32 NewDisplayEveryNth ) = 0;
};