// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

class FBuildPatchFileAttributesImpl : public FBuildPatchFileAttributes
{
public:
	FBuildPatchFileAttributesImpl(const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, const FString& VerifyDirectory, const FString& StagedFileDirectory, bool bUseStageDirectory, FBuildPatchProgress* BuildProgress);

	virtual ~FBuildPatchFileAttributesImpl(){}

	virtual bool ApplyAttributes(bool bForce) override;

private:
	FString SelectFullFilePath(const FString& BuildFile);
	void SetupFileAttributes(const FString& FilePath, const FFileManifestData& FileManifest);
	bool GetCurrentFileAttributes(const FString& FilePath, bool& OutFileExists, bool& OutIsReadonly, bool& OutIsCompressed, bool& OutIsUnixExecutable);
	bool SetFileReadOnlyFlag(const FString& FilePath, bool IsReadOnly);
	bool SetFileCompressionFlag(const FString& FilePath, bool IsCompressed);
	bool SetFileUnixExecutableFlag(const FString& FilePath, bool IsUnixExecutable);

private:
	FBuildPatchAppManifestRef NewManifest;
	FBuildPatchAppManifestPtr OldManifest;
	const FString VerifyDirectory;
	const FString StagedFileDirectory;
	const bool bUseStageDirectory;
	FBuildPatchProgress* BuildProgress;
};

FBuildPatchFileAttributesImpl::FBuildPatchFileAttributesImpl(const FBuildPatchAppManifestRef& InNewManifest, const FBuildPatchAppManifestPtr& InOldManifest, const FString& InVerifyDirectory, const FString& InStagedFileDirectory, bool bInUseStageDirectory, FBuildPatchProgress* InBuildProgress)
	: NewManifest(InNewManifest)
	, OldManifest(InOldManifest)
	, VerifyDirectory(InVerifyDirectory)
	, StagedFileDirectory(InStagedFileDirectory)
	, bUseStageDirectory(bInUseStageDirectory)
	, BuildProgress(InBuildProgress)
{
	BuildProgress->SetStateProgress(EBuildPatchProgress::SettingAttributes, 0.0f);
}

bool FBuildPatchFileAttributesImpl::ApplyAttributes(bool)
{
	// We need to set attributes for all files in the new build that require it
	TArray<FString> BuildFileList = NewManifest->GetBuildFileList();
	BuildProgress->SetStateProgress(EBuildPatchProgress::SettingAttributes, 0.0f);
	for (int32 BuildFileIdx = 0; BuildFileIdx < BuildFileList.Num() && !FBuildPatchInstallError::HasFatalError(); ++BuildFileIdx)
	{
		const FString& BuildFile = BuildFileList[BuildFileIdx];
		const FFileManifestData* FileManifest = NewManifest->GetFileManifest(BuildFile);
		if (FileManifest != nullptr)
		{
			SetupFileAttributes(SelectFullFilePath(BuildFile), *FileManifest);
		}
		BuildProgress->SetStateProgress(EBuildPatchProgress::SettingAttributes, BuildFileIdx / float(BuildFileList.Num()));
	}
	BuildProgress->SetStateProgress(EBuildPatchProgress::SettingAttributes, 1.0f);

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
	bool FileExists;
	bool IsReadOnly;
	bool IsCompressed;
	bool IsUnixExecutable;

	// First check file attributes as it's much faster to read and do nothing
	bool KnownAttributes = GetCurrentFileAttributes(FilePath, FileExists, IsReadOnly, IsCompressed, IsUnixExecutable);

	// If we know the file is missing, skip out
	if (KnownAttributes && !FileExists)
	{
		return;
	}

	// Set compression attribute
	if (!KnownAttributes || IsCompressed != FileManifest.bIsCompressed)
	{
		// Must make not readonly if required
		if (!KnownAttributes || IsReadOnly)
		{
			IsReadOnly = false;
			SetFileReadOnlyFlag(FilePath, false);
		}
		SetFileCompressionFlag(FilePath, FileManifest.bIsCompressed);
	}

	// Set executable attribute
	if (!KnownAttributes || IsUnixExecutable != FileManifest.bIsUnixExecutable)
	{
		// Must make not readonly if required
		if (!KnownAttributes || IsReadOnly)
		{
			IsReadOnly = false;
			SetFileReadOnlyFlag(FilePath, false);
		}
		SetFileUnixExecutableFlag(FilePath, FileManifest.bIsUnixExecutable);
	}

	// Set readonly attribute
	if (!KnownAttributes || IsReadOnly != FileManifest.bIsReadOnly)
	{
		SetFileReadOnlyFlag(FilePath, FileManifest.bIsReadOnly);
	}
}

bool FBuildPatchFileAttributesImpl::SetFileReadOnlyFlag(const FString& FilePath, bool IsReadOnly)
{
	if (!IPlatformFile::GetPlatformPhysical().SetReadOnly(*FilePath, IsReadOnly))
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not set readonly flag %s"), *FilePath);
		return false;
	}
	return true;
}

#if PLATFORM_WINDOWS
// Start of region that uses windows types
#include "AllowWindowsPlatformTypes.h"
#include <wtypes.h>
#include <ioapiset.h>
#include <winbase.h>
#include <fileapi.h>
#include <winioctl.h>

