// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PakFilePrivatePCH.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "FileManagerGeneric.h"
#include "ModuleManager.h"
#include "IPlatformFileModule.h"
#include "IOBase.h"
#include "BigInt.h"
#include "SignedArchiveReader.h"
#include "PublicKey.inl"
#include "AES.h"
#include "GenericPlatformChunkInstall.h"

DEFINE_LOG_CATEGORY(LogPakFile);


/**
 * Class to handle correctly reading from a compressed file within a compressed package
 */
class FPakSimpleEncryption
{
public:
	enum
	{
		Alignment = FAES::AESBlockSize,
	};

	static FORCEINLINE int64 AlignReadRequest(int64 Size) 
	{
		return Align(Size, Alignment);
	}

	static FORCEINLINE void DecryptBlock(void* Data, int64 Size)
	{
#ifdef AES_KEY
		FAES::DecryptData((uint8*)Data, Size);
#endif
	}
};

/**
 * Thread local class to manage working buffers for file compression
 */
class FCompressionScratchBuffers : public TThreadSingleton<FCompressionScratchBuffers>
{
public:
	FCompressionScratchBuffers()
		: TempBufferSize(0)
		, ScratchBufferSize(0)
	{}

	int64				TempBufferSize;
	TAutoPtr<uint8>		TempBuffer;
	int64				ScratchBufferSize;
	TAutoPtr<uint8>		ScratchBuffer;

	void EnsureBufferSpace(int64 CompressionBlockSize, int64 ScrachSize)
	{
		if(TempBufferSize < CompressionBlockSize)
		{
			TempBufferSize = CompressionBlockSize;
			TempBuffer.Reset((uint8*)FMemory::Malloc(TempBufferSize));
		}
		if(ScratchBufferSize < ScrachSize)
		{
			ScratchBufferSize = ScrachSize;
			ScratchBuffer.Reset((uint8*)FMemory::Malloc(ScratchBufferSize));
		}
	}
};

/**
 * Class to handle correctly reading from a compressed file within a pak
 */
template< typename EncryptionPolicy = FPakNoEncryption >
class FPakCompressedReaderPolicy
{
public:
	class FPakUncompressTask : public FNonAbandonableTask
	{
	public:
		uint8*				UncompressedBuffer;
		int32				UncompressedSize;
		uint8*				CompressedBuffer;
		int32				CompressedSize;
		ECompressionFlags	Flags;
		void*				CopyOut;
		int64				CopyOffset;
		int64				CopyLength;

