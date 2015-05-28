// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

class FBuildPatchFileAttributesImpl : public FBuildPatchFileAttributes
{
public:
	FBuildPatchFileAttributesImpl(const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, const FString& VerifyDirectory, const FString& StagedFileDirectory, bool bUseStageDirectory);

	virtual ~FBuildPatchFileAttributesImpl(){}

	virtual bool ApplyAttributes(bool bForce) override;

private:
	FBuildPatchAppManifestRef NewManifest;
	FBuildPatchAppManifestPtr OldManifest;
	const FString VerifyDirectory;
	const FString StagedFileDirectory;
	const bool bUseStageDirectory;

	FString SelectFullFilePath(const FString& BuildFile);
	void SetupFileAttributes(const FString& FilePath, const FFileManifestData& FileManifest);
	bool SetFileCompressionFlag(const FString& Filepath, bool bIsCompressed);
	bool SetExecutableFlag(const FString& Filepath);
};

FBuildPatchFileAttributesImpl::FBuildPatchFileAttributesImpl(const FBuildPatchAppManifestRef& InNewManifest, const FBuildPatchAppManifestPtr& InOldManifest, const FString& InVerifyDirectory, const FString& InStagedFileDirectory, bool bInUseStageDirectory)
	: NewManifest(InNewManifest)
	, OldManifest(InOldManifest)
	, VerifyDirectory(InVerifyDirectory)
	, StagedFileDirectory(InStagedFileDirectory)
	, bUseStageDirectory(bInUseStageDirectory)
{}

bool FBuildPatchFileAttributesImpl::ApplyAttributes(bool bForce)
{
	// We need to set attributes for all files in the new build that require it
    {
        TArray<FString> BuildFiles;
        NewManifest->GetFileList(BuildFiles);
        for (const FString& BuildFile : BuildFiles)
        {
            // Break if quitting
            if (FBuildPatchInstallError::HasFatalError())
            {
                break;
            }

            const FFileManifestData* FileManifest = NewManifest->GetFileManifest(BuildFile);
            if (FileManifest != nullptr)
            {
                // Apply
                const bool bHasAttrib = FileManifest->bIsReadOnly || FileManifest->bIsCompressed || FileManifest->bIsUnixExecutable;
                if (bHasAttrib || bForce)
                {
                    SetupFileAttributes(SelectFullFilePath(BuildFile), *FileManifest);
                }
            }
        }
    }

	// We also need to check if any attributes have been removed, unless we forced anyway
	if (OldManifest.IsValid() && !bForce)
	{
		TArray<FString> BuildFiles;
		OldManifest->GetFileList(BuildFiles);
		for (const FString& BuildFile : BuildFiles)
		{
			// Break if quitting
			if (FBuildPatchInstallError::HasFatalError())
			{
				break;
			}

			const FFileManifestData* NewFileManifest = NewManifest->GetFileManifest(BuildFile);
			const FFileManifestData* OldFileManifest = OldManifest->GetFileManifest(BuildFile);
			if (NewFileManifest != nullptr)
			{
				const bool bAttribChanged = (OldFileManifest->bIsReadOnly && !NewFileManifest->bIsReadOnly) || (OldFileManifest->bIsCompressed && !NewFileManifest->bIsCompressed);
				if (bAttribChanged)
				{
					SetupFileAttributes(SelectFullFilePath(BuildFile), *NewFileManifest);
				}
			}
		}
	}

	// We don't fail on this step currently
	return true;
}

FString FBuildPatchFileAttributesImpl::SelectFullFilePath(const FString& BuildFile)
{
	if (bUseStageDirectory)
	{
		FString StagedFilename = StagedFileDirectory / BuildFile;
		if (IFileManager::Get().FileSize(*StagedFilename) != -1)
		{
			return MoveTemp(StagedFilename);
		}
	}
	FString InstallFilename = VerifyDirectory / BuildFile;
	return MoveTemp(InstallFilename);
}

