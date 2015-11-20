// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTFile.cpp: WinRT implementations of File functions
=============================================================================*/

#include "CorePrivatePCH.h"

#include "AllowWinRTPlatformTypes.h"

// make an FTimeSpan object that represents the "epoch" for time_t (from a _stat struct)
const FDateTime WinRTEpoch(1970, 1, 1);

/** 
 * WinRT file handle implementation
 */
class CORE_API FFileHandleWinRT : public IFileHandle
{
	enum {READWRITE_SIZE = 1024 * 1024};
	HANDLE FileHandle;

	FORCEINLINE int64 FileSeek(int64 Distance, uint32 MoveMethod)
	{
		LARGE_INTEGER li;
		li.QuadPart = Distance;
		if (SetFilePointerEx(FileHandle, li, &li, MoveMethod) == false)
		{
			li.QuadPart = -1;
		}
		return li.QuadPart;
	}

	FORCEINLINE bool IsValid()
	{
		return ((FileHandle != NULL) && (FileHandle != INVALID_HANDLE_VALUE));
	}

public:
	FFileHandleWinRT(HANDLE InFileHandle = NULL)
		: FileHandle(InFileHandle)
	{
	}

	virtual ~FFileHandleWinRT()
	{
		CloseHandle(FileHandle);
		FileHandle = NULL;
	}

	virtual int64 Tell() override
	{
		check(IsValid());
		return FileSeek(0, FILE_CURRENT);
	}

	virtual bool Seek(int64 NewPosition) override
	{
		check(IsValid());
		check(NewPosition >= 0);
		return (FileSeek(NewPosition, FILE_BEGIN) != -1);
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(IsValid());
		check(NewPositionRelativeToEnd <= 0);
		return (FileSeek(NewPositionRelativeToEnd, FILE_END) != -1);
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		check(IsValid());
		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			DWORD Result=0;
			if (!ReadFile(FileHandle, Destination, DWORD(ThisSize), &Result, NULL) || (Result != DWORD(ThisSize)))
			{
				return false;
			}
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}
		return true;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(IsValid());
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			DWORD Result=0;
			if (!WriteFile(FileHandle, Source, DWORD(ThisSize), &Result, NULL) || (Result != DWORD(ThisSize)))
			{
				return false;
			}
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		return true;
	}

protected:
	HANDLE GetHandle()
	{
		return FileHandle;
	}

	friend class FWinRTFile;
};

/**
 * WinRT File I/O implementation
 */
class CORE_API FWinRTFile : public IPlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename)
	{
		FString Result(Filename);
		// Convert to proper '/' (versus '\')
		FPaths::MakeStandardFilename(Result);
#if 0

		if ((Result.StartsWith(TEXT("//")) == false) && (Result[1] != TEXT(':')))
		{
			if (Result.StartsWith(TEXT("../../..")) == true)
			{
				// Slap on the fake filename
				FString FullPath(FString(FPlatformProcess::BaseDir()));
				FullPath *= Result;
				FPaths::CollapseRelativeDirectories(FullPath);
				Result = FullPath;
			}
			else
			{
				// This is not a relative directory... which could mean trouble
				//const TCHAR* BadResult = *Result;
				//appOutputDebugStringf(TEXT("Non-relative directory in NormalizeFilename... %s\n"), BadResult);
			}
		}
		if (Result.StartsWith(TEXT("/")))
		{
			Result = Result.Right(Result.Len() - 1);
		}
		FString BaseDir = FPlatformProcess::BaseDir();
		if (Result.StartsWith(BaseDir))
		{
			Result = Result.Right(Result.Len() - BaseDir.Len());
		}
		return Result;
#else
		if ((Result.StartsWith(TEXT("//")) == false) && (Result[1] != TEXT(':')))
		{
			if (Result.StartsWith(TEXT("../../..")) == true)
			{
				// Slap on the fake filename
				FString FullPath = FPlatformProcess::BaseDir();
				FullPath /= Result;
				FPaths::CollapseRelativeDirectories(FullPath);
				Result = FullPath;
			}
			else
			{
				// This is not a relative directory... which could mean trouble
				//const TCHAR* BadResult = *Result;
				//appOutputDebugStringf(TEXT("Non-relative directory in NormalizeFilename... %s\n"), BadResult);
			}
		}
#endif
		return Result;
	}
	
	virtual FString NormalizeDirectory(const TCHAR* Directory)
	{
		// WinRT must account for the fact that ../../.. isn't correct
		return NormalizeFilename(Directory);
	}

