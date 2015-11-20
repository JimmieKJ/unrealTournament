// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "OodleTrainerCommandlet.generated.h"


/**
 * Commandlet for processing UE4 packet captures, through Oodle's training API, for generating compressed state dictionaries.
 *
 * Commands:
 *	- "Enable":
 *		- Inserts the Oodle PacketHandler into the games packet handler component list, and initializes Oodle *Engine.ini settings
 *
 *
 *	- "MergePackets OutputFile PacketFile1,PacketFile2,PacketFileN":
 *		- Takes the specified packet capture files, and merges them into a single packet capture file
 *
 *	- "MergePackets OutputFile All Directory":
 *		- As above, but merges all capture files in the specified directory.
 *
 *
 * @todo #JohnB: Unimplemented commands:
 *	- "GenerateDictionary OutputFile PacketFile"
 *		- Takes the specified packet capture file, and processes it into an dictionary state file, used for compressing packet data.
 *		- The packet file (usually consisting of many merged packet files), should be at least 100MB in size (see Oodle docs)
 *		- @todo #JohnB: Implement
 *
 *
 *	- "PacketInfo PacketFile":
 *		- Outputs information about the packet file, such as the MB amount of data recorded, per net connection channel, and data types
 *		- @todo #JohnB: Only implement, if deciding to actually capture/track this kind of data
 */
UCLASS()
class UOodleTrainerCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};


