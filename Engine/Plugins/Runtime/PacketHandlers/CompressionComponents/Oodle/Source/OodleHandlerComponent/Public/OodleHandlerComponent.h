// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// @todo #JohnB: Separate module-based header code, from other class implementations, so that you can setup the PCH.h file correctly
#include "PacketHandler.h"
#include "ModuleManager.h"
#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(OodleHandlerComponentLog, Log, All);


/**
 * Whether or not to enable Oodle dev code (packet capturing, dictionary training, and automatic dictionary finding) in shipping mode.
 *
 * This may be useful for multiplayer game mod authors, to optimize netcode compression for their mod (not officially supported).
 * However, Oodle compression makes the games network protocol harder to reverse-engineer - enabling this removes that slight benefit.
 */
#define OODLE_DEV_SHIPPING	FALSE


#if HAS_OODLE_SDK
#include "OodleArchives.h"

#if UE4_OODLE_VER >= 200
#include "oodle2.h"
#else
#include "oodle.h"
#endif


#define CAPTURE_EXT TEXT(".ucap")


/** Stats */

#if STATS

// @todo #JohnB: Deprecate Oodle in 'stat net' stats, and rely only on 'stat oodle'?
#define OODLE_STAT_NET 0

#if OODLE_STAT_NET
// NOTE: These stats can not be compared to normal 'stat net' stats - these stats do not count packet overhead, 'stat net' does
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Raw (bytes)"), STAT_Net_Oodle_OutRaw, STATGROUP_Net, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Compressed (bytes)"), STAT_Net_Oodle_OutCompressed, STATGROUP_Net, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Raw (bytes)"), STAT_Net_Oodle_InRaw, STATGROUP_Net, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Compressed (bytes)"), STAT_Net_Oodle_InCompressed, STATGROUP_Net, );
#endif


DECLARE_STATS_GROUP(TEXT("Oodle"), STATGROUP_Oodle, STATCAT_Advanced)

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Raw (bytes)"), STAT_Oodle_OutRaw, STATGROUP_Oodle, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Compressed (bytes)"), STAT_Oodle_OutCompressed, STATGROUP_Oodle, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Savings %"), STAT_Oodle_OutSavings, STATGROUP_Oodle, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Total Savings %"), STAT_Oodle_OutTotalSavings, STATGROUP_Oodle, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Raw (bytes)"), STAT_Oodle_InRaw, STATGROUP_Oodle, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Compressed (bytes)"), STAT_Oodle_InCompressed, STATGROUP_Oodle, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Savings %"), STAT_Oodle_InSavings, STATGROUP_Oodle, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Total Savings %"), STAT_Oodle_InTotalSavings, STATGROUP_Oodle, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Packet Overhead (bits)"), STAT_Oodle_PacketOverhead, STATGROUP_Oodle, );

#if !UE_BUILD_SHIPPING
DECLARE_CYCLE_STAT_EXTERN(TEXT("Oodle Out Compress Time"), STAT_Oodle_OutCompressTime, STATGROUP_Oodle, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Oodle In Decompress Time"), STAT_Oodle_InDecompressTime, STATGROUP_Oodle, );
#endif

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Dictionary Count"), STAT_Oodle_DictionaryCount, STATGROUP_Oodle, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle Dictionary Bytes"), STAT_Oodle_DictionaryBytes, STATGROUP_Oodle, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle Shared Bytes"), STAT_Oodle_SharedBytes, STATGROUP_Oodle, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle State Bytes"), STAT_Oodle_StateBytes, STATGROUP_Oodle, );
#endif


/** Globals */

/** The directory Oodle packet captures are saved to */
extern FString GOodleSaveDir;

/** The directory Oodle dictionaries are saved/loaded to/from */
extern FString GOodleContentDir;


#if STATS
// @todo #JohnB: The stats collecting is a bit crude, and should probably be giving per-dictionary stats

/**
 * Stores Oodle net traffic stats, accumulated over the past second, before passing it to the stats system
 */
class FOodleNetStats
{
private:
	/** Accumulated stats since last update */