bool FBuildPatchFileAttributesImpl::GetCurrentFileAttributes(const FString& FilePath, bool& OutFileExists, bool& OutIsReadonly, bool& OutIsCompressed, bool& OutIsUnixExecutable)
{
	DWORD FileAttributes = ::GetFileAttributes(*FilePath);
	DWORD Error = ::GetLastError();
	if (FileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		OutFileExists = true;
		OutIsReadonly = (FileAttributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY;
		OutIsCompressed = (FileAttributes & FILE_ATTRIBUTE_COMPRESSED) == FILE_ATTRIBUTE_COMPRESSED;
		OutIsUnixExecutable = false;
		return true;
	}
	else if (Error == ERROR_PATH_NOT_FOUND || Error == ERROR_FILE_NOT_FOUND)
	{
		OutFileExists = false;
		OutIsReadonly = false;
		OutIsCompressed = false;
		OutIsUnixExecutable = false;
		return true;
	}
	return false;
}

bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& FilePath, bool bIsCompressed)
{
	// Get the file handle
	HANDLE FileHandle = ::CreateFile(
		*FilePath,								// Path to file
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
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not open file to set compression flag %d Error:%d File:%s"), bIsCompressed, Error, *FilePath);
		return false;
	}

	// Send the compression control code to the device
	uint16 Message = bIsCompressed ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
	uint16* pMessage = &Message;
	DWORD Dummy = 0;
	LPDWORD pDummy = &Dummy;
	bool bSuccess = ::DeviceIoControl(
		FileHandle,								// The file handle
		FSCTL_SET_COMPRESSION,					// Control code
		pMessage,								// Our message
		sizeof(uint16),							// Our message size
		NULL,									// Not used
		0,										// Not used
		pDummy,									// The value returned will be meaningless, but is required
		NULL									// No overlap structure, we a running this synchronously
		) == TRUE;
	Error = ::GetLastError();
	const bool bFileSystemUnsupported = Error == ERROR_INVALID_FUNCTION;
	if (bSuccess == false && bFileSystemUnsupported == false)
	{
		GLog->Logf(TEXT("BuildPatchServices: WARNING: Could not set compression flag %d Error:%d File:%s"), bIsCompressed, Error, *FilePath);
	}

	// Close the open file handle
	::CloseHandle(FileHandle);

	// We treat unsupported as not being a failure
	return bSuccess == TRUE || bFileSystemUnsupported;
}

bool FBuildPatchFileAttributesImpl::SetFileUnixExecutableFlag(const FString& FilePath, bool IsUnixExecutable)
{
	// Not implemented
	return true;
}
// End of region that uses windows types
#include "HideWindowsPlatformTypes.h"

#elif PLATFORM_MAC
bool FBuildPatchFileAttributesImpl::GetCurrentFileAttributes(const FString& FilePath, bool& OutFileExists, bool& OutIsReadonly, bool& OutIsCompressed, bool& OutIsUnixExecutable)
{
	struct stat FileInfo;
	int32 Result = stat(TCHAR_TO_UTF8(*FilePath), &FileInfo);
	int32 Error = errno;
	if (Result == 0)
	{
		mode_t ExeFlags = S_IXUSR | S_IXGRP | S_IXOTH;
		OutFileExists = true;
		OutIsReadonly = (FileInfo.st_mode & S_IWUSR) == 0;
		OutIsCompressed = false;
		OutIsUnixExecutable = (FileInfo.st_mode & ExeFlags) == ExeFlags;
		return true;
	}
	else if (Error == ENOTDIR || Error == ENOENT)
	{
		OutFileExists = false;
		OutIsReadonly = false;
		OutIsCompressed = false;
		OutIsUnixExecutable = false;
		return true;
	}
	return false;
}

bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& FilePath, bool bIsCompressed)
{
	// Not implemented
	return true;
}

bool FBuildPatchFileAttributesImpl::SetFileUnixExecutableFlag(const FString& FilePath, bool IsUnixExecutable)
{
	bool bSuccess = false;
	// Set executable permission bit
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*FilePath), &FileInfo) == 0)
	{
		mode_t ExeFlags = S_IXUSR | S_IXGRP | S_IXOTH;
		FileInfo.st_mode = IsUnixExecutable ? (FileInfo.st_mode | ExeFlags) : (FileInfo.st_mode & ~ExeFlags);
		bSuccess = chmod(TCHAR_TO_UTF8(*FilePath), FileInfo.st_mode) == 0;
	}
	return bSuccess;
}
#elif PLATFORM_LINUX
bool FBuildPatchFileAttributesImpl::GetCurrentFileAttributes(const FString& FilePath, bool& OutFileExists, bool& OutIsReadonly, bool& OutIsCompressed, bool& OutIsUnixExecutable)
{
	// Not implemented
	return true;
}

bool FBuildPatchFileAttributesImpl::SetFileCompressionFlag(const FString& FilePath, bool bIsCompressed)
{
	// Not implemented
	return true;
}

bool FBuildPatchFileAttributesImpl::SetFileUnixExecutableFlag(const FString& FilePath, bool IsUnixExecutable)
{
	// Not implemented
	return true;
}
#endif

TSharedRef<FBuildPatchFileAttributes> FBuildPatchFileAttributesFactory::Create(const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, const FString& VerifyDirectory, const FString& StagedFileDirectory, FBuildPatchProgress* BuildProgress)
{
	return MakeShareable(new FBuildPatchFileAttributesImpl(NewManifest, OldManifest, VerifyDirectory, StagedFileDirectory, !StagedFileDirectory.IsEmpty(), BuildProgress));
}
