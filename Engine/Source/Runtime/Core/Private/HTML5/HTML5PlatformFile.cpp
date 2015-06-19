// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5File.cpp: HTML5 platform implementations of File functions
=============================================================================*/

#include "CorePrivatePCH.h"

#include <sys/types.h>
#include <sys/stat.h>
#if PLATFORM_HTML5_WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if PLATFORM_HTML5_WIN32
# ifndef S_ISDIR /* NEXT */
# define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
# define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
# endif

# ifndef S_IRUSR
# define S_IRUSR S_IREAD
# define S_IWUSR S_IWRITE
# define S_IXUSR S_IEXEC
# endif	/* S_IRUSR */

/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */
#include <sys/utime.h>
#else
#include <dirent.h>
#include <utime.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime HTML5Epoch(1970, 1, 1);

/** 
 * HTML5 file handle implementation
**/
class CORE_API FFileHandleHTML5 : public IFileHandle
{
	enum {READWRITE_SIZE = 1024 * 1024};
	int32 FileHandle;

	FORCEINLINE bool IsValid()
	{
		return FileHandle != -1;
	}

public:
	FFileHandleHTML5(int32 InFileHandle = -1)
		: FileHandle(InFileHandle)
	{
	}

	virtual ~FFileHandleHTML5()
	{
		close(FileHandle);
		FileHandle = -1;
	}

	virtual int64 Tell() override
	{
		check(IsValid());
		return lseek(FileHandle, 0, SEEK_CUR);
	}

	virtual bool Seek(int64 NewPosition) override
	{
		check(IsValid());
		check(NewPosition >= 0);
		return lseek(FileHandle, NewPosition, SEEK_SET) != -1;
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		check(IsValid());
		check(NewPositionRelativeToEnd <= 0);
		return lseek(FileHandle, NewPositionRelativeToEnd, SEEK_END) != -1;
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		check(IsValid());
		while (BytesToRead)
		{
			check(BytesToRead >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			check(Destination);
			if (read(FileHandle, Destination, ThisSize) != ThisSize)
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
			if (write(FileHandle, Source, ThisSize) != ThisSize)
			{
				return false;
			}
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		return true;
	}
};

/**
 * HTML5 File I/O implementation
**/
class CORE_API FHTML5PlatformFile : public IPhysicalPlatformFile
{
protected:
	virtual FString NormalizeFilename(const TCHAR* Filename)
	{
#if PLATFORM_HTML5_WIN32
		FString Result(Filename);
		FPaths::NormalizeFilename(Result);
		if (Result.StartsWith(TEXT("//")))
		{
			Result = FString(TEXT("\\\\")) + Result.RightChop(2);
		}
		return FPaths::ConvertRelativePathToFull(Result);
#else
		FString Result(Filename);
		Result.ReplaceInline(TEXT("\\"), TEXT("/"));
		Result.ReplaceInline(TEXT("../"), TEXT(""));
		Result.ReplaceInline(TEXT(".."), TEXT(""));
		Result.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
		Result = FString(TEXT("/")) + Result; 
		return Result; 
#endif
	}

	virtual FString NormalizeDirectory(const TCHAR* Directory)
	{
#if PLATFORM_HTML5_WIN32
		FString Result(Directory);
		FPaths::NormalizeDirectoryName(Result);
		if (Result.StartsWith(TEXT("//")))
		{
			Result = FString(TEXT("\\\\")) + Result.RightChop(2);
		}
		return FPaths::ConvertRelativePathToFull(Result);
#else
		FString Result(Directory);
		Result.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Result.EndsWith(TEXT("/")))
		{
			Result.LeftChop(1);
		}
		return Result;
#endif
	}

public:
	virtual bool FileExists(const TCHAR* Filename) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) != -1)
		{
			return S_ISREG(FileInfo.st_mode);
		}
		return false;
	}

	virtual int64 FileSize(const TCHAR* Filename) override
	{
		struct stat FileInfo;
		FileInfo.st_size = -1;
		stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo);
		// make sure to return -1 for directories
		if (S_ISDIR(FileInfo.st_mode))
		{
			FileInfo.st_size = -1;
		}
		return FileInfo.st_size;
	}

	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		return unlink(TCHAR_TO_UTF8(*NormalizeFilename(Filename))) == 0;
	}

	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), F_OK) == -1)
		{
			return false; // file doesn't exist
		}
		if (access(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), W_OK) == -1)
		{
			return errno == EACCES;
		}
		return false;
	}

	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return rename(TCHAR_TO_UTF8(*NormalizeFilename(From)), TCHAR_TO_UTF8(*NormalizeFilename(To))) != -1;
	}

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) != -1)
		{
			if (bNewReadOnlyValue)
			{
				FileInfo.st_mode &= ~S_IWUSR;
			}
			else
			{
				FileInfo.st_mode |= S_IWUSR;
			}
			return chmod(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), FileInfo.st_mode) > 0;
		}
		return false;
	}

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
		return HTML5Epoch + TimeSinceEpoch;
	}

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return;
		}

		// change the modification time only
		struct utimbuf Times;
		Times.actime = FileInfo.st_atime;
		Times.modtime = (DateTime - HTML5Epoch).GetTotalSeconds();
		utime(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &Times);
	}

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override
	{
		// get file times
		struct stat FileInfo;
		if(stat(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), &FileInfo) == -1)
		{
			return FDateTime::MinValue();
		}

		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
		return HTML5Epoch + TimeSinceEpoch;
	}

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override
	{
		return Filename;
	}

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite /*= false*/) override
	{
		FString fn = NormalizeFilename(Filename);
		int32 Handle = open(TCHAR_TO_UTF8(*fn), O_RDONLY | O_BINARY);
		if (Handle != -1)
		{
			return new FFileHandleHTML5(Handle);
		}

#if PLATFORM_HTML5_WIN32
		int err = errno;
		UE_LOG(LogTemp, Display, TEXT("OpenRead: %s failed, errno %d"), (*fn), err);
#endif
		return NULL;
	}

	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		int Flags = O_CREAT;
		if (bAppend)
		{
			Flags |= O_APPEND;
		}
		else
		{
			Flags |= O_TRUNC;
		}
		if (bAllowRead)
		{
			Flags |= O_RDWR;
		}
		else
		{
			Flags |= O_WRONLY;
		}
		int32 Handle = open(TCHAR_TO_UTF8(*NormalizeFilename(Filename)), Flags, S_IRUSR | S_IWUSR);
		if (Handle != -1)
		{
			FFileHandleHTML5* FileHandleHTML5 = new FFileHandleHTML5(Handle);
			if (bAppend)
			{
				FileHandleHTML5->SeekFromEnd(0);
			}
			return FileHandleHTML5;
		}
		return NULL;
	}

	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NormalizeFilename(Directory)), &FileInfo) != -1)
		{
			return S_ISDIR(FileInfo.st_mode);
		}
		return false;
	}
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
#if PLATFORM_HTML5_WIN32
		return _mkdir(TCHAR_TO_ANSI(*NormalizeFilename(Directory))) || (errno == EEXIST);