		void DoWork()
		{
			// Decrypt and Uncompress from memory to memory.
			int64 EncryptionSize = EncryptionPolicy::AlignReadRequest(CompressedSize);
			EncryptionPolicy::DecryptBlock(CompressedBuffer, EncryptionSize);
			FCompression::UncompressMemory(Flags, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, false);
			if (CopyOut)
			{
				FMemory::Memcpy(CopyOut, UncompressedBuffer+CopyOffset, CopyLength);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			// TODO: This is called too early in engine startup.
			return TStatId();
			//RETURN_QUICK_DECLARE_CYCLE_STAT(FPakUncompressTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	FPakCompressedReaderPolicy(const FPakFile& InPakFile, const FPakEntry& InPakEntry, FArchive* InPakReader)
		: PakFile(InPakFile)
		, PakEntry(InPakEntry)
		, PakReader(InPakReader)
	{
	}

	/** Pak file that own this file data */
	const FPakFile&		PakFile;
	/** Pak file entry for this file. */
	const FPakEntry&	PakEntry;
	/** Pak file archive to read the data from. */
	FArchive*			PakReader;

	FORCEINLINE int64 FileSize() const
	{
		return PakEntry.UncompressedSize;
	}

	void Serialize(int64 DesiredPosition, void* V, int64 Length)
	{
		const int32 CompressionBlockSize = PakEntry.CompressionBlockSize;
		uint32 CompressionBlockIndex = DesiredPosition / CompressionBlockSize;
		uint8* WorkingBuffers[2];
		int64 DirectCopyStart = DesiredPosition % PakEntry.CompressionBlockSize;
		FAsyncTask<FPakUncompressTask> UncompressTask;
		FCompressionScratchBuffers& ScratchSpace = FCompressionScratchBuffers::Get();
		bool bStartedUncompress = false;

		int64 WorkingBufferRequiredSize = FCompression::CompressMemoryBound((ECompressionFlags)PakEntry.CompressionMethod,CompressionBlockSize);
		WorkingBufferRequiredSize = EncryptionPolicy::AlignReadRequest(WorkingBufferRequiredSize);
		ScratchSpace.EnsureBufferSpace(CompressionBlockSize, WorkingBufferRequiredSize*2);
		WorkingBuffers[0] = ScratchSpace.ScratchBuffer;
		WorkingBuffers[1] = ScratchSpace.ScratchBuffer + WorkingBufferRequiredSize;

		while (Length > 0)
		{
			const FPakCompressedBlock& Block = PakEntry.CompressionBlocks[CompressionBlockIndex];
			int64 Pos = CompressionBlockIndex * CompressionBlockSize;
			int64 CompressedBlockSize = Block.CompressedEnd-Block.CompressedStart;
			int64 UncompressedBlockSize = FMath::Min<int64>(PakEntry.UncompressedSize-Pos, PakEntry.CompressionBlockSize);
			int64 ReadSize = EncryptionPolicy::AlignReadRequest(CompressedBlockSize);
			int64 WriteSize = FMath::Min<int64>(UncompressedBlockSize - DirectCopyStart, Length);
			PakReader->Seek(Block.CompressedStart);
			PakReader->Serialize(WorkingBuffers[CompressionBlockIndex & 1],ReadSize);
			if (bStartedUncompress)
			{
				UncompressTask.EnsureCompletion();
				bStartedUncompress = false;
			}

			FPakUncompressTask& TaskDetails = UncompressTask.GetTask();
			if (DirectCopyStart == 0 && Length >= CompressionBlockSize)
			{
				// Block can be decompressed directly into output buffer
				TaskDetails.Flags = (ECompressionFlags)PakEntry.CompressionMethod;
				TaskDetails.UncompressedBuffer = (uint8*)V;
				TaskDetails.UncompressedSize = UncompressedBlockSize;
				TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
				TaskDetails.CompressedSize = CompressedBlockSize;
				TaskDetails.CopyOut = nullptr;
			}
			else
			{
				// Block needs to be copied from a working buffer
				TaskDetails.Flags = (ECompressionFlags)PakEntry.CompressionMethod;
				TaskDetails.UncompressedBuffer = (uint8*)ScratchSpace.TempBuffer;
				TaskDetails.UncompressedSize = UncompressedBlockSize;
				TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
				TaskDetails.CompressedSize = CompressedBlockSize;
				TaskDetails.CopyOut = V;
				TaskDetails.CopyOffset = DirectCopyStart;
				TaskDetails.CopyLength = WriteSize;
			}
			
			if (Length == WriteSize)
			{
				UncompressTask.StartSynchronousTask();
			}
			else
			{
				UncompressTask.StartBackgroundTask();
			}
			bStartedUncompress = true;
			V = (void*)((uint8*)V + WriteSize);
			Length -= WriteSize;
			DirectCopyStart = 0;
			++CompressionBlockIndex;
		}

		if(bStartedUncompress)
		{
			UncompressTask.EnsureCompletion();
		}
	}
};

bool FPakEntry::VerifyPakEntriesMatch(const FPakEntry& FileEntryA, const FPakEntry& FileEntryB)
{
	bool bResult = true;
	if (FileEntryA.Size != FileEntryB.Size)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file size mismatch, got: %lld, expected: %lld"), FileEntryB.Size, FileEntryA.Size);
		bResult = false;		
	}
	if (FileEntryA.UncompressedSize != FileEntryB.UncompressedSize)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header uncompressed file size mismatch, got: %lld, expected: %lld"), FileEntryB.UncompressedSize, FileEntryA.UncompressedSize);
		bResult = false;
	}
	if (FileEntryA.CompressionMethod != FileEntryB.CompressionMethod)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file compression method mismatch, got: %d, expected: %d"), FileEntryB.CompressionMethod, FileEntryA.CompressionMethod);
		bResult = false;
	}
	if (FMemory::Memcmp(FileEntryA.Hash, FileEntryB.Hash, sizeof(FileEntryA.Hash)) != 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak file hash does not match its index entry"));
		bResult = false;
	}
	return bResult;
}

