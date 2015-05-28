// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Engine.h: Unreal engine public header file.
=============================================================================*/

#ifndef _INC_ENGINE_UTILS
#define _INC_ENGINE_UTILS

#include "Engine/Brush.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "HitProxies.h"

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit an actor.
struct HActor : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API )
	AActor* Actor;
	const UPrimitiveComponent* PrimComponent;
	int32 MaterialIndex;

	HActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent )
		: Actor( InActor ) 
		, PrimComponent( InPrimComponent )
		, MaterialIndex( -1 )
		{}

	HActor(AActor* InActor, const UPrimitiveComponent* InPrimComponent, int32 InMaterialIndex)
		: Actor( InActor )
		, PrimComponent( InPrimComponent )
		, MaterialIndex( InMaterialIndex )
		{}

	HActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority) 
		: HHitProxy( InPriority )
		, Actor( InActor )
		, PrimComponent( InPrimComponent )
		, MaterialIndex( -1 )
		{}

	HActor(AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority, int32 InMaterialIndex)
		: HHitProxy(InPriority)
		, Actor(InActor)
		, PrimComponent(InPrimComponent)
		, MaterialIndex(InMaterialIndex)
		{}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );

		UPrimitiveComponent* NonConstPrimComp = const_cast<UPrimitiveComponent*>(PrimComponent);
		Collector.AddReferencedObject( NonConstPrimComp );
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

//
//	HBSPBrushVert
//

struct HBSPBrushVert : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );
	TWeakObjectPtr<ABrush>	Brush;
	FVector* Vertex;
	HBSPBrushVert(ABrush* InBrush,FVector* InVertex):
		HHitProxy(HPP_UI),
		Brush(InBrush),
		Vertex(InVertex)
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		//@todo: Brush Hit proxies are currently referencing between UWorld's (undesired), 
		// once this issue is resolved remove the TWeakObjectPtr and replace with Standard ABrush*.
		// Also uncomment this line:
		//Collector.AddReferencedObject( Brush );
	}
};

//
//	HStaticMeshVert
//

struct HStaticMeshVert : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );
	AActor*	Actor;
	FVector Vertex;
	HStaticMeshVert(AActor* InActor,FVector InVertex):
		HHitProxy(HPP_UI),
		Actor(InActor),
		Vertex(InVertex)
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );
	}
};

// Hit an actor even with translucency
struct HTranslucentActor : public HActor
{
	DECLARE_HIT_PROXY( ENGINE_API )
	HTranslucentActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent ) 
		: HActor( InActor, InPrimComponent ) 
		{}

	HTranslucentActor( AActor* InActor, const UPrimitiveComponent* InPrimComponent, EHitProxyPriority InPriority) 
		: HActor(InActor, InPrimComponent, InPriority)
		{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return true;
	}
};


/*-----------------------------------------------------------------------------
	Iterator for the editor that loops through all selected actors.
-----------------------------------------------------------------------------*/

/**
 * Abstract base class for actor iteration. Implements all operators and relies on IsSuitable
 * to be overridden by derived classed. Also the code currently relies on the ++ operator being
 * called from within the constructor which is bad as it uses IsSuitable which is a virtual function.
 * This means that all derived classes are treated as "final" and they need to manually call
 * ++(*this) in their constructor so it ends up calling the right one. The long term plan is to
 * replace the use of virtual functions with template tricks.
 * Note that when Playing In Editor, this will find actors only in CurrentWorld.
 */
class FActorIteratorBase
{
protected:
	/** Current world we are iterating upon						*/
	UWorld* CurrentWorld;
	/** Results from the GetObjectsOfClass query				*/
	TArray<UObject*> ObjectArray;
	/** index of the current element in the object array		*/
	int32 Index;
	/** Whether we already reached the end						*/
	bool	ReachedEnd;
	/** Number of actors that have been considered thus far		*/
	int32		ConsideredCount;
	/** Current actor pointed to by actor iterator				*/
	AActor*	CurrentActor;
	/** Delegate for catching spawned actors					*/
	FOnActorSpawned::FDelegate ActorSpawnedDelegate;
	/** Contains any actors spawned during iteration			*/
	TArray<AActor*> SpawnedActorArray;
	/** The class type we are iterating, kept for filtering		*/
	UClass* DesiredClass;
	/** Handle to the registered OnActorSpawned delegate		*/
	FDelegateHandle ActorSpawnedDelegateHandle;