	/** Input traffic compressed packet length */
	uint32 InCompressedLength;

	/** Input traffic decompressed packet length */
	uint32 InDecompressedLength;

	/** Output traffic compressed packet length */
	uint32 OutCompressedLength;

	/** Output traffic uncompressed packet length */
	uint32 OutUncompressedLength;


	/** Time of the last stats update */
	double LastStatsUpdate;


	/** Process lifetime stats */

	/** Total compressed input packets length */
	uint64 TotalInCompressedLength;

	/** Total decompressed input packets length */
	uint64 TotalInDecompressedLength;

	/** Total compressed output packets length */
	uint64 TotalOutCompressedLength;

	/** Total uncompressed output packets length */
	uint64 TotalOutUncompressedLength;

public:
	/**
	 * Base constructor
	 */
	FOodleNetStats()
		: InCompressedLength(0)
		, InDecompressedLength(0)
		, OutCompressedLength(0)
		, OutUncompressedLength(0)
		, LastStatsUpdate(0.0)
		, TotalInCompressedLength(0)
		, TotalInDecompressedLength(0)
		, TotalOutCompressedLength(0)
		, TotalOutUncompressedLength(0)
	{
	}

	/**
	 * Process incoming packet stats
	 *
	 * @param CompressedLength		The compressed size of the input packet
	 * @param DecompressedLength	The decompressed size of the input packet
	 */
	FORCEINLINE void IncomingStats(uint32 CompressedLength, uint32 DecompressedLength)
	{
		InCompressedLength += CompressedLength;
		TotalInCompressedLength += CompressedLength;
		InDecompressedLength += DecompressedLength;
		TotalInDecompressedLength += DecompressedLength;

		CheckForUpdate();
	}

	/**
	 * Process outgoing packet stats
	 *
	 * @param CompressedLength		The compressed size of the output packet
	 * @param UncompressedLength	The uncompressed size of the output packets
	 */
	FORCEINLINE void OutgoingStats(uint32 CompressedLength, uint32 UncompressedLength)
	{
		OutCompressedLength += CompressedLength;
		TotalOutCompressedLength += CompressedLength;
		OutUncompressedLength += UncompressedLength;
		TotalOutUncompressedLength += UncompressedLength;

		CheckForUpdate();
	}

	/**
	 * Checks to see if the main stats are due an update, and triggers an update if so
	 */
	FORCEINLINE void CheckForUpdate()
	{
		float DeltaTime = FPlatformTime::Seconds() - LastStatsUpdate;

		if (DeltaTime > 1.f)
		{
			UpdateStats(DeltaTime);
			LastStatsUpdate = FPlatformTime::Seconds();
		}
	}

	/**
	 * Passes up the accumulated stats, to the main engine stats tracking
	 *
	 * @param DeltaTime		The exact time since last stats update
	 */
	void UpdateStats(float DeltaTime);
};
#endif


/**
 * The mode that the Oodle packet handler should operate in
 */
enum EOodleHandlerMode
{
	Capturing,	// Stores packet captures for the server
	Release		// Compresses packet data, based on the dictionary file
};

/**
 * Encapsulates Oodle dictionary data loaded from file, to be wrapped in a shared pointer (auto-deleting when no longer in use)
 */
struct FOodleDictionary
{
	/** Size of the hash table used for the dictionary */
	uint32 HashTableSize;

	/** The raw dictionary data */
	uint8* DictionaryData;

	/** The size of the dictionary */
	uint32 DictionarySize;

	/** Shared dictionary state */
	OodleNetwork1_Shared* SharedDictionary;

	/** The size of the shared dictionary data (stored only for memory accounting) */
	uint32 SharedDictionarySize;

	/** The uncompacted compressor state */
	OodleNetwork1UDP_State* CompressorState;

	/** The size of CompressorState */
	uint32 CompressorStateSize;


private:
	FOodleDictionary()
	{
	}

	FOodleDictionary(const FOodleDictionary&) = delete;
	FOodleDictionary& operator=(const FOodleDictionary&) = delete;

public:

	/**
	 * Base constructor
	 */
	FOodleDictionary(uint32 InHashTableSize, uint8* InDictionaryData, uint32 InDictionarySize, OodleNetwork1_Shared* InSharedDictionary,
						uint32 InSharedDictionarySize, OodleNetwork1UDP_State* InInitialCompressorState, uint32 InCompressorStateSize);

	/**
	 * Base destructor
	 */
	~FOodleDictionary();
};


/**
 * PacketHandler component for implementing Oodle support.
 *
 * Implementation uses trained/dictionary-based UDP compression.
 */
class OODLEHANDLERCOMPONENT_API OodleHandlerComponent : public HandlerComponent
{
public:
	/** Initializes default data */
	OodleHandlerComponent();

	/** Default Destructor */
	~OodleHandlerComponent();

	/**
	 * Initializes first-run config settings
	 */
	static void InitFirstRunConfig();


	/**
	 * Initializes FOodleDictionary data, from the specified dictionary file
	 *
	 * @param FilePath			The dictionary file path
	 * @param OutDictionary		The FOodleDictionary shared pointer to write to
	 */
	void InitializeDictionary(FString FilePath, TSharedPtr<FOodleDictionary>& OutDictionary);

	/**
	 * Frees the local reference to FOodleDictionary data, and removes it from memory if it was the last reference
	 *
	 * @param InDictionary		The FOodleDictionary shared pointer being freed
	 */
	void FreeDictionary(TSharedPtr<FOodleDictionary>& InDictionary);

	/**
	 * Resolves and returns the default dictionary file paths.
	 *
	 * @param OutServerDictionary	The server dictionary path
	 * @param OutClientDictionary	The client dictionary path
	 * @param bFailFatal			Whether or not failure to set the dictionary paths, should be fatal
	 * @return						Whether or not the dictionary paths were successfully set
	 */
	bool GetDictionaryPaths(FString& OutServerDictionary, FString& OutClientDictionary, bool bFailFatal=true);

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	/**
	 * Searches the game directory for alternate/fallback dictionary files, using the *.udic file extension.
	 * NOTE: This is non-shipping-only, as release games MUST have well-determined dictionary files (for net-binary-compatibility)
	 *
	 * @param OutServerDictionary	The server dictionary path
	 * @param OutClientDictionary	The client dictionary path
	 * @param bTestOnly				Whether this is being used to test for presence of alternate dictionaries (disables some logging)
	 * @return						Whether or not alternate dictionaries were found
	 */
	bool FindFallbackDictionaries(FString& OutServerDictionary, FString& OutClientDictionary, bool bTestOnly=false);
#endif

	virtual void Initialize() override;

	virtual bool IsValid() const override;

	virtual void Incoming(FBitReader& Packet) override;

	virtual void Outgoing(FBitWriter& Packet) override;

	virtual void IncomingConnectionless(FString Address, FBitReader& Packet) override
	{
	}

	virtual void OutgoingConnectionless(FString Address, FBitWriter& Packet) override
	{
	}

	virtual int32 GetPacketOverheadBits() override;

	virtual uint8 GetBitAlignment() override;

	virtual bool DoesResetBitAlignment() override;


protected:
	/** Whether or not Oodle is enabled */
	bool bEnableOodle;

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	/** File to log input packets to */
	FPacketCaptureArchive* InPacketLog;

	/** File to log output packets to */
	FPacketCaptureArchive* OutPacketLog;

	/** Search for dictionary files and use them if present - switching mode to Release in process - don't use in shipping */
	bool bUseDictionaryIfPresent;
#endif

	/** Modes of the component  */
	EOodleHandlerMode Mode;

	/** Server (Outgoing) dictionary data */
	TSharedPtr<FOodleDictionary> ServerDictionary;

	/** Client (Incoming - relative to server) dictionary data */
	TSharedPtr<FOodleDictionary> ClientDictionary;
};
#endif


/**
 * Oodle Module Interface
 */
class FOodleComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
private:
	/** Reference to the Oodle library handle */
	void* OodleDllHandle;

public:
	FOodleComponentModuleInterface()
		: OodleDllHandle(NULL)
	{
	}

	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
