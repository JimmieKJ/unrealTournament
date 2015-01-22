// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorUserData.h"
#include "KismetEditorUtilities.h"

namespace nLiveEditorUserData
{
	static const int32 DataVersion = 3;
};

FLiveEditorUserData::FLiveEditorUserData()
{
	LoadFromDisk();
}

FLiveEditorUserData::~FLiveEditorUserData()
{
}

void FLiveEditorUserData::AddBlueprintBinding( const FString &BlueprintName, const FString &EventName, const FLiveEditBinding &Binding, bool SaveNow )
{
	//store the binding in our map
	TMap<FString, struct FLiveEditBinding> *BlueprintBindingMap = BlueprintBindings.Find(BlueprintName);
	if ( BlueprintBindingMap == NULL )
	{
		TMap<FString, struct FLiveEditBinding> NewMap;
		BlueprintBindingMap = &BlueprintBindings.Add( BlueprintName, NewMap );
	}
	BlueprintBindingMap->Add( EventName, Binding );

	if ( SaveNow )
	{
		Save();
	}
}

bool FLiveEditorUserData::HasBlueprintBindings( const UBlueprint &Blueprint ) const
{
	const TMap<FString, struct FLiveEditBinding> *BlueprintBindingMap = BlueprintBindings.Find(Blueprint.GetName());
	return ( BlueprintBindingMap != NULL );
}

const FLiveEditBinding *FLiveEditorUserData::GetBinding( class FString &BlueprintName, const FString &EventName ) const
{
	const TMap<FString, FLiveEditBinding> *BlueprintBindingMap = BlueprintBindings.Find(BlueprintName);
	if ( BlueprintBindingMap == NULL )
		return NULL;

	const FLiveEditBinding *Binding = BlueprintBindingMap->Find( EventName );
	return Binding;
}

void FLiveEditorUserData::SaveCheckpointData( const FString &BlueprintName, const TMap< int32, TArray<FLiveEditorCheckpointData> > &Data, bool SaveNow )
{
	Checkpoints.Add( BlueprintName, Data );

	if ( SaveNow )
	{
		Save();
	}
}

bool FLiveEditorUserData::LoadCheckpointData( const FString &BlueprintName, TMap< int32, TArray<FLiveEditorCheckpointData> > &Data )
{
	TMap< int32, TArray<FLiveEditorCheckpointData> > *StoredData = Checkpoints.Find(BlueprintName);
	if ( StoredData != NULL )
	{
		Data.Empty();
		for ( TMap< int32, TArray<FLiveEditorCheckpointData> >::TConstIterator It(*StoredData); It; ++It )
		{
			Data.Add( (*It).Key, (*It).Value );
		}
		return true;
	}
	else
	{
		return false;
	}
}

void FLiveEditorUserData::Save()
{
	SaveToDisk();
}

void FLiveEditorUserData::SaveToDisk()
{
	FString FileName = FString( TEXT( "LiveEditorUserData.bin" ) );
	FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::CloudDir());
	FString FullPath = FString::Printf( TEXT( "%s%s" ), *FileLocation, *FileName );

	FArchive* SaveFile = IFileManager::Get().CreateFileWriter( *FullPath );

	int32 Version = nLiveEditorUserData::DataVersion;
	*SaveFile << Version;

	*SaveFile << BlueprintBindings;
	*SaveFile << Checkpoints;

	SaveFile->Close();
}

void FLiveEditorUserData::LoadFromDisk()
{
	FString FileName = FString( TEXT( "LiveEditorUserData.bin" ) );
	FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::CloudDir());
	FString FullPath = FString::Printf( TEXT( "%s%s" ), *FileLocation, *FileName );

	FArchive* SaveFile = IFileManager::Get().CreateFileReader( *FullPath );
	if ( SaveFile == NULL )
		return;

	int32 Version = -1;
	*SaveFile << Version;

	if ( Version >= 2 ) //Tool is not backwards compatible with any data versions less than 2
	{
		*SaveFile << BlueprintBindings;
	}
	if ( Version >= 3 ) //Version 3 introduced Checkpoints to the save data so discard the data if it's older than that
	{
		*SaveFile << Checkpoints;
	}

	SaveFile->Close();
}
