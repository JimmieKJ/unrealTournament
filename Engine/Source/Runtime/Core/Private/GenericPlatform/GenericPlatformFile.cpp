// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Templates/ScopedPointer.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "UniquePtr.h"

#if USE_NEW_ASYNC_IO

#include "AsyncFileHandle.h"

class FGenericBaseRequest;
class FGenericAsyncReadFileHandle;

class FGenericReadRequestWorker : public FNonAbandonableTask
{
	FGenericBaseRequest& ReadRequest;
public:
	FGenericReadRequestWorker(FGenericBaseRequest* InReadRequest)
		: ReadRequest(*InReadRequest)
	{
	}
	void DoWork();
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericReadRequestWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

class FGenericBaseRequest : public IAsyncReadRequest
{
protected:
	FAsyncTask<FGenericReadRequestWorker>* Task;
	IPlatformFile* LowerLevel;
	const TCHAR* Filename;
public:
	FGenericBaseRequest(IPlatformFile* InLowerLevel, const TCHAR* InFilename, FAsyncFileCallBack* CompleteCallback, bool bInSizeRequest, uint8* UserSuppliedMemory = nullptr)
		: IAsyncReadRequest(CompleteCallback, bInSizeRequest, UserSuppliedMemory)
		, Task(nullptr)
		, LowerLevel(InLowerLevel)
		, Filename(InFilename)
	{
	}

	void Start()
	{
		if (FPlatformProcess::SupportsMultithreading())
		{
			Task->StartBackgroundTask(GIOThreadPool);
		}
		else
		{
			Task->StartSynchronousTask();
			WaitCompletionImpl(0.0f); // might as well finish it now
		}
	}

	virtual ~FGenericBaseRequest()
	{
		if (Task)
		{
			Task->EnsureCompletion(); // if the user polls, then we might never actual sync completion of the task until now, this will almost always be done, however we need to be sure the task is clear
			delete Task; 
		}
	}

	virtual void PerformRequest() = 0;

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		if (Task)
		{
			bool bResult;
			if (TimeLimitSeconds <= 0.0f)
			{
				Task->EnsureCompletion();
				bResult = true;
			}
			else
			{
				bResult = Task->WaitCompletionWithTimeout(TimeLimitSeconds);
			}
			if (bResult)
			{
				check(bCompleteAndCallbackCalled);
				delete Task;
				Task = nullptr;
			}
		}
	}
	virtual void CancelImpl() override
	{
		if (Task)
		{
			if (Task->Cancel())
			{
				delete Task;
				Task = nullptr;
				SetComplete();
			}
		}
	}
};

class FGenericSizeRequest : public FGenericBaseRequest
{
public:
	FGenericSizeRequest(IPlatformFile* InLowerLevel, const TCHAR* InFilename, FAsyncFileCallBack* CompleteCallback)
		: FGenericBaseRequest(InLowerLevel, InFilename, CompleteCallback, true)
	{
		Task = new FAsyncTask<FGenericReadRequestWorker>(this);
		Start();
	}
	virtual void PerformRequest() override
	{
		if (!bCanceled)
		{
			check(LowerLevel && Filename);
			Size = LowerLevel->FileSize(Filename);
		}
		SetComplete();
	}
};

class FGenericReadRequest : public FGenericBaseRequest
{
	FGenericAsyncReadFileHandle* Owner;
	int64 Offset;
	int64 BytesToRead;
	EAsyncIOPriority Priority;
public:
	FGenericReadRequest(FGenericAsyncReadFileHandle* InOwner, IPlatformFile* InLowerLevel, const TCHAR* InFilename, FAsyncFileCallBack* CompleteCallback, uint8* UserSuppliedMemory, int64 InOffset, int64 InBytesToRead, EAsyncIOPriority InPriority)
		: FGenericBaseRequest(InLowerLevel, InFilename, CompleteCallback, false, UserSuppliedMemory)
		, Owner(InOwner)
		, Offset(InOffset)
		, BytesToRead(InBytesToRead)
		, Priority(InPriority)
	{
		check(Offset >= 0 && BytesToRead > 0);
		if (CheckForPrecache()) 
		{
			SetComplete();
		}
		else
		{
			Task = new FAsyncTask<FGenericReadRequestWorker>(this);
			Start();
		}
	}
	~FGenericReadRequest();
	bool CheckForPrecache();
	virtual void PerformRequest() override
	{
		if (!bCanceled)
		{
			IFileHandle* Handle = LowerLevel->OpenRead(Filename);
			if (Handle)
			{
				if (BytesToRead == MAX_uint64)
				{
					BytesToRead = Handle->Size() - Offset;
					check(BytesToRead > 0);
				}
				if (!bUserSuppliedMemory)
				{
					check(!Memory);
					Memory = (uint8*)FMemory::Malloc(BytesToRead);
					INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
				}
				check(Memory);
				Handle->Seek(Offset);
				Handle->Read(Memory, BytesToRead);
				delete Handle;
			}
		}
		SetComplete();
	}
	uint8* GetContainedSubblock(uint8* UserSuppliedMemory, int64 InOffset, int64 InBytesToRead)
	{
		if (InOffset >= Offset && InOffset + InBytesToRead <= Offset + BytesToRead &&
			this->PollCompletion())
		{
			check(Memory);
			if (!UserSuppliedMemory)
			{
				UserSuppliedMemory = (uint8*)FMemory::Malloc(InBytesToRead);
				INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, InBytesToRead);
			}
			FMemory::Memcpy(UserSuppliedMemory, Memory + InOffset - Offset, InBytesToRead);
			return UserSuppliedMemory;
		}
		return nullptr;
	}
};

