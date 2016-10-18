// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "RequiredProgramMainCPPInclude.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "BigInt.h"
#include "SignedArchiveWriter.h"
#include "KeyGenerator.h"
#include "AES.h"
#include "UniquePtr.h"

IMPLEMENT_APPLICATION(UnrealPak, "UnrealPak");

struct FPakCommandLineParameters
{
	FPakCommandLineParameters()
		: CompressionBlockSize(64*1024)
		, CompressionBitWindow(DEFAULT_ZLIB_BIT_WINDOW)
		, FileSystemBlockSize(0)
		, PatchFilePadAlign(0)
		, GeneratePatch(false)
	{}

	int32  CompressionBlockSize;
	int32  CompressionBitWindow;
	int64  FileSystemBlockSize;
	int64  PatchFilePadAlign;
	bool   GeneratePatch;
	FString SourcePatchPakFilename;
	FString SourcePatchDiffDirectory;
};

struct FPakEntryPair
{
	FString Filename;
	FPakEntry Info;
};

struct FPakInputPair
{
	FString Source;
	FString Dest;
	uint64 SuggestedOrder; 
	bool bNeedsCompression;
	bool bNeedEncryption;

	FPakInputPair()
		: SuggestedOrder(MAX_uint64)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
	{}

	FPakInputPair(const FString& InSource, const FString& InDest)
		: Source(InSource)
		, Dest(InDest)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
	{}

	FORCEINLINE bool operator==(const FPakInputPair& Other) const
	{
		return Source == Other.Source;
	}
};

struct FPakEntryOrder
{
	FPakEntryOrder() : Order(MAX_uint64) {}
	FString Filename;
	uint64  Order;
};

struct FCompressedFileBuffer
{
	FCompressedFileBuffer()
		: OriginalSize(0)
		,TotalCompressedSize(0)
		,FileCompressionBlockSize(0)
		,CompressedBufferSize(0)
	{

	}

	void Reinitialize(FArchive* File,ECompressionFlags CompressionMethod,int64 CompressionBlockSize)
	{
		OriginalSize = File->TotalSize();
		TotalCompressedSize = 0;
		FileCompressionBlockSize = 0;
		FileCompressionMethod = CompressionMethod;
		CompressedBlocks.Reset();
		CompressedBlocks.AddUninitialized((OriginalSize+CompressionBlockSize-1)/CompressionBlockSize);
	}

	void EnsureBufferSpace(int64 RequiredSpace)
	{
		if(RequiredSpace > CompressedBufferSize)
		{
			uint8* NewPtr = (uint8*)FMemory::Malloc(RequiredSpace);
			FMemory::Memcpy(NewPtr,CompressedBuffer.Get(),CompressedBufferSize);
			CompressedBuffer.Reset(NewPtr);
			CompressedBufferSize = RequiredSpace;
		}
	}

	bool CompressFileToWorkingBuffer(const FPakInputPair& InFile,uint8*& InOutPersistentBuffer,int64& InOutBufferSize,ECompressionFlags CompressionMethod,const int32 CompressionBlockSize,const int32 CompressionBitWindow);

	int64						 OriginalSize;
	int64						 TotalCompressedSize;
	int32						 FileCompressionBlockSize;
	ECompressionFlags			 FileCompressionMethod;
	TArray<FPakCompressedBlock>  CompressedBlocks;
	int64						 CompressedBufferSize;
	TUniquePtr<uint8>		     CompressedBuffer;
};

FString GetLongestPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString LongestPath;
	int32 MaxNumDirectories = 0;

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		FString& Filename = FilesToAdd[FileIndex].Dest;
		int32 NumDirectories = 0;
		for (int32 Index = 0; Index < Filename.Len(); Index++)
		{
			if (Filename[Index] == '/')
			{
				NumDirectories++;
			}
		}
		if (NumDirectories > MaxNumDirectories)
		{
			LongestPath = Filename;
			MaxNumDirectories = NumDirectories;
		}
	}
	return FPaths::GetPath(LongestPath) + TEXT("/");
}

FString GetCommonRootPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString Root = GetLongestPath(FilesToAdd);
	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num() && Root.Len(); FileIndex++)
	{
		FString Filename(FilesToAdd[FileIndex].Dest);
		FString Path = FPaths::GetPath(Filename) + TEXT("/");
		int32 CommonSeparatorIndex = -1;
		int32 SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive);
		while (SeparatorIndex >= 0)
		{
			if (FCString::Strnicmp(*Root, *Path, SeparatorIndex + 1) != 0)
			{
				break;
			}
			CommonSeparatorIndex = SeparatorIndex;
			if (CommonSeparatorIndex + 1 < Path.Len())
			{
				SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CommonSeparatorIndex + 1);
			}
			else
			{
				break;
			}
		}
		if ((CommonSeparatorIndex + 1) < Root.Len())
		{
			Root = Root.Mid(0, CommonSeparatorIndex + 1);
		}
	}
	return Root;
}

bool FCompressedFileBuffer::CompressFileToWorkingBuffer(const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, ECompressionFlags CompressionMethod, const int32 CompressionBlockSize, const int32 CompressionBitWindow)
{
	TAutoPtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	if(!FileHandle.IsValid())
	{
		TotalCompressedSize = 0;
		return false;
	}

	Reinitialize(FileHandle.GetOwnedPointer(), CompressionMethod, CompressionBlockSize);
	const int64 FileSize = OriginalSize;
	const int64 PaddedEncryptedFileSize = Align(FileSize,FAES::AESBlockSize);
	if(InOutBufferSize < PaddedEncryptedFileSize)
	{
		InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer,PaddedEncryptedFileSize);
		InOutBufferSize = FileSize;
	}

	// Load to buffer
	FileHandle->Serialize(InOutPersistentBuffer,FileSize);

	// Build buffers for working
	int64 UncompressedSize = FileSize;
	int32 CompressionBufferSize = Align(FCompression::CompressMemoryBound(CompressionMethod,CompressionBlockSize),FAES::AESBlockSize);
	EnsureBufferSpace(Align(FCompression::CompressMemoryBound(CompressionMethod,FileSize),FAES::AESBlockSize));


	TotalCompressedSize = 0;
	int64 UncompressedBytes = 0;
	int32 CurrentBlock = 0;
	while(UncompressedSize)
	{
		int32 BlockSize = (int32)FMath::Min<int64>(UncompressedSize,CompressionBlockSize);
		int32 MaxCompressedBlockSize = FCompression::CompressMemoryBound(CompressionMethod, BlockSize);
		int32 CompressedBlockSize = FMath::Max<int32>(CompressionBufferSize, MaxCompressedBlockSize);
		FileCompressionBlockSize = FMath::Max<uint32>(BlockSize, FileCompressionBlockSize);
		EnsureBufferSpace(Align(TotalCompressedSize+CompressedBlockSize,FAES::AESBlockSize));
		if(!FCompression::CompressMemory(CompressionMethod,CompressedBuffer.Get()+TotalCompressedSize,CompressedBlockSize,InOutPersistentBuffer+UncompressedBytes,BlockSize,CompressionBitWindow))
		{
			return false;
		}
		UncompressedSize -= BlockSize;
		UncompressedBytes += BlockSize;

		CompressedBlocks[CurrentBlock].CompressedStart = TotalCompressedSize;
		CompressedBlocks[CurrentBlock].CompressedEnd = TotalCompressedSize+CompressedBlockSize;
		++CurrentBlock;

		TotalCompressedSize += CompressedBlockSize;

		if(InFile.bNeedEncryption)
		{
			int32 EncryptionBlockPadding = Align(TotalCompressedSize,FAES::AESBlockSize);
			for(int64 FillIndex=TotalCompressedSize; FillIndex < EncryptionBlockPadding; ++FillIndex)
			{
				// Fill the trailing buffer with random bytes from file
				CompressedBuffer.Get()[FillIndex] = CompressedBuffer.Get()[rand()%TotalCompressedSize];
			}
			TotalCompressedSize += EncryptionBlockPadding - TotalCompressedSize;
		}
	}

	return true;
}

