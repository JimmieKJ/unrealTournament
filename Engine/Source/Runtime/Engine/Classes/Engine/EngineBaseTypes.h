// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *	This file is for shared structs and enums that need to be declared before the rest of Engine.
 *  The typical use case is for structs used in the renderer and also in script code.
 */

#include "TaskGraphInterfaces.h"
#include "EngineBaseTypes.generated.h"

//
//	EInputEvent
//
UENUM()
enum EInputEvent
{
	IE_Pressed              =0,
	IE_Released             =1,
	IE_Repeat               =2,
	IE_DoubleClick          =3,
	IE_Axis                 =4,
	IE_MAX                  =5,
};

/** Type of tick we wish to perform on the level */
enum ELevelTick
{
	LEVELTICK_TimeOnly = 0,	// Update the level time only.
	LEVELTICK_ViewportsOnly = 1,	// Update time and viewports.
	LEVELTICK_All = 2,	// Update all.
	LEVELTICK_PauseTick = 3,	// Delta time is zero, we are paused. Components don't tick.
};

/** Determines which ticking group a tick function belongs to */
UENUM()
enum ETickingGroup
{
	/** Any item that needs to be executed before physics simulation starts */
	TG_PrePhysics,

	/** Special tick group that starts physics simulation */							
	TG_StartPhysics,

	/** Any item that can be run in parallel with our physics simulation work */
	TG_DuringPhysics,

	/** Special tick group that ends physics simulation */
	TG_EndPhysics,

	/** Any item that needs physics to be complete before being executed */
	TG_PreCloth,

	/** Any item that needs to be updated after rigid body simulation is done, but before cloth is simulation is done */
	TG_StartCloth,

	/** Any item that can be run during cloth simulation */	
	TG_EndCloth,

	/** Any item that needs rigid body and cloth sim to be complete before being executed */
	TG_PostPhysics,

	/** Any item that needs the update work to be done before being ticked */
	TG_PostUpdateWork,

	/** Special tick group that is not actually a tick group. After every tick group this is repeatedly re-run until there are no more newly spawned items to run */
	TG_NewlySpawned,

	TG_MAX,
};

/**
 * This is small structure to hold prerequisite tick functions
 */
USTRUCT()
struct FTickPrerequisite
{
	GENERATED_USTRUCT_BODY()

	/** Tick functions live inside of UObjects, so we need a separate weak pointer to the UObject solely for the purpose of determining if PrerequisiteTickFunction is still valid **/
	TWeakObjectPtr<class UObject> PrerequisiteObject;



		/** Pointer to the actual tick function and must be completed prior to our tick running **/
		struct FTickFunction*		PrerequisiteTickFunction;

		/** Noop constructor **/
		FTickPrerequisite()
		: PrerequisiteTickFunction(nullptr)
		{
		}
		/** 
		 * Constructor
		 * @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
		 * @param TargetTickFunction - Actual tick function to use as a prerequisite
		**/
		FTickPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
		: PrerequisiteObject(TargetObject)
		, PrerequisiteTickFunction(&TargetTickFunction)
		{
			check(PrerequisiteTickFunction);
		}
		/** Equality operator, used to prevent duplicates and allow removal by value **/
		bool operator==(const FTickPrerequisite& Other) const
		{
			return PrerequisiteObject == Other.PrerequisiteObject &&
				PrerequisiteTickFunction == Other.PrerequisiteTickFunction;
		}
		/** Return the tick function, if it is still valid. Can be null if the tick function was null or the containing UObject has been garbage collected. **/
		struct FTickFunction* Get()
		{
			if (PrerequisiteObject.IsValid(true))
			{
				return PrerequisiteTickFunction;
			}
			return nullptr;
		}
	
};

/** 
* Abstract Base class for all tick functions.
**/
USTRUCT()
struct ENGINE_API FTickFunction
{
	GENERATED_USTRUCT_BODY()

public:
	// The following UPROPERTYs are for configuration and inherited from the CDO/archetype/blueprint etc

