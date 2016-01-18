// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OodleHandlerComponentPCH.h"

#if HAS_OODLE_SDK
#include "OodleArchives.h"


/**
 * FPacketCaptureArchive
 */

void FPacketCaptureArchive::SerializeCaptureHeader()
{
	uint32 Magic = CAPTURE_HEADER_MAGIC;

	InnerArchive << Magic;
	InnerArchive << CaptureVersion;

	if (IsLoading())
	{
		check(Magic == CAPTURE_HEADER_MAGIC);
		check(CaptureVersion <= CAPTURE_FILE_VERSION);
	}
	else if (IsSaving() && bImmediateFlush)
	{
		Flush();
	}
}

void FPacketCaptureArchive::SerializePacket(void* PacketData, uint32& PacketSize)
{
	uint32 PacketBufferSize = (IsLoading() ? PacketSize : 0);
	InnerArchive << PacketSize;

	if (IsLoading())
	{
		// Max 128MB packet - excessive, but this is not meant to be a perfect security check
		if (PacketBufferSize < PacketSize || !ensure(PacketSize <= 134217728))
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Bad PacketSize value '%i' in loading packet capture file"), PacketSize);
			SetError();

			return;
		}

		if (PacketSize > (InnerArchive.TotalSize() - InnerArchive.Tell()))
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("PacketSize '%i' greater than remaining file data '%i'. Truncated file? ")
					TEXT("(run server with -forcelogflush to reduce chance of truncated capture files)"),
					PacketSize, (InnerArchive.TotalSize() - InnerArchive.Tell()));

			SetError();

			return;
		}
	}

	InnerArchive.Serialize(PacketData, PacketSize);

	if (IsSaving() && bImmediateFlush)
	{
		Flush();
	}
}

void FPacketCaptureArchive::AppendPacketFile(FPacketCaptureArchive& InPacketFile)
{
	check(IsSaving());
	check(Tell() != 0);	// Can't append a packet before writing the header
	check(InPacketFile.IsLoading());
	check(InPacketFile.Tell() == 0);

	// Read past the header
	InPacketFile.SerializeCaptureHeader();

	check(CaptureVersion == InPacketFile.CaptureVersion);

	// For appending, only support 1MB packets
	const uint32 BufferSize = 1048576;
	uint8* ReadBuffer = new uint8[BufferSize];

	// Iterate through all packets
	while (InPacketFile.Tell() < InPacketFile.TotalSize())
	{
		uint32 PacketSize = BufferSize;

		InPacketFile.SerializePacket((void*)ReadBuffer, PacketSize);

		if (InPacketFile.IsError())
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Error reading packet capture data. Skipping rest of file."));

			break;
		}


		SerializePacket(ReadBuffer, PacketSize);
	}

	delete[] ReadBuffer;


	if (IsSaving() && bImmediateFlush)
	{
		Flush();
	}
}
#endif

