// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_BUILDPATCHGENERATION

#include "BuildPatchServicesPrivatePCH.h"

#include "ManifestBuilder.h"
#include "BuildStreamer.h"

namespace BuildPatchServices
{
	class FFileManifestBuilder
	{
	public:
		FFileManifestBuilder();
		~FFileManifestBuilder();

		uint64 CurrentDataPos;

		FFileSpan FileSpan;

		FFileManifestData* FileManifest;
	};

	FFileManifestBuilder::FFileManifestBuilder()
		: CurrentDataPos(0)
		, FileManifest(nullptr)
	{

	}

	FFileManifestBuilder::~FFileManifestBuilder()
	{

	}

	class FManifestBuilderImpl
		: public FManifestBuilder
	{
	public:
		FManifestBuilderImpl(const FManifestDetails& Details, const FBuildStreamerRef& BuildStreamer);
		virtual ~FManifestBuilderImpl();

		virtual void AddDataScanner(FDataScannerRef Scanner) override;
		virtual void SaveToFile(const FString& Filename) override;
	private:
		void BuildManifest();
		FDataScannerPtr GetNextScanner();

	private:
		FBuildStreamerRef BuildStreamer;
		FBuildPatchAppManifestRef Manifest;
		TMap<FString, FFileAttributes> FileAttributesMap;
		FCriticalSection DataScannersCS;
		TArray<FDataScannerRef> DataScanners;
		FThreadSafeBool EndOfData;
		TFuture<void> Future;
		FEvent* CheckForWork;

		FFileManifestBuilder FileBuilder;
	};

	FManifestBuilderImpl::FManifestBuilderImpl(const FManifestDetails& InDetails, const FBuildStreamerRef& InBuildStreamer)
		: BuildStreamer(InBuildStreamer)
		, Manifest(MakeShareable(new FBuildPatchAppManifest()))
		, FileAttributesMap(InDetails.FileAttributesMap)
		, EndOfData(false)
		, CheckForWork(FPlatformProcess::GetSynchEventFromPool(true))
	{
		Manifest->Data->bIsFileData = InDetails.bIsFileData;
		Manifest->Data->AppID = InDetails.AppId;
		Manifest->Data->AppName = InDetails.AppName;
		Manifest->Data->BuildVersion = InDetails.BuildVersion;
		Manifest->Data->LaunchExe = InDetails.LaunchExe;
		Manifest->Data->LaunchCommand = InDetails.LaunchCommand;
		Manifest->Data->PrereqName = InDetails.PrereqName;
		Manifest->Data->PrereqPath = InDetails.PrereqPath;
		Manifest->Data->PrereqArgs = InDetails.PrereqArgs;
		for (const auto& CustomField : InDetails.CustomFields)
		{
			int32 VarType = CustomField.Value.GetType();
			if (VarType == EVariantTypes::Float || VarType == EVariantTypes::Double)
			{
				Manifest->SetCustomField(CustomField.Key, (double)CustomField.Value);
			}
			else if (VarType == EVariantTypes::Int8 || VarType == EVariantTypes::Int16 || VarType == EVariantTypes::Int32 || VarType == EVariantTypes::Int64 ||
				VarType == EVariantTypes::UInt8 || VarType == EVariantTypes::UInt16 || VarType == EVariantTypes::UInt32 || VarType == EVariantTypes::UInt64)
			{
				Manifest->SetCustomField(CustomField.Key, (int64)CustomField.Value);
			}
			else if (VarType == EVariantTypes::String)
			{
				Manifest->SetCustomField(CustomField.Key, CustomField.Value.GetValue<FString>());
			}
		}
		TFunction<void()> Task = [this]() { BuildManifest(); };
		Future = Async(EAsyncExecution::Thread, Task);
	}

	FManifestBuilderImpl::~FManifestBuilderImpl()
	{
		EndOfData = true;
		CheckForWork->Trigger();
		Future.Wait();
		FPlatformProcess::ReturnSynchEventToPool(CheckForWork);
		CheckForWork = nullptr;
	}

	void FManifestBuilderImpl::AddDataScanner(FDataScannerRef Scanner)
	{
		DataScannersCS.Lock();
		DataScanners.Add(MoveTemp(Scanner));
		DataScannersCS.Unlock();
		CheckForWork->Trigger();
	}

	void FManifestBuilderImpl::SaveToFile(const FString& Filename)
	{
		// Wait for builder to complete
		EndOfData = true;
		CheckForWork->Trigger();
		Future.Wait();

		Manifest->Data->ManifestFileVersion = EBuildPatchAppManifestVersion::GetLatestJsonVersion();
		Manifest->SaveToFile(Filename, false);
	}

