// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IFileHandle;
class FApplePlatformFile;


/**
 * iOS File I/O implementation
**/
class CORE_API FIOSPlatformFile : public FApplePlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename);
	virtual FString NormalizeDirectory(const TCHAR* Directory);

public:
	//virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override;

	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual void SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime) override;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;

	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor);

private:
	FString ConvertToIOSPath(const FString& Filename, bool bForWrite);

	FString ReadDirectory;
	FString WriteDirectory;
};