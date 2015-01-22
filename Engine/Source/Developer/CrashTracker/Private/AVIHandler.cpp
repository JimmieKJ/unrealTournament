// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashTrackerPrivatePCH.h"


// RIFF specific data structures: Lists and Chunks
template <typename DataType>
struct FRIFFList
{
	uint8 ID[4];
	uint32 Size;
	uint8 Type[4];
	DataType Data;

	FRIFFList(const TCHAR* InType, const DataType& InData)
		: Size(sizeof(Type) + sizeof(Data))
		, Data(InData)
	{
		ID[0] = 'L';
		ID[1] = 'I';
		ID[2] = 'S';
		ID[3] = 'T';
		for (uint32 i = 0; i < 4; ++i)
		{
			Type[i] = InType[i];
		}
	}
};


template <>
struct FRIFFList<void>
{
	uint8 ID[4];
	uint32 Size;
	uint8 Type[4];

	FRIFFList(const TCHAR* InType, uint32 InSize)
		: Size(sizeof(Type) + InSize)
	{
		ID[0] = 'L';
		ID[1] = 'I';
		ID[2] = 'S';
		ID[3] = 'T';
		for (uint32 i = 0; i < 4; ++i)
		{
			Type[i] = InType[i];
		}
	}
};


template <typename DataType>
struct FRIFFChunk
{
	uint8 ID[4];
	uint32 Size;
	DataType Data;

	FRIFFChunk(const TCHAR* InType, const DataType& InData)
		: Size(sizeof(Data))
		, Data(InData)
	{
		for (uint32 i = 0; i < 4; ++i)
		{
			ID[i] = InType[i];
		}
	}
};


template <>
struct FRIFFChunk<void>
{
	uint8 ID[4];
	uint32 Size;

	FRIFFChunk(const TCHAR* InType, uint32 InSize)
		: Size(InSize)
	{
		for (uint32 i = 0; i < 4; ++i)
		{
			ID[i] = InType[i];
		}
	}
};


// AVI Specific data structures for the RIFF AVI header
#define AVI_HAS_INDEX 0x00000010

struct FAVIMainHeader
{
	uint32 MicrosecondsPerFrame;
	uint32 MaxBytesPerSec;
	uint32 PaddingGranularity;
	uint32 Flags;
	uint32 TotalFrames;
	uint32 InitialFrames;
	uint32 Streams;
	uint32 SuggestedBufferSize;
	uint32 Width;
	uint32 Height;
	uint32 Reserved[4];

	FAVIMainHeader(uint32 InFrames, uint32 InWidth, uint32 InHeight, uint32 InMicrosecondsPerFrame, uint32 JPEGSize)
		: MicrosecondsPerFrame(InMicrosecondsPerFrame)
		, MaxBytesPerSec(1000000 * (JPEGSize / InFrames) / InMicrosecondsPerFrame)
		, PaddingGranularity(0)
		, Flags(AVI_HAS_INDEX)
		, TotalFrames(InFrames)
		, InitialFrames(0)
		, Streams(1)
		, SuggestedBufferSize(0)
		, Width(InWidth)
		, Height(InHeight)
	{
		for (uint32 i = 0; i < 4; ++i) {Reserved[i] = 0;}
	}
};


struct FAVIStreamHeader
{
	uint8 Type[4];
	uint8 Handler[4];
	uint32 Flags;
	uint16 Priority;
	uint16 Language;
	uint32 InitialFrames;
	uint32 Scale;
	uint32 Rate;
	uint32 Start;
	uint32 Length;
	uint32 SuggestedBufferSize;
	uint32 Quality;
	uint32 SampleSize;
	uint16 Left;
	uint16 Top;
	uint16 Right;
	uint16 Bottom;

	FAVIStreamHeader(uint32 InWidth, uint32 InHeight, uint32 InFrames, uint32 InMicrosecondsPerFrame)
		: Flags(0)
		, Priority(0)
		, Language(0)
		, InitialFrames(0)
		, Scale(InMicrosecondsPerFrame)
		, Rate(1000000)
		, Start(0)
		, Length(InFrames)
		, SuggestedBufferSize(0)
		, Quality(0)
		, SampleSize(0)
		, Left(0)
		, Top(0)
		, Right(InWidth)
		, Bottom(InHeight)
	{
		Type[0] = 'v';
		Type[1] = 'i';
		Type[2] = 'd';
		Type[3] = 's';
		Handler[0] = 'M';
		Handler[1] = 'J';
		Handler[2] = 'P';
		Handler[3] = 'G';
	}
};


struct FAVIStreamFormat
{
	uint32 BufferSize;
	uint32 Width;
	uint32 Height;
	uint16 Planes;
	uint16 BitCount;
	uint8 Compression[4];
	uint32 ImageSize;
	uint32 XPixelsPerMeter;
	uint32 YPixelsPerMeter;
	uint32 UsedColors;
	uint32 ImportantColors;

	FAVIStreamFormat(uint32 InWidth, uint32 InHeight)
		: BufferSize(sizeof(FAVIStreamFormat))
		, Width(InWidth)
		, Height(InHeight)
		, Planes(1)
		, BitCount(24)
		, ImageSize(InWidth * InHeight * 3)
		, XPixelsPerMeter(0)
		, YPixelsPerMeter(0)
		, UsedColors(0)
		, ImportantColors(0)
	{
		Compression[0] = 'M';
		Compression[1] = 'J';
		Compression[2] = 'P';
		Compression[3] = 'G';
	}
};