void FGenericReadRequestWorker::DoWork()
{
	ReadRequest.PerformRequest();
}

class FGenericAsyncReadFileHandle final : public IAsyncReadFileHandle
{
	IPlatformFile* LowerLevel;
	FString Filename;
	TArray<FGenericReadRequest*> LiveRequests; // linear searches could be improved
	FThreadSafeCounter LiveRequestsNonConcurrent; // This file handle should not be used concurrently; we test that with this.
public:
	FGenericAsyncReadFileHandle(IPlatformFile* InLowerLevel, const TCHAR* InFilename)
		: LowerLevel(InLowerLevel)
		, Filename(InFilename)
	{
	}
	~FGenericAsyncReadFileHandle()
	{
		check(!LiveRequests.Num()); // must delete all requests before you delete the handle
	}
	void RemoveRequest(FGenericReadRequest* Req)
	{
		check(LiveRequestsNonConcurrent.Increment() == 1);
		verify(LiveRequests.Remove(Req) == 1);
		check(LiveRequestsNonConcurrent.Decrement() == 0);
	}
	uint8* GetPrecachedBlock(uint8* UserSuppliedMemory, int64 InOffset, int64 InBytesToRead)
	{
		check(LiveRequestsNonConcurrent.Increment() == 1);
		uint8* Result = nullptr;
		for (FGenericReadRequest* Req : LiveRequests)
		{
			Result = Req->GetContainedSubblock(UserSuppliedMemory, InOffset, InBytesToRead);
			if (Result)
			{
				break;
			}
		}
		check(LiveRequestsNonConcurrent.Decrement() == 0);
		return Result;
	}
	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override
	{
		return new FGenericSizeRequest(LowerLevel, *Filename, CompleteCallback);
	}
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) override
	{
		FGenericReadRequest* Result = new FGenericReadRequest(this, LowerLevel, *Filename, CompleteCallback, UserSuppliedMemory, Offset, BytesToRead, Priority);
		if (Priority == AIOP_Precache) // only precache requests are tracked for possible reuse
		{
			check(LiveRequestsNonConcurrent.Increment() == 1);
			LiveRequests.Add(Result);
			check(LiveRequestsNonConcurrent.Decrement() == 0);
		}
		return Result;
	}

};

FGenericReadRequest::~FGenericReadRequest()
{
	if (Memory)
	{
		// this can happen with a race on cancel, it is ok, they didn't take the memory, free it now
		if (!bUserSuppliedMemory)
		{
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			FMemory::Free(Memory);
		}
		Memory = nullptr;
	}
	if (Priority == AIOP_Precache) // only precache requests are tracked for possible reuse
	{
		Owner->RemoveRequest(this);
	}
	Owner = nullptr;
}

bool FGenericReadRequest::CheckForPrecache()
{
	if (Priority > AIOP_Precache)  // only requests at higher than precache priority check for existing blocks to copy from 
	{
		check(!Memory || bUserSuppliedMemory);
		uint8* Result = Owner->GetPrecachedBlock(Memory, Offset, BytesToRead);
		if (Result)
		{
			check(!bUserSuppliedMemory || Memory == Result);
			Memory = Result;
			return true;
		}
	}
	return false;
}

IAsyncReadFileHandle* IPlatformFile::OpenAsyncRead(const TCHAR* Filename)
{
	return new FGenericAsyncReadFileHandle(this, Filename);
}

DEFINE_STAT(STAT_AsyncFileMemory);
DEFINE_STAT(STAT_AsyncFileHandles);
DEFINE_STAT(STAT_AsyncFileRequests);

#endif //USE_NEW_ASYNC_IO

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

void IPlatformFile::GetTimeStampPair(const TCHAR* PathA, const TCHAR* PathB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB)
{
	if (GetLowerLevel())
	{
		GetLowerLevel()->GetTimeStampPair(PathA, PathB, OutTimeStampA, OutTimeStampB);
	}
	else
	{
		OutTimeStampA = GetTimeStamp(PathA);
		OutTimeStampB = GetTimeStamp(PathB);
	}
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
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
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

bool IPlatformFile::IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	class FStatRecurse : public FDirectoryStatVisitor
	{
	public:
		IPlatformFile&			PlatformFile;
		FDirectoryStatVisitor&	Visitor;
		FStatRecurse(IPlatformFile&	InPlatformFile, FDirectoryStatVisitor& InVisitor)
			: PlatformFile(InPlatformFile)
			, Visitor(InVisitor)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData) override
		{
			bool Result = Visitor.Visit(FilenameOrDirectory, StatData);
			if (Result && StatData.bIsDirectory)
			{
				Result = PlatformFile.IterateDirectoryStat(FilenameOrDirectory, *this);
			}
			return Result;
		}
	};
	FStatRecurse Recurse(*this, Visitor);
	return IterateDirectoryStat(Directory, Recurse);
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


bool IPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
	const int64 MaxBufferSize = 1024*1024;

	TUniquePtr<IFileHandle> FromFile(OpenRead(From, (ReadFlags & EPlatformFileRead::AllowWrite) != EPlatformFileRead::None));
	if (!FromFile)
	{
		return false;
	}
	TUniquePtr<IFileHandle> ToFile(OpenWrite(To, false, (WriteFlags & EPlatformFileWrite::AllowRead) != EPlatformFileWrite::None));
	if (!ToFile)
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