bool CopyFileToPak(FArchive& InPak, const FString& InMountPoint, const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, FPakEntryPair& OutNewEntry)
{	
	TAutoPtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	bool bFileExists = FileHandle.IsValid();
	if (bFileExists)
	{
		const int64 FileSize = FileHandle->TotalSize();
		const int64 PaddedEncryptedFileSize = Align(FileSize, FAES::AESBlockSize); 
		OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
		OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
		OutNewEntry.Info.Size = FileSize;
		OutNewEntry.Info.UncompressedSize = FileSize;
		OutNewEntry.Info.CompressionMethod = COMPRESS_None;
		OutNewEntry.Info.bEncrypted = InFile.bNeedEncryption;

		if (InOutBufferSize < PaddedEncryptedFileSize)
		{
			InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer, PaddedEncryptedFileSize);
			InOutBufferSize = FileSize;
		}

		// Load to buffer
		FileHandle->Serialize(InOutPersistentBuffer, FileSize);

		{
			int64 SizeToWrite = FileSize;
			if (InFile.bNeedEncryption)
			{
				for(int64 FillIndex=FileSize; FillIndex < PaddedEncryptedFileSize && InFile.bNeedEncryption; ++FillIndex)
				{
					// Fill the trailing buffer with random bytes from file
					InOutPersistentBuffer[FillIndex] = InOutPersistentBuffer[rand()%FileSize];
				}

				//Encrypt the buffer before writing it to disk
				FAES::EncryptData(InOutPersistentBuffer, PaddedEncryptedFileSize);
				// Update the size to be written
				SizeToWrite = PaddedEncryptedFileSize;
				OutNewEntry.Info.bEncrypted = true;
			}

			// Calculate the buffer hash value
			FSHA1::HashBuffer(InOutPersistentBuffer,FileSize,OutNewEntry.Info.Hash);

			// Write to file
			OutNewEntry.Info.Serialize(InPak,FPakInfo::PakFile_Version_Latest);
			InPak.Serialize(InOutPersistentBuffer,SizeToWrite);
		}
	}
	return bFileExists;
}

bool CopyCompressedFileToPak(FArchive& InPak, const FString& InMountPoint, const FPakInputPair& InFile, const FCompressedFileBuffer& CompressedFile, FPakEntryPair& OutNewEntry)
{
	if (CompressedFile.TotalCompressedSize == 0)
	{
		return false;
	}

	int64 HeaderTell = InPak.Tell();
	OutNewEntry.Info.CompressionMethod = CompressedFile.FileCompressionMethod;
	OutNewEntry.Info.CompressionBlocks.AddUninitialized(CompressedFile.CompressedBlocks.Num());

	int64 TellPos = InPak.Tell() + OutNewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
	const TArray<FPakCompressedBlock>& Blocks = CompressedFile.CompressedBlocks;
	for (int32 BlockIndex = 0, BlockCount = CompressedFile.CompressedBlocks.Num(); BlockIndex < BlockCount; ++BlockIndex)
	{
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedStart = Blocks[BlockIndex].CompressedStart + TellPos;
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedEnd = Blocks[BlockIndex].CompressedEnd + TellPos;
	}

	if (InFile.bNeedEncryption)
	{
		FAES::EncryptData(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);
	}

	//Hash the final buffer thats written
	FSHA1 Hash;
	Hash.Update(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);
	Hash.Final();

	// Update file size & Hash
	OutNewEntry.Info.CompressionBlockSize = CompressedFile.FileCompressionBlockSize;
	OutNewEntry.Info.UncompressedSize = CompressedFile.OriginalSize;
	OutNewEntry.Info.Size = CompressedFile.TotalCompressedSize;
	Hash.GetHash(OutNewEntry.Info.Hash);

	//	Write the header, then the data
	OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
	OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
	OutNewEntry.Info.bEncrypted = InFile.bNeedEncryption;
	OutNewEntry.Info.Serialize(InPak,FPakInfo::PakFile_Version_Latest);
	InPak.Serialize(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);

	return true;
}

void ProcessOrderFile(int32 ArgC, TCHAR* ArgV[], TMap<FString, uint64>& OrderMap)
{
	// List of all items to add to pak file
	FString ResponseFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("-order="), ResponseFile))
	{
		FString Text;
		UE_LOG(LogPakFile, Display, TEXT("Loading pak order file %s..."), *ResponseFile);
		if (FFileHelper::LoadFileToString(Text, *ResponseFile))
		{
			// Read all lines
			TArray<FString> Lines;
			Text.ParseIntoArray(Lines, TEXT("\n"), true);
			for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
			{
				Lines[EntryIndex].ReplaceInline(TEXT("\r"), TEXT(""));
				Lines[EntryIndex].ReplaceInline(TEXT("\n"), TEXT(""));
				int32 OpenOrderNumber = EntryIndex;
				if (Lines[EntryIndex].FindLastChar('"', OpenOrderNumber))
				{
					FString ReadNum = Lines[EntryIndex].RightChop(OpenOrderNumber+1);
					Lines[EntryIndex] = Lines[EntryIndex].Left(OpenOrderNumber+1);
					ReadNum.Trim();
					if (ReadNum.IsNumeric())
					{
						OpenOrderNumber = FCString::Atoi(*ReadNum);
					}
				}
				Lines[EntryIndex] = Lines[EntryIndex].TrimQuotes();
				FString Path=FString::Printf(TEXT("%s"), *Lines[EntryIndex]);
				FPaths::NormalizeFilename(Path);
				OrderMap.Add(Path, OpenOrderNumber);
			}
			UE_LOG(LogPakFile, Display, TEXT("Finished loading pak order file %s."), *ResponseFile);
		}
		else 
		{
			UE_LOG(LogPakFile, Display, TEXT("Unable to load pak order file %s."), *ResponseFile);
		}
	}
}