struct FAVIListStream
{
	FRIFFChunk<FAVIStreamHeader> StreamHeader;
	FRIFFChunk<FAVIStreamFormat> StreamFormat;

	FAVIListStream(uint32 InFrames, uint32 InWidth, uint32 InHeight, uint32 InMicrosecondsPerFrame)
		: StreamHeader(TEXT("strh"), FAVIStreamHeader(InWidth, InHeight, InFrames, InMicrosecondsPerFrame))
		, StreamFormat(TEXT("strf"), FAVIStreamFormat(InWidth, InHeight))
	{
	}
};


struct FAVIListHeader
{
	FRIFFChunk<FAVIMainHeader> MainHeader;
	FRIFFList<FAVIListStream> StreamList;

	FAVIListHeader(uint32 InFrames, uint32 InWidth, uint32 InHeight, uint32 InMicrosecondsPerFrame, uint32 JPEGSize)
		: MainHeader(TEXT("avih"), FAVIMainHeader(InFrames, InWidth, InHeight, InMicrosecondsPerFrame, JPEGSize))
		, StreamList(TEXT("strl"), FAVIListStream(InFrames, InWidth, InHeight, InMicrosecondsPerFrame))
	{
	}
};


void DumpOutMJPEGAVI(const TArray<FCompressedDataFrame*>& CompressedFrames, FString OutputPath, int32 Width, int32 Height, int32 FPS)
{
	int32 FrameCount = CompressedFrames.Num();
	int32 MicrosecondsPerFrame = 1000000 / FPS;

	// pad out files with data first
	int32 TotalJPEGSize = 0;
	for (int32 i = 0; i < FrameCount; ++i)
	{
		FCompressedDataFrame* DataFrame = CompressedFrames[i];

		check(DataFrame && DataFrame->Data && DataFrame->ActualSize <= DataFrame->BufferSize);

		int32 Padding = FMath::Min(DataFrame->BufferSize - DataFrame->ActualSize,
			(4 - (DataFrame->ActualSize % 4)) % 4);
		int32 TotalSize = DataFrame->ActualSize + Padding;
		FMemory::Memset(DataFrame->Data + DataFrame->ActualSize, 0, Padding);
		DataFrame->ActualSize = TotalSize;

		TotalJPEGSize += DataFrame->ActualSize;
	}

	// write out the avi file
	FArchive* Ar = IFileManager::Get().CreateFileWriter(*OutputPath, FILEWRITE_EvenIfReadOnly);

	if(Ar)
	{
		uint32 FileSize = 4 +
			sizeof(FRIFFList<FAVIListHeader>) +
			sizeof(FRIFFList<void>) + TotalJPEGSize + 8 * FrameCount +
			sizeof(FRIFFChunk<void>) + 16 * FrameCount;
		Ar->Serialize((void*)"RIFF", 4);
		Ar->Serialize(&FileSize, sizeof(FileSize));
		Ar->Serialize((void*)"AVI ", 4);

		FRIFFList<FAVIListHeader> ListHeader = FRIFFList<FAVIListHeader>(TEXT("hdrl"), FAVIListHeader(FrameCount, Width, Height, MicrosecondsPerFrame, TotalJPEGSize));
		Ar->Serialize(&ListHeader, sizeof(ListHeader));

		FRIFFList<void> MoviList = FRIFFList<void>(TEXT("movi"), TotalJPEGSize + 8 * FrameCount);
		Ar->Serialize(&MoviList, sizeof(MoviList));
		for (int32 i = 0; i < FrameCount; ++i)
		{
			FCompressedDataFrame* DataFrame = CompressedFrames[i];

			FRIFFChunk<void> DataChunk = FRIFFChunk<void>(TEXT("00dc"), DataFrame->ActualSize);
			Ar->Serialize(&DataChunk, sizeof(DataChunk));

			// replace bytes 7-10 (JFIF) with AVI1
			Ar->Serialize(DataFrame->Data, 6);
			Ar->Serialize((void*)"AVI1", 4);
			Ar->Serialize(DataFrame->Data + 10, DataFrame->ActualSize - 10);
		}

		FRIFFChunk<void> Idx1Chunk = FRIFFChunk<void>(TEXT("idx1"), FrameCount * 16);
		Ar->Serialize(&Idx1Chunk, sizeof(Idx1Chunk));
		int32 PreviousOffset = 0;
		for (int32 i = 0; i < FrameCount; ++i)
		{
			FCompressedDataFrame* DataFrame = CompressedFrames[i];

			int32 CurrentOffset = 4;
			if (i > 0)
			{
				FCompressedDataFrame* PreviousFrame = CompressedFrames[i - 1];
				CurrentOffset = PreviousOffset + PreviousFrame->ActualSize + 8;
			}

			int Flags = 0;
			Ar->Serialize((void*)"00dc", 4);
			Ar->Serialize(&Flags, sizeof(Flags));
			Ar->Serialize(&CurrentOffset, sizeof(CurrentOffset));
			Ar->Serialize(&DataFrame->ActualSize, sizeof(DataFrame->ActualSize));

			PreviousOffset = CurrentOffset;
		}

		delete Ar;
	}
}