	/**
	 * Default ctor, inits everything
	 */
	FActorIteratorBase( UWorld* InWorld, TSubclassOf<class AActor> InClass = AActor::StaticClass() ) :
		CurrentWorld( InWorld ),
		Index( -1 ),
		ReachedEnd( false ),
		ConsideredCount( 0 ),
		CurrentActor( NULL ),
		DesiredClass(InClass)
	{
		check(IsInGameThread());
		check(CurrentWorld);
		EObjectFlags ExcludeFlags = RF_ClassDefaultObject|RF_PendingKill;
		GetObjectsOfClass(InClass, ObjectArray, true, ExcludeFlags);

		ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FActorIteratorBase::OnActorSpawned);
		ActorSpawnedDelegateHandle = CurrentWorld->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
	}

	~FActorIteratorBase()
	{
		CurrentWorld->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	}

	void OnActorSpawned(AActor* InActor)
	{
		if (InActor->IsA(DesiredClass))
		{
			SpawnedActorArray.AddUnique(InActor);
		}
	}

public:

	

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator*()
	{
		check(CurrentActor);
		checkf(!CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *CurrentActor->GetFullName());
		return CurrentActor;
	}
	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator->()
	{
		check(CurrentActor);
		checkf(!CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *CurrentActor->GetFullName());
		return CurrentActor;
	}
	/**
	 * Returns whether the iterator has reached the end and no longer points
	 * to a suitable actor.
	 *
	 * @return true if iterator points to a suitable actor, false if it has reached the end
	 */
	FORCEINLINE operator bool()
	{
		return !ReachedEnd;
	}

	/**
	 * Clears the current Actor in the array (setting it to NULL).
	 */
	void ClearCurrent()
	{
		check(!ReachedEnd);
		CurrentWorld->RemoveActor(CurrentActor, true);
	}

	/**
	 * Returns the number of actors considered thus far. Can be used in combination
	 * with GetProgressDenominator to gauge progress iterating over all actors.
	 *
	 * @return number of actors considered thus far.
	 */
	int32 GetProgressNumerator()
	{
		return ConsideredCount;
	}
};

/**
 * Default level iteration filter. Doesn't cull levels out.
 */
struct FDefaultLevelFilter
{
	/**
	 * Used to examine whether this level is valid for iteration or not
	 *
	 * @param Level the level to check for iteration
	 *
	 * @return true if the level can be iterated, false otherwise
	 */
	bool CanIterateLevel(ULevel* Level) const
	{
		return true;
	}
};

/**
 * Filter class that prevents ticking of levels that are still loading
 */
struct FTickableLevelFilter
{
	/**
	 * Used to examine whether this level is valid for iteration or not
	 *
	 * @param Level the level to check for iteration
	 *
	 * @return true if the level can be iterated, false otherwise
	 */
	static bool CanIterateLevel(ULevel* Level)
	{
		return Level->bIsVisible;
	}
};

/**
 * Template class used to avoid the virtual function call cost for filtering
 * actors by certain characteristics
 */