static void CommandLineParseHelper(const TCHAR* InCmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while(FParse::Token(InCmdLine,NextToken,false))
	{
		if((**NextToken == TCHAR('-')))
		{
			new(Switches)FString(NextToken.Mid(1));
		}
		else
		{
			new(Tokens)FString(NextToken);
		}
	}
}

void ProcessCommandLine(int32 ArgC, TCHAR* ArgV[], TArray<FPakInputPair>& Entries, FPakCommandLineParameters& CmdLineParameters)
{
	// List of all items to add to pak file
	FString ResponseFile;
	FString ClusterSizeString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-blocksize="), ClusterSizeString) && 
		FParse::Value(FCommandLine::Get(), TEXT("-blocksize="), CmdLineParameters.FileSystemBlockSize))
	{
		if (ClusterSizeString.EndsWith(TEXT("MB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024*1024;
		}
		else if (ClusterSizeString.EndsWith(TEXT("KB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024;
		}
	}
	else
	{
		CmdLineParameters.FileSystemBlockSize = 0;
	}

	if (!FParse::Value(FCommandLine::Get(), TEXT("-bitwindow="), CmdLineParameters.CompressionBitWindow))
	{
		CmdLineParameters.CompressionBitWindow = DEFAULT_ZLIB_BIT_WINDOW;
	}

	if (!FParse::Value(FCommandLine::Get(), TEXT("-patchpaddingalign="), CmdLineParameters.PatchFilePadAlign))
	{
		CmdLineParameters.PatchFilePadAlign = 0;
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("-create="), ResponseFile))
	{
		TArray<FString> Lines;

		CmdLineParameters.GeneratePatch = FParse::Value(FCommandLine::Get(), TEXT("-generatepatch="), CmdLineParameters.SourcePatchPakFilename);

		bool bCompress = FParse::Param(FCommandLine::Get(), TEXT("compress"));
		bool bEncrypt = FParse::Param(FCommandLine::Get(), TEXT("encrypt"));

		if (IFileManager::Get().DirectoryExists(*ResponseFile))
		{
			IFileManager::Get().FindFilesRecursive(Lines, *ResponseFile, TEXT("*"), true, false);
		}
		else
		{
			FString Text;
			UE_LOG(LogPakFile, Display, TEXT("Loading response file %s"), *ResponseFile);
			if (FFileHelper::LoadFileToString(Text, *ResponseFile))
			{
				// Remove all carriage return characters.
				Text.ReplaceInline(TEXT("\r"), TEXT(""));
				// Read all lines
				Text.ParseIntoArray(Lines, TEXT("\n"), true);
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Failed to load %s"), *ResponseFile);
			}
		}

		for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
		{
			TArray<FString> SourceAndDest;
			TArray<FString> Switches;
			CommandLineParseHelper(*Lines[EntryIndex].Trim(), SourceAndDest, Switches);
			if( SourceAndDest.Num() == 0)
			{
				continue;
			}
			FPakInputPair Input;

			Input.Source = SourceAndDest[0];
			FPaths::NormalizeFilename(Input.Source);
			if (SourceAndDest.Num() > 1)
			{
				Input.Dest = FPaths::GetPath(SourceAndDest[1]);
			}
			else
			{
				Input.Dest = FPaths::GetPath(Input.Source);
			}
			FPaths::NormalizeFilename(Input.Dest);
			FPakFile::MakeDirectoryFromPath(Input.Dest);

			//check for compression switches
			for (int32 Index = 0; Index < Switches.Num(); ++Index)
			{
				if (Switches[Index] == TEXT("compress"))
				{
					Input.bNeedsCompression = true;
				}
				if (Switches[Index] == TEXT("encrypt"))
				{
					Input.bNeedEncryption = true;
				}
			}
			Input.bNeedsCompression |= bCompress;
			Input.bNeedEncryption |= bEncrypt;

			UE_LOG(LogPakFile, Log, TEXT("Added file Source: %s Dest: %s"), *Input.Source, *Input.Dest);
			Entries.Add(Input);
		}			
	}
	else
	{
		// Override destination path.
		FString MountPoint;
		FParse::Value(FCommandLine::Get(), TEXT("-dest="), MountPoint);
		FPaths::NormalizeFilename(MountPoint);
		FPakFile::MakeDirectoryFromPath(MountPoint);

		// Parse comand line params. The first param after the program name is the created pak name
		for (int32 Index = 2; Index < ArgC; Index++)
		{
			// Skip switches and add everything else to the Entries array
			TCHAR* Param = ArgV[Index];
			if (Param[0] != '-')
			{
				FPakInputPair Input;
				Input.Source = Param;
				FPaths::NormalizeFilename(Input.Source);
				if (MountPoint.Len() > 0)
				{
					FString SourceDirectory( FPaths::GetPath(Input.Source) );
					FPakFile::MakeDirectoryFromPath(SourceDirectory);
					Input.Dest = Input.Source.Replace(*SourceDirectory, *MountPoint, ESearchCase::IgnoreCase);
				}
				else
				{
					Input.Dest = FPaths::GetPath(Input.Source);
					FPakFile::MakeDirectoryFromPath(Input.Dest);
				}
				FPaths::NormalizeFilename(Input.Dest);
				Entries.Add(Input);
			}
		}
	}
	UE_LOG(LogPakFile, Display, TEXT("Added %d entries to add to pak file."), Entries.Num());
}

void CollectFilesToAdd(TArray<FPakInputPair>& OutFilesToAdd, const TArray<FPakInputPair>& InEntries, const TMap<FString, uint64>& OrderMap)
{
	UE_LOG(LogPakFile, Display, TEXT("Collecting files to add to pak file..."));
	const double StartTime = FPlatformTime::Seconds();

	// Start collecting files
	TSet<FString> AddedFiles;	
	for (int32 Index = 0; Index < InEntries.Num(); Index++)
	{
		const FPakInputPair& Input = InEntries[Index];
		const FString& Source = Input.Source;
		bool bCompression = Input.bNeedsCompression;
		bool bEncryption = Input.bNeedEncryption;


		FString Filename = FPaths::GetCleanFilename(Source);
		FString Directory = FPaths::GetPath(Source);
		FPaths::MakeStandardFilename(Directory);
		FPakFile::MakeDirectoryFromPath(Directory);

		if (Filename.IsEmpty())
		{
			Filename = TEXT("*.*");
		}
		if ( Filename.Contains(TEXT("*")) )
		{
			// Add multiple files
			TArray<FString> FoundFiles;
			IFileManager::Get().FindFilesRecursive(FoundFiles, *Directory, *Filename, true, false);

			for (int32 FileIndex = 0; FileIndex < FoundFiles.Num(); FileIndex++)
			{
				FPakInputPair FileInput;
				FileInput.Source = FoundFiles[FileIndex];
				FPaths::MakeStandardFilename(FileInput.Source);
				FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
				const uint64* FoundOrder = OrderMap.Find(FileInput.Dest);
				if(FoundOrder)
				{
					FileInput.SuggestedOrder = *FoundOrder;
				}
				FileInput.bNeedsCompression = bCompression;
				FileInput.bNeedEncryption = bEncryption;
				if (!AddedFiles.Contains(FileInput.Source))
				{
					OutFilesToAdd.Add(FileInput);
					AddedFiles.Add(FileInput.Source);
				}
				else
				{
					int32 FoundIndex;
					OutFilesToAdd.Find(FileInput,FoundIndex);
					OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
					OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
				}
			}
		}
		else
		{
			// Add single file
			FPakInputPair FileInput;
			FileInput.Source = Input.Source;
			FPaths::MakeStandardFilename(FileInput.Source);
			FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
			const uint64* FoundOrder = OrderMap.Find(FileInput.Dest);
			if (FoundOrder)
			{
				FileInput.SuggestedOrder = *FoundOrder;
			}
			FileInput.bNeedEncryption = bEncryption;
			FileInput.bNeedsCompression = bCompression;

			if (AddedFiles.Contains(FileInput.Source))
			{
				int32 FoundIndex;
				OutFilesToAdd.Find(FileInput, FoundIndex);
				OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
				OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
			}
			else
			{
				OutFilesToAdd.Add(FileInput);
				AddedFiles.Add(FileInput.Source);
			}
		}
	}

	// Sort by suggested order then alphabetically
	struct FInputPairSort
	{
		FORCEINLINE bool operator()(const FPakInputPair& A, const FPakInputPair& B) const
		{
			return A.SuggestedOrder == B.SuggestedOrder ? A.Dest < B.Dest : A.SuggestedOrder < B.SuggestedOrder;
		}
	};
	OutFilesToAdd.Sort(FInputPairSort());
	UE_LOG(LogPakFile, Display, TEXT("Collected %d files in %.2lfs."), OutFilesToAdd.Num(), FPlatformTime::Seconds() - StartTime);
}

bool BufferedCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, void* Buffer, int64 BufferSize)
{	
	// Align down
	BufferSize = BufferSize & ~(FAES::AESBlockSize-1);
	int64 RemainingSizeToCopy = Entry.Size;
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		// If file is encrypted so we need to account for padding
		int64 SizeToRead = Entry.bEncrypted ? Align(SizeToCopy,FAES::AESBlockSize) : SizeToCopy;

		Source.Serialize(Buffer,SizeToRead);
		if (Entry.bEncrypted)
		{
			FAES::DecryptData((uint8*)Buffer,SizeToRead);
		}
		Dest.Serialize(Buffer, SizeToCopy);
		RemainingSizeToCopy -= SizeToRead;
	}
	return true;
}

bool UncompressCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize)
{
	if (Entry.UncompressedSize == 0)
	{
		return false;
	}

	int64 WorkingSize = Entry.CompressionBlockSize;
	int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound((ECompressionFlags)Entry.CompressionMethod, WorkingSize);
	WorkingSize += MaxCompressionBlockSize;
	if (BufferSize < WorkingSize)
	{
		PersistentBuffer = (uint8*)FMemory::Realloc(PersistentBuffer, WorkingSize);
		BufferSize = WorkingSize;
	}

	uint8* UncompressedBuffer = PersistentBuffer+MaxCompressionBlockSize;

	for (uint32 BlockIndex=0, BlockIndexNum=Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
	{
		uint32 CompressedBlockSize = Entry.CompressionBlocks[BlockIndex].CompressedEnd - Entry.CompressionBlocks[BlockIndex].CompressedStart;
		uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize*BlockIndex, Entry.CompressionBlockSize);
		Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart);
		uint32 SizeToRead = Entry.bEncrypted ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
		Source.Serialize(PersistentBuffer, SizeToRead);

		if (Entry.bEncrypted)
		{
			FAES::DecryptData(PersistentBuffer, SizeToRead);
		}

		if(!FCompression::UncompressMemory((ECompressionFlags)Entry.CompressionMethod,UncompressedBuffer,UncompressedBlockSize,PersistentBuffer,CompressedBlockSize))
		{
			return false;
		}
		Dest.Serialize(UncompressedBuffer,UncompressedBlockSize);
	}

	return true;
}

/**
 * Creates a pak file writer. This can be a signed writer if the encryption keys are specified in the command line
 */
FArchive* CreatePakWriter(const TCHAR* Filename)
{
	FArchive* Writer = IFileManager::Get().CreateFileWriter(Filename);
	FString KeyFilename;
	if (Writer && FParse::Value(FCommandLine::Get(), TEXT("sign="), KeyFilename, false))
	{
		FKeyPair Pair;
		if (KeyFilename.StartsWith(TEXT("0x")))
		{
			TArray<FString> KeyValueText;
			if (KeyFilename.ParseIntoArray(KeyValueText, TEXT("+"), true) == 3)
			{
				Pair.PrivateKey.Exponent.Parse(KeyValueText[0]);
				Pair.PrivateKey.Modulus.Parse(KeyValueText[1]);
				Pair.PublicKey.Exponent.Parse(KeyValueText[2]);
				Pair.PublicKey.Modulus = Pair.PrivateKey.Modulus;
				UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from command line."));
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Expected 3 values, got %d, when parsing %s"), KeyValueText.Num(), *KeyFilename);
				Pair.PrivateKey.Exponent.Zero();
			}
		}
		else if (!ReadKeysFromFile(*KeyFilename, Pair))
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to load signature keys %s."), *KeyFilename);
		}

		if (!TestKeys(Pair))
		{
			Pair.PrivateKey.Exponent.Zero();
		}

		if (!Pair.PrivateKey.Exponent.IsZero())
		{
			UE_LOG(LogPakFile, Display, TEXT("Creating signed pak %s."), Filename);
			Writer = new FSignedArchiveWriter(*Writer, Filename, Pair.PublicKey, Pair.PrivateKey);
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to create a signed pak writer."));
			delete Writer;
			Writer = NULL;
		}		
	}
	return Writer;
}