FPakFile::FPakFile(const TCHAR* Filename, bool bIsSigned)
	: PakFilename(Filename)
	, bSigned(bIsSigned)
	, bIsValid(false)
{
	FArchive* Reader = GetSharedReader(NULL);
	if (Reader)
	{
		Timestamp = IFileManager::Get().GetTimeStamp(Filename);
		Initialize(Reader);
	}
}

FPakFile::FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename, bool bIsSigned)
	: PakFilename(Filename)
	, bSigned(bIsSigned)
	, bIsValid(false)
{
	FArchive* Reader = GetSharedReader(LowerLevel);
	if (Reader)
	{
		Timestamp = LowerLevel->GetTimeStamp(Filename);
		Initialize(Reader);
	}
}

FPakFile::FPakFile(FArchive* Archive)
	: bSigned(false)
	, bIsValid(false)
{
	Initialize(Archive);
}

FPakFile::~FPakFile()
{
}

FArchive* FPakFile::CreatePakReader(const TCHAR* Filename)
{
	FArchive* ReaderArchive = IFileManager::Get().CreateFileReader(Filename);
	return SetupSignedPakReader(ReaderArchive);
}

FArchive* FPakFile::CreatePakReader(IFileHandle& InHandle, const TCHAR* Filename)
{
	FArchive* ReaderArchive = new FArchiveFileReaderGeneric(&InHandle, Filename, InHandle.Size());
	return SetupSignedPakReader(ReaderArchive);
}

FArchive* FPakFile::SetupSignedPakReader(FArchive* ReaderArchive)
{
#if !USING_SIGNED_CONTENT
	if (bSigned || FParse::Param(FCommandLine::Get(), TEXT("signedpak")) || FParse::Param(FCommandLine::Get(), TEXT("signed")))
#endif
	{	
		if (!Decryptor.IsValid())
		{
			Decryptor = new FChunkCacheWorker(ReaderArchive);
		}
		ReaderArchive = new FSignedArchiveReader(ReaderArchive, Decryptor);
	}
	return ReaderArchive;
}

void FPakFile::Initialize(FArchive* Reader)
{
	if (Reader->TotalSize() < Info.GetSerializedSize())
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Corrupted pak file (too short)."));
	}
	else
	{
		// Serialize trailer and check if everything is as expected.
		Reader->Seek(Reader->TotalSize() - Info.GetSerializedSize());
		Info.Serialize(*Reader);
		check(Info.Magic == FPakInfo::PakFile_Magic);
		check(Info.Version >= FPakInfo::PakFile_Version_Initial && Info.Version <= FPakInfo::PakFile_Version_Latest);

		LoadIndex(Reader);
		// LoadIndex should crash in case of an error, so just assume everything is ok if we got here.
		bIsValid = true;
	}	
}

void FPakFile::LoadIndex(FArchive* Reader)
{
	if (Reader->TotalSize() < (Info.IndexOffset + Info.IndexSize))
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index offset in pak file."));
	}
	else
	{
		// Load index into memory first.
		Reader->Seek(Info.IndexOffset);
		TArray<uint8> IndexData;
		IndexData.AddUninitialized(Info.IndexSize);
		Reader->Serialize(IndexData.GetData(), Info.IndexSize);
		FMemoryReader IndexReader(IndexData);

		// Check SHA1 value.
		uint8 IndexHash[20];
		FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), IndexHash);
		if (FMemory::Memcmp(IndexHash, Info.IndexHash, sizeof(IndexHash)) != 0)
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index in pak file (CRC mismatch)."));
		}

		// Read the default mount point and all entries.
		int32 NumEntries = 0;
		IndexReader << MountPoint;
		IndexReader << NumEntries;

		MakeDirectoryFromPath(MountPoint);
		// Allocate enough memory to hold all entries (and not reallocate while they're being added to it).
		Files.Empty(NumEntries);

		for (int32 EntryIndex = 0; EntryIndex < NumEntries; EntryIndex++)
		{
			// Serialize from memory.
			FPakEntry Entry;
			FString Filename;
			IndexReader << Filename;
			Entry.Serialize(IndexReader, Info.Version);

			// Add new file info.
			Files.Add(Entry);

			// Construct Index of all directories in pak file.
			FString Path = FPaths::GetPath(Filename);
			MakeDirectoryFromPath(Path);
			FPakDirectory* Directory = Index.Find(Path);
			if (Directory != NULL)
			{
				Directory->Add(Filename, &Files.Last());	
			}
			else
			{
				FPakDirectory NewDirectory;
				NewDirectory.Add(Filename, &Files.Last());
				Index.Add(Path, NewDirectory);

				// add the parent directories up to the mount point
				while (MountPoint != Path)
				{
					Path = Path.Left(Path.Len()-1);
					int32 Offset = 0;
					if (Path.FindLastChar('/', Offset))
					{
						Path = Path.Left(Offset);
						MakeDirectoryFromPath(Path);
						if (Index.Find(Path) == NULL)
						{
							FPakDirectory ParentDirectory;
							Index.Add(Path, ParentDirectory);
						}
					}
					else
					{
						Path = MountPoint;
					}
				}
			}
		}
	}
}