	/**
	 * Defines the minimum tick group for this tick function. These groups determine the relative order of when objects tick during a frame update.
	 * Given prerequisites, the tick may be delayed.
	 *
	 * @see ETickingGroup 
	 * @see FTickFunction::AddPrerequisite()
	 */
	UPROPERTY()
	TEnumAsByte<enum ETickingGroup> TickGroup;

protected:
	/** Internal data that indicates the tick group we actually executed in (it may have been delayed due to prerequisites) **/
	TEnumAsByte<enum ETickingGroup> ActualTickGroup;

public:
	/** Bool indicating that this function should execute even if the game is paused. Pause ticks are very limited in capabilities. **/
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	uint32 bTickEvenWhenPaused:1;

	/** If false, this tick function will never be registered and will never tick. Only settable in defaults. */
	UPROPERTY()
	uint32 bCanEverTick:1;

	/** If true, this tick function will start enabled, but can be disabled later on. */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	uint32 bStartWithTickEnabled:1;

	/** If we allow this tick to run on a dedicated server */
	UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay)
	uint32 bAllowTickOnDedicatedServer:1;

	/** If false, this tick will run on the game thread, otherwise it will run on any thread in parallel with the game thread and in parallel with other "async ticks" **/
	uint32 bRunOnAnyThread:1;

private:
	/** If true, means that this tick function is in the master array of tick functions **/
	uint32 bRegistered:1;

	/** 
	 * If false, this tick will not fire, nor will any other tick that has this tick function as an EnableParent. 
	 * CAUTION: Do not set this directly
	 **/
	uint32 bTickEnabled:1;

	/** Internal data to track if we have started visiting this tick function yet this frame **/
	int32 TickVisitedGFrameCounter;

	/** Internal data to track if we have finshed visiting this tick function yet this frame **/
	int32 TickQueuedGFrameCounter;

private:
	/** Completion handle for the task that will run this tick. Caution, this is no reset to nullptr until an unspecified future time **/
	FGraphEventRef CompletionHandle;

	/** Prerequisites for this tick function **/
	TArray<struct FTickPrerequisite> Prerequisites;

public:
	/** 
	 * If the "EnableParent" is not enabled, then this tick function is implicitly disabled as well. 
	 * Caution, there is no protection on this strong pointer. It is assumed your enable parent will not be destroyed before you are.
	 **/
	FTickFunction*								EnableParent;
	/** Back pointer to the FTickTaskLevel containing this tick function if it is registered **/
	class FTickTaskLevel*						TickTaskLevel;

	/** Default constructor, intitalizes to reasonable defaults **/
	FTickFunction();
	/** Destructor, unregisters the tick function **/
	virtual ~FTickFunction();

	/** 
	 * Adds the tick function to the master list of tick functions. 
	 * @param Level - level to place this tick function in
	 **/
	void RegisterTickFunction(class ULevel* Level);
	/** Removes the tick function from the master list of tick functions. **/
	void UnRegisterTickFunction();
	/** See if the tick function is currently registered */
	bool IsTickFunctionRegistered() const { return bRegistered; }

	/** Enables or disables this tick function. **/
	void SetTickFunctionEnable(bool bInEnabled);
	/** Returns whether the tick function is currently enabled */
	bool IsTickFunctionEnabled() const { return bTickEnabled; }

	/**
	* Gets the current completion handle of this tick function, so it can be delayed until a later point when some additional
	* tasks have been completed.  Only valid after TG_PreAsyncWork has started and then only until the TickFunction itself has
	* started to run
	**/
	FGraphEventRef GetCompletionHandle() const
	{
		return CompletionHandle;
	}
	/** 
	* Gets the action tick group that this function will execute in this frame.
	* Only valid after TG_PreAsyncWork has started through the end of the frame.
	**/
	TEnumAsByte<enum ETickingGroup> GetActualTickGroup() const
	{
		return ActualTickGroup;
	}

	/** 
	 * Adds a tick function to the list of prerequisites...in other words, adds the requirement that TargetTickFunction is called before this tick function is 
	 * @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
	 * @param TargetTickFunction - Actual tick function to use as a prerequisite
	 **/
	void AddPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);
	/** 
	 * Removes a prerequisite that was previously added.
	 * @param TargetObject - UObject containing this tick function. Only used to verify that the other pointer is still usable
	 * @param TargetTickFunction - Actual tick function to use as a prerequisite
	 **/
	void RemovePrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);

	/**
	 * @return a reference to prerequisites for this tick function.
	 */
	TArray<struct FTickPrerequisite>& GetPrerequisites()
	{
		return Prerequisites;
	}

	/**
	 * @return a reference to prerequisites for this tick function (const).
	 */
	const TArray<struct FTickPrerequisite>& GetPrerequisites() const
	{
		return Prerequisites;
	}