bool CreatePakFile(const TCHAR* Filename, TArray<FPakInputPair>& FilesToAdd, const FPakCommandLineParameters& CmdLineParameters)
{	
	const double StartTime = FPlatformTime::Seconds();

	// Create Pak
	TAutoPtr<FArchive> PakFileHandle(CreatePakWriter(Filename));
	if (!PakFileHandle.IsValid())
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to create pak file \"%s\"."), Filename);
		return false;
	}

	FPakInfo Info;
	TArray<FPakEntryPair> Index;
	FString MountPoint = GetCommonRootPath(FilesToAdd);
	uint8* ReadBuffer = NULL;
	int64 BufferSize = 0;
	ECompressionFlags CompressionMethod = COMPRESS_None;
	FCompressedFileBuffer CompressedFileBuffer;

	uint8* PaddingBuffer = nullptr;
	int64 PaddingBufferSize = 0;
	if (CmdLineParameters.PatchFilePadAlign > 0)
	{
		PaddingBufferSize = CmdLineParameters.PatchFilePadAlign;
		PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
		FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
	}

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		//  Remember the offset but don't serialize it with the entry header.
		int64 NewEntryOffset = PakFileHandle->Tell();
		FPakEntryPair NewEntry;

		//check if this file requested to be compression
		int64 OriginalFileSize = IFileManager::Get().FileSize(*FilesToAdd[FileIndex].Source);
		int64 RealFileSize = OriginalFileSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
		CompressionMethod = (FilesToAdd[FileIndex].bNeedsCompression && OriginalFileSize > 0) ? COMPRESS_Default : COMPRESS_None;

		if (CompressionMethod != COMPRESS_None)
		{
			if (CompressedFileBuffer.CompressFileToWorkingBuffer(FilesToAdd[FileIndex], ReadBuffer, BufferSize, CompressionMethod, CmdLineParameters.CompressionBlockSize, CmdLineParameters.CompressionBitWindow))
			{
				// Check the compression ratio, if it's too low just store uncompressed. Also take into account read size
				// if we still save 64KB it's probably worthwhile compressing, as that saves a file read operation in the runtime.
				// TODO: drive this threashold from the command line
				float PercentLess = ((float)CompressedFileBuffer.TotalCompressedSize/(OriginalFileSize/100.f));
				if (PercentLess > 90.f && (OriginalFileSize-CompressedFileBuffer.TotalCompressedSize) < 65536)
				{
					CompressionMethod = COMPRESS_None;
				}
				else
				{
					NewEntry.Info.CompressionMethod = CompressionMethod;
					NewEntry.Info.CompressionBlocks.AddUninitialized(CompressedFileBuffer.CompressedBlocks.Num());
					RealFileSize = CompressedFileBuffer.TotalCompressedSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
					NewEntry.Info.CompressionBlocks.Reset();
				}
			}
			else
			{
				// Compression failed. Include file uncompressed and warn the user.
				UE_LOG(LogPakFile, Warning, TEXT("File \"%s\" failed compression. File will be saved uncompressed."), *FilesToAdd[FileIndex].Source);
				CompressionMethod = COMPRESS_None;
			}
		}

		// Account for file system block size, which is a boundary we want to avoid crossing.
		if (CmdLineParameters.FileSystemBlockSize > 0 && OriginalFileSize != INDEX_NONE && RealFileSize <= CmdLineParameters.FileSystemBlockSize)
		{
			if ((NewEntryOffset / CmdLineParameters.FileSystemBlockSize) != ((NewEntryOffset+RealFileSize) / CmdLineParameters.FileSystemBlockSize))
			{
				//File crosses a block boundary, so align it to the beginning of the next boundary
				int64 OldOffset = NewEntryOffset;
				NewEntryOffset = AlignArbitrary(NewEntryOffset, CmdLineParameters.FileSystemBlockSize);
				int64 PaddingRequired = NewEntryOffset - OldOffset;
				
				if (PaddingRequired > 0)
				{
					// If we don't already have a padding buffer, create one
					if (PaddingBuffer == nullptr)
					{
						PaddingBufferSize = 64 * 1024;
						PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
						FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
					}

					while (PaddingRequired > 0)
					{
						int64 AmountToWrite = FMath::Min(PaddingRequired, PaddingBufferSize);
						PakFileHandle->Serialize(PaddingBuffer, AmountToWrite);
						PaddingRequired -= AmountToWrite;
					}
					
					check(PakFileHandle->Tell() == NewEntryOffset);
				}
			}
		}

		// Some platforms provide patch download size reduction by diffing the patch files.  However, they often operate on specific block
		// sizes when dealing with new data within the file.  Pad files out to the given alignment to work with these systems more nicely.
		if (CmdLineParameters.PatchFilePadAlign > 0 && OriginalFileSize != INDEX_NONE)
		{	
			NewEntryOffset = AlignArbitrary(NewEntryOffset, CmdLineParameters.PatchFilePadAlign);
			int64 CurrentLoc = PakFileHandle->Tell();
			int64 PaddingSize = NewEntryOffset - CurrentLoc;
			check(PaddingSize <= PaddingBufferSize);

			//have to pad manually with 0's.  File locations skipped by Seek and never written are uninitialized which would defeat the whole purpose
			//of padding for certain platforms patch diffing systems.
			PakFileHandle->Serialize(PaddingBuffer, PaddingSize);
			check(PakFileHandle->Tell() == NewEntryOffset);						
		}

		
		bool bCopiedToPak;
		if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
		{
			bCopiedToPak = CopyCompressedFileToPak(*PakFileHandle, MountPoint, FilesToAdd[FileIndex], CompressedFileBuffer, NewEntry);
		}
		else
		{
			bCopiedToPak = CopyFileToPak(*PakFileHandle, MountPoint, FilesToAdd[FileIndex], ReadBuffer, BufferSize, NewEntry);
		}
		
		if (bCopiedToPak)
		{
			// Update offset now and store it in the index (and only in index)
			NewEntry.Info.Offset = NewEntryOffset;
			Index.Add(NewEntry);
			if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
			{
				float PercentLess = ((float)NewEntry.Info.Size/(NewEntry.Info.UncompressedSize/100.f));
				UE_LOG(LogPakFile, Log, TEXT("Added compressed file \"%s\", %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes. "), *NewEntry.Filename, PercentLess, NewEntry.Info.Size, NewEntry.Info.UncompressedSize);
			}
			else
			{
				UE_LOG(LogPakFile, Log, TEXT("Added file \"%s\", %lld bytes."), *NewEntry.Filename, NewEntry.Info.Size);
			}
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Missing file \"%s\" will not be added to PAK file."), *FilesToAdd[FileIndex].Source);
		}
	}

	FMemory::Free(PaddingBuffer);
	FMemory::Free(ReadBuffer);
	ReadBuffer = NULL;

	// Remember IndexOffset
	Info.IndexOffset = PakFileHandle->Tell();

	// Serialize Pak Index at the end of Pak File
	TArray<uint8> IndexData;
	FMemoryWriter IndexWriter(IndexData);
	IndexWriter.SetByteSwapping(PakFileHandle->ForceByteSwapping());
	int32 NumEntries = Index.Num();
	IndexWriter << MountPoint;
	IndexWriter << NumEntries;
	for (int32 EntryIndex = 0; EntryIndex < Index.Num(); EntryIndex++)
	{
		FPakEntryPair& Entry = Index[EntryIndex];
		IndexWriter << Entry.Filename;
		Entry.Info.Serialize(IndexWriter, Info.Version);
	}
	PakFileHandle->Serialize(IndexData.GetData(), IndexData.Num());

	FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), Info.IndexHash);
	Info.IndexSize = IndexData.Num();

	// Save trailer (offset, size, hash value)
	Info.Serialize(*PakFileHandle);

	UE_LOG(LogPakFile, Display, TEXT("Added %d files, %lld bytes total, time %.2lfs."), Index.Num(), PakFileHandle->TotalSize(), FPlatformTime::Seconds() - StartTime);

	PakFileHandle->Close();
	PakFileHandle.Reset();

	return true;
}