#else 
		return mkdir(TCHAR_TO_ANSI(*NormalizeFilename(Directory)), S_IRWXG) || (errno == EEXIST);
#endif 
	}

	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
#if PLATFORM_HTML5_WIN32
		return false;
#else
		return rmdir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)));
#endif
	}

	bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
	{
#if !PLATFORM_HTML5_WIN32 
		bool Result = false;
		// If Directory is an empty string, assume that we want to iterate Binaries/Mac (current dir), but because we're an app bundle, iterate bundle's Contents/Frameworks instead
		DIR* Handle = opendir(TCHAR_TO_UTF8(*NormalizeFilename(Directory)));
		if (Handle)
		{
			Result = true;
			struct dirent *Entry;
			while ((Entry = readdir(Handle)) != NULL)
			{
				if (FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT("..")))
				{
					Result = Visitor.Visit(*(FString(Directory) / UTF8_TO_TCHAR(Entry->d_name)), Entry->d_type == DT_DIR);
				}
			}
			closedir(Handle);
		}
		return Result;
#else 
		bool Result = false;
		// If Directory is an empty string, assume that we want to iterate Binaries/Mac (current dir), but because we're an app bundle, iterate bundle's Contents/Frameworks instead
		FString FilePath = FPaths::Combine(*NormalizeFilename(Directory), TEXT("*"));
		struct _finddata_t c_file;
		long hFile = _findfirst(TCHAR_TO_UTF8(*FilePath), &c_file);

		if (hFile  != -1L )
		{
			Result = true;
			while ( _findnext(hFile,&c_file) == 0 )
			{
				if (FCString::Strcmp(UTF8_TO_TCHAR(c_file.name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(c_file.name), TEXT("..")))
				{
					Result = Visitor.Visit(*(FString(Directory) / UTF8_TO_TCHAR(c_file.name)), ((c_file.attrib & _A_SUBDIR) != 0));
				}
			}
			_findclose(hFile);
		}
		return Result;
#endif 
	}
}; 

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FHTML5PlatformFile Singleton;
	return Singleton;
}
