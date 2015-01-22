// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotData.cpp: Implements the FScreenShotData class.
=============================================================================*/

#include "ScreenShotComparisonToolsPrivatePCH.h"


/* IScreenShotData interface
 *****************************************************************************/

void FScreenShotBaseNode::AddScreenShotData( const FScreenShotDataItem& InScreenDataItem )
{
	TSharedRef<IScreenShotData> ScreenShotNode = AddChild( InScreenDataItem.ViewName );
	ScreenShotNode->AddScreenShotData( InScreenDataItem );
}


const FString& FScreenShotBaseNode::GetAssetName() const
{
	// This could be a dynamic brush. At the moment, it is just a brush name
	return AssetName;
}


TArray<IScreenShotDataPtr>& FScreenShotBaseNode::GetChildren()
{
	return Children;
}


TArray<IScreenShotDataPtr>& FScreenShotBaseNode::GetFilteredChildren()
{
	// Get the child list after filtering
	return FilteredChildren;
}


const FString& FScreenShotBaseNode::GetName() const
{
	// The item name e.g. screen shot number / platform name
	return ItemName;
}


EScreenShotDataType::Type FScreenShotBaseNode::GetScreenNodeType()
{ 
	return EScreenShotDataType::SSDT_Base; 
};

void FScreenShotBaseNode::SetDisplayEveryNthScreenshot( int32 NewDisplayEveryNth )
{
	for ( int32 Index = 0; Index < Children.Num(); Index++ )
	{
		Children[Index]->SetDisplayEveryNthScreenshot(NewDisplayEveryNth);
	}
}


bool FScreenShotBaseNode::SetFilter( TSharedPtr< ScreenShotFilterCollection > ScreenFilter )
{
	FilteredChildren.Empty();

	if ( ScreenFilter.IsValid() )
	{
		for ( int32 Index = 0; Index < Children.Num(); Index++ )
		{
			if ( Children[Index]->SetFilter( ScreenFilter ) )
			{
				FilteredChildren.Add( Children[Index] );
			}
		}
	}

	return true;
}


/* FScreenShotBaseNode implementation
 *****************************************************************************/
	
IScreenShotDataRef FScreenShotBaseNode::AddChild( const FString& ChildName )
{
	// See if child node exists - if not, add one
	TSharedPtr< IScreenShotData > ScreenShotNode = NULL;
	for ( int32 Index = 0; Index < Children.Num(); Index++ )
	{
		if ( Children[Index]->GetName() == ChildName )
		{
			ScreenShotNode = Children[ Index ];
			break;
		}
	}

	if ( !ScreenShotNode.IsValid() )
	{
		ScreenShotNode = CreateNode( ChildName );
	}

	return ScreenShotNode.ToSharedRef();
}


IScreenShotDataRef FScreenShotBaseNode::CreateNode( const FString& ChildName )
{
	TSharedPtr< IScreenShotData > ScreenShotNode = MakeShareable( new FScreenShotScreenNode( ChildName ) );
	Children.Add( ScreenShotNode.ToSharedRef() );

	return ScreenShotNode.ToSharedRef();
}