FArchive* FPakFile::GetSharedReader(IPlatformFile* LowerLevel)
{
	uint32 Thread = FPlatformTLS::GetCurrentThreadId();
	FArchive* PakReader = NULL;
	{
		FScopeLock ScopedLock(&CriticalSection);
		TAutoPtr<FArchive>* ExistingReader = ReaderMap.Find(Thread);
		if (ExistingReader)
		{
			PakReader = *ExistingReader;
		}
	}
	if (!PakReader)
	{
		// Create a new FArchive reader and pass it to the new handle.
		if (LowerLevel != NULL)
		{
			IFileHandle* PakHandle = LowerLevel->OpenRead(*GetFilename());
			if (PakHandle)
			{
				PakReader = CreatePakReader(*PakHandle, *GetFilename());
			}
		}
		else
		{
			PakReader = CreatePakReader(*GetFilename());
		}
		if (!PakReader)
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Unable to create pak \"%s\" handle"), *GetFilename());
		}
		{
			FScopeLock ScopedLock(&CriticalSection);
			ReaderMap.Emplace(Thread, PakReader);
		}		
	}
	return PakReader;
}

#if !UE_BUILD_SHIPPING
class FPakExec : private FSelfRegisteringExec
{
	FPakPlatformFile& PlatformFile;

public:

	FPakExec(FPakPlatformFile& InPlatformFile)
		: PlatformFile(InPlatformFile)
	{}