template<typename FILTER_CLASS,typename LEVEL_FILTER_CLASS = FDefaultLevelFilter>
class TActorIteratorBase :
	public FActorIteratorBase
{
protected:
	/**
	 * Ref to the class that is going to filter actors
	 */
	const FILTER_CLASS& FilterClass;
	/**
	 * Ref to the object that is going to filter levels
	 */
	const LEVEL_FILTER_CLASS& LevelFilterClass;

	/**
	 * Hide the constructor as construction on this class should only be done by subclasses
	 */
	TActorIteratorBase( UWorld* InWorld, TSubclassOf<class AActor> InClass = AActor::StaticClass(),
		const FILTER_CLASS& InFilterClass = FILTER_CLASS(),
		const LEVEL_FILTER_CLASS& InLevelClass = LEVEL_FILTER_CLASS())
	:	FActorIteratorBase( InWorld, InClass ),
		FilterClass(InFilterClass),
		LevelFilterClass(InLevelClass)
	{
	}

public:
	/**
	 * Iterates to next suitable actor.
	 */
	void operator++()
	{
		// Use local version to avoid LHSs as compiler is not required to write out member variables to memory.
		AActor*		LocalCurrentActor	= NULL;
		int32		LocalIndex			= Index;
		while(++LocalIndex < (ObjectArray.Num() + SpawnedActorArray.Num()))
		{
			if (LocalIndex < ObjectArray.Num())
			{
				LocalCurrentActor = static_cast<AActor*>(ObjectArray[LocalIndex]);
			}
			else
			{
				LocalCurrentActor = SpawnedActorArray[LocalIndex - ObjectArray.Num()];
			}
			ConsideredCount++;

			if ( LocalCurrentActor != NULL
				&& FilterClass.IsSuitable(LocalCurrentActor)
				&& LevelFilterClass.CanIterateLevel(LocalCurrentActor->GetLevel())
				&& LocalCurrentActor->GetWorld() == CurrentWorld)
			{
				// ignore non-persistent world settings
				if (LocalCurrentActor->GetLevel() == CurrentWorld->PersistentLevel
					|| !LocalCurrentActor->IsA(AWorldSettings::StaticClass()))
				{
					CurrentActor = LocalCurrentActor;
					Index = LocalIndex;
					return;
				}
			}
		}
		CurrentActor = NULL;
		ReachedEnd = true;
	}
};

/**
 * Base filter class for actor filtering in iterators.
 */
class FActorFilter
{
public:
	/**
	 * Determines whether this is a valid actor or not
	 *
	 * @param	Actor	Actor to check
	 * @return	true if actor is != NULL, false otherwise
	 */
	FORCEINLINE bool IsSuitable( AActor* Actor ) const
	{
		return Actor != NULL;
	}
};

/**
 * Actor iterator
 * Note that when Playing In Editor, this will find actors only in CurrentWorld
 */
class FActorIterator :
	public TActorIteratorBase<FActorFilter, FTickableLevelFilter>
{
public:
	/**
	 * Constructor, inits the starting position for iteration
	 * Also optionally takes in a custom UWorld
	 */
	FActorIterator(UWorld* InWorld )
		:TActorIteratorBase(InWorld)
	{
		++(*this);
	}
};

/**
 * Template actor iterator.
 */