private:
	/**
	 * Queues a tick function for execution from the game thread
	 * @param TickContext - context to tick in
	 */
	void QueueTickFunction(class FTickTaskSequencer& TTS, const struct FTickContext& TickContext);

	/** 
	 * Abstract function actually execute the tick. 
	 * @param DeltaTime - frame time to advance, in seconds
	 * @param TickType - kind of tick for this frame
	 * @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
	 * @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	 **/
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(0); // you cannot make this pure virtual in script because it wants to create constructors.
	}
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage()
	{
		check(0); // you cannot make this pure virtual in script because it wants to create constructors.
		return FString(TEXT("invalid"));
	}
	
	friend class FTickTaskSequencer;
	friend class FTickTaskManager;
	friend class FTickTaskLevel;
};

template<>
struct TStructOpsTypeTraits<FTickFunction> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};

/** 
* Tick function that calls AActor::TickActor
**/
USTRUCT()
struct FActorTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/**  AActor  that is the target of this tick **/
	class AActor*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	ENGINE_API virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	ENGINE_API virtual FString DiagnosticMessage() override;
};

template<>
struct TStructOpsTypeTraits<FActorTickFunction> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};

/** 
* Tick function that calls UActorComponent::ConditionalTick
**/
USTRUCT()
struct FActorComponentTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/**  AActor  component that is the target of this tick **/
	class UActorComponent*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	ENGINE_API virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	ENGINE_API virtual FString DiagnosticMessage() override;
};


template<>
struct TStructOpsTypeTraits<FActorComponentTickFunction> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};
/** 
* Tick function that calls UPrimitiveComponent::PostPhysicsTick
**/
USTRUCT()
struct FPrimitiveComponentPostPhysicsTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** PrimitiveComponent component that is the target of this tick **/
	class UPrimitiveComponent*	Target;

	/** 
		* Abstract function actually execute the tick. 
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage() override;
};

template<>
struct TStructOpsTypeTraits<FPrimitiveComponentPostPhysicsTickFunction> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};

/** Types of network failures broadcast from the engine */
UENUM(BlueprintType)
namespace ENetworkFailure
{
	enum Type
	{
		/** A relevant net driver has already been created for this service */
		NetDriverAlreadyExists,
		/** The net driver creation failed */
		NetDriverCreateFailure,
		/** The net driver failed its Listen() call */
		NetDriverListenFailure,
		/** A connection to the net driver has been lost */
		ConnectionLost,
		/** A connection to the net driver has timed out */
		ConnectionTimeout,
		/** The net driver received an NMT_Failure message */
		FailureReceived,
		/** The client needs to upgrade their game */
		OutdatedClient,
		/** The server needs to upgrade their game */
		OutdatedServer,
		/** There was an error during connection to the game */
		PendingConnectionFailure
	};
}


namespace ENetworkFailure
{
	inline const TCHAR* ToString(ENetworkFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case NetDriverAlreadyExists:
			return TEXT("NetDriverAlreadyExists");
		case NetDriverCreateFailure:
			return TEXT("NetDriverCreateFailure");
		case NetDriverListenFailure:
			return TEXT("NetDriverListenFailure");
		case ConnectionLost:
			return TEXT("ConnectionLost");
		case ConnectionTimeout:
			return TEXT("ConnectionTimeout");
		case FailureReceived:
			return TEXT("FailureReceived");
		case OutdatedClient:
			return TEXT("OutdatedClient");
		case OutdatedServer:
			return TEXT("OutdatedServer");
		case PendingConnectionFailure:
			return TEXT("PendingConnectionFailure");
		}
		return TEXT("Unknown ENetworkFailure error occurred.");
	}
}

