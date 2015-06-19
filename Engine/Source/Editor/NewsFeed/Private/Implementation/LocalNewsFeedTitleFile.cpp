// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NewsFeedPrivatePCH.h"


IOnlineTitleFilePtr FLocalNewsFeedTitleFile::Create( const FString& RootDirectory )
{
	return MakeShareable( new FLocalNewsFeedTitleFile( RootDirectory ) );
}


FLocalNewsFeedTitleFile::FLocalNewsFeedTitleFile( const FString& InRootDirectory )
	: RootDirectory(InRootDirectory)
{ }


bool FLocalNewsFeedTitleFile::GetFileContents(const FString& DLName, TArray<uint8>& FileContents)
{
	const TArray< uint8 >* ExistingFileContents = DLNameToFileContents.Find( DLName );

	if (ExistingFileContents == nullptr)
	{
		return false;
	}

	FileContents.Append(*ExistingFileContents);

	return true;
}


bool FLocalNewsFeedTitleFile::ClearFiles( )
{
	FileHeaders.Empty();
	DLNameToFileContents.Empty();

	return true;
}


bool FLocalNewsFeedTitleFile::ClearFile( const FString& DLName )
{
	bool ClearedFile = false;
	const FString FileName = GetFileNameFromDLName(DLName);

	for (int Index = 0; Index < FileHeaders.Num(); Index++)
	{
		if (FileHeaders[Index].DLName == DLName)
		{
			FileHeaders.RemoveAt(Index);
			ClearedFile = true;
		}
	}

	DLNameToFileContents.Remove(DLName);

	return ClearedFile;
}


void FLocalNewsFeedTitleFile::DeleteCachedFiles(bool bSkipEnumerated)
{
	// not implemented
}

bool FLocalNewsFeedTitleFile::EnumerateFiles(const FPagedQuery& Page)
{
	const FString WildCard = FPaths::Combine(*RootDirectory, TEXT("*"));

	TArray<FString> Filenames;
	IFileManager::Get().FindFiles(Filenames, *WildCard, true, false);

	bool Success = Filenames.Num() > 0;

	if (Success)
	{
		for (int32 FileIdx = 0; FileIdx < Filenames.Num(); ++FileIdx)
		{
			const FString Filename = Filenames[FileIdx];

			FCloudFileHeader NewHeader;
			NewHeader.FileName = Filename;
			NewHeader.DLName = Filename + FileIdx;
			NewHeader.FileSize = 0;
			NewHeader.Hash.Empty();

			FileHeaders.Add( NewHeader );
		}
	}

	TriggerOnEnumerateFilesCompleteDelegates(Success);

	return Success;
}

void FLocalNewsFeedTitleFile::GetFileList(TArray<FCloudFileHeader>& InFileHeaders)
{
	InFileHeaders.Append( FileHeaders );
}


bool FLocalNewsFeedTitleFile::ReadFile(const FString& DLName)
{
	const TArray< uint8 >* ExistingFileContents = DLNameToFileContents.Find( DLName );

	if (ExistingFileContents != nullptr)
	{
		TriggerOnReadFileCompleteDelegates(true, DLName);

		return true;
	}

	const FString FileName = GetFileNameFromDLName( DLName );

	TArray<uint8> FileContents;

	if (!FFileHelper::LoadFileToArray(FileContents, *(RootDirectory + FileName)))
	{
		TriggerOnReadFileCompleteDelegates(false, DLName);

		return false;
	}

	DLNameToFileContents.Add(DLName, FileContents);
	TriggerOnReadFileCompleteDelegates(true, DLName);

	return true;
}


FString FLocalNewsFeedTitleFile::GetFileNameFromDLName( const FString& DLName ) const
{
	for (int Index = 0; Index < FileHeaders.Num(); Index++)
	{
		if (FileHeaders[Index].DLName == DLName)
		{
			return FileHeaders[Index].FileName;
		}
	}

	return FString();
}
