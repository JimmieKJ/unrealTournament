// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotChangeListNode.h: Declares the FScreenShotChangeListNode class.
=============================================================================*/

#pragma once

class FScreenShotPlatformNode : public FScreenShotBaseNode
{
public:

	/**
	* Construct test data given a name.
	*
	* @param InName  The Name of this test data item.
	* @param InName  The Name of this asset.
	*/
	FScreenShotPlatformNode( const FString& InName, const FString& InAssetName = "" )
	: FScreenShotBaseNode( InName, InAssetName ),
	DisplayEveryNthScreenshot(0)
	{}

public:

	// Begin IScreenShotData interface

	virtual void AddScreenShotData( const FScreenShotDataItem& InScreenDataItem ) override 
	{
		FString ScreenShotNumber = FString::Printf( TEXT( "CL #%d"), InScreenDataItem.ChangeListNumber );
		TSharedRef<FScreenShotBaseNode> ChildNode = MakeShareable( new FScreenShotBaseNode( ScreenShotNumber, InScreenDataItem.AssetName) );
		Children.Add( ChildNode );
	};


	virtual EScreenShotDataType::Type GetScreenNodeType() override 
	{ 
		return EScreenShotDataType::SSDT_Platform; 
	};


	virtual bool SetFilter( TSharedPtr< ScreenShotFilterCollection > ScreenFilter ) override 
	{
		FilteredChildren.Empty();

		bool bPassesFilter = ScreenFilter->PassesAllFilters( SharedThis( this ) );
		if( bPassesFilter )
		{
			for ( int32 Index = 0; Index < Children.Num(); Index++ )
			{
				if( DisplayEveryNthScreenshot > 1 && Index % DisplayEveryNthScreenshot != 0 )
				{
					continue;
				}
				FilteredChildren.Add( Children[Index] );
			}
		}

		return bPassesFilter;
	};

	virtual void SetDisplayEveryNthScreenshot( int32 NewLastNth ) override
	{
		DisplayEveryNthScreenshot = NewLastNth;
		if( FilteredChildren.Num() > 0 )
		{
			FilteredChildren.Empty();
			for ( int32 Index = 0; Index < Children.Num(); Index++ )
			{
				if( DisplayEveryNthScreenshot > 1 && Index % DisplayEveryNthScreenshot != 0 )
				{
					continue;
				}
				FilteredChildren.Add( Children[Index] );
			}
		}
	}


	// End IScreenShotData interface

private:
	// If > 1 we will only show every Nth screenshot.
	uint32 DisplayEveryNthScreenshot;
};