public:
	/**
	 * Initializes platform file.
	 *
	 * @param Inner Platform file to wrap by this file.
	 * @param CommandLineParam Optional parameter passed in the commandline.
	 * @return true if the initialization was successful, false otherise. */
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override
	{
		// Physical platform file should never wrap anything.
		check(Inner == NULL);
		return true;
	}

	/** Gets the platform file wrapped by this file. */
	virtual IPlatformFile* GetLowerLevel() override
	{
		return NULL;
	}

	/** Gets this platform file type name. */
	virtual const TCHAR* GetName() const override
	{
		return IPlatformFile::GetPhysicalTypeName();
	}

	virtual bool FileExists(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA AttribData;
		if (GetFileAttributesEx(*NormalizeFilename(Filename), GetFileExInfoStandard, (void*)&AttribData) == TRUE)
		{
			if ((AttribData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return true;
			}
		}
		return false;
	}
	
	virtual int64 FileSize(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (GetFileAttributesExW(*NormalizeFilename(Filename), GetFileExInfoStandard, &Info) == TRUE)
		{
			if ((Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				LARGE_INTEGER li;
				li.HighPart = Info.nFileSizeHigh;
				li.LowPart = Info.nFileSizeLow;
				return li.QuadPart;
			}
		}
		return -1;
	}
	
	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		return !!DeleteFileW(*NormalizeFilename(Filename));
	}
	
	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		WIN32_FILE_ATTRIBUTE_DATA Info;
		if (!GetFileAttributesExW(*NormalizeFilename(Filename), GetFileExInfoStandard, &Info))
		{
			// Couldn't find or there was an error...
			return true;
		}
		return ((Info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0);
	}
	
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return !!MoveFileEx(*NormalizeFilename(From), *NormalizeFilename(To), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}
	
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		return !!SetFileAttributesW(*NormalizeFilename(Filename), bNewReadOnlyValue ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
	}
	
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		// get file times
		struct _stat FileInfo;
		FString NormalizedFilename = NormalizeFilename(Filename);
		if(_wstat(*NormalizedFilename, &FileInfo))
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
		return WinRTEpoch + TimeSinceEpoch;
	}

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		// get file times
		struct _stat FileInfo;
		if(_wstat(*NormalizeFilename(Filename), &FileInfo))
		{
			return;
		}

		// Open the file.
		FFileHandleWinRT* WinRTFile = (FFileHandleWinRT*)(OpenWrite(Filename, true, true));
		if (WinRTFile != NULL)
		{
			HANDLE hFile = WinRTFile->GetHandle();
			// Setup the last write time
			FILETIME LastWriteTime;
			SYSTEMTIME SysTime;
			SysTime.wYear = DateTime.GetYear();
			SysTime.wMonth = DateTime.GetMonth();
			SysTime.wDay = DateTime.GetDay();
			SysTime.wDayOfWeek = DateTime.GetDayOfWeek();
			SysTime.wHour = DateTime.GetHour();
			SysTime.wMinute = DateTime.GetMinute();
			SysTime.wSecond = DateTime.GetSecond();
			SysTime.wMilliseconds = DateTime.GetMillisecond();
			SystemTimeToFileTime(&SysTime, &LastWriteTime);

			CONST FILETIME* lpCreationTime = &LastWriteTime;
			CONST FILETIME* lpLastAccessTime = &LastWriteTime;
			CONST FILETIME* lpLastWriteTime = &LastWriteTime;

			//@todo.WinRT: Currently setting all the same... only set WriteTime?
// 			if (SetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime) == false)
// 			{
// 				UE_LOG(LogTemp, Warning, TEXT("SetTimeStamp: Failed to SetFileTime on %s"), Filename);
// 			}

			delete WinRTFile;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetTimeStamp: Failed to open file %s"), Filename);
		}
	}

	/** Return the last access time of a file. Returns FDateTime::MinValue on failure **/
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename)
	{
		struct _stat FileInfo;
		FString NormalizedFilename = NormalizeFilename(Filename);
		if(_wstat(*NormalizedFilename, &FileInfo))
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
		return WinRTEpoch + TimeSinceEpoch;
	}

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		DWORD  Access    = GENERIC_READ;
		DWORD  WinFlags  = FILE_SHARE_READ | (bAllowWrite ? FILE_SHARE_WRITE : 0);
		DWORD  Create    = OPEN_EXISTING;
		FString NormalizedFilename = NormalizeFilename(Filename);
		HANDLE Handle	 = CreateFile2(*NormalizedFilename, Access, WinFlags, Create, NULL);
		if(Handle != INVALID_HANDLE_VALUE)
		{
			return new FFileHandleWinRT(Handle);
		}
		return NULL;
	}

	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = 0, bool bAllowRead = 0) override
	{
		DWORD  Access    = GENERIC_WRITE;
		DWORD  WinFlags  = bAllowRead ? FILE_SHARE_READ : 0;
		DWORD  Create    = bAppend ? OPEN_ALWAYS : CREATE_ALWAYS;
		HANDLE Handle	 = CreateFile2(*NormalizeFilename(Filename), Access, WinFlags, Create, NULL);
		if(Handle != INVALID_HANDLE_VALUE)
		{
			FFileHandleWinRT *PlatformFileHandle = new FFileHandleWinRT(Handle);
			if (bAppend)
			{
				PlatformFileHandle->SeekFromEnd(0);
			}
			return PlatformFileHandle;
		}
		return NULL;
	}
	
	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		WIN32_FILE_ATTRIBUTE_DATA AttribData;
		if (GetFileAttributesEx(*NormalizeDirectory(Directory), GetFileExInfoStandard, (void*)&AttribData) == TRUE)
		{
			if ((AttribData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				return true;
			}
		}
		return false;
	}
	
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
		return CreateDirectoryW(*NormalizeDirectory(Directory), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
	}
	
	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
		RemoveDirectoryW(*NormalizeDirectory(Directory));
		return !DirectoryExists(Directory);
	}
	
	bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("WINRT: IterateDirectory on %s\n"), Directory);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\t%s\n"), *NormalizeDirectory(Directory));

		bool Result = false;
		WIN32_FIND_DATAW Data;
		FString NormalizedDir = NormalizeDirectory(Directory) / TEXT("*.*");
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\t%s\n"), *NormalizedDir);
		HANDLE Handle = FindFirstFileExW(
			*NormalizedDir,
			FindExInfoStandard,
			&Data,
			FindExSearchNameMatch,
			NULL,
			FIND_FIRST_EX_LARGE_FETCH
			);
		if (Handle != INVALID_HANDLE_VALUE)
		{
			Result = true;
			do
			{
				if (FCString::Strcmp(Data.cFileName, TEXT(".")) && FCString::Strcmp(Data.cFileName, TEXT("..")))
				{
					Result = Visitor.Visit(*(FString(Directory) / Data.cFileName), !!(Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY));
				}
			} while (Result && FindNextFileW(Handle, &Data));
			FindClose(Handle);
		}
		return Result;
	}

	/** Create a directory, including any parent directories and return true if the directory was created or already existed. **/
	virtual bool CreateDirectoryTree(const TCHAR* Directory)
	{
		FString LocalFilename(NormalizeDirectory(Directory));
		FPaths::NormalizeDirectoryName(LocalFilename);
		const TCHAR* LocalPath = *LocalFilename;
		int32 CreateCount = 0;
		for (TCHAR Full[MAX_UNREAL_FILENAME_LENGTH]=TEXT( "" ), *Ptr=Full; ; *Ptr++=*LocalPath++ )
		{
			if (((*LocalPath) == TEXT('/')) || (*LocalPath== 0))
			{
				*Ptr = 0;
				if ((Ptr != Full) && !FPaths::IsDrive(Full))
				{
					if (!CreateDirectory(Full))
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
};

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FWinRTFile Singleton;
	return Singleton;
}

#include "HideWinRTPlatformTypes.h"