bool TestPakFile(const TCHAR* Filename)
{	
	FPakFile PakFile(Filename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	if (PakFile.IsValid())
	{
		return PakFile.Check();
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), Filename);
		return false;
	}
}

bool ListFilesInPak(const TCHAR * InPakFilename, int64 SizeFilter = 0)
{
	FPakFile PakFile(InPakFilename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	int32 FileCount = 0;
	int64 FileSize = 0;
	int64 FilteredSize = 0;

	if (PakFile.IsValid())
	{
		TArray<FPakFile::FFileIterator> Records;

		for (FPakFile::FFileIterator It(PakFile); It; ++It)
		{
			Records.Add(It);
		}

		struct FOffsetSort
		{
			FORCEINLINE bool operator()(const FPakFile::FFileIterator& A, const FPakFile::FFileIterator& B) const
			{
				return A.Info().Offset < B.Info().Offset;
			}
		};

		Records.Sort(FOffsetSort());

		for (auto It : Records)
		{
			const FPakEntry& Entry = It.Info();
			if (Entry.Size >= SizeFilter)
			{
				UE_LOG(LogPakFile, Display, TEXT("\"%s\" offset: %lld, size: %d bytes, sha1: %s."), *It.Filename(), Entry.Offset, Entry.Size, *BytesToHex(Entry.Hash, sizeof(Entry.Hash)));
				FilteredSize += Entry.Size;
			}
			FileSize += Entry.Size;
			FileCount++;
		}
		UE_LOG(LogPakFile, Display, TEXT("%d files (%lld bytes), (%lld filtered bytes)."), FileCount, FileSize, FilteredSize);

		return true;
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), InPakFilename);
		return false;
	}
}

bool ExtractFilesFromPak(const TCHAR* InPakFilename, const TCHAR* InDestPath, bool bUseMountPoint = false)
{
	FPakFile PakFile(InPakFilename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	if (PakFile.IsValid())
	{
		FString DestPath(InDestPath);
		FArchive& PakReader = *PakFile.GetSharedReader(NULL);
		const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
		void* Buffer = FMemory::Malloc(BufferSize);
		int64 CompressionBufferSize = 0;
		uint8* PersistantCompressionBuffer = NULL;
		int32 ErrorCount = 0;
		int32 FileCount = 0;

		FString PakMountPoint = bUseMountPoint ? PakFile.GetMountPoint().Replace( TEXT("../../../"), TEXT("")) : TEXT("");

		for (FPakFile::FFileIterator It(PakFile); It; ++It, ++FileCount)
		{
			const FPakEntry& Entry = It.Info();
			PakReader.Seek(Entry.Offset);
			uint32 SerializedCrcTest = 0;
			FPakEntry EntryInfo;
			EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);
			if (EntryInfo == Entry)
			{
				FString DestFilename(DestPath / PakMountPoint /  It.Filename());

				TAutoPtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
				if (FileHandle.IsValid())
				{
					if (Entry.CompressionMethod == COMPRESS_None)
					{
						BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize);
					}
					else
					{
						UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize);
					}
					UE_LOG(LogPakFile, Display, TEXT("Extracted \"%s\" to \"%s\"."), *It.Filename(), *DestFilename);
				}
				else
				{
					UE_LOG(LogPakFile, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
					ErrorCount++;
				}
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
				ErrorCount++;
			}
		}
		FMemory::Free(Buffer);
		FMemory::Free(PersistantCompressionBuffer);

		UE_LOG(LogPakFile, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);

		return true;
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), InPakFilename);
		return false;
	}
}

void CreateDiffRelativePathMap(TArray<FString>& FileNames, const FString& RootPath, TMap<FName, FString>& OutMap)
{
	for (int32 i = 0; i < FileNames.Num(); ++i)
	{
		const FString& FullPath = FileNames[i];
		FString RelativePath = FullPath.Mid(RootPath.Len());
		OutMap.Add(FName(*RelativePath), FullPath);
	}
}

