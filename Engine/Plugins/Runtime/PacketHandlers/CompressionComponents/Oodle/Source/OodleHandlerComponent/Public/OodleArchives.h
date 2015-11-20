// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if HAS_OODLE_SDK
#include "ArchiveBase.h"

#define CAPTURE_HEADER_MAGIC	0x41091CC4
#define CAPTURE_FILE_VERSION	0x00000001

#define DICTIONARY_HEADER_MAGIC	0x1B1BACD4
#define DICTIONARY_FILE_VERSION	0x00000001


// @todo #JohnB: Add to the header, whether the capture file is an input or output capture type,
//					and use that to give a warning when merging mismatched input/output capture files
//					(actually, make that error by default, requiring a special -blah commandline parm, to override the error)


/**
 * @todo #JohnB
 */
class FOodleArchiveBase : public FArchiveProxy
{
protected:
	/** Whether or not to flush immediately */
	bool bImmediateFlush;

public:
	/**
	 * Base constructor
	 */
	FOodleArchiveBase(FArchive& InInnerArchive)
		: FArchiveProxy(InInnerArchive)
	{
		bImmediateFlush = FParse::Param(FCommandLine::Get(), TEXT("FORCELOGFLUSH"));
	}

	/**
	 * Deletes the inner archive
	 */
	void DeleteInnerArchive()
	{
		delete (FArchive*)&InnerArchive;
	}
};


/**
 * Archive for handling packet capture (.ucap) files
 */
class FPacketCaptureArchive : public FOodleArchiveBase
{
protected:
	/** Capture file format version */
	uint32 CaptureVersion;

public:
	/**
	 * Base constructor
	 */
	FPacketCaptureArchive(FArchive& InInnerArchive)
		: FOodleArchiveBase(InInnerArchive)
	{
		if (IsSaving())
		{
			CaptureVersion = CAPTURE_FILE_VERSION;
		}
		else
		{
			CaptureVersion = 0;
		}
	}


	/**
	 * Serializes the file header, containing the file format UID (magic) and file version
	 */
	void SerializeCaptureHeader();

	/**
	 * Serialize an individual packet to/from the archive.
	 * It is possible for there to be an incomplete packet stored - in which case, attempting to read will set the archives error mode
	 *
	 * @param PacketData	The location of/for the packet data
	 * @param PacketSize	For saving, the size of the packet, for loading, inputs the size of the buffer, and outputs the packet size
	 */
	void SerializePacket(void* PacketData, uint32& PacketSize);


	/**
	 * Used for merging multiple packet files. Appends the specified packet file to this one.
	 *
	 * @param InPacketFile	The packet file archive, to append to this one.
	 */
	void AppendPacketFile(FPacketCaptureArchive& InPacketFile);
};


// @todo #JohnB: Remove once complete
/*
	Values to include in header:
		- Dictionary size (as relating to Oodle API methods)
		- Hash table size (also relating to Oodle API)
*/

/**
 * @todo #JohnB
 */
class FOodleDictionaryArchive : public FOodleArchiveBase
{
protected:
	/** Dictionary file format version */
	uint32 DictionaryVersion;

public:
	/**
	 * Base constructor
	 */
	FOodleDictionaryArchive(FArchive& InInnerArchive)
		: FOodleArchiveBase(InInnerArchive)
	{
	}
};

#endif



