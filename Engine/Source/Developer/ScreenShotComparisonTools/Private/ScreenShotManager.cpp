// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotManager.cpp: Implements the FScreenShotManager class.
=============================================================================*/

#include "ScreenShotComparisonToolsPrivatePCH.h"


/* FScreenShotManager structors
 *****************************************************************************/

FScreenShotManager::FScreenShotManager( const IMessageBusRef& InMessageBus )
{
	MessageEndpoint = FMessageEndpoint::Builder("FScreenShotToolsModule", InMessageBus)
		.Handling<FAutomationWorkerScreenImage>(this, &FScreenShotManager::HandleScreenShotMessage);

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Subscribe<FAutomationWorkerScreenImage>();
	}
}


/* FScreenShotManager interface
 *****************************************************************************/

void FScreenShotManager::CreateData()
{
	// Clear existing data
	CachedPlatformList.Empty();
	ScreenShotDataArray.Empty();

	// Root directory
	FString PathName = FPaths::AutomationDir();

	// Array to store the list of platforms
	TArray<FString> TempPlatforms;

	TArray<FString> AllPNGFiles;
	IFileManager::Get().FindFilesRecursive( AllPNGFiles, *PathName, TEXT("*.png"), true, false );

	const FString RenderOutputValidation(TEXT("RenderOutputValidation"));

	for ( int32 PathIndex = 0; PathIndex < AllPNGFiles.Num(); PathIndex++ )
	{
		// <Stream or NetworkPath>/<CLNumber or Name>/<OS>_<RHI>/Project(<Project>) Map(<Map>) Time(<Time>s).png

		FString PathString = AllPNGFiles[PathIndex];
		FString PathRemainder;

		// remove ../../../ in front
		FPaths::MakePathRelativeTo(PathString, *PathName);

		// remove RenderOutputValidation in front
		{
			FString PathFront;
			if (PathString.Split(TEXT("/"), &PathFront, &PathRemainder, ESearchCase::CaseSensitive, ESearchDir::FromStart) )
			{
				if(PathFront != RenderOutputValidation)
				{
					continue;
				}
			}
			else
			{
				continue;
			}
		}

		// get Changelist
		FString CLName;
		if (PathRemainder.Split(TEXT("/"), &CLName, &PathRemainder, ESearchCase::CaseSensitive, ESearchDir::FromStart) )
		{
			int32 ChangeListNumber = 0;

			if(CLName.Len() > 2 && CLName[0]== TCHAR('C') && CLName[1]== TCHAR('L'))
			{
				// todo: later we want to support arbitrary names as well
				ChangeListNumber = FCString::Atoi(*CLName.RightChop(2));
			}

			// get Platform and RHI
			FString Platform_RHI;
			if (PathRemainder.Split(TEXT("/"), &Platform_RHI, &PathRemainder, ESearchCase::CaseSensitive, ESearchDir::FromStart) )
			{
				if(!PathRemainder.Contains(TEXT("/")))
				{
					// without extension
					FString ScreenShotName = *FPaths::GetBaseFilename(PathRemainder);

					TempPlatforms.AddUnique( Platform_RHI );

					// e.g. Map(RenderTestMap) Actor(GameEngine_0) Time(1.61s)
					if ( !ScreenShotName.IsEmpty() )
					{
 						FScreenShotDataItem ScreenItem( ScreenShotName, Platform_RHI, AllPNGFiles[PathIndex], ChangeListNumber );
 						ScreenShotDataArray.Add( ScreenItem );
					}
				}
			}
		}
	}

	// Sort change list.
	struct FSortDataItems
	{
		FORCEINLINE bool operator()( const FScreenShotDataItem A, const FScreenShotDataItem B ) const 
		{ 
			if ( B.ViewName == A.ViewName )
			{
				return B.ChangeListNumber < A.ChangeListNumber;
			}
			else
			{
				return FCString::Stricmp( *B.ViewName, *A.ViewName ) > 0;
			}
		}
	};
	ScreenShotDataArray.Sort( FSortDataItems() );

	// Add platforms for searching
	CachedPlatformList.Add( MakeShareable(new FString("Any") ) );

	for ( int32 PlatformsCounter = 0; PlatformsCounter < TempPlatforms.Num(); PlatformsCounter++ )
	{
		TSharedPtr<FString> PlatformName = MakeShareable( new FString( TempPlatforms[ PlatformsCounter ] ) );
		CachedPlatformList.Add( PlatformName );
	}

	SetFilter( NULL );
}


/* IScreenShotManager interface
 *****************************************************************************/

void FScreenShotManager::GenerateLists()
{
	// Create the screen shot tree root
	ScreenShotRoot = MakeShareable( new FScreenShotBaseNode( "ScreenShotRoot" ) ); 

	// Create the screen shot data
	CreateData();

	// Populate the screen shot tree with the dummy screen shot data
	for ( int32 Index = 0; Index < ScreenShotDataArray.Num(); Index++ )
	{
		ScreenShotRoot->AddScreenShotData( ScreenShotDataArray[ Index ] );
	}
}


TArray<TSharedPtr<FString> >& FScreenShotManager::GetCachedPlatfomList()
{
	return CachedPlatformList;
}


TArray<IScreenShotDataPtr>& FScreenShotManager::GetLists()
{
	return ScreenShotRoot->GetFilteredChildren();
}


void FScreenShotManager::RegisterScreenShotUpdate(const FOnScreenFilterChanged& InDelegate )
{
	// Set the callback delegate
	ScreenFilterChangedDelegate = InDelegate;
}


void FScreenShotManager::SetFilter( TSharedPtr< ScreenShotFilterCollection > InFilter )
{
	// Set the filter on the root screen shot
	ScreenShotRoot->SetFilter( InFilter );

	// Update the UI
	ScreenFilterChangedDelegate.ExecuteIfBound();
}

void FScreenShotManager::SetDisplayEveryNthScreenshot(int32 NewNth)
{
	ScreenShotRoot->SetDisplayEveryNthScreenshot(NewNth);

	// Update the UI
	ScreenFilterChangedDelegate.ExecuteIfBound();
}


/* IScreenShotManager event handlers
 *****************************************************************************/

void FScreenShotManager::HandleScreenShotMessage( const FAutomationWorkerScreenImage& Message, const IMessageContextRef& Context )
{
	bool bTree = true;
	FString FileName = FPaths::RootDir() + Message.ScreenShotName;
	IFileManager::Get().MakeDirectory( *FPaths::GetPath(FileName), bTree );
	FFileHelper::SaveArrayToFile( Message.ScreenImage, *FileName );

	// Regenerate the UI
	GenerateLists();

	ScreenShotRoot->SetFilter(MakeShareable( new ScreenShotFilterCollection()));
}