bool DiffFilesInPaks(const FString InPakFilename1, const FString InPakFilename2)
{
	int32 NumUniquePAK1 = 0;
	int32 NumUniquePAK2 = 0;
	int32 NumDifferentContents = 0;
	int32 NumEqualContents = 0;

	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	UE_LOG(LogPakFile, Log, TEXT("FileEventType, FileName, Size1, Size2"));

	// Allow the suppression of unique file logging for one or both files
	const bool bLogUniques = !FParse::Param(FCommandLine::Get(), TEXT("nouniques"));
	const bool bLogUniques1 = bLogUniques && !FParse::Param(FCommandLine::Get(), TEXT("nouniquesfile1"));
	const bool bLogUniques2 = bLogUniques && !FParse::Param(FCommandLine::Get(), TEXT("nouniquesfile2"));

	FPakFile PakFile1(*InPakFilename1, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	FPakFile PakFile2(*InPakFilename2, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	if (PakFile1.IsValid() && PakFile2.IsValid())
	{		
		FArchive& PakReader1 = *PakFile1.GetSharedReader(NULL);
		FArchive& PakReader2 = *PakFile2.GetSharedReader(NULL);

		const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
		void* Buffer = FMemory::Malloc(BufferSize);
		int64 CompressionBufferSize = 0;
		uint8* PersistantCompressionBuffer = NULL;
		int32 ErrorCount = 0;
		int32 FileCount = 0;		
		
		//loop over pak1 entries.  compare against entry in pak2.
		for (FPakFile::FFileIterator It(PakFile1); It; ++It, ++FileCount)
		{
			const FString& PAK1FileName = It.Filename();

			//double check entry info and move pakreader into place
			const FPakEntry& Entry1 = It.Info();
			PakReader1.Seek(Entry1.Offset);

			FPakEntry EntryInfo1;
			EntryInfo1.Serialize(PakReader1, PakFile1.GetInfo().Version);

			if (EntryInfo1 != Entry1)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry1Invalid, %s, 0, 0"), *PAK1FileName);
				continue;
			}
			
			//see if entry exists in other pak							
			const FPakEntry* Entry2 = PakFile2.Find(PakFile1.GetMountPoint() / PAK1FileName);
			if (Entry2 == nullptr)
			{
				++NumUniquePAK1;
				if (bLogUniques1)
				{
					UE_LOG(LogPakFile, Log, TEXT("UniqueToFirstPak, %s, %i, 0"), *PAK1FileName, EntryInfo1.UncompressedSize);
				}
				continue;
			}

			//double check entry info and move pakreader into place
			PakReader2.Seek(Entry2->Offset);
			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);
			if (EntryInfo2 != *Entry2)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry2Invalid, %s, 0, 0"), *PAK1FileName);
				continue;;
			}

			//check sizes first as quick compare.
			if (EntryInfo1.UncompressedSize != EntryInfo2.UncompressedSize)
			{
				UE_LOG(LogPakFile, Log, TEXT("FilesizeDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				continue;
			}
			
			//serialize and memcompare the two entries
			{
				void* PAKDATA1 = FMemory::Malloc(EntryInfo1.UncompressedSize);
				void* PAKDATA2 = FMemory::Malloc(EntryInfo2.UncompressedSize);
				FBufferWriter PAKWriter1(PAKDATA1, EntryInfo1.UncompressedSize, false);
				FBufferWriter PAKWriter2(PAKDATA2, EntryInfo2.UncompressedSize, false);

				if (EntryInfo1.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter1, PakReader1, Entry1, Buffer, BufferSize);
				}
				else
				{
					UncompressCopyFile(PAKWriter1, PakReader1, Entry1, PersistantCompressionBuffer, CompressionBufferSize);
				}

				if (EntryInfo2.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter2, PakReader2, *Entry2, Buffer, BufferSize);
				}
				else
				{
					UncompressCopyFile(PAKWriter2, PakReader2, *Entry2, PersistantCompressionBuffer, CompressionBufferSize);
				}

				if (FMemory::Memcmp(PAKDATA1, PAKDATA2, EntryInfo1.UncompressedSize) != 0)
				{
					++NumDifferentContents;
					UE_LOG(LogPakFile, Log, TEXT("ContentsDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				}
				else
				{
					++NumEqualContents;
				}
				FMemory::Free(PAKDATA1);
				FMemory::Free(PAKDATA2);
			}			
		}
		
		//check for files unique to the second pak.
		for (FPakFile::FFileIterator It(PakFile2); It; ++It, ++FileCount)
		{
			const FPakEntry& Entry2 = It.Info();
			PakReader2.Seek(Entry2.Offset);

			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);

			if (EntryInfo2 == Entry2)
			{
				const FString& PAK2FileName = It.Filename();
				const FPakEntry* Entry1 = PakFile1.Find(PakFile2.GetMountPoint() / PAK2FileName);
				if (Entry1 == nullptr)
				{
					++NumUniquePAK2;
					if (bLogUniques2)
					{
						UE_LOG(LogPakFile, Log, TEXT("UniqueToSecondPak, %s, 0, %i"), *PAK2FileName, Entry2.UncompressedSize);
					}
					continue;
				}
			}
		}

		FMemory::Free(Buffer);
		Buffer = nullptr;
	}

	UE_LOG(LogPakFile, Log, TEXT("Comparison complete"));
	UE_LOG(LogPakFile, Log, TEXT("Unique to first pak: %i, Unique to second pak: %i, Num Different: %i, NumEqual: %i"), NumUniquePAK1, NumUniquePAK2, NumDifferentContents, NumEqualContents);	
	return true;
}

bool GenerateHashForFile( FString Filename, uint8 FileHash[16])
{
	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);

	if ( File == NULL )
		return false;

	uint64 TotalSize = File->TotalSize();

	uint8* ByteBuffer = new uint8[TotalSize];

	File->Serialize(ByteBuffer, TotalSize);

	delete File;
	File = NULL;

	FMD5 FileHasher;
	FileHasher.Update(ByteBuffer, TotalSize);

	delete[] ByteBuffer;

	FileHasher.Final(FileHash);

	return true;


	// uint8 DestFileHash[20];

}


void RemoveIdenticalFiles( TArray<FPakInputPair>& FilesToPak, const FString& SourceDirectory )
{
	for ( int I = FilesToPak.Num()-1; I >= 0; --I )
	{
		const auto& NewFile = FilesToPak[I]; 

		FString SourceFilename = SourceDirectory / NewFile.Dest.Replace(TEXT("../../../"), TEXT(""));
		int64 SourceTotalSize = IFileManager::Get().FileSize(*SourceFilename);

		FString DestFilename = NewFile.Source;
		int64 DestTotalSize = IFileManager::Get().FileSize(*DestFilename);
		
		if ( SourceTotalSize != DestTotalSize )
		{
			// file size doesn't match 
			UE_LOG(LogPakFile, Display, TEXT("Source file size for %s %d bytes doesn't match %s %d bytes"), *SourceFilename, SourceTotalSize, *DestFilename, DestTotalSize);
			continue;
		}

		uint8 SourceFileHash[16];
		if ( GenerateHashForFile(SourceFilename, SourceFileHash) == false )
		{
			// file size doesn't match 
			UE_LOG(LogPakFile, Display, TEXT("Source file size %s doesn't exist will be included in build"), *SourceFilename);
			continue;
		}
		uint8 DestFileHash[16];
		if ( GenerateHashForFile( DestFilename, DestFileHash ) == false )
		{
			// destination file was removed don't really care about it
			UE_LOG(LogPakFile, Display, TEXT("File was removed from destination cooked content %s not included in patch"), *DestFilename);
			continue;
		}

		int32 Diff = FMemory::Memcmp( SourceFileHash, DestFileHash, sizeof( DestFileHash ) );
		if ( Diff != 0 )
		{
			UE_LOG(LogPakFile, Display, TEXT("Source file hash for %s doesn't match dest file hash %s and will be included in patch"), *SourceFilename, *DestFilename);
			continue;
		}

		UE_LOG(LogPakFile, Display, TEXT("Source file %s matches dest file %s and will not be included in patch"), *SourceFilename, *DestFilename);
		// remove fromt eh files to pak list
		FilesToPak.RemoveAt(I);
	}
}

