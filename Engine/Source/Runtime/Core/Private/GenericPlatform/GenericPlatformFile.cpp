// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ModuleManager.h"


int64 IFileHandle::Size()
{
	int64 Current = Tell();
	SeekFromEnd();
	int64 Result = Tell();
	Seek(Current);
	return Result;
}

const TCHAR* IPlatformFile::GetPhysicalTypeName()
{
	return TEXT("PhysicalFile");
}

bool IPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	class FRecurse : public FDirectoryVisitor
	{
	public:
		IPlatformFile&		PlatformFile;
		FDirectoryVisitor&	Visitor;
		FRecurse(IPlatformFile&	InPlatformFile, FDirectoryVisitor& InVisitor)
			: PlatformFile(InPlatformFile)
			, Visitor(InVisitor)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			bool Result = Visitor.Visit(FilenameOrDirectory, bIsDirectory);
			if (Result && bIsDirectory)
			{
				Result = PlatformFile.IterateDirectory(FilenameOrDirectory, *this);
			}
			return Result;
		}
	};
	FRecurse Recurse(*this, Visitor);
	return IterateDirectory(Directory, Recurse);
}

bool IPlatformFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
	class FRecurse : public FDirectoryVisitor
	{
	public:
		IPlatformFile&		PlatformFile;
		FRecurse(IPlatformFile&	InPlatformFile)
			: PlatformFile(InPlatformFile)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				PlatformFile.IterateDirectory(FilenameOrDirectory, *this);
				PlatformFile.DeleteDirectory(FilenameOrDirectory);
			}
			else
			{
				PlatformFile.SetReadOnly(FilenameOrDirectory, false);
				PlatformFile.DeleteFile(FilenameOrDirectory);
			}
			return true; // continue searching
		}
	};
	FRecurse Recurse(*this);
	Recurse.Visit(Directory, true);
	return !DirectoryExists(Directory);
}


bool IPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From)
{
	const int64 MaxBufferSize = 1024*1024;

	TAutoPtr<IFileHandle> FromFile(OpenRead(From));
	if (!FromFile.IsValid())
	{
		return false;
	}
	TAutoPtr<IFileHandle> ToFile(OpenWrite(To));
	if (!ToFile.IsValid())
	{
		return false;
	}
	int64 Size = FromFile->Size();
	if (Size < 1)
	{
		check(Size == 0);
		return true;
	}
	int64 AllocSize = FMath::Min<int64>(MaxBufferSize, Size);
	check(AllocSize);
	uint8* Buffer = (uint8*)FMemory::Malloc(int32(AllocSize));
	check(Buffer);
	while (Size)
	{
		int64 ThisSize = FMath::Min<int64>(AllocSize, Size);
		FromFile->Read(Buffer, ThisSize);
		ToFile->Write(Buffer, ThisSize);
		Size -= ThisSize;
		check(Size >= 0);
	}
	FMemory::Free(Buffer);
	return true;
}

bool IPlatformFile::CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting)
{
	check(DestinationDirectory);
	check(Source);

	FString DestDir(DestinationDirectory);
	FPaths::NormalizeDirectoryName(DestDir);

	FString SourceDir(Source);
	FPaths::NormalizeDirectoryName(SourceDir);

	// Does Source dir exist?
	if (!DirectoryExists(*SourceDir))
	{
		return false;
	}

	// Destination directory exists already or can be created ?
	if (!DirectoryExists(*DestDir) &&
		!CreateDirectory(*DestDir))
	{
		return false;
	}

	// Copy all files and directories
	struct FCopyFilesAndDirs : public FDirectoryVisitor
	{
		IPlatformFile & PlatformFile;
		const TCHAR* SourceRoot;
		const TCHAR* DestRoot;
		bool bOverwrite;

		FCopyFilesAndDirs(IPlatformFile& InPlatformFile, const TCHAR* InSourceRoot, const TCHAR* InDestRoot, bool bInOverwrite)
			: PlatformFile(InPlatformFile)
			, SourceRoot(InSourceRoot)
			, DestRoot(InDestRoot)
			, bOverwrite(bInOverwrite)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			FString NewName(FilenameOrDirectory);
			// change the root
			NewName = NewName.Replace(SourceRoot, DestRoot);

			if (bIsDirectory)
			{
				// create new directory structure
				if (!PlatformFile.CreateDirectoryTree(*NewName) && !PlatformFile.DirectoryExists(*NewName))
				{
					return false;
				}
			}
			else
			{
				// Delete destination file if it exists and we are overwriting
				if (PlatformFile.FileExists(*NewName) && bOverwrite)
				{
					PlatformFile.DeleteFile(*NewName);
				}

				// Copy file from source
				if (!PlatformFile.CopyFile(*NewName, FilenameOrDirectory))
				{
					// Not all files could be copied
					return false;
				}
			}
			return true; // continue searching
		}
	};

	// copy files and directories visitor
	FCopyFilesAndDirs CopyFilesAndDirs(*this, *SourceDir, *DestDir, bOverwriteAllExisting);

	// create all files subdirectories and files in subdirectories!
	return IterateDirectoryRecursively(*SourceDir, CopyFilesAndDirs);
}

FString IPlatformFile::ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename )
{
	return FPaths::ConvertRelativePathToFull(Filename);
}

FString IPlatformFile::ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename )
{
	return FPaths::ConvertRelativePathToFull(Filename);
}

bool IPlatformFile::CreateDirectoryTree(const TCHAR* Directory)
{
	FString LocalFilename(Directory);
	FPaths::NormalizeDirectoryName(LocalFilename);
	const TCHAR* LocalPath = *LocalFilename;
	int32 CreateCount = 0;
	const int32 MaxCharacters = MAX_UNREAL_FILENAME_LENGTH - 1;
	int32 Index = 0;
	for (TCHAR Full[MaxCharacters + 1] = TEXT( "" ), *Ptr = Full; Index < MaxCharacters; *Ptr++ = *LocalPath++, Index++)
	{
		if (((*LocalPath) == TEXT('/')) || (*LocalPath== 0))
		{
			*Ptr = 0;
			if ((Ptr != Full) && !FPaths::IsDrive( Full ))
			{
				if (!CreateDirectory(Full) && !DirectoryExists(Full))
				{
					break;
				}
				CreateCount++;
			}
		}
		if (*LocalPath == 0)
		{
			break;
		}
	}
	return DirectoryExists(*LocalFilename);
}

bool IPhysicalPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	// Physical platform file should never wrap anything.
	check(Inner == NULL);
	return true;
}