/** Types of server travel failures broadcast by the engine */
UENUM(BlueprintType)
namespace ETravelFailure
{
	enum Type
	{
		/** No level found in the loaded package */
		NoLevel,
		/** LoadMap failed on travel (about to Browse to default map) */
		LoadMapFailure,
		/** Invalid URL specified */
		InvalidURL,
		/** A package is missing on the client */
		PackageMissing,
		/** A package version mismatch has occurred between client and server */
		PackageVersion,
		/** A package is missing and the client is unable to download the file */
		NoDownload,
		/** General travel failure */
		TravelFailure,
		/** Cheat commands have been used disabling travel */
		CheatCommands,
		/** Failed to create the pending net game for travel */
		PendingNetGameCreateFailure,
		/** Failed to save before travel */
		CloudSaveFailure,
		/** There was an error during a server travel to a new map */
		ServerTravelFailure,
		/** There was an error during a client travel to a new map */
		ClientTravelFailure,
	};
}

namespace ETravelFailure
{
	inline const TCHAR* ToString(ETravelFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case NoLevel:
			return TEXT("NoLevel");
		case LoadMapFailure:
			return TEXT("LoadMapFailure");
		case InvalidURL:
			return TEXT("InvalidURL");
		case PackageMissing:
			return TEXT("PackageMissing");
		case PackageVersion:
			return TEXT("PackageVersion");
		case NoDownload:
			return TEXT("NoDownload");
		case TravelFailure:
			return TEXT("TravelFailure");
		case CheatCommands:
			return TEXT("CheatCommands");
		case PendingNetGameCreateFailure:
			return TEXT("PendingNetGameCreateFailure");
		case ServerTravelFailure:
			return TEXT("ServerTravelFailure");
		case ClientTravelFailure:
			return TEXT("ClientTravelFailure");
		case CloudSaveFailure:
			return TEXT("CloudSaveFailure");
		}
		return TEXT("Unknown ETravelFailure error occurred.");
	}
}

// Traveling from server to server.
UENUM()
enum ETravelType
{
	// Absolute URL.
	TRAVEL_Absolute,
	// Partial (carry name, reset server).
	TRAVEL_Partial,
	// Relative URL.
	TRAVEL_Relative,
	TRAVEL_MAX,
};

/** Types of demo play failures broadcast from the engine */
UENUM(BlueprintType)
namespace EDemoPlayFailure
{
	enum Type
	{
		/** A Generic failure */
		Generic,
		/** Demo was not found */
		DemoNotFound,
		/** Demo is corrupt */
		Corrupt,
		/** Invalid version */
		InvalidVersion,
	};
}

namespace EDemoPlayFailure
{
	inline const TCHAR* ToString(EDemoPlayFailure::Type FailureType)
	{
		switch (FailureType)
		{
		case Generic:
			return TEXT("Gneric");
		case DemoNotFound:
			return TEXT("DemoNotFound");
		case Corrupt:
			return TEXT("Corrupt");
		case InvalidVersion:
			return TEXT("InvalidVersion");
		}

		return TEXT("Unknown EDemoPlayFailure error occurred.");
	}
}

//URL structure.
USTRUCT()
struct ENGINE_API FURL
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Protocol;    // Protocol, i.e. "unreal" or "http".

	UPROPERTY()
	FString Host;    // Optional hostname, i.e. "204.157.115.40" or "unreal.epicgames.com", blank if local.

	UPROPERTY()
	int32 Port;    // Optional host port.

	UPROPERTY()
	FString Map;    // Map name, i.e. "SkyCity", default is "Entry".

	UPROPERTY()
	FString RedirectURL;    // Optional place to download Map if client does not possess it

	UPROPERTY()
	TArray<FString> Op;    // Options.

	UPROPERTY()
	FString Portal;    // Portal to enter through, default is "".

	UPROPERTY()
	int32 Valid;

	// Statics.
	static FUrlConfig UrlConfig;
	static bool bDefaultsInitialized;

	// Constructors.
	/* FURL() prevent default from being generated */
	explicit FURL( ENoInit ) { }
	FURL( const TCHAR* Filename=nullptr );
	FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type );
	static void StaticInit();
	static void StaticExit();

	/**
	 * Static: Removes any special URL characters from the specified string
	 *
	 * @param Str String to be filtered
	 */
	static void FilterURLString( FString& Str );

	// Functions.
	bool IsInternal() const;
	bool IsLocalInternal() const;
	bool HasOption( const TCHAR* Test ) const;
	const TCHAR* GetOption( const TCHAR* Match, const TCHAR* Default ) const;
	void LoadURLConfig( const TCHAR* Section, const FString& Filename=GGameIni );
	void SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const FString& Filename=GGameIni ) const;
	void AddOption( const TCHAR* Str );
	void RemoveOption( const TCHAR* Key, const TCHAR* Section = nullptr, const FString& Filename = GGameIni);
	FString ToString( bool FullyQualified=0 ) const;
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FURL& U );

	// Operators.
	bool operator==( const FURL& Other ) const;
};

/**
 * The network mode the game is currently running.
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Replication/
 */
enum ENetMode
{
	/** Standalone: a game without networking, with one or more local players. Still considered a server because it has all server functionality. */
	NM_Standalone,

	/** Dedicated server: server with no local players. */
	NM_DedicatedServer,

	/** Listen server: a server that also has a local player who is hosting the game, available to other players on the network. */
	NM_ListenServer,

	/**
	 * Network client: client connected to a remote server.
	 * Note that every mode less than this value is a kind of server, so checking NetMode < NM_Client is always some variety of server.
	 */
	NM_Client,

	NM_MAX,
};

/**
 * Define view modes to get specific show flag settings (some on, some off and some are not altered)
 * Don't change the order, the ID is serialized with the editor
 */
UENUM()
enum EViewModeIndex 
{
	// Wireframe w/ brushes
	VMI_BrushWireframe = 0,
	// Wireframe w/ BSP
	VMI_Wireframe = 1,
	// Unlit
	VMI_Unlit = 2,
	// Lit
	VMI_Lit = 3,
	VMI_Lit_DetailLighting = 4,
	// Lit wo/ materials
	VMI_LightingOnly = 5,
	// Colored according to light count.
	VMI_LightComplexity = 6,
	// Colored according to shader complexity.
	VMI_ShaderComplexity = 8,
	// Colored according to world-space LightMap texture density.
	VMI_LightmapDensity = 9,
	// Colored according to light count - showing lightmap texel density on texture mapped objects.
	VMI_LitLightmapDensity = 10,
	VMI_ReflectionOverride = 11,
	VMI_VisualizeBuffer = 12,
//	VMI_VoxelLighting = 13,
	// Colored according to stationary light overlap.
	VMI_StationaryLightOverlap = 14,
//	VMI_VertexDensity = 15,
	VMI_CollisionPawn = 15, 
	VMI_CollisionVisibility = 16, 
	VMI_Max UMETA(Hidden),

	VMI_Unknown = 255 UMETA(Hidden),
};


/** Settings to allow designers to override the automatic expose */
USTRUCT()
struct FExposureSettings
{
	GENERATED_USTRUCT_BODY()

	FExposureSettings()
		: LogOffset(0), bFixed(false)
	{
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d"), LogOffset, bFixed ? 1 : 0);
	}

	void SetFromString(const TCHAR *In)
	{
		// set to default
		*this = FExposureSettings();

		const TCHAR* Comma = FCString::Strchr(In, TEXT(','));
		check(Comma);

		const int32 BUFFER_SIZE = 128;
		TCHAR Buffer[BUFFER_SIZE];
		check((Comma-In)+1 < BUFFER_SIZE);
		
		FCString::Strncpy(Buffer, In, (Comma-In)+1);
		LogOffset = FCString::Atoi(Buffer);
		bFixed = !!FCString::Atoi(Comma+1);
	}

	// usually -4:/16 darker .. +4:16x brighter
	UPROPERTY()
	int32 LogOffset;
	// true: fixed exposure using the LogOffset value, false: automatic eye adaptation
	UPROPERTY()
	bool bFixed;
};


UCLASS(abstract, config=Engine)
class UEngineBaseTypes : public UObject
{
	GENERATED_UCLASS_BODY()

};