	/** Console commands **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if (FParse::Command(&Cmd, TEXT("Mount")))
		{
			PlatformFile.HandleMountCommand(Cmd, Ar);
			return true;
		}
		if (FParse::Command(&Cmd, TEXT("Unmount")))
		{
			PlatformFile.HandleUnmountCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PakList")))
		{
			PlatformFile.HandlePakListCommand(Cmd, Ar);
			return true;
		}
		return false;
	}
};
static TAutoPtr<FPakExec> GPakExec;

void FPakPlatformFile::HandleMountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		const FString MountPoint = FParse::Token(Cmd, false);
		Mount(*PakFilename, 0, MountPoint.IsEmpty() ? NULL : *MountPoint);
	}
}

void FPakPlatformFile::HandleUnmountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		Unmount(*PakFilename);
	}
}

void FPakPlatformFile::HandlePakListCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	for (auto Pak : Paks)
	{
		Ar.Logf(TEXT("%s"), *Pak.PakFile->GetFilename());
	}	
}
#endif // !UE_BUILD_SHIPPING

FPakPlatformFile::FPakPlatformFile()
	: LowerLevel(NULL)
	, bSigned(false)
{
}

FPakPlatformFile::~FPakPlatformFile()
{
	FCoreDelegates::OnMountPak.Unbind();
	FCoreDelegates::OnUnmountPak.Unbind();

	// We need to flush async IO... if it hasn't been shut down already.
	if (FIOSystem::HasShutdown() == false)
	{
		FIOSystem& IOSystem = FIOSystem::Get();
		IOSystem.BlockTillAllRequestsFinishedAndFlushHandles();
	}

	{
		FScopeLock ScopedLock(&PakListCritical);
		for (int32 PakFileIndex = 0; PakFileIndex < PakFiles.Num(); PakFileIndex++)
		{
			delete PakFiles[PakFileIndex].PakFile;
			PakFiles[PakFileIndex].PakFile = nullptr;
		}
	}	
}

void FPakPlatformFile::FindPakFilesInDirectory(IPlatformFile* LowLevelFile, const TCHAR* Directory, TArray<FString>& OutPakFiles)
{
	// Helper class to find all pak files.
	class FPakSearchVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FoundPakFiles;
		IPlatformChunkInstall* ChunkInstall;
	public:
		FPakSearchVisitor(TArray<FString>& InFoundPakFiles, IPlatformChunkInstall* InChunkInstall)
			: FoundPakFiles(InFoundPakFiles)
			, ChunkInstall(InChunkInstall)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString Filename(FilenameOrDirectory);
				if (FPaths::GetExtension(Filename) == TEXT("pak"))
				{
					// if a platform supports chunk style installs, make sure that the chunk a pak file resides in is actually fully installed before accepting pak files from it
					if (ChunkInstall)
					{
						FString ChunkIdentifier(TEXT("pakchunk"));
						FString BaseFilename = FPaths::GetBaseFilename(Filename);
						if (BaseFilename.StartsWith(ChunkIdentifier))
						{
							int32 DelimiterIndex = 0;
							int32 StartOfChunkIndex = ChunkIdentifier.Len();

							BaseFilename.FindChar(TEXT('-'), DelimiterIndex);
							FString ChunkNumberString = BaseFilename.Mid(StartOfChunkIndex, DelimiterIndex-StartOfChunkIndex);
							int32 ChunkNumber = 0;
							TTypeFromString<int32>::FromString(ChunkNumber, *ChunkNumberString);
							if (ChunkInstall->GetChunkLocation(ChunkNumber) == EChunkLocation::NotAvailable)
							{
								return true;
							}
						}
					}
					FoundPakFiles.Add(Filename);
				}
			}
			return true;
		}
	};
	// Find all pak files.
	FPakSearchVisitor Visitor(OutPakFiles, FPlatformMisc::GetPlatformChunkInstall());
	LowLevelFile->IterateDirectoryRecursively(Directory, Visitor);
}

void FPakPlatformFile::FindAllPakFiles(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders, TArray<FString>& OutPakFiles)
{
	// Find pak files from the specified directories.	
	for (int32 FolderIndex = 0; FolderIndex < PakFolders.Num(); ++FolderIndex)
	{
		FindPakFilesInDirectory(LowLevelFile, *PakFolders[FolderIndex], OutPakFiles);
	}
}

void FPakPlatformFile::GetPakFolders(const TCHAR* CmdLine, TArray<FString>& OutPakFolders)
{
#if !UE_BUILD_SHIPPING
	// Command line folders
	FString PakDirs;
	if (FParse::Value(CmdLine, TEXT("-pakdir="), PakDirs))
	{
		TArray<FString> CmdLineFolders;
		PakDirs.ParseIntoArray(CmdLineFolders, TEXT("*"), true);
		OutPakFolders.Append(CmdLineFolders);
	}
#endif

	// @todo plugin urgent: Needs to handle plugin Pak directories, too
	// Hardcoded locations
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::GameContentDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::GameSavedDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::EngineContentDir()));
}

bool FPakPlatformFile::CheckIfPakFilesExist(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders)
{
	TArray<FString> FoundPakFiles;
	FindAllPakFiles(LowLevelFile, PakFolders, FoundPakFiles);
	return FoundPakFiles.Num() > 0;
}

bool FPakPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
#if !USING_SIGNED_CONTENT
	bool Result = FParse::Param(CmdLine, TEXT("Pak")) || FParse::Param(CmdLine, TEXT("Signedpak")) || FParse::Param(CmdLine, TEXT("Signed"));
	if (FPlatformProperties::RequiresCookedData() && !Result && !FParse::Param(CmdLine, TEXT("NoPak")))
	{
		TArray<FString> PakFolders;
		GetPakFolders(CmdLine, PakFolders);
		Result = CheckIfPakFilesExist(Inner, PakFolders);
	}
	return Result;
#else
	return true;
#endif
}

bool FPakPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	// Inner is required.
	check(Inner != NULL);
	LowerLevel = Inner;

#if !USING_SIGNED_CONTENT
	bSigned = FParse::Param(CmdLine, TEXT("Signedpak")) || FParse::Param(CmdLine, TEXT("Signed"));
	if (!bSigned)
	{
		// Even if -signed is not provided in the command line, use signed reader if the hardcoded key is non-zero.
		FEncryptionKey DecryptionKey;
		DecryptionKey.Exponent.Parse(DECRYPTION_KEY_EXPONENT);
		DecryptionKey.Modulus.Parse(DECYRPTION_KEY_MODULUS);
		bSigned = !DecryptionKey.Exponent.IsZero() && !DecryptionKey.Modulus.IsZero();
	}
#else
	bSigned = true;
#endif
	
	TArray<FString> PaksToLoad;
#if !UE_BUILD_SHIPPING
	// Optionally get a list of pak filenames to load, only these paks will be mounted
	FString CmdLinePaksToLoad;
	if (FParse::Value(CmdLine, TEXT("-paklist="), CmdLinePaksToLoad))
	{
		CmdLinePaksToLoad.ParseIntoArray(PaksToLoad, TEXT("+"), true);
	}
#endif

	// Find and mount pak files from the specified directories.
	TArray<FString> PakFolders;
	GetPakFolders(CmdLine, PakFolders);
	TArray<FString> FoundPakFiles;
	FindAllPakFiles(LowerLevel, PakFolders, FoundPakFiles);
	// Sort in descending order.
	FoundPakFiles.Sort(TGreater<FString>());
	// Mount all found pak files
	for (int32 PakFileIndex = 0; PakFileIndex < FoundPakFiles.Num(); PakFileIndex++)
	{
		const FString& PakFilename = FoundPakFiles[PakFileIndex];
		bool bLoadPak = true;
		if (PaksToLoad.Num() && !PaksToLoad.Contains(FPaths::GetBaseFilename(PakFilename)))
		{
			bLoadPak = false;
		}
		if (bLoadPak)
		{
			// hardcode default load ordering of game main pak -> game content -> engine content -> saved dir
			// would be better to make this config but not even the config system is initialized here so we can't do that
			uint32 PakOrder = 0;
			if (PakFilename.StartsWith(FString::Printf(TEXT("%sPaks/%s-"), *FPaths::GameContentDir(), FApp::GetGameName())))
			{
				PakOrder = 4;
			}
			else if (PakFilename.StartsWith(FPaths::GameContentDir()))
			{
				PakOrder = 3;
			}
			else if (PakFilename.StartsWith(FPaths::EngineContentDir()))
			{
				PakOrder = 2;
			}
			else if (PakFilename.StartsWith(FPaths::GameSavedDir()))
			{
				PakOrder = 1;
			}

			Mount(*PakFilename, PakOrder);
		}
	}

#if !UE_BUILD_SHIPPING
	GPakExec = new FPakExec(*this);
#endif // !UE_BUILD_SHIPPING

	FCoreDelegates::OnMountPak.BindRaw(this, &FPakPlatformFile::HandleMountPakDelegate);
	FCoreDelegates::OnUnmountPak.BindRaw(this, &FPakPlatformFile::HandleUnmountPakDelegate);
	return !!LowerLevel;
}

bool FPakPlatformFile::Mount(const TCHAR* InPakFilename, uint32 PakOrder, const TCHAR* InPath /*= NULL*/)
{
	bool bSuccess = false;
	TSharedPtr<IFileHandle> PakHandle = MakeShareable(LowerLevel->OpenRead(InPakFilename));
	if (PakHandle.IsValid())
	{
		FPakFile* Pak = new FPakFile(LowerLevel, InPakFilename, bSigned);
		if (Pak->IsValid())
		{
			if (InPath != NULL)
			{
				Pak->SetMountPoint(InPath);
			}
			FString PakFilename = InPakFilename;
			if ( PakFilename.EndsWith(TEXT("_P.pak")) )
			{
				PakOrder += 100;
			}
			{
				// Add new pak file
				FScopeLock ScopedLock(&PakListCritical);
				FPakListEntry Entry;
				Entry.ReadOrder = PakOrder;
				Entry.PakFile = Pak;
				PakFiles.Add(Entry);
				PakFiles.Sort();
			}
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Failed to mount pak \"%s\", pak is invalid."), InPakFilename);
		}
	}
	else
	{
		UE_LOG(LogPakFile, Warning, TEXT("Pak \"%s\" does not exist!"), InPakFilename);
	}
	return bSuccess;
}