FString GetPakPath(const TCHAR* SpecifiedPath, bool bIsForCreation)
{
	FString PakFilename(SpecifiedPath);
	FPaths::MakeStandardFilename(PakFilename);
	
	// if we are trying to open (not create) it, but BaseDir relative doesn't exist, look in LaunchDir
	if (!bIsForCreation && !FPaths::FileExists(PakFilename))
	{
		PakFilename = FPaths::LaunchDir() + SpecifiedPath;

		if (!FPaths::FileExists(PakFilename))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Existing pak file %s could not be found (checked against binary and launch directories)"), SpecifiedPath);
			return TEXT("");
		}
	}
	
	return PakFilename;
}

/**
 * Application entry point
 * Params:
 *   -Test test if the pak file is healthy
 *   -Extract extracts pak file contents (followed by a path, i.e.: -extract D:\ExtractedPak)
 *   -Create=filename response file to create a pak file with
 *   -Sign=filename use the key pair in filename to sign a pak file, or: -sign=key_hex_values_separated_with_+, i.e: -sign=0x123456789abcdef+0x1234567+0x12345abc
 *    where the first number is the private key exponend, the second one is modulus and the third one is the public key exponent.
 *   -Signed use with -extract and -test to let the code know this is a signed pak
 *   -GenerateKeys=filename generates encryption key pair for signing a pak file
 *   -P=prime will use a predefined prime number for generating encryption key file
 *   -Q=prime same as above, P != Q, GCD(P, Q) = 1 (which is always true if they're both prime)
 *   -GeneratePrimeTable=filename generates a prime table for faster prime number generation (.inl file)
 *   -TableMax=number maximum prime number in the generated table (default is 10000)
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	if (ArgC < 2)
	{
		UE_LOG(LogPakFile, Error, TEXT("No pak file name specified. Usage:"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Test"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -List"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Extract <ExtractDir>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Create=<ResponseFile> [Options]"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Dest=<MountPoint>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GenerateKeys=<KeyFilename>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GeneratePrimeTable=<KeyFilename> [-TableMax=<N>]"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename1> <PakFilename2> -diff"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak -TestEncryption"));
		UE_LOG(LogPakFile, Error, TEXT("  Options:"));
		UE_LOG(LogPakFile, Error, TEXT("    -blocksize=<BlockSize>"));
		UE_LOG(LogPakFile, Error, TEXT("    -bitwindow=<BitWindow>"));
		UE_LOG(LogPakFile, Error, TEXT("    -compress"));
		UE_LOG(LogPakFile, Error, TEXT("    -encrypt"));
		UE_LOG(LogPakFile, Error, TEXT("    -order=<OrderingFile>"));
		UE_LOG(LogPakFile, Error, TEXT("    -diff (requires 2 filenames first)"));
		return 1;
	}

	FPakCommandLineParameters CmdLineParameters;
	int32 Result = 0;
	FString KeyFilename;
	if (FParse::Value(FCommandLine::Get(), TEXT("GenerateKeys="), KeyFilename, false))
	{
		Result = GenerateKeys(*KeyFilename) ? 0 : 1;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("GeneratePrimeTable="), KeyFilename, false))
	{
		int64 MaxPrimeValue = 10000;
		FParse::Value(FCommandLine::Get(), TEXT("TableMax="), MaxPrimeValue);
		GeneratePrimeNumberTable(MaxPrimeValue, *KeyFilename);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("TestEncryption")))
	{
		void TestEncryption();
		TestEncryption();
	}
	else 
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("Test")))
		{
			FString PakFilename = GetPakPath(ArgV[1], false);
			Result = TestPakFile(*PakFilename) ? 0 : 1;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("List")))
		{
			int64 SizeFilter = 0;
			FParse::Value(FCommandLine::Get(), TEXT("SizeFilter="), SizeFilter);

			FString PakFilename = GetPakPath(ArgV[1], false);
			Result = ListFilesInPak(*PakFilename, SizeFilter);
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("Diff")))
		{
			FString PakFilename1 = GetPakPath(ArgV[1], false);
			FString PakFilename2 = GetPakPath(ArgV[2], false);
			Result = DiffFilesInPaks(*PakFilename1, *PakFilename2);
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("Extract")))
		{
			FString PakFilename = GetPakPath(ArgV[1], false);
			if (ArgC < 4)
			{
				UE_LOG(LogPakFile, Error, TEXT("No extraction path specified."));
				Result = 1;
			}
			else
			{
				FString DestPath = (ArgV[2][0] == '-') ? ArgV[3] : ArgV[2];
				Result = ExtractFilesFromPak(*PakFilename, *DestPath) ? 0 : 1;
			}
		}
		else
		{
			// since this is for creation, we pass true to make it not look in LaunchDir
			FString PakFilename = GetPakPath(ArgV[1], true);

			// List of all items to add to pak file
			TArray<FPakInputPair> Entries;
			ProcessCommandLine(ArgC, ArgV, Entries, CmdLineParameters);
			TMap<FString, uint64> OrderMap;
			ProcessOrderFile(ArgC, ArgV, OrderMap);

			if (Entries.Num() == 0)
			{
				UE_LOG(LogPakFile, Error, TEXT("No files specified to add to pak file."));
				Result = 1;
			}
			else
			{
				if ( CmdLineParameters.GeneratePatch )
				{
					FString OutputPath;
					if (!FParse::Value(FCommandLine::Get(), TEXT("TempFiles="), OutputPath))
					{
						OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
					}

					IFileManager::Get().DeleteDirectory(*OutputPath);

					UE_LOG(LogPakFile, Display, TEXT("Generating patch from %s."), *CmdLineParameters.SourcePatchPakFilename );

					if ( ExtractFilesFromPak( *CmdLineParameters.SourcePatchPakFilename, *OutputPath, true ) == false )
					{
						UE_LOG(LogPakFile, Error, TEXT("Unable to extract files from source pak file for patch") );
					}
					CmdLineParameters.SourcePatchDiffDirectory = OutputPath;
				}


				// Start collecting files
				TArray<FPakInputPair> FilesToAdd;
				CollectFilesToAdd(FilesToAdd, Entries, OrderMap);

				if ( CmdLineParameters.GeneratePatch )
				{
					// if we are generating a patch here we remove files which are already shipped...
					RemoveIdenticalFiles(FilesToAdd, CmdLineParameters.SourcePatchDiffDirectory);
				}


				Result = CreatePakFile(*PakFilename, FilesToAdd, CmdLineParameters) ? 0 : 1;

				if (CmdLineParameters.GeneratePatch)
				{
					FString OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
					// delete the temporary directory
					IFileManager::Get().DeleteDirectory(*OutputPath, false, true);
				}
			}
		}
	}
	GLog->Flush();

	return Result;
}