	void FManifestBuilderImpl::BuildManifest()
	{
		TMap<FGuid, FChunkInfo> ChunkInfoLookup;
		bool Running = true;
		while (Running)
		{
			FDataScannerPtr NextScanner = GetNextScanner();
			if (NextScanner.IsValid())
			{
				FDataScanResult ScanResult = NextScanner->GetResultWhenComplete();
				ChunkInfoLookup.Append(ScanResult.ChunkInfo);

				// Always reverse for now
				if (ScanResult.DataStructure.Num() > 0)
				{
					FChunkPart& ChunkPart = ScanResult.DataStructure[0];
					if (ChunkPart.DataOffset != FileBuilder.CurrentDataPos)
					{
						check(ChunkPart.DataOffset < FileBuilder.CurrentDataPos); // Missing data!

						bool FoundPosition = false;
						uint64 DataCount = 0;
						for (int32 FileIdx = 0; FileIdx < Manifest->Data->FileManifestList.Num() && !FoundPosition; ++FileIdx)
						{
							FFileManifestData& FileManifest = Manifest->Data->FileManifestList[FileIdx];
							FileManifest.Init();
							uint64 FileStartIdx = DataCount;
							uint64 FileEndIdx = FileStartIdx + FileManifest.GetFileSize();
							if (FileEndIdx > ChunkPart.DataOffset)
							{
								for (int32 ChunkIdx = 0; ChunkIdx < FileManifest.FileChunkParts.Num() && !FoundPosition; ++ChunkIdx)
								{
									FChunkPartData& ChunkPartData = FileManifest.FileChunkParts[ChunkIdx];
									uint64 ChunkPartEndIdx = DataCount + ChunkPartData.Size;
									if (ChunkPartEndIdx < ChunkPart.DataOffset)
									{
										DataCount += ChunkPartData.Size;
									}
									else if (ChunkPartEndIdx > ChunkPart.DataOffset)
									{
										ChunkPartData.Size = ChunkPart.DataOffset - DataCount;
										FileBuilder.CurrentDataPos = DataCount + ChunkPartData.Size;
										FileManifest.FileChunkParts.SetNum(ChunkIdx + 1, false);
										FileManifest.FileChunkParts.Emplace();
										Manifest->Data->FileManifestList.SetNum(FileIdx + 1, false);
										FileBuilder.FileManifest = &Manifest->Data->FileManifestList.Last();
										bool FoundFile = BuildStreamer->GetFileSpan(FileStartIdx, FileBuilder.FileSpan);
										check(FoundFile); // Incorrect positional tracking
										FoundPosition = true;
									}
									else
									{
										FileBuilder.CurrentDataPos = DataCount + ChunkPartData.Size;
										FileManifest.FileChunkParts.SetNum(ChunkIdx + 1, false);
										FileManifest.FileChunkParts.Emplace();
										Manifest->Data->FileManifestList.SetNum(FileIdx + 1, false);
										FileBuilder.FileManifest = &Manifest->Data->FileManifestList.Last();
										bool FoundFile = BuildStreamer->GetFileSpan(FileStartIdx, FileBuilder.FileSpan);
										check(FoundFile); // Incorrect positional tracking
										FoundPosition = true;
									}
								}
							}
							else if (FileEndIdx < ChunkPart.DataOffset)
							{
								DataCount += FileManifest.GetFileSize();
							}
							else
							{
								FileBuilder.FileManifest = nullptr;
								FileBuilder.CurrentDataPos = DataCount + FileManifest.GetFileSize();
								Manifest->Data->FileManifestList.SetNum(FileIdx + 1, false);
								FoundPosition = true;
							}
						}

						check(ChunkPart.DataOffset == FileBuilder.CurrentDataPos);
						check(FileBuilder.FileManifest == nullptr || FileBuilder.FileSpan.Filename == Manifest->Data->FileManifestList.Last().Filename);
					}
				}

				for (int32 idx = 0; idx < ScanResult.DataStructure.Num(); ++idx)
				{
					FChunkPart& ChunkPart = ScanResult.DataStructure[idx];
					// Starting new file?
					if (FileBuilder.FileManifest == nullptr)
					{
						Manifest->Data->FileManifestList.Emplace();
						FileBuilder.FileManifest = &Manifest->Data->FileManifestList.Last();

						bool FoundFile = BuildStreamer->GetFileSpan(FileBuilder.CurrentDataPos, FileBuilder.FileSpan);
						check(FoundFile); // Incorrect positional tracking

						FileBuilder.FileManifest->Filename = FileBuilder.FileSpan.Filename;
						FileBuilder.FileManifest->FileChunkParts.Emplace();
					}

					FChunkPartData& FileChunkPartData = FileBuilder.FileManifest->FileChunkParts.Last();
					FileChunkPartData.Guid = ChunkPart.ChunkGuid;
					FileChunkPartData.Offset = (FileBuilder.CurrentDataPos - ChunkPart.DataOffset) + ChunkPart.ChunkOffset;

					// Process data into file manifests
					int64 FileDataLeft = (FileBuilder.FileSpan.StartIdx + FileBuilder.FileSpan.Size) - FileBuilder.CurrentDataPos;
					int64 ChunkDataLeft = (ChunkPart.DataOffset + ChunkPart.PartSize) - FileBuilder.CurrentDataPos;
					check(FileDataLeft > 0);
					check(ChunkDataLeft > 0);

					if (ChunkDataLeft >= FileDataLeft)
					{
						FileBuilder.CurrentDataPos += FileDataLeft;
						FileChunkPartData.Size = FileDataLeft;
					}
					else
					{
						FileBuilder.CurrentDataPos += ChunkDataLeft;
						FileChunkPartData.Size = ChunkDataLeft;
					}

					FileDataLeft = (FileBuilder.FileSpan.StartIdx + FileBuilder.FileSpan.Size) - FileBuilder.CurrentDataPos;
					ChunkDataLeft = (ChunkPart.DataOffset + ChunkPart.PartSize) - FileBuilder.CurrentDataPos;
					check(FileDataLeft == 0 || ChunkDataLeft == 0);
					// End of file?
					if (FileDataLeft == 0)
					{
						// Fill out rest of data??
						FFileSpan FileSpan;
						bool FoundFile = BuildStreamer->GetFileSpan(FileBuilder.FileSpan.StartIdx, FileSpan);
						check(FoundFile); // Incorrect positional tracking
						check(FileSpan.Filename == FileBuilder.FileManifest->Filename);
						FMemory::Memcpy(FileBuilder.FileManifest->FileHash.Hash, FileSpan.SHAHash.Hash, FSHA1::DigestSize);
						FFileAttributes Attributes = FileAttributesMap.FindRef(FileSpan.Filename);
						FileBuilder.FileManifest->bIsUnixExecutable = Attributes.bUnixExecutable || FileSpan.IsUnixExecutable;
						FileBuilder.FileManifest->SymlinkTarget = FileSpan.SymlinkTarget;
						FileBuilder.FileManifest->bIsReadOnly = Attributes.bReadOnly;
						FileBuilder.FileManifest->bIsCompressed = Attributes.bCompressed;
						FileBuilder.FileManifest->InstallTags = Attributes.InstallTags.Array();
						FileBuilder.FileManifest->Init();
						check(FileBuilder.FileManifest->GetFileSize() == FileBuilder.FileSpan.Size);
						FileBuilder.FileManifest = nullptr;
					}
					else if (ChunkDataLeft == 0)
					{
						FileBuilder.FileManifest->FileChunkParts.Emplace();
					}

					// Continue with this chunk?
					if (ChunkDataLeft > 0)
					{
						--idx;
					}
				}
			}
			else
			{
				if (EndOfData)
				{
					Running = false;
				}
				else
				{
					CheckForWork->Wait();
					CheckForWork->Reset();
				}
			}
		}

		// Fill out chunk list from only chunks that remain referenced
		TSet<FGuid> ReferencedChunks;
		for (const auto& FileManifest : Manifest->Data->FileManifestList)
		{
			for (const auto& ChunkPart : FileManifest.FileChunkParts)
			{
				if (ReferencedChunks.Contains(ChunkPart.Guid) == false)
				{
					auto& ChunkInfo = ChunkInfoLookup[ChunkPart.Guid];
					ReferencedChunks.Add(ChunkPart.Guid);
					Manifest->Data->ChunkList.Emplace();
					auto& ChunkInfoData = Manifest->Data->ChunkList.Last();
					ChunkInfoData.Guid = ChunkPart.Guid;
					ChunkInfoData.Hash = ChunkInfo.Hash;
					FMemory::Memcpy(ChunkInfoData.ShaHash.Hash, ChunkInfo.ShaHash.Hash, FSHA1::DigestSize);
					ChunkInfoData.FileSize = ChunkInfo.ChunkFileSize;
					ChunkInfoData.GroupNumber = FCrc::MemCrc32(&ChunkPart.Guid, sizeof(FGuid)) % 100;
				}
			}
		}

		// Get empty files
		FSHA1 EmptyHasher;
		EmptyHasher.Final();
		const TArray< FString >& EmptyFileList = BuildStreamer->GetEmptyFiles();
		for (const auto& EmptyFile : EmptyFileList)
		{
			Manifest->Data->FileManifestList.Emplace();
			FFileManifestData& EmptyFileManifest = Manifest->Data->FileManifestList.Last();
			EmptyFileManifest.Filename = EmptyFile;
			EmptyHasher.GetHash(EmptyFileManifest.FileHash.Hash);
		}

		// Fill out lookups
		Manifest->InitLookups();
	}

	FDataScannerPtr FManifestBuilderImpl::GetNextScanner()
	{
		FDataScannerPtr Result;
		DataScannersCS.Lock();
		if (DataScanners.Num() > 0)
		{
			Result = DataScanners[0];
			DataScanners.RemoveAt(0);
		}
		DataScannersCS.Unlock();
		return Result;
	}

	FManifestBuilderRef FManifestBuilderFactory::Create(const FManifestDetails& Details, const FBuildStreamerRef& BuildStreamer)
	{
		return MakeShareable(new FManifestBuilderImpl(Details, BuildStreamer));
	}
}

#endif