template <typename ActorType>
class TActorIterator : 
	public TActorIteratorBase< FActorFilter >
{
public:
	/**
	 * Constructor, inits the starting position for iteration
	 */
	TActorIterator( UWorld* InWorld, TSubclassOf<ActorType> InClass = ActorType::StaticClass() )
		: TActorIteratorBase< FActorFilter >( InWorld, InClass )
	{
		++(*this);
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE ActorType* operator*()
	{
		check(FActorIteratorBase::CurrentActor);
		checkf(!FActorIteratorBase::CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *FActorIteratorBase::CurrentActor->GetFullName());
		return CastChecked<ActorType>(FActorIteratorBase::CurrentActor);
	}
	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE ActorType* operator->()
	{
		check(FActorIteratorBase::CurrentActor);
		checkf(!FActorIteratorBase::CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *FActorIteratorBase::CurrentActor->GetFullName());
		return CastChecked<ActorType>(FActorIteratorBase::CurrentActor);
	}
};

/**
 * Filters actors by whether or not they are selected
 */
class FSelectedActorFilter :
	public FActorFilter
{
public:
	/**
	 * Determines if the actor should be returned during iteration or not
	 *
	 * @param	Actor	Actor to check
	 * @return	true if actor is selected, false otherwise
	 */
	FORCEINLINE bool IsSuitable( AActor* Actor ) const
	{
		return Actor ? Actor->IsSelected() : false;
	}
};

/**
 * Selected actor iterator
 */
class FSelectedActorIterator :
	public TActorIteratorBase<FSelectedActorFilter>
{
public:
	/**
	 * Constructor, inits the starting position for iteration
	 */
	FSelectedActorIterator( UWorld* InWorld )
		:TActorIteratorBase( InWorld )
	{
		++(*this);
	}
};

/**
 * Filters actors by whether or not they are network relevant
 */
class FNetRelevantActorFilter :
	public FActorFilter
{
	/**
	 * The name of the net driver to filter actors by
	 */
	FName NetDriverName;

public:
	/** Filter by the name specified */
	FNetRelevantActorFilter(FName InNetDriverName) :
		NetDriverName(InNetDriverName)
	{
	}
	/** Default to the game network driver if not specified */
	FNetRelevantActorFilter() :
		NetDriverName(NAME_GameNetDriver)
	{
	}

	/**
	 * Determines whether this actor is marked for the specified net driver or not
	 *
	 * @param	Actor	Actor to check the net driver name of
	 * @return	true if actor is != NULL, false otherwise
	 */
	FORCEINLINE bool IsSuitable( AActor* Actor ) const
	{
		return Actor != NULL && Actor->NetDriverName == NetDriverName;
	}
};

/**
 * Net relevant actor iterator
 */
class FNetRelevantActorIterator :
	public TActorIteratorBase<FNetRelevantActorFilter, FTickableLevelFilter>
{
	/** Holds a filter class for the lifetime of this iterator */
	FNetRelevantActorFilter FilterByNetDriverName;

	/** Hidden on purpose */
	FNetRelevantActorIterator();

public:
	/**
	 * Positions the iterator at the first network relevant actor
	 */
	FNetRelevantActorIterator( UWorld* InWorld, FName NetDriverName ) :
		TActorIteratorBase( InWorld, AActor::StaticClass(), FilterByNetDriverName ),
		FilterByNetDriverName( NetDriverName )
	{
		++(*this);
	}
};

/**
 * An output device that forwards output to both the log and the console.
 */
class ENGINE_API FConsoleOutputDevice : public FStringOutputDevice
{
public:

	/**
	 * Minimal initialization constructor.
	 */
	FConsoleOutputDevice(class UConsole* InConsole):
		FStringOutputDevice(TEXT("")),
		Console(InConsole)
	{}

	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const class FName& Category) override;

private:

	/** The console which output is written to. */
	UConsole* Console;
};


/**
 *	Renders stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation	Location of camera
 *	@param ViewRotation	Rotation of camera
 */
ENGINE_API void DrawStatsHUD( UWorld* InWorld, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<struct FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation );

/** SubLevel status information */
struct FSubLevelStatus
{
	FName				PackageName;
	EStreamingStatus	StreamingStatus;
	int32				LODIndex;
	bool				bPlayerInside;
};

/**
 *	Gathers SubLevels status from a provided world
 *	@param InWorld		World to gather sublevels stats from
 *	@return				sublevels status (streaming state, LOD index, where player is)
 */
TArray<FSubLevelStatus> GetSubLevelsStatus( UWorld* InWorld );

#if !UE_BUILD_SHIPPING

// Helper class containing information about UObject assets referenced.
struct FContentComparisonAssetInfo
{
	/** Name of the asset */
	FString AssetName;
	/** The resource size of the asset */
	int32 ResourceSize;

	/** Constructor */
	FContentComparisonAssetInfo()
	{
		FMemory::Memzero(this, sizeof(FContentComparisonAssetInfo));
	}

	/** operator == */
	bool operator==(const FContentComparisonAssetInfo& Other)
	{
		return (
			(AssetName == Other.AssetName) &&
			(ResourceSize == Other.ResourceSize)
			);
	}

	/** operator = */
	FContentComparisonAssetInfo& operator=(const FContentComparisonAssetInfo& Other)
	{
		AssetName = Other.AssetName;
		ResourceSize = Other.ResourceSize;
		return *this;
	}
};

/** Helper class for performing the content comparison console command */
class FContentComparisonHelper
{
public:
	FContentComparisonHelper();
	virtual ~FContentComparisonHelper();

	/**
	 *	Compare the classes derived from the given base class.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	bool					true if successful, false if not
	 */
	virtual bool CompareClasses(const FString& InBaseClassName, int32 InRecursionDepth);

