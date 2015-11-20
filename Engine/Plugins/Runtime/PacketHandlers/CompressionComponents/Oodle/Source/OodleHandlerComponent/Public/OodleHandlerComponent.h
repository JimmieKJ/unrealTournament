// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// @todo #JohnB: Separate module-based header code, from other class implementations, so that you can setup the PCH.h file correctly
#include "PacketHandler.h"
#include "ModuleManager.h"
#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(OodleHandlerComponentLog, Log, All);


#if HAS_OODLE_SDK
#include "OodleArchives.h"

#if UE4_OODLE_VER >= 200
#include "oodle2.h"
#else
#include "oodle.h"
#endif


// Whether or not to utilize the Oodle-example-code based training and packet capture code (in the process of deprecation)
#define EXAMPLE_CAPTURE_FORMAT 0

#if EXAMPLE_CAPTURE_FORMAT
	#define CAPTURE_EXT TEXT(".bin")
#else
	#define CAPTURE_EXT TEXT(".ucap")
#endif


/** Globals */
extern FString GOodleSaveDir;


/**
 * The mode that the Oodle packet handler should operate in
 */
enum EOodleHandlerMode
{
	Training,	// Stores packet captures for the server
	Release		// Compresses packet data, based on the dictionary file
};


/**
 * Stores dictionary data from an individual file (typically one dictionary each, for server/client)
 *
 * @todo #JohnB: In its current form, this is temporary/stopgap, based on the example code
 */
struct FOodleDictionary
{
	/** Base constructor */
	FOodleDictionary()
		: Dictionary(NULL)
		, SharedDictionaryData(NULL)
		, CompressorState(NULL)
	{
	}

	/* Dictionary */
	void* Dictionary; 

	/* Shared Compressor */
	OodleNetwork1_Shared* SharedDictionaryData;

	/* State of Oodle */
	OodleNetwork1UDP_State* CompressorState;
};


/**
 * PacketHandler component for implementing Oodle support.
 *
 * Implementation uses trained/dictionary-based UDP compression.
 */
class OODLEHANDLERCOMPONENT_API OodleHandlerComponent : public HandlerComponent
{
public:
	/* Initializes default data */
	OodleHandlerComponent();

	/* Default Destructor */
	~OodleHandlerComponent();

	/**
	 * Initializes first-run config settings
	 */
	static void InitFirstRunConfig();


	/**
	 * Initializes FOodleDictionary data, from the specified dictionary file
	 *
	 * @param FilePath		The dictionary file path
	 * @param OutDictionary	The FOodleDictionary struct to write to
	 */
	void InitializeDictionary(FString FilePath, FOodleDictionary& OutDictionary);

	/**
	 * Resolves and returns the default dictionary file paths.
	 *
	 * @param OutServerDictionary	The server dictionary path
	 * @param OutClientDictionary	The client dictionary path
	 * @param bFailFatal			Whether or not failure to set the dictionary paths, should be fatal
	 * @return						Whether or not the dictionary paths were successfully set
	 */
	bool GetDictionaryPaths(FString& OutServerDictionary, FString& OutClientDictionary, bool bFailFatal=true);

#if !UE_BUILD_SHIPPING
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

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

protected:
	/* How many bytes have been saved from compressing the data */
	uint32 BytesSaved;

#if EXAMPLE_CAPTURE_FORMAT
	/* File to log packets to */
	IFileHandle* PacketLogFile;

	/** Whether or not to append data to a statically named log file */
	bool bPacketLogAppend;
#else
	/* File to log input packets to */
	FPacketCaptureArchive* InPacketLog;

	/* File to log output packets to */
	FPacketCaptureArchive* OutPacketLog;
#endif


	/** Whether or not Oodle is enabled */
	bool bEnableOodle;

#if !UE_BUILD_SHIPPING
	/** Search for dictionary files and use them if present - switching mode to Release in process - don't use in shipping */
	bool bUseDictionaryIfPresent;
#endif

	/* 
	* Modes of the component
	* Specify this in .ini by
	* [OodleHandlerComponent]
	* Mode=Training
	*/
	EOodleHandlerMode Mode;


	/** Server (Outgoing) dictionary data */
	FOodleDictionary ServerDictionary;

	/** Client (Incoming - relative to server) dictionary data */
	FOodleDictionary ClientDictionary;
};

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Oodle Bytes Raw"), STAT_Oodle_BytesRaw, STATGROUP_Net, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Oodle Bytes Compressed"), STAT_Oodle_BytesCompressed, STATGROUP_Net, );
#endif

/* Reliability Module Interface */
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
