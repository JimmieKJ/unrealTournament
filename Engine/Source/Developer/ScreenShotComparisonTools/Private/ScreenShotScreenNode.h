// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotScreenNode.h: Declares the FScreenShotScreenNode class.
=============================================================================*/

#pragma once

class FScreenShotScreenNode : public FScreenShotBaseNode
{
public:

	/**
	* Construct test data given a name.
	*
	* @param InName  The Name of this test data item.
	* @param InAssetName  The Name of this asset.
	*/
	FScreenShotScreenNode( const FString& InName, const FString& InAssetName = "" )
	: FScreenShotBaseNode( InName, InAssetName )
	{}

public:

	// Begin IScreenShotData interface

	virtual void AddScreenShotData( const FScreenShotDataItem& InScreenDataItem ) override 
	{
		// First stage of adding screen shot data - create the screen shot node, then add the platform
		TSharedRef<IScreenShotData> ScreenNode = AddChild( InScreenDataItem.DeviceName );
		ScreenNode->AddScreenShotData( InScreenDataItem );
	};


	virtual EScreenShotDataType::Type GetScreenNodeType() override 
	{ 
		return EScreenShotDataType::SSDT_ScreenView; 
	};

	virtual bool SetFilter( TSharedPtr< ScreenShotFilterCollection > ScreenFilter ) override
	{
		FilteredChildren.Empty();

		bool bPassesFilter = ScreenFilter->PassesAllFilters( SharedThis( this ) );

		if( bPassesFilter )
		{
			for ( int32 Index = 0; Index < Children.Num(); Index++ )
			{
				if ( Children[Index]->SetFilter( ScreenFilter ) )
				{
					FilteredChildren.Add( Children[Index] );
				}
			}
		}

		return FilteredChildren.Num() > 0;
	}

	// End IScreenShotData interface

	// Begin SScreenShotNodeBase interface

	virtual TSharedRef<IScreenShotData> CreateNode( const FString& ChildName ) override 
	{
		TSharedPtr< IScreenShotData > ScreenShotNode = MakeShareable( new FScreenShotPlatformNode( ChildName ) );;
		Children.Add( ScreenShotNode.ToSharedRef() );
		return ScreenShotNode.ToSharedRef();
	};

	// End SScreenShotNodeBase interface
};