	/**
	 *	Compare the classes derived from the given base class, ignoring specified base classes.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InBaseClassesToIgnore	The base classes to ignore when processing objects.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	bool					true if successful, false if not
	 */
	virtual bool CompareClasses(const FString& InBaseClassName, const TArray<FString>& InBaseClassesToIgnore, int32 InRecursionDepth);

	/**
	 *	Recursive function for collecting objects referenced by the given object.
	 *
	 *	@param	InStartObject			The object to collect the referencees for
	 *	@param	InCurrDepth				The current depth being processed
	 *	@param	InMaxDepth				The maximum depth to traverse the reference chain
	 *	@param	OutCollectReferences	The resulting referenced object list
	 */
	void RecursiveObjectCollection(UObject* InStartObject, int32 InCurrDepth, int32 InMaxDepth, TMap<UObject*,bool>& OutCollectedReferences);

protected:
	TMap<FString,bool> ReferenceClassesOfInterest;
};
#endif

namespace EngineUtils
{
	enum EAssetToLoad
	{
		ATL_Regular,
		ATL_Class,
	};

	/** Loads all the assets found in the specified path and subpaths */
	ENGINE_API bool FindOrLoadAssetsByPath(const FString& Path, TArray<UObject*>& OutAssets, EAssetToLoad Type);
}

/** Helper class for serializing flags describing which data have been stripped (if any). */
class FStripDataFlags
{
	/** Serialized engine strip flags (up to 8 flags). */
	uint8 GlobalStripFlags;
	/** Serialized per-class strip flags (user defined, up to 8 flags). */
	uint8 ClassStripFlags;

public:

	/** Engine strip flags */
	enum EStrippedData
	{
		None = 0,

		/* Editor data */
		Editor = 1,
		/* All data not required for dedicated server to work correctly (usually includes editor data). */
		Server = 2,

		// Add global flags here (up to 8 including the already defined ones).

		/** All flags */
		All = 0xff
	};

	/** 
	 * Constructor.
	 * Serializes strip data flags. Global (engine) flags are automatically generated from target platform
	 * when saving. Class flags need to be defined by the user.
	 *
	 * @param Ar - Archive to serialize with.
	 * @param InClassFlags - User defined per class flags .
	 * @param InVersion - Minimal strip version required to serialize strip flags
	 */
	FStripDataFlags( FArchive& Ar, uint8 InClassFlags = 0, int32 InVersion = VER_UE4_OLDEST_LOADABLE_PACKAGE );

	/** 
	 * Constructor.
	 * Serializes strip data flags. Global (engine) flags are user defined and will not be automatically generated
	 * when saving. Class flags also need to be defined by the user.
	 *
	 * @param Ar - Archive to serialize with.
	 * @param InClassFlags - User defined per class flags.
	 * @param InVersion - Minimal version required to serialize strip flags
	 */
	FStripDataFlags( FArchive& Ar, uint8 InGlobalFlags, uint8 InClassFlags, int32 InVersion = VER_UE4_OLDEST_LOADABLE_PACKAGE );

	/**
	 * Checks if FStripDataFlags::Editor flag is set or not
	 *
	 * @return true if FStripDataFlags::Editor is set, false otherwise.
	 */
	FORCEINLINE bool IsEditorDataStripped() const
	{
		return (GlobalStripFlags & FStripDataFlags::Editor) != 0;
	}

	/**
	 * Checks if FStripDataFlags::Server flag is set or not
	 *
	 * @return true if FStripDataFlags::Server is set, false otherwise.
	 */
	bool IsDataStrippedForServer() const
	{
		return (GlobalStripFlags & FStripDataFlags::Server) != 0;
	}

	/**
	 * Checks if user defined flags are set or not.
	 *
	 * @InFlags - User defined flags to check.
	 * @return true if the specified user defined flags are set, false otherwise.
	 */
	bool IsClassDataStripped(uint8 InFlags) const
	{
		return (ClassStripFlags & InFlags) != 0;
	}
};

#endif // _INC_ENGINE_UTILS
