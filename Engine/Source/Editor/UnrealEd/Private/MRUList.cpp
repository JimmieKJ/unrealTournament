// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ConfigCacheIni.h"
#include "MessageLog.h"

FMRUList::FMRUList(const FString& InINISection, const int32 InitMaxItems)
	:	MaxItems( InitMaxItems ),
		INISection( InINISection )
{
}


FMRUList::~FMRUList()
{
	Items.Empty();
}


void FMRUList::Cull()
{
	while( Items.Num() > GetMaxItems() )
	{
		Items.RemoveAt( Items.Num()-1 );
	}
}


void FMRUList::ReadFromINI()
{
	InternalReadINI( Items, INISection, TEXT("MRUItem"), GetMaxItems() );
}


void FMRUList::WriteToINI() const
{
	InternalWriteINI( Items, INISection, TEXT("MRUItem") );
}


void FMRUList::MoveToTop(int32 InItem)
{
	check( InItem > -1 && InItem < Items.Num() );

	TArray<FString> WkArray;
	WkArray = Items;

	const FString Save = WkArray[InItem];
	WkArray.RemoveAt( InItem );

	Items.Empty();
	new(Items)FString( *Save );
	Items += WkArray;
}


void FMRUList::AddMRUItem(const FString& InItem)
{
	FString CleanedName = FPaths::ConvertRelativePathToFull(InItem);

	// See if the item already exists in the list.  If so,
	// move it to the top of the list and leave.
	const int32 ItemIndex = Items.Find( CleanedName );
	if ( ItemIndex != INDEX_NONE )
	{
		MoveToTop( ItemIndex );
	}
	else
	{
		// Item is new, so add it to the bottom of the list.
		if( CleanedName.Len() )
		{
			new(Items) FString( *CleanedName );
			MoveToTop( Items.Num()-1 );
		}

		Cull();
	}
	
	WriteToINI();
}


int32 FMRUList::FindMRUItemIdx(const FString& InItem) const
{
	for( int32 mru = 0 ; mru < Items.Num() ; ++mru )
	{
		if( Items[mru] == InItem )
		{
			return mru;
		}
	}

	return INDEX_NONE;
}


void FMRUList::RemoveMRUItem(const FString& InItem)
{
	RemoveMRUItem( FindMRUItemIdx( InItem ) );
}


void FMRUList::RemoveMRUItem(int32 InItem)
{
	// Find the item and remove it.
	check( InItem > -1 && InItem < GetMaxItems() );
	Items.RemoveAt( InItem );
}


void FMRUList::InternalReadINI( TArray<FString>& OutItems, const FString& INISection, const FString& INIKeyBase, int32 NumElements )
{
	// Clear existing items
	OutItems.Empty();

	// Iterate over the maximum number of provided elements
	for( int32 ItemIdx = 0 ; ItemIdx < NumElements ; ++ItemIdx )
	{
		// Try to find data for a key formed as "INIKeyBaseItemIdx" for the provided INI section. If found, add the data to the output item array.
		FString CurItem;
		if ( GConfig->GetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), CurItem, GEditorPerProjectIni ) )
		{
			OutItems.AddUnique( FPaths::ConvertRelativePathToFull(CurItem) );
		}
	}
}


void FMRUList::InternalWriteINI( const TArray<FString>& InItems, const FString& INISection, const FString& INIKeyBase )
{
	GConfig->EmptySection( *INISection, GEditorPerProjectIni );

	for ( int32 ItemIdx = 0; ItemIdx < InItems.Num(); ++ItemIdx )
	{
		GConfig->SetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), *InItems[ ItemIdx ], GEditorPerProjectIni );
	}

	GConfig->Flush( false, GEditorPerProjectIni );
}


bool FMRUList::VerifyMRUFile(int32 InItem)
{
	check( InItem > -1 && InItem < GetMaxItems() );
	const FString filename = Items[InItem];

	// If the file doesn't exist, tell the user about it, remove the file from the list
	if( IFileManager::Get().FileSize( *filename ) == -1 )
	{
		FMessageLog EditorErrors("EditorErrors");
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FileName"), FText::FromString(filename));
		EditorErrors.Warning(FText::Format( NSLOCTEXT("MRUList", "Error_FileDoesNotExist", "File does not exist : '{FileName}'.  It will be removed from the recent items list."), Arguments ) );
		EditorErrors.Notify(NSLOCTEXT("MRUList", "Notification_FileDoesNotExist", "File does not exist! Removed from recent items list!"));
		RemoveMRUItem( InItem );
		WriteToINI();

		return false;
	}

	// Otherwise, move the file to the top of the list
	MoveToTop( InItem );
	WriteToINI();

	return true;
}