bool FPakPlatformFile::Unmount(const TCHAR* InPakFilename)
{
	{
		FScopeLock ScopedLock(&PakListCritical); 

		for (int32 PakIndex = 0; PakIndex < PakFiles.Num(); PakIndex++)
		{
			if (PakFiles[PakIndex].PakFile->GetFilename() == InPakFilename)
			{
				delete PakFiles[PakIndex].PakFile;
				PakFiles.RemoveAt(PakIndex);
				return true;
			}
		}
	}

	return false;
}

IFileHandle* FPakPlatformFile::CreatePakFileHandle(const TCHAR* Filename, FPakFile* PakFile, const FPakEntry* FileEntry)
{
	IFileHandle* Result = NULL;
	FArchive* PakReader = PakFile->GetSharedReader(LowerLevel);

	// Create the handle.
	if (FileEntry->CompressionMethod != COMPRESS_None && PakFile->GetInfo().Version >= FPakInfo::PakFile_Version_CompressionEncryption)
	{
		if (FileEntry->bEncrypted)
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<FPakSimpleEncryption> >(*PakFile, *FileEntry, PakReader, true);
		}
		else
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<> >(*PakFile, *FileEntry, PakReader, true);
		}
	}
	else if (FileEntry->bEncrypted)
	{
		Result = new FPakFileHandle< FPakReaderPolicy<FPakSimpleEncryption> >(*PakFile, *FileEntry, PakReader, true);
	}
	else
	{
		Result = new FPakFileHandle<>(*PakFile, *FileEntry, PakReader, true);
	}

	return Result;
}