void FBuildPatchFileAttributesImpl::SetupFileAttributes(const FString& FilePath, const FFileManifestData& FileManifest)
{
	// File must not be readonly to be able to set attributes
	IPlatformFile::GetPlatformPhysical().SetReadOnly(*FilePath, false);
	// Set correct attributes
	SetFileCompressionFlag(FilePath, FileManifest.bIsCompressed);
	if (!IPlatformFile::GetPlatformPhysical().SetReadOnly(*FilePath, FileManifest.bIsReadOnly))
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not set readonly flag %s"), *FilePath);
	}
	if (FileManifest.bIsUnixExecutable && !SetExecutableFlag(FilePath))
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not set executable flag %s"), *FilePath);
	}
}

#if PLATFORM_WINDOWS
// Start of region that uses windows types
#include "AllowWindowsPlatformTypes.h"
#include <wtypes.h>
#include <ioapiset.h>
#include <winbase.h>
#include <fileapi.h>
#include <winioctl.h>

bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& Filepath, bool bIsCompressed)
{
	// Get the file handle
	HANDLE FileHandle = ::CreateFile(
		*Filepath,								// Path to file
		GENERIC_READ | GENERIC_WRITE,			// Read and write access
		FILE_SHARE_READ | FILE_SHARE_WRITE,		// Share access for DeviceIoControl
		NULL,									// Default security
		OPEN_EXISTING,							// Open existing file
		FILE_ATTRIBUTE_NORMAL,					// No specific attributes
		NULL									// No template file handle
		);
	uint32 Error = ::GetLastError();
	if (FileHandle == NULL || FileHandle == INVALID_HANDLE_VALUE)
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not open file to set compression flag %d Error:%d File:%s"), bIsCompressed, Error, *Filepath);
		return false;
	}

	// Send the compression control code to the device
	uint16 Message = bIsCompressed ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
	uint16* pMessage = &Message;
	DWORD Dummy = 0;
	LPDWORD pDummy = &Dummy;
	BOOL bSuccess = ::DeviceIoControl(
		FileHandle,								// The file handle
		FSCTL_SET_COMPRESSION,					// Control code
		pMessage,								// Our message
		sizeof(uint16),							//
		NULL,									// Not used
		0,										// Not used
		pDummy,									// The value returned will be meaningless, but is required
		NULL									// No overlap structure, we a running this synchronously
		);
	Error = ::GetLastError();
	const bool bFileSystemUnsupported = Error == ERROR_INVALID_FUNCTION;
	if (bSuccess == FALSE && bFileSystemUnsupported == false)
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not set compression flag %d Error:%d File:%s"), bIsCompressed, Error, *Filepath);
	}

	// Close the open file handle
	::CloseHandle(FileHandle);

	// We treat unsupported as not being a failure
	return bSuccess == TRUE || bFileSystemUnsupported;
}

bool FBuildPatchFileAttributesImpl::SetExecutableFlag(const FString& Filepath)
{
	// Not implemented
	return true;
}
// End of region that uses windows types
#include "HideWindowsPlatformTypes.h"

#elif PLATFORM_MAC
bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& Filepath, bool bIsCompressed)
{
	// Not implemented
	return true;
}

bool FBuildPatchFileAttributesImpl::SetExecutableFlag(const FString& Filepath)
{
	bool bSuccess = false;
	// Enable executable permission bit
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*Filepath), &FileInfo) == 0)
	{
		bSuccess = chmod(TCHAR_TO_UTF8(*Filepath), FileInfo.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
	}
	return bSuccess;
}
#elif PLATFORM_LINUX
bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& Filepath, bool bIsCompressed)
{
	// Not implemented
	return true;
}

bool FBuildPatchFileAttributesImpl::SetExecutableFlag(const FString& Filepath)
{
	// Not implemented
	return true;
}
#endif

TSharedRef<FBuildPatchFileAttributes> FBuildPatchFileAttributesFactory::Create(const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, const FString& VerifyDirectory, const FString& StagedFileDirectory)
{
	return MakeShareable(new FBuildPatchFileAttributesImpl(NewManifest, OldManifest, VerifyDirectory, StagedFileDirectory, !StagedFileDirectory.IsEmpty()));
}