bool FPakPlatformFile::HandleMountPakDelegate(const FString& PakFilePath, uint32 PakOrder)
{
	return Mount(*PakFilePath, PakOrder);
}

bool FPakPlatformFile::HandleUnmountPakDelegate(const FString& PakFilePath)
{
	return Unmount(*PakFilePath);
}

IFileHandle* FPakPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	IFileHandle* Result = NULL;
	FPakFile* PakFile = NULL;
	const FPakEntry* FileEntry = FindFileInPakFiles(Filename, &PakFile);	
	if (FileEntry != NULL)
	{
		Result = CreatePakFileHandle(Filename, PakFile, FileEntry);
	}
#if !USING_SIGNED_CONTENT
	else if (!bSigned)
	{
		// Default to wrapped file but only if we don't force use signed content
		Result = LowerLevel->OpenRead(Filename);
	}
#endif
	return Result;
}

bool FPakPlatformFile::BufferedCopyFile(IFileHandle& Dest, IFileHandle& Source, const int64 FileSize, uint8* Buffer, const int64 BufferSize) const
{	
	int64 RemainingSizeToCopy = FileSize;
	// Continue copying chunks using the buffer
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		if (Source.Read(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		if (Dest.Write(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		RemainingSizeToCopy -= SizeToCopy;
	}
	return true;
}

bool FPakPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From)
{
	bool Result = false;
	FPakFile* PakFile = NULL;
	const FPakEntry* FileEntry = FindFileInPakFiles(From, &PakFile);	
	if (FileEntry != NULL)
	{
		// Copy from pak to LowerLevel->
		// Create handles both files.
		TAutoPtr<IFileHandle> DestHandle(LowerLevel->OpenWrite(To));
		TAutoPtr<IFileHandle> SourceHandle(CreatePakFileHandle(From, PakFile, FileEntry));

		if (DestHandle.IsValid() && SourceHandle.IsValid())
		{
			const int64 BufferSize = 64 * 1024; // Copy in 64K chunks.
			uint8* Buffer = (uint8*)FMemory::Malloc(BufferSize);
			Result = BufferedCopyFile(*DestHandle, *SourceHandle, SourceHandle->Size(), Buffer, BufferSize);
			FMemory::Free(Buffer);
		}
	}
	else
	{
		Result = LowerLevel->CopyFile(To, From);
	}
	return Result;
}


/**
 * Module for the pak file
 */
class FPakFileModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new FPakPlatformFile());
		return AutoDestroySingleton.GetOwnedPointer();
	}
};

IMPLEMENT_MODULE(FPakFileModule, PakFile);
