// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Engine: The base class of the global application object classes.
//=============================================================================

#pragma once
#include "World.h"
#include "Engine.generated.h"

class FScreenSaverInhibitor;
class UDeviceProfileManager;
class FViewport;
class FCommonViewportClient;
class FCanvas;


UENUM()
enum EFullyLoadPackageType
{
	/** Load the packages when the map in Tag is loaded */
	FULLYLOAD_Map,
	/** Load the packages before the game class in Tag is loaded. The Game name MUST be specified in the URL (game=Package.GameName). Useful for loading packages needed to load the game type (a DLC game type, for instance) */
	FULLYLOAD_Game_PreLoadClass,
	/** Load the packages after the game class in Tag is loaded. Will work no matter how game is specified in UWorld::SetGameMode. Useful for modifying shipping gametypes by loading more packages (mutators, for instance) */
	FULLYLOAD_Game_PostLoadClass,
	/** Fully load the package as long as the DLC is loaded */
	FULLYLOAD_Always,
	/** Load the package for a mutator that is active */
	FULLYLOAD_Mutator,
	FULLYLOAD_MAX,
};

/**
 * A transition type.
 */
UENUM()
enum ETransitionType
{
	TT_None,
	TT_Paused,
	TT_Loading,
	TT_Saving,
	TT_Connecting,
	TT_Precaching,
	TT_WaitingToConnect,
	TT_MAX,
};

UENUM()
enum EConsoleType
{
	CONSOLE_Any,
	CONSOLE_Mobile,
	CONSOLE_MAX,
};

/** Struct to help hold information about packages needing to be fully-loaded for DLC, etc */
USTRUCT()
struct FFullyLoadedPackagesInfo
{
	GENERATED_USTRUCT_BODY()

	/** When to load these packages */
	UPROPERTY()
	TEnumAsByte<enum EFullyLoadPackageType> FullyLoadType;

	/** When this map or gametype is loaded, the packages in the following array will be loaded and added to root, then removed from root when map is unloaded */
	UPROPERTY()
	FString Tag;

	/** The list of packages that will be fully loaded when the above Map is loaded */
	UPROPERTY()
	TArray<FName> PackagesToLoad;

	/** List of objects that were loaded, for faster cleanup */
	UPROPERTY()
	TArray<class UObject*> LoadedObjects;


	FFullyLoadedPackagesInfo()
		: FullyLoadType(0)
	{
	}

};

/** level streaming updates that should be applied immediately after committing the map change */
USTRUCT()
struct FLevelStreamingStatus
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName PackageName;

	UPROPERTY()
	uint32 bShouldBeLoaded:1;

	UPROPERTY()
	uint32 bShouldBeVisible:1;

	UPROPERTY()
	uint32 LODIndex;


	/** Constructors */
	FLevelStreamingStatus(FName InPackageName, bool bInShouldBeLoaded, bool bInShouldBeVisible, int32 InLODIndex)
	: PackageName(InPackageName), bShouldBeLoaded(bInShouldBeLoaded), bShouldBeVisible(bInShouldBeVisible), LODIndex(InLODIndex)
	{}
	FLevelStreamingStatus()
	{
		FMemory::Memzero(this, sizeof(FLevelStreamingStatus));
		LODIndex = INDEX_NONE;
	}
	
};

/**
 * Container for describing various types of netdrivers available to the engine
 * The engine will try to construct a netdriver of a given type and, failing that,
 * the fallback version.
 */
USTRUCT()
struct FNetDriverDefinition
{
	GENERATED_USTRUCT_BODY()

	/** Unique name of this net driver definition */
	UPROPERTY()
	FName DefName;

	/** Class name of primary net driver */
	UPROPERTY()
	FName DriverClassName;

	/** Class name of the fallback net driver if the main net driver class fails to initialize */
	UPROPERTY()
	FName DriverClassNameFallback;

	FNetDriverDefinition() :
		DefName(NAME_None),
		DriverClassName(NAME_None),
		DriverClassNameFallback(NAME_None)
	{
	}
};

/**
 * Active and named net drivers instantiated from an FNetDriverDefinition
 * The net driver will remain instantiated on this struct until it is destroyed
 */
USTRUCT()
struct FNamedNetDriver
{
	GENERATED_USTRUCT_BODY()

	/** Instantiation of named net driver */
	UPROPERTY(transient)
	class UNetDriver* NetDriver;

	/** Definition associated with this net driver */
	FNetDriverDefinition* NetDriverDef;

	FNamedNetDriver() :
		NetDriver(nullptr),
		NetDriverDef(nullptr)
	{}

	FNamedNetDriver(class UNetDriver* InNetDriver, FNetDriverDefinition* InNetDriverDef) :
		NetDriver(InNetDriver),
		NetDriverDef(InNetDriverDef)
	{}

	~FNamedNetDriver() {}
};

/** FWorldContext
 *	A context for dealing with UWorlds at the engine level. As the engine brings up and destroys world, we need a way to keep straight
 *	what world belongs to what.
 *
 *	WorldContexts can be thought of as a track. By default we have 1 track that we load and unload levels on. Adding a second context is adding
 *	a second track; another track of progression for worlds to live on. 
 *
 *	For the GameEngine, there will be one WorldContext until we decide to support multiple simultaneous worlds.
 *	For the EditorEngine, there may be one WorldContext for the EditorWorld and one for the PIE World.
 *
 *	FWorldContext provides both a way to manage 'the current PIE UWorld*' as well as state that goes along with connecting/travelling to 
 *  new worlds.
 *
 *	FWorldContext should remain internal to the UEngine classes. Outside code should not keep pointers or try to manage FWorldContexts directly.
 *	Outside code can steal deal with UWorld*, and pass UWorld*s into Engine level functions. The Engine code can look up the relevant context 
 *	for a given UWorld*.
 *
 *  For convenience, FWorldContext can maintain outside pointers to UWorld*s. For example, PIE can tie UWorld* UEditorEngine::PlayWorld to the PIE
 *	world context. If the PIE UWorld changes, the UEditorEngine::PlayWorld pointer will be automatically updated. This is done with AddRef() and
 *  SetCurrentWorld().
 *
 */
USTRUCT()
struct FWorldContext
{
	GENERATED_USTRUCT_BODY()

	/**************************************************************/
	
	TEnumAsByte<EWorldType::Type>	WorldType;

	FSeamlessTravelHandler SeamlessTravelHandler;

	FName ContextHandle;

	/** URL to travel to for pending client connect */
	FString TravelURL;

	/** TravelType for pending client connects */
	uint8 TravelType;

	/** URL the last time we traveled */
	UPROPERTY()
	struct FURL LastURL;

	/** last server we connected to (for "reconnect" command) */
	UPROPERTY()
	struct FURL LastRemoteURL;

	UPROPERTY()
	UPendingNetGame * PendingNetGame;

	/** A list of tag/array pairs that is used at LoadMap time to fully load packages that may be needed for the map/game with DLC, but we can't use DynamicLoadObject to load from the packages */
	UPROPERTY()
	TArray<struct FFullyLoadedPackagesInfo> PackagesToFullyLoad;

	/**
	 * Array of package/ level names that need to be loaded for the pending map change. First level in that array is
	 * going to be made a fake persistent one by using ULevelStreamingPersistent.
	 */
	TArray<FName> LevelsToLoadForPendingMapChange;

	/** Array of already loaded levels. The ordering is arbitrary and depends on what is already loaded and such.	*/
	UPROPERTY()
	TArray<class ULevel*> LoadedLevelsForPendingMapChange;

	/** Human readable error string for any failure during a map change request. Empty if there were no failures.	*/
	FString PendingMapChangeFailureDescription;

	/** If true, commit map change the next frame.																	*/
	uint32 bShouldCommitPendingMapChange:1;

	/** Handles to object references; used by the engine to e.g. the prevent objects from being garbage collected.	*/
	UPROPERTY()
	TArray<class UObjectReferencer*> ObjectReferencers;

	UPROPERTY()
	TArray<struct FLevelStreamingStatus> PendingLevelStreamingStatusUpdates;

	UPROPERTY()
	class UGameViewportClient* GameViewport;

	UPROPERTY()
	class UGameInstance* OwningGameInstance;

	/** A list of active net drivers */
	UPROPERTY(transient)
	TArray<FNamedNetDriver> ActiveNetDrivers;

	int32	PIEInstance;

	FString	PIEPrefix;

	FString PIERemapPrefix;

	bool	RunAsDedicated;

	/** Is this world context waiting for an online login to complete (for PIE) */
	bool	bWaitingOnOnlineSubsystem;

	/**************************************************************/

	/** Outside pointers to CurrentWorld that should be kept in sync if current world changes  */
	TArray<UWorld**> ExternalReferences;

	/** Adds an external reference */
	void AddRef(UWorld*& WorldPtr)
	{
		WorldPtr = ThisCurrentWorld;
		ExternalReferences.AddUnique(&WorldPtr);
	}

	/** Removes an external reference */
	void RemoveRef(UWorld*& WorldPtr)
	{
		ExternalReferences.Remove(&WorldPtr);
		WorldPtr = nullptr;
	}

	/** Set CurrentWorld and update external reference pointers to reflect this*/
	void SetCurrentWorld(UWorld *World)
	{
		for(int32 idx=0; idx < ExternalReferences.Num(); ++idx)
		{
			if (ExternalReferences[idx] && *ExternalReferences[idx] == ThisCurrentWorld)
			{
				*ExternalReferences[idx] = World;
			}
		}

		ThisCurrentWorld = World;
	}

	/** Collect FWorldContext references for garbage collection */
	void AddReferencedObjects(FReferenceCollector& Collector, const UObject* ReferencingObject);

	FORCEINLINE UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	FWorldContext()
		: WorldType(EWorldType::None)
		, ContextHandle(NAME_None)
		, TravelURL()
		, TravelType(0)
		, PendingNetGame(nullptr)
		, bShouldCommitPendingMapChange(0)
		, GameViewport(nullptr)
		, OwningGameInstance(nullptr)
		, PIEInstance(INDEX_NONE)
		, RunAsDedicated(false)
		, bWaitingOnOnlineSubsystem(false)
		, ThisCurrentWorld(nullptr)
	{
	}

private:

	UWorld*	ThisCurrentWorld;
};


USTRUCT()
struct FStatColorMapEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(globalconfig)
	float In;

	UPROPERTY(globalconfig)
	FColor Out;


	FStatColorMapEntry()
		: In(0)
		, Out(ForceInit)
	{
	}

};

USTRUCT()
struct FStatColorMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(globalconfig)
	FString StatName;

	UPROPERTY(globalconfig)
	TArray<struct FStatColorMapEntry> ColorMap;

	UPROPERTY(globalconfig)
	uint32 DisableBlend:1;


	FStatColorMapping()
		: DisableBlend(false)
	{
	}

};

/** Info about one note dropped in the map during PIE. */
USTRUCT()
struct FDropNoteInfo
{
	GENERATED_USTRUCT_BODY()

	/** Location to create Note actor in edited level. */
	UPROPERTY()
	FVector Location;

	/** Rotation to create Note actor in edited level. */
	UPROPERTY()
	FRotator Rotation;

	/** Text to assign to Note actor in edited level. */
	UPROPERTY()
	FString Comment;


	FDropNoteInfo()
		: Location(ForceInit)
		, Rotation(ForceInit)
	{
	}

};

/************************************/

/** On-screen debug message handling */
/** Helper struct for tracking on screen messages. */
USTRUCT()
struct FScreenMessageString
{
	GENERATED_USTRUCT_BODY()

	/** The 'key' for this message. */
	UPROPERTY(transient)
	uint64 Key;

	/** The message to display. */
	UPROPERTY(transient)
	FString ScreenMessage;

	/** The color to display the message in. */
	UPROPERTY(transient)
	FColor DisplayColor;

	/** The number of frames to display it. */
	UPROPERTY(transient)
	float TimeToDisplay;

	/** The number of frames it has been displayed so far. */
	UPROPERTY(transient)
	float CurrentTimeDisplayed;



		FScreenMessageString()
		: Key(0)
		, DisplayColor(ForceInit)
		, TimeToDisplay(0)
		, CurrentTimeDisplayed(0)
		{
		}
	
};

UENUM()
namespace EMatineeCaptureType
{
	enum Type
	{
		AVI		UMETA(DisplayName="AVI Movie"),
		BMP		UMETA(DisplayName="BMP Image Sequence"),
		PNG		UMETA(DisplayName="PNG Image Sequence"),
		JPEG	UMETA(DisplayName="JPEG Image Sequence")
	};
}

USTRUCT()
struct FGameNameRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString OldGameName;

	UPROPERTY()
	FString NewGameName;
};


USTRUCT()
struct FClassRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ObjectName;

	UPROPERTY()
	FString OldClassName;

	UPROPERTY()
	FString NewClassName;

	UPROPERTY()
	FString OldSubobjName;

	UPROPERTY()
	FString NewSubobjName;

	UPROPERTY()
	bool InstanceOnly;
};

USTRUCT()
struct FStructRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString OldStructName;

	UPROPERTY()
	FString NewStructName;
};

USTRUCT()
struct FPluginRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString OldPluginName;

	UPROPERTY()
	FString NewPluginName;
};

class IAnalyticsProvider;

DECLARE_DELEGATE_OneParam(FBeginStreamingPauseDelegate, FViewport*);
DECLARE_DELEGATE(FEndStreamingPauseDelegate);

/**
 * Abstract base class of all Engine classes, responsible for management of systems critical to editor or game systems.
 * Also defines default classes for certain engine systems.
 */
UCLASS(abstract, config=Engine, defaultconfig, transient)
class ENGINE_API UEngine
	: public UObject
	, public FExec
{
	GENERATED_UCLASS_BODY()

private:
	// Fonts.
	UPROPERTY()
	class UFont* TinyFont;

public:
	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, meta=(AllowedClasses="Font", DisplayName="Tiny Font"))
	FStringAssetReference TinyFontName;

private:
	/** @todo document */
	UPROPERTY()
	class UFont* SmallFont;

public:
	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, meta=(AllowedClasses="Font", DisplayName="Small Font"))
	FStringAssetReference SmallFontName;

private:
	/** @todo document */
	UPROPERTY()
	class UFont* MediumFont;

public:
	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, meta=(AllowedClasses="Font", DisplayName="Medium Font"))
	FStringAssetReference MediumFontName;

private:
	/** @todo document */
	UPROPERTY()
	class UFont* LargeFont;

public:
	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, meta=(AllowedClasses="Font", DisplayName="Large Font"))
	FStringAssetReference LargeFontName;

private:
	/** @todo document */
	UPROPERTY()
	class UFont* SubtitleFont;

public:
	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, meta=(AllowedClasses="Font", DisplayName="Subtitle Font"), AdvancedDisplay)
	FStringAssetReference SubtitleFontName;

private:
	/** Any additional fonts that script may use without hard-referencing the font. */
	UPROPERTY()
	TArray<class UFont*> AdditionalFonts;

public:
	/** The "outermost" active matinee, if any. */
	TWeakObjectPtr<AMatineeActor> ActiveMatinee;

	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=Fonts, AdvancedDisplay)
	TArray<FString> AdditionalFontNames;

	/** The class to use for the game console. */
	UPROPERTY()
	TSubclassOf<class UConsole>  ConsoleClass;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="Console", DisplayName="Console Class"))
	FStringClassReference ConsoleClassName;

	/** The class to use for the game viewport client. */
	UPROPERTY()
	TSubclassOf<class UGameViewportClient>  GameViewportClientClass;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="GameViewportClient", DisplayName="Game Viewport Client Class"))
	FStringClassReference GameViewportClientClassName;

	/** The class to use for local players. */
	UPROPERTY()
	TSubclassOf<class ULocalPlayer>  LocalPlayerClass;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="LocalPlayer", DisplayName="Local Player Class"))
	FStringClassReference LocalPlayerClassName;

	/** The class for WorldSettings **/
	UPROPERTY()
	TSubclassOf<class AWorldSettings>  WorldSettingsClass;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="WorldSettings", DisplayName="World Settings Class"))
	FStringClassReference WorldSettingsClassName;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, meta=(MetaClass="NavigationSystem", DisplayName="Navigation System Class"))
	FStringClassReference NavigationSystemClassName;

	/** The class for NavigationSystem **/
	UPROPERTY()
	TSubclassOf<class UNavigationSystem>  NavigationSystemClass;
	
	/** Name of behavior tree manager class */
	UPROPERTY(globalconfig, noclear, meta=(MetaClass="AvoidanceManager", DisplayName="Avoidance Manager Class"))
	FStringClassReference AvoidanceManagerClassName;
	
	/** The class for behavior tree manager **/
	UPROPERTY()
	TSubclassOf<class UAvoidanceManager>  AvoidanceManagerClass;

	/** PhysicsCollisionHandler class we should use by default **/
	UPROPERTY()
	TSubclassOf<class UPhysicsCollisionHandler>	PhysicsCollisionHandlerClass;

	/** Name of PhysicsCollisionHandler class we should use by default. */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="PhysicsCollisionHandler", DisplayName="Physics Collision Handler Class"), AdvancedDisplay)
	FStringClassReference PhysicsCollisionHandlerClassName;

	UPROPERTY(globalconfig, noclear, meta=(MetaClass="GameUserSettings", DisplayName="Game User Settings Class"))
	FStringClassReference GameUserSettingsClassName;

	UPROPERTY()
	TSubclassOf<class UGameUserSettings> GameUserSettingsClass;

	/** name of Controller class to be used as default AIController class for pawns */
	UPROPERTY(globalconfig, noclear, meta = (MetaClass = "AI", DisplayName = "Default AIController class for all Pawns"))
	FStringClassReference AIControllerClassName;

	/** Global instance of the user game settings */
	UPROPERTY()
	class UGameUserSettings* GameUserSettings;

	/** @todo document */
	UPROPERTY()
	TSubclassOf<class ALevelScriptActor>  LevelScriptActorClass;

	/** @todo document */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="LevelScriptActor", DisplayName="Level Script Actor Class"))
	FStringClassReference LevelScriptActorClassName;
	
	/** Name of the base class to use for new blueprints, configurable on a per-game basis */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="Object", AllowAbstract="true", IsBlueprintBaseOnly="true", DisplayName="Default Blueprint Base Class"), AdvancedDisplay)
	FStringClassReference DefaultBlueprintBaseClassName;

	/** Name of a singleton class to create at startup time, configurable per game */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="Object", DisplayName="Game Singleton Class"), AdvancedDisplay)
	FStringClassReference GameSingletonClassName;

	/** A UObject spawned at initialization time to handle game-specific data */
	UPROPERTY()
	UObject *GameSingleton;

	/** The tire type used when no tire type is explicitly applied. */
	UPROPERTY()
	class UTireType* DefaultTireType;

	/** Path to the default tire type */
	UPROPERTY(globalconfig, EditAnywhere, Category=DefaultClasses, meta=(AllowedClasses="TireType", DisplayName="Default Tire Type"), AdvancedDisplay)
	FStringAssetReference DefaultTireTypeName;

	/** The class to use previewing camera animations. */
	UPROPERTY()
	TSubclassOf<class APawn>  DefaultPreviewPawnClass;

	/** The name of the class to use when previewing camera animations. */
	UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="Pawn", DisplayName="Default Preview Pawn Class"), AdvancedDisplay)
	FStringClassReference DefaultPreviewPawnClassName;

	/** Path that levels for play on console will be saved to (relative to FPaths::GameSavedDir()) */
	UPROPERTY(config)
	FString PlayOnConsoleSaveDir;

	/** A global default texture. */
	UPROPERTY()
	class UTexture2D* DefaultTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DefaultTextureName;

	/** A global default diffuse texture.*/
	UPROPERTY()
	class UTexture* DefaultDiffuseTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DefaultDiffuseTextureName;

	/** @todo document */
	UPROPERTY()
	class UTexture2D* DefaultBSPVertexTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DefaultBSPVertexTextureName;

	/** Texture used to get random image grain values for post processing */
	UPROPERTY()
	class UTexture2D* HighFrequencyNoiseTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference HighFrequencyNoiseTextureName;

	/** Texture used to blur out of focus content, mimics the Bokeh shape of actual cameras */
	UPROPERTY()
	class UTexture2D* DefaultBokehTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DefaultBokehTextureName;

	/** The material used to render wireframe meshes. */
	UPROPERTY()
	class UMaterial* WireframeMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference WireframeMaterialName;

#if WITH_EDITORONLY_DATA
	/** A translucent material used to render things in geometry mode. */
	UPROPERTY()
	class UMaterial* GeomMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference GeomMaterialName;
#endif

	/** A material used to render debug meshes. */
	UPROPERTY()
	class UMaterial* DebugMeshMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DebugMeshMaterialName;

	/** Material used for visualizing level membership in lit view port modes. */
	UPROPERTY()
	class UMaterial* LevelColorationLitMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference LevelColorationLitMaterialName;

	/** Material used for visualizing level membership in unlit view port modes. */
	UPROPERTY()
	class UMaterial* LevelColorationUnlitMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference LevelColorationUnlitMaterialName;

	/** Material used for visualizing lighting only w/ lightmap texel density. */
	UPROPERTY()
	class UMaterial* LightingTexelDensityMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference LightingTexelDensityName;

	/** Material used for visualizing level membership in lit view port modes. Uses shading to show axis directions. */
	UPROPERTY()
	class UMaterial* ShadedLevelColorationLitMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference ShadedLevelColorationLitMaterialName;

	/** Material used for visualizing level membership in unlit view port modes.  Uses shading to show axis directions. */
	UPROPERTY()
	class UMaterial* ShadedLevelColorationUnlitMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference ShadedLevelColorationUnlitMaterialName;

	/** Material used to indicate that the associated BSP surface should be removed. */
	UPROPERTY()
	class UMaterial* RemoveSurfaceMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference RemoveSurfaceMaterialName;

	/** Material that renders vertex color as emmissive. */
	UPROPERTY()
	class UMaterial* VertexColorMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorMaterialName;

	/** Material for visualizing vertex colors on meshes in the scene (color only, no alpha) */
	UPROPERTY()
	class UMaterial* VertexColorViewModeMaterial_ColorOnly;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorViewModeMaterialName_ColorOnly;

	/** Material for visualizing vertex colors on meshes in the scene (alpha channel as color) */
	UPROPERTY()
	class UMaterial* VertexColorViewModeMaterial_AlphaAsColor;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorViewModeMaterialName_AlphaAsColor;

	/** Material for visualizing vertex colors on meshes in the scene (red only) */
	UPROPERTY()
	class UMaterial* VertexColorViewModeMaterial_RedOnly;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorViewModeMaterialName_RedOnly;

	/** Material for visualizing vertex colors on meshes in the scene (green only) */
	UPROPERTY()
	class UMaterial* VertexColorViewModeMaterial_GreenOnly;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorViewModeMaterialName_GreenOnly;

	/** Material for visualizing vertex colors on meshes in the scene (blue only) */
	UPROPERTY()
	class UMaterial* VertexColorViewModeMaterial_BlueOnly;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference VertexColorViewModeMaterialName_BlueOnly;

#if WITH_EDITORONLY_DATA
	/** Material used to render bone weights on skeletal meshes */
	UPROPERTY()
	class UMaterial* BoneWeightMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference BoneWeightMaterialName;
#endif

	/** Material used to render constraint limits */
	UPROPERTY()
	class UMaterial* ConstraintLimitMaterial;

	UPROPERTY()
	class UMaterialInstanceDynamic * ConstraintLimitMaterialX;

	UPROPERTY()
	class UMaterialInstanceDynamic * ConstraintLimitMaterialY;

	UPROPERTY()
	class UMaterialInstanceDynamic * ConstraintLimitMaterialZ;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference ConstraintLimitMaterialName;

	/** Material that renders a message about lightmap settings being invalid. */
	UPROPERTY()
	class UMaterial* InvalidLightmapSettingsMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference InvalidLightmapSettingsMaterialName;

	/** Material that renders a message about preview shadows being used. */
	UPROPERTY()
	class UMaterial* PreviewShadowsIndicatorMaterial;

	/** @todo document */
	UPROPERTY(globalconfig, EditAnywhere, Category=DefaultMaterials, meta=(AllowedClasses="Material", DisplayName="Preview Shadows Indicator Material"))
	FStringAssetReference PreviewShadowsIndicatorMaterialName;

	/** Material that 'fakes' lighting, used for arrows, widgets. */
	UPROPERTY()
	class UMaterial* ArrowMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference ArrowMaterialName;

	/** @todo document */
	UPROPERTY(globalconfig)
	FLinearColor LightingOnlyBrightness;

	/** The colors used to render light complexity. */
	UPROPERTY(globalconfig)
	TArray<FColor> LightComplexityColors;

	/** The colors used to render shader complexity. */
	UPROPERTY(globalconfig)
	TArray<FLinearColor> ShaderComplexityColors;

	/** The colors used to render stationary light overlap. */
	UPROPERTY(globalconfig)
	TArray<FLinearColor> StationaryLightOverlapColors;

	/**
	* Complexity limits for the various complexity view mode combinations.
	* These limits are used to map instruction counts to ShaderComplexityColors.
	*/
	UPROPERTY(globalconfig)
	float MaxPixelShaderAdditiveComplexityCount;

	UPROPERTY(globalconfig)
	float MaxES2PixelShaderAdditiveComplexityCount;

	/** Range for the lightmap density view mode. */
	/** Minimum lightmap density value for coloring. */
	UPROPERTY(globalconfig)
	float MinLightMapDensity;

	/** Ideal lightmap density value for coloring. */
	UPROPERTY(globalconfig)
	float IdealLightMapDensity;

	/** Maximum lightmap density value for coloring. */
	UPROPERTY(globalconfig)
	float MaxLightMapDensity;

	/** If true, then render gray scale density. */
	UPROPERTY(globalconfig)
	uint32 bRenderLightMapDensityGrayscale:1;

	/** The scale factor when rendering gray scale density. */
	UPROPERTY(globalconfig)
	float RenderLightMapDensityGrayscaleScale;

	/** The scale factor when rendering color density. */
	UPROPERTY(globalconfig)
	float RenderLightMapDensityColorScale;

	/** The color to render vertex mapped objects in for LightMap Density view mode. */
	UPROPERTY(globalconfig)
	FLinearColor LightMapDensityVertexMappedColor;

	/** The color to render selected objects in for LightMap Density view mode. */
	UPROPERTY(globalconfig)
	FLinearColor LightMapDensitySelectedColor;

	/** @todo document */
	UPROPERTY(globalconfig)
	TArray<struct FStatColorMapping> StatColorMappings;

#if WITH_EDITORONLY_DATA
	/** A material used to render the sides of the builder brush/volumes/etc. */
	UPROPERTY()
	class UMaterial* EditorBrushMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference EditorBrushMaterialName;
#endif

	/** PhysicalMaterial to use if none is defined for a particular object. */
	UPROPERTY()
	class UPhysicalMaterial* DefaultPhysMaterial;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference DefaultPhysMaterialName;

	UPROPERTY(config)
	TArray<FGameNameRedirect> ActiveGameNameRedirects;

	UPROPERTY(config)
	TArray<FClassRedirect> ActiveClassRedirects;

	UPROPERTY(config)
	TArray<FPluginRedirect> ActivePluginRedirects;

	UPROPERTY(config)
	TArray<FStructRedirect> ActiveStructRedirects;

	/** Texture used for pre-integrated skin shading */
	UPROPERTY()
	class UTexture2D* PreIntegratedSkinBRDFTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference PreIntegratedSkinBRDFTextureName;

	/** Texture used to do font rendering in shaders */
	UPROPERTY()
	class UTexture2D* MiniFontTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference MiniFontTextureName;

	/** Texture used as a placeholder for terrain weight-maps to give the material the correct texture format. */
	UPROPERTY()
	class UTexture* WeightMapPlaceholderTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference WeightMapPlaceholderTextureName;

	/** Texture used to display LightMapDensity */
	UPROPERTY()
	class UTexture2D* LightMapDensityTexture;

	/** @todo document */
	UPROPERTY(globalconfig)
	FStringAssetReference LightMapDensityTextureName;

	// Variables.

	/** Engine loop, used for callbacks from the engine module into launch. */
	class IEngineLoop* EngineLoop;

	/** The view port representing the current game instance. Can be 0 so don't use without checking. */
	UPROPERTY()
	class UGameViewportClient* GameViewport;

	/** Array of deferred command strings/ execs that get executed at the end of the frame */
	UPROPERTY()
	TArray<FString> DeferredCommands;

	/** @todo document */
	UPROPERTY()
	int32 TickCycles;

	/** @todo document */
	UPROPERTY()
	int32 GameCycles;

	/** @todo document */
	UPROPERTY()
	int32 ClientCycles;

	/** The distance of the camera's near clipping plane. */
	UPROPERTY(EditAnywhere, config, Category=Settings)
	float NearClipPlane;

	/** Can the editor report usage analytics (types of assets being spawned, etc...) back to Epic in order for us to improve the editor user experience?  Note: The editor must be restarted for changes to take effect. */
	UPROPERTY(EditAnywhere, config, Category=Settings, AdvancedDisplay)
	uint32 bEditorAnalyticsEnabled:1;

	/** Can a runtime game/application report anonymous hardware survey statistics (such as display resolution and GPU model) back to Epic? */
	UPROPERTY(EditAnywhere, config, Category=Settings, AdvancedDisplay)
	uint32 bHardwareSurveyEnabled:1;

	/** Flag for completely disabling subtitles for localized sounds. */
	UPROPERTY(EditAnywhere, config, Category=Subtitles)
	uint32 bSubtitlesEnabled:1;

	/** Flag for forcibly disabling subtitles even if you try to turn them back on they will be off */
	UPROPERTY(EditAnywhere, config, Category=Subtitles)
	uint32 bSubtitlesForcedOff:1;

	/** Time in seconds (game time) we should wait between purging object references to objects that are pending kill */
	UPROPERTY(EditAnywhere, config, Category=Settings, AdvancedDisplay)
	float TimeBetweenPurgingPendingKillObjects;

	/** Whether to allow background level streaming. */
	UPROPERTY(EditAnywhere, config, Category=LevelStreaming)
	uint32 bUseBackgroundLevelStreaming:1;

	/** Maximum amount of time to spend doing asynchronous loading (ms per frame) */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay)
	float AsyncLoadingTimeLimit;

	/** Whether to use the entire time limit even if blocked on I/O */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay)
	uint32 bAsyncLoadingUseFullTimeLimit:1;
	
	/** Additional time to spend asynchronous loading during a "high priority" load */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay)
	float PriorityAsyncLoadingExtraTime;

	/** Maximum allowed time to spend for actor registration steps during level streaming (ms per frame)*/
	UPROPERTY(EditAnywhere, config, Category=LevelStreaming, AdvancedDisplay)
	float LevelStreamingActorsUpdateTimeLimit;
	
	/** Batching granularity used to register actor components during level streaming */
	UPROPERTY(EditAnywhere, config, Category=LevelStreaming, AdvancedDisplay)
	int32 LevelStreamingComponentsRegistrationGranularity;

	/** Script maximum loop iteration count used as a threshold to warn users about script execution runaway */
	UPROPERTY(EditAnywhere, config, Category=Blueprints)
	int32 MaximumLoopIterationCount;

	/** @todo document */
	UPROPERTY(config)
	uint32 bEnableEditorPSysRealtimeLOD:1;

	/** Whether to enable framerate smoothing.																		*/
	UPROPERTY(config, EditAnywhere, Category=Framerate)
	uint32 bSmoothFrameRate:1;

	/** Range of framerates in which smoothing will kick in */
	UPROPERTY(config, EditAnywhere, Category=Framerate, meta=(UIMin=0, UIMax=200))
	FFloatRange SmoothedFrameRateRange;

	/** 
	 * Whether we should check for more than N pawns spawning in a single frame.  
	 * Basically, spawning pawns and all of their attachments can be slow.  And on consoles it
	 * can be really slow.  If this bool is true we will display a 
	 **/
	UPROPERTY(config)
	uint32 bCheckForMultiplePawnsSpawnedInAFrame:1;

	/** If bCheckForMultiplePawnsSpawnedInAFrame==true, then we will check to see that no more than this number of pawns are spawned in a frame. **/
	UPROPERTY(config)
	int32 NumPawnsAllowedToBeSpawnedInAFrame;

	/**
	 * Whether or not the LQ lightmaps should be generated during lighting rebuilds.
	 */
	UPROPERTY(globalconfig)
	uint32 bShouldGenerateLowQualityLightmaps:1;

	/**
	 * Bool that indicates that 'console' input is desired. This flag is mis named as it is used for a lot of gameplay related things
	 * (e.g. increasing collision size, changing vehicle turning behavior, modifying put down/up weapon speed, bot behavior)
	 *
	 * currently set when you are running a console build (implicitly or explicitly via ?param on the commandline)
	 */
	uint32 bUseConsoleInput:1;

	// Color preferences.
	UPROPERTY()
	FColor C_WorldBox;

	/** @todo document */
	UPROPERTY()
	FColor C_BrushWire;

	/** @todo document */
	UPROPERTY()
	FColor C_AddWire;

	/** @todo document */
	UPROPERTY()
	FColor C_SubtractWire;

	/** @todo document */
	UPROPERTY()
	FColor C_SemiSolidWire;

	/** @todo document */
	UPROPERTY()
	FColor C_NonSolidWire;

	/** @todo document */
	UPROPERTY()
	FColor C_WireBackground;

	/** @todo document */
	UPROPERTY()
	FColor C_ScaleBoxHi;

	/** @todo document */
	UPROPERTY()
	FColor C_VolumeCollision;

	/** @todo document */
	UPROPERTY()
	FColor C_BSPCollision;

	/** @todo document */
	UPROPERTY()
	FColor C_OrthoBackground;

	/** @todo document */
	UPROPERTY()
	FColor C_Volume;

	/** @todo document */
	UPROPERTY()
	FColor C_BrushShape;

	/** Fudge factor for tweaking the distance based miplevel determination */
	UPROPERTY(EditAnywhere, Category=LevelStreaming, AdvancedDisplay)
	float StreamingDistanceFactor;

	/** The current transition type. */
	UPROPERTY()
	TEnumAsByte<enum ETransitionType> TransitionType;

	/** The current transition description text. */
	UPROPERTY()
	FString TransitionDescription;

	/** The gamemode for the destination map */
	UPROPERTY()
	FString TransitionGameMode;

	/** Level of detail range control for meshes */
	UPROPERTY(config)
	float MeshLODRange;

	/** whether mature language is allowed **/
	UPROPERTY(config)
	uint32 bAllowMatureLanguage:1;

	/** camera rotation (deg) beyond which occlusion queries are ignored from previous frame (because they are likely not valid) */
	UPROPERTY(config)
	float CameraRotationThreshold;

	/** camera movement beyond which occlusion queries are ignored from previous frame (because they are likely not valid) */
	UPROPERTY(config)
	float CameraTranslationThreshold;

	/** The amount of time a primitive is considered to be probably visible after it was last actually visible. */
	UPROPERTY(config)
	float PrimitiveProbablyVisibleTime;

	/** Max screen pixel fraction where retesting when unoccluded is worth the GPU time. */
	UPROPERTY(config)
	float MaxOcclusionPixelsFraction;

	/** Do not use Ageia PhysX hardware */
	UPROPERTY(globalconfig)
	uint32 bDisablePhysXHardwareSupport:1;

	/** Whether to pause the game if focus is lost. */
	UPROPERTY(config)
	uint32 bPauseOnLossOfFocus:1;

	/**
	 *	The maximum allowed size to a ParticleEmitterInstance::Resize call.
	 *	If larger, the function will return without resizing.
	 */
	UPROPERTY(config)
	int32 MaxParticleResize;

	/** If the resize request is larger than this, spew out a warning to the log */
	UPROPERTY(config)
	int32 MaxParticleResizeWarn;

	/** @todo document */
	UPROPERTY(transient)
	TArray<struct FDropNoteInfo> PendingDroppedNotes;

	/** Error correction data for replicating simulated physics (rigid bodies) */
	UPROPERTY(config)
	FRigidBodyErrorCorrection PhysicErrorCorrection;

	/** Number of times to tick each client per second */
	UPROPERTY(globalconfig)
	float NetClientTicksPerSecond;

	/** true if the engine needs to perform a delayed global component reregister (really just for editor) */
	UPROPERTY(transient)
	uint32 bHasPendingGlobalReregister:1;

	/** Current display gamma setting */
	UPROPERTY(config)
	float DisplayGamma;

	/** Minimum desired framerate setting */
	UPROPERTY(config, EditAnywhere, Category=Framerate, meta=(UIMin=0, ClampMin=0))
	float MinDesiredFrameRate;

private:
	/** Default color of selected objects in the level viewport (additive) */
	UPROPERTY(globalconfig)
	FLinearColor DefaultSelectedMaterialColor;

	/** Color of selected objects in the level viewport (additive) */
	UPROPERTY(transient)
	FLinearColor SelectedMaterialColor;

	/** Color of the selection outline color.  Generally the same as selected material color unless the selection material color is being overridden */
	UPROPERTY(transient)
	FLinearColor SelectionOutlineColor;

	/** An override to use in some cases instead of the selected material color */
	UPROPERTY(transient)
	FLinearColor SelectedMaterialColorOverride;

	/** Whether or not selection color is being overriden */
	UPROPERTY(transient)
	bool bIsOverridingSelectedColor;
public:

	/** If true, then disable OnScreenDebug messages. Can be toggled in real-time. */
	UPROPERTY(globalconfig)
	uint32 bEnableOnScreenDebugMessages:1;

	/** If true, then disable the display of OnScreenDebug messages (used when running) */
	UPROPERTY(transient)
	uint32 bEnableOnScreenDebugMessagesDisplay:1;

	/** If true, then skip drawing map warnings on screen even in non (UE_BUILD_SHIPPING || UE_BUILD_TEST) builds */
	UPROPERTY(globalconfig)
	uint32 bSuppressMapWarnings:1;

	/** if set, cook game classes into standalone packages (as defined in [Cooker.MPGameContentCookStandalone]) and load the appropriate
	 * one at game time depending on the gametype specified on the URL
	 * (the game class should then not be referenced in the maps themselves)
	 */
	UPROPERTY(globalconfig)
	uint32 bCookSeparateSharedMPGameContent:1;

	/** determines whether AI logging should be processed or not */
	UPROPERTY(globalconfig)
	uint32 bDisableAILogging:1;

	UPROPERTY(globalconfig)
	uint32 bEnableVisualLogRecordingOnStart;

private:

	/** Used to hold the device profile manager and device profiles */
	UPROPERTY(transient)
	UDeviceProfileManager* DeviceProfileManager;

public:

	/**
	 * Get the reference to the engines device profile manager
	 *
	 * return A reference to the dpm singleton, initializes if not already.
	 */
	UDeviceProfileManager* GetDeviceProfileManager();

private:

	/** Whether the engine should be playing sounds.  If false at initialization time the AudioDevice will not be created */
	uint32 bUseSound:1;

	/** Semaphore to control screen saver inhibitor thread access. */
	UPROPERTY(transient)
	int32 ScreenSaverInhibitorSemaphore;

public:

	/** determines if we should start the matinee capture as soon as the game loads */
	UPROPERTY(transient)
	uint32 bStartWithMatineeCapture:1;

	/** should we compress the capture */
	UPROPERTY(transient)
	uint32 bCompressMatineeCapture:1;

	/** the name of the matine that we want to record */
	UPROPERTY(transient)
	FString MatineeCaptureName;

	/** The package name where the matinee belongs to */
	UPROPERTY(transient)
	FString MatineePackageCaptureName;

	/** the fps of the matine that we want to record */
	UPROPERTY(transient)
	int32 MatineeCaptureFPS;

	/** The capture type, e.g. AVI or Screen Shots */
	UPROPERTY(transient)
	TEnumAsByte<EMatineeCaptureType::Type> MatineeCaptureType;

	/** Whether or not to disable texture streaming during matinee movie capture */
	UPROPERTY(transient)
	bool bNoTextureStreaming;

	/** true if the the user cannot modify levels that are read only. */
	UPROPERTY(transient)
	uint32 bLockReadOnlyLevels:1;

	/** Particle event manager **/
	UPROPERTY(globalconfig)
	FString ParticleEventManagerClassPath;

	/** A collection of messages to display on-screen. */
	TArray<struct FScreenMessageString> PriorityScreenMessages;

	/** Used to alter the intensity level of the selection highlight on selected objects */
	UPROPERTY(transient)
	float SelectionHighlightIntensity;

	/** Used to alter the intensity level of the selection highlight on selected BSP surfaces */
	UPROPERTY(transient)
	float BSPSelectionHighlightIntensity;

	/** Used to alter the intensity level of the selection highlight on hovered objects */
	UPROPERTY(transient)
	float HoverHighlightIntensity;

	/** Used to alter the intensity level of the selection highlight on selected billboard objects */
	UPROPERTY(transient)
	float SelectionHighlightIntensityBillboards;

	/** Delegate handling when streaming pause begins. Set initially in FStreamingPauseRenderingModule::StartupModule() but can then be overriden by games. */
	void RegisterBeginStreamingPauseRenderingDelegate( FBeginStreamingPauseDelegate* InDelegate );
	FBeginStreamingPauseDelegate* BeginStreamingPauseDelegate;

	/** Delegate handling when streaming pause ends. Set initially in FStreamingPauseRenderingModule::StartupModule() but can then be overriden by games. */
	void RegisterEndStreamingPauseRenderingDelegate( FEndStreamingPauseDelegate* InDelegate );
	FEndStreamingPauseDelegate* EndStreamingPauseDelegate;

	/** 
	 * Error message event relating to server travel failures 
	 * 
	 * @param Type type of travel failure
	 * @param ErrorString additional error message
	 */
	DECLARE_EVENT_ThreeParams(UEngine, FOnTravelFailure, UWorld*, ETravelFailure::Type, const FString&);
	FOnTravelFailure TravelFailureEvent;

	/** 
	 * Error message event relating to network failures 
	 * 
	 * @param Type type of network failure
	 * @param Name name of netdriver that generated the failure
	 * @param ErrorString additional error message
	 */
	DECLARE_EVENT_FourParams(UEngine, FOnNetworkFailure, UWorld*, UNetDriver*, ENetworkFailure::Type, const FString&);
	FOnNetworkFailure NetworkFailureEvent;

	// for IsInitialized()
	bool bIsInitialized;

	/**
	 * Get the color to use for object selection
	 */
	const FLinearColor& GetSelectedMaterialColor() const { return bIsOverridingSelectedColor ? SelectedMaterialColorOverride : SelectedMaterialColor; }

	const FLinearColor& GetSelectionOutlineColor() const { return SelectionOutlineColor; }

	const FLinearColor& GetHoveredMaterialColor() const { return GetSelectedMaterialColor(); }

	/**
	 * Sets the selected material color.  
	 * Do not use this if you plan to override the selected material color.  Use OverrideSelectedMaterialColor instead
	 * This is set by the editor preferences
	 *
	 * @param SelectedMaterialColor	The new selection color
	 */
	void SetSelectedMaterialColor( const FLinearColor& InSelectedMaterialColor ) { SelectedMaterialColor = InSelectedMaterialColor; }

	void SetSelectionOutlineColor( const FLinearColor& InSelectionOutlineColor ) { SelectionOutlineColor = InSelectionOutlineColor; }

	/**
	 * Sets an override color to use instead of the user setting
	 *
	 * @param OverrideColor	The override color to use
	 */
	void OverrideSelectedMaterialColor( const FLinearColor& OverrideColor );

	/**
	 * Restores the selected material color back to the user setting
	 */
	void RestoreSelectedMaterialColor();
public:

	/** A collection of messages to display on-screen. */
	TMap<int32, FScreenMessageString> ScreenMessages;

	/** The audio device */
	class FAudioDevice* AudioDevice;

	/** Reference to the stereoscopic rendering interace, if any */
	TSharedPtr< class IStereoRendering > StereoRenderingDevice;

	/** Reference to the HMD device that is attached, if any */
	TSharedPtr< class IHeadMountedDisplay > HMDDevice;
	
	/** Triggered when a world is added. */	
	DECLARE_EVENT_OneParam( UEngine, FWorldAddedEvent , UWorld* );
	
	/** Return the world added event. */
	FWorldAddedEvent&		OnWorldAdded() { return WorldAddedEvent; }
	
	/** Triggered when a world is destroyed. */	
	DECLARE_EVENT_OneParam( UEngine, FWorldDestroyedEvent , UWorld* );
	
	/** Return the world destroyed event. */	
	FWorldDestroyedEvent&	OnWorldDestroyed() { return WorldDestroyedEvent; }
	
	/** Needs to be called when a world is added to broadcast messages. */	
	virtual void			WorldAdded( UWorld* World );
	
	/** Needs to be called when a world is destroyed to broadcast messages. */	
	virtual void			WorldDestroyed( UWorld* InWorld );

	virtual bool IsInitialized() const { return bIsInitialized; }

#if WITH_EDITOR

	/** Editor-only event triggered when the actor list of the world has changed */
	DECLARE_EVENT( UEngine, FLevelActorListChangedEvent );
	FLevelActorListChangedEvent& OnLevelActorListChanged() { return LevelActorListChangedEvent; }

	/** Called by internal engine systems after a world's actor list changes in a way not specifiable through other LevelActor__Events to notify other subsystems */
	void BroadcastLevelActorListChanged() { LevelActorListChangedEvent.Broadcast(); }

	/** Editor-only event triggered when actors are added to the world */
	DECLARE_EVENT_OneParam( UEngine, FLevelActorAddedEvent, AActor* );
	FLevelActorAddedEvent& OnLevelActorAdded() { return LevelActorAddedEvent; }

	/** Called by internal engine systems after a level actor has been added */
	void BroadcastLevelActorAdded(AActor* InActor) { LevelActorAddedEvent.Broadcast(InActor); }

	/** Editor-only event triggered when actors are deleted from the world */
	DECLARE_EVENT_OneParam( UEngine, FLevelActorDeletedEvent, AActor* );
	FLevelActorDeletedEvent& OnLevelActorDeleted() { return LevelActorDeletedEvent; }

	/** Called by internal engine systems after level actors have changed to notify other subsystems */
	void BroadcastLevelActorDeleted(AActor* InActor) { LevelActorDeletedEvent.Broadcast(InActor); }

	/** Editor-only event triggered when actors are attached in the world */
	DECLARE_EVENT_TwoParams( UEngine, FLevelActorAttachedEvent, AActor*, const AActor* );
	FLevelActorAttachedEvent& OnLevelActorAttached() { return LevelActorAttachedEvent; }

	/** Called by internal engine systems after a level actor has been attached */
	void BroadcastLevelActorAttached(AActor* InActor, const AActor* InParent) { LevelActorAttachedEvent.Broadcast(InActor, InParent); }

	/** Editor-only event triggered when actors are detached in the world */
	DECLARE_EVENT_TwoParams( UEngine, FLevelActorDetachedEvent, AActor*, const AActor* );
	FLevelActorDetachedEvent& OnLevelActorDetached() { return LevelActorDetachedEvent; }

	/** Called by internal engine systems after a level actor has been detached */
	void BroadcastLevelActorDetached(AActor* InActor, const AActor* InParent) { LevelActorDetachedEvent.Broadcast(InActor, InParent); }

	/** Editor-only event triggered when actors' folders are changed */
	DECLARE_EVENT_TwoParams( UEngine, FLevelActorFolderChangedEvent, const AActor*, FName );
	FLevelActorFolderChangedEvent& OnLevelActorFolderChanged() { return LevelActorFolderChangedEvent; }

	/** Called by internal engine systems after a level actor's folder has been changed */
	void BroadcastLevelActorFolderChanged(const AActor* InActor, FName OldPath) { LevelActorFolderChangedEvent.Broadcast(InActor, OldPath); }

	/** Editor-only event triggered after an actor is moved, rotated or scaled (AActor::PostEditMove) */
	DECLARE_EVENT_OneParam( UEngine, FOnActorMovedEvent, AActor* );
	FOnActorMovedEvent& OnActorMoved() { return OnActorMovedEvent; }

	/** Called by internal engine systems after an actor has been moved to notify other subsystems */
	void BroadcastOnActorMoved( AActor* Actor ) { OnActorMovedEvent.Broadcast( Actor ); }

	/** Editor-only event triggered when actors are being requested to be renamed */
	DECLARE_EVENT_OneParam( UEngine, FLevelActorRequestRenameEvent, const AActor* );
	FLevelActorRequestRenameEvent& OnLevelActorRequestRename() { return LevelActorRequestRenameEvent; }

	/** Called by internal engine systems after a level actor has been requested to be renamed */
	void BroadcastLevelActorRequestRename(const AActor* InActor) { LevelActorRequestRenameEvent.Broadcast(InActor); }

#endif // #if WITH_EDITOR

	/** Event triggered after a server travel failure of any kind has occurred */
	FOnTravelFailure& OnTravelFailure() { return TravelFailureEvent; }
	/** Called by internal engine systems after a travel failure has occurred */
	void BroadcastTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString = TEXT(""))
	{
		TravelFailureEvent.Broadcast(InWorld, FailureType, ErrorString);
	}

	/** Event triggered after a network failure of any kind has occurred */
	FOnNetworkFailure& OnNetworkFailure() { return NetworkFailureEvent; }
	/** Called by internal engine systems after a network failure has occurred */
	void BroadcastNetworkFailure(UWorld * World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString = TEXT(""))
	{
		NetworkFailureEvent.Broadcast(World, NetDriver, FailureType, ErrorString);
	}

	// Begin UObject interface.
	virtual void FinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface.

	/** Initialize the game engine. */
	virtual void Init(IEngineLoop* InEngineLoop);

	/** Called at shutdown, just before the exit purge.	 */
	virtual void PreExit();
	virtual void ShutdownAudioDevice();

	/** Called at startup, in the middle of FEngineLoop::Init.	 */
	void ParseCommandline();

	// Begin FExec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out=*GLog ) override;
	// End FExec Interface

	/** 
	 * Exec command handlers
	 */
	bool HandleFlushLogCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleGameVerCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStatCommand( UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStartMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStopMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCrackURLCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDeferCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	bool HandleCeCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpTicksCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleGammaCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	// Only compile in when STATS is set
#if STATS
	bool HandleDumpParticleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif

	// Compile in Debug or Development
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if WITH_HOT_RELOAD
	bool HandleHotReloadCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif
	bool HandleDumpConsoleCommandsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleShowMaterialDrawEventsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpAvailableResolutionsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleAnimSeqStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCountDisabledParticleItemsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleViewnamesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleFreezeStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );		// Smedis
	bool HandleFreezeAllCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );			// Smedis
	bool HandleFlushIOManagerCommand( const TCHAR* Cmd, FOutputDevice& Ar );						// Smedis
	bool HandleToggleRenderingThreadCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleRHIThreadCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleRecompileShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleRecompileGlobalShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpShaderStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpMaterialStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleProfileGPUCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleProfileCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleProfileGPUHitchesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleShaderComplexityCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleFreezeRenderingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleShowSelectedLightmapCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Compile in Debug, Development, and Test
#if !UE_BUILD_SHIPPING
	bool HandleShowLogCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStartFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStopFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleDumpLevelScriptActorsCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleKismetEventCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleRemoteTextureStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListParticleSystemsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSpawnedActorsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleLogoutStatLevelsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleMemReportCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleMemReportDeferredCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleParticleMeshUsageCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpParticleCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListPreCacheMapPackagesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListLoadedPackagesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDebugCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleMergeMeshCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleContentComparisonCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTogglegtPsysLODCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleObjCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTestslateGameUICommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDirCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTrackParticleRenderingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpAllocatorStats( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleHeapCheckCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleOnscreenDebugMessageDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleOnscreenDebugMessageSystemCommand( const TCHAR* Cmd, FOutputDevice& Ar );	
	bool HandleDisableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar );			
	bool HandleEnableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar );			
	bool HandleToggleAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar );			
	bool HandleConfigHashCommand( const TCHAR* Cmd, FOutputDevice& Ar );						
	bool HandleConfigMemCommand( const TCHAR* Cmd, FOutputDevice& Ar );							
#endif // !UE_BUILD_SHIPPING

	/** Update everything. */
	virtual void Tick( float DeltaSeconds, bool bIdleMode ) PURE_VIRTUAL(UEngine::Tick,);

	/**
	 * Update FApp::CurrentTime/ FApp::DeltaTime while taking into account max tick rate.
	 */
	void UpdateTimeAndHandleMaxTickRate();

	/** Executes the deferred commands **/
	void TickDeferredCommands();

	/** Get tick rate limiter. */
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing = true) const;

	/** Updates the running average delta time */
	virtual void UpdateRunningAverageDeltaTime(float DeltaTime, bool bAllowFrameRateSmoothing = true);

	/** Whether we're allowed to do frame rate smoothing */
	bool IsAllowedFramerateSmoothing(bool bAllowFrameRateSmoothing) const;

	/**
	 * Pauses / un-pauses the game-play when focus of the game's window gets lost / gained.
	 * @param EnablePause true to pause; false to unpause the game
	 */
	virtual void OnLostFocusPause( bool EnablePause );

	/** Functions to start and finish the hardware survey. */
	void InitHardwareSurvey();
	void TickHardwareSurvey();

	static FString HardwareSurveyBucketResolution(uint32 DisplayWidth, uint32 DisplayHeight);
	static FString HardwareSurveyBucketVRAM(uint32 VidMemoryMB);
	static FString HardwareSurveyBucketRAM(uint32 MemoryMB);
	static FString HardwareSurveyGetResolutionClass(uint32 LargestDisplayHeight);

	/** 
	 * Returns the average game/render/gpu/total time since this function was last called
	 */
	void GetAverageUnitTimes( TArray<float>& AverageTimes );

	/**
	 * Updates the values used to calculate the average game/render/gpu/total time
	 */
	void SetAverageUnitTimes(float FrameTime, float RenderThreadTime, float GameThreadTime, float GPUFrameTime);

	/**
	 * @return true to throttle CPU usage based on current state (usually editor minimized or not in foreground)
	 */
	virtual bool ShouldThrottleCPUUsage() const;
protected:
	/** 
	 * Determines whether a hardware survey should be run now.
	 * Default implementation checks in config for when the survey was last performed
	 * The default interval is determined by EngineDefs::HardwareSurveyInterval
	 *
	 * @return	true if the engine should run the hardware survey now
	 */
	virtual bool IsHardwareSurveyRequired();

	/** 
	 * Runs when a hardware survey has been completed successfully.
	 * Default implementation sets some config values to remember when the survey happened then
	 * takes raw hardware survey results and records events using the engine's analytics provider.
	 *
	 * @param SurveyResults		The raw survey results generated by the platform hardware survey code
	 */
	virtual void OnHardwareSurveyComplete(const struct FHardwareSurveyResults& SurveyResults);
	
public:
	/** 
	 * Return a reference to the GamePlayers array. 
	 */

	TArray<class ULocalPlayer*>::TConstIterator	GetLocalPlayerIterator(UWorld *World);
	TArray<class ULocalPlayer*>::TConstIterator GetLocalPlayerIterator(const UGameViewportClient *Viewport);

	const TArray<class ULocalPlayer*>& GetGamePlayers(UWorld *World) const;
	const TArray<class ULocalPlayer*>& GetGamePlayers(const UGameViewportClient *Viewport) const;

	/**
	 *	Returns the first ULocalPlayer that matches the given ControllerId. 
	 *  This will search across all world contexts.
	 */
	class ULocalPlayer* FindFirstLocalPlayerFromControllerId(int32 ControllerId) const;

	/**
	 * return the number of entries in the GamePlayers array
	 */
	int32 GetNumGamePlayers(UWorld *InWorld);
	int32 GetNumGamePlayers(const UGameViewportClient *InViewport);

	/**
	 * return the ULocalPlayer with the given index.
	 *
	 * @param	InPlayer		Index of the player required
	 *
	 * @returns	pointer to the LocalPlayer with the given index
	 */
	ULocalPlayer* GetGamePlayer( UWorld * InWorld, int32 InPlayer );
	ULocalPlayer* GetGamePlayer( const UGameViewportClient* InViewport, int32 InPlayer );
	
	/**
	 * return the first ULocalPlayer in the GamePlayers array.
	 *
	 * @returns	first ULocalPlayer or nullptr if the array is empty
	 */
	ULocalPlayer* GetFirstGamePlayer( UWorld *InWorld );
	ULocalPlayer* GetFirstGamePlayer(const UGameViewportClient *InViewport );
	ULocalPlayer* GetFirstGamePlayer( UPendingNetGame *PendingNetGame );

	/**
	 * returns the first ULocalPlayer that should be used for debug purposes.
	 * This should only be used in very special cases where no UWorld* is available!
	 * Anything using this will not function properly under multiple worlds.
	 * Always prefer to use GetFirstGamePlayer() or even better - FLocalPlayerIterator
	 *
	 * @returns the first ULocalPlayer
	 */
	ULocalPlayer* GetDebugLocalPlayer();

	/** Clean up the GameViewport */
	void CleanupGameViewport();

	/** Allows the editor to accept or reject the drawing of wire frame brush shapes based on mode and tool. */
	virtual bool ShouldDrawBrushWireframe( class AActor* InActor ) { return true; }

	/** Returns whether or not the map build in progressed was canceled by the user. */
	virtual bool GetMapBuildCancelled() const
	{
		return false;
	}

	/**
	 * Sets the flag that states whether or not the map build was canceled.
	 *
	 * @param InCancelled	New state for the canceled flag.
	 */
	virtual void SetMapBuildCancelled( bool InCancelled )
	{
		// Intentionally empty.
	}

	/**
	 * Computes a color to use for property coloration for the given object.
	 *
	 * @param	Object		The object for which to compute a property color.
	 * @param	OutColor	[out] The returned color.
	 * @return				true if a color was successfully set on OutColor, false otherwise.
	 */
	virtual bool GetPropertyColorationColor(class UObject* Object, FColor& OutColor);

	/** Uses StatColorMappings to find a color for this stat's value. */
	bool GetStatValueColoration(const FString& StatName, float Value, FColor& OutColor);

	/** @return true if selection of translucent objects in perspective view ports is allowed */
	virtual bool AllowSelectTranslucent() const
	{
		// The editor may override this to disallow translucent selection based on user preferences
		return true;
	}

	/** @return true if only editor-visible levels should be loaded in Play-In-Editor sessions */
	virtual bool OnlyLoadEditorVisibleLevelsInPIE() const
	{
		// The editor may override this to apply the user's preference state
		return true;
	}

	/**
	 * Enables or disables the ScreenSaver (PC only)
	 *
	 * @param bEnable	If true the enable the screen saver, if false disable it.
	 */
	void EnableScreenSaver( bool bEnable );
	
	/**
	 * Get the index of the provided sprite category
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 *
	 * @return	Index of the provided sprite category, if possible; INDEX_NONE otherwise
	 */
	virtual int32 GetSpriteCategoryIndex( const FName& InSpriteCategory )
	{
		// The editor may override this to handle sprite categories as necessary
		return INDEX_NONE;
	}

	/** Looks up the GUID of a package on disk. The package must NOT be in the auto-download cache.
	 * This may require loading the header of the package in question and is therefore slow.
	 */
	static FGuid GetPackageGuid(FName PackageName);

	/**
	 * Returns whether we are running on a console platform or on the PC.
	 * @param ConsoleType - if specified, only returns true if we're running on the specified platform
	 *
	 * @return true if we're on a console, false if we're running on a PC
	 */
	bool IsConsoleBuild(EConsoleType ConsoleType = CONSOLE_Any) const;

	/** Add a FString to the On-screen debug message system */
	void AddOnScreenDebugMessage(uint64 Key,float TimeToDisplay,FColor DisplayColor,const FString& DebugMessage);

	/** Add a FString to the On-screen debug message system */
	void AddOnScreenDebugMessage(int32 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage);

	/** Retrieve the message for the given key */
	bool OnScreenDebugMessageExists(uint64 Key);

	/** Clear any existing debug messages */
	void ClearOnScreenDebugMessages();

	/** Capture screenshots and performance metrics */
	void PerformanceCapture(const FString& CaptureName);

	/**
	 * Ticks the FPS chart.
	 *
	 * @param DeltaSeconds	Time in seconds passed since last tick.
	 */
	virtual void TickFPSChart( float DeltaSeconds );

	/**
	 * Starts the FPS chart data capture.
	 */
	virtual void StartFPSChart();

	/**
	 * Stops the FPS chart data capture.
	 */
	virtual void StopFPSChart();

	/**
	 * Dumps the FPS chart information to the passed in archive.
	 *
	 * @param	InMapName	Name of the map (Or Global)
	 * @param	bForceDump	Whether to dump even if FPS chart info is not enabled.
	 */
	virtual void DumpFPSChart( const FString& InMapName, bool bForceDump = false );

private:

	/**
	 * Dumps the FPS chart information to HTML.
	 */
	virtual void DumpFPSChartToHTML( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName  );

	/**
	 * Dumps the FPS chart information to the log.
	 */
	virtual void DumpFPSChartToLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName );

	/**
	 * Dumps the FPS chart information to the special stats log file.
	 */
	virtual void DumpFPSChartToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName );

	/**
	 * Dumps the frame times information to the special stats log file.
	 */
	virtual void DumpFrameTimesToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName );

	/**
	 * Callback for external UI being opened.
	 *
	 * @param bInIsOpening			true if the UI is opening, false if it is being closed.
	*/
	void OnExternalUIChange(bool bInIsOpening);

protected:

	/**
	 * Handles freezing/unfreezing of rendering 
	 * 
	 * @param InWorld	World context
	 */
	virtual void ProcessToggleFreezeCommand( UWorld* InWorld )
	{
		// Intentionally empty.
	}

	/** Handles frezing/unfreezing of streaming */
	 virtual void ProcessToggleFreezeStreamingCommand(UWorld* InWorld)
	 {
		// Intentionally empty.
	 }

public:
	/** @return the GIsEditor flag setting */
	bool IsEditor();

	/** @return the audio device (will be None if sound is disabled) */
	virtual class FAudioDevice* GetAudioDevice()
	{
		return AudioDevice;
	}

	/** @return whether we're currently running in split screen (more than one local player) */
	bool IsSplitScreen(UWorld *InWorld);

	/** @return whether we're currently running with stereoscopic 3D enabled for the specified viewport (or globally, if viewport is NULL) */
	bool IsStereoscopic3D(FViewport* InViewport = nullptr);

	/**
	 * Adds a world location as a secondary view location for purposes of texture streaming.
	 * Lasts one frame, or a specified number of seconds (for overriding locations only).
	 *
	 * @param InLoc					Location to add to texture streaming for this frame
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
	 * @param OverrideDuration		How long the streaming system should keep checking this location if bOverrideLocation is true, in seconds. 0 means just for the next Tick.
	 */
	void AddTextureStreamingSlaveLoc(FVector InLoc, float BoostFactor, bool bOverrideLocation, float OverrideDuration);

	/** 
	 * Obtain a world object pointer from an object with has a world context.
	 * This should be be overridden to cater for game specific object types that do not derive from the Actor class.
	 *
	 * @param Object		Object whose owning world we require.
	 * @param bChecked      Allows calling function to specify not to do ensure check and that a nullptr return value is acceptable
	 * returns				The world to which the object belongs.
	 */
	UWorld* GetWorldFromContextObject(const UObject* Object, bool bChecked = true) const;


	/** 
	 * mostly done to check if PIE is being set up, go GWorld is going to change, and it's not really _the_G_World_
	 * NOTE: hope this goes away once PIE and regular game triggering are not that separate code paths
	 */
	virtual bool IsSettingUpPlayWorld() const { return false; }

	/**
	 * Retrieves the LocalPlayer for the player which has the ControllerId specified
	 *
	 * @param	ControllerId	the game pad index of the player to search for
	 * @return	The player that has the ControllerId specified, or nullptr if no players have that ControllerId
	 */
	ULocalPlayer* GetLocalPlayerFromControllerId( const UGameViewportClient* InViewport, const int32 ControllerId ) const;
	ULocalPlayer* GetLocalPlayerFromControllerId( UWorld * InWorld, const int32 ControllerId ) const;

	void SwapControllerId(ULocalPlayer *NewPlayer, const int32 CurrentControllerId, const int32 NewControllerID) const;

	/** 
	 * Find a Local Player Controller, which may not exist at all if this is a server.
	 * @return first found LocalPlayerController. Fine for single player, in split screen, one will be picked. 
	 */
	class APlayerController* GetFirstLocalPlayerController(UWorld *InWorld);

	/** Gets all local players associated with the engine. 
	 *	This function should only be used in rare cases where no UWorld* is available to get a player list associated with the world.
	 *  E.g, - use GetFirstLocalPlayerController(UWorld *InWorld) when possible!
	 */
	void GetAllLocalPlayerControllers(TArray<APlayerController*>	& PlayerList);

	/** Returns the GameViewport widget */
	virtual TSharedPtr<class SViewport> GetGameViewportWidget() const
	{
		return nullptr;
	}

	/** Returns the current display gamma value */
	float GetDisplayGamma() const { return DisplayGamma; }

	virtual void FocusNextPIEWorld(UWorld *CurrentPieWorld, bool previous=false) { }

	virtual class UGameViewportClient* GetNextPIEViewport(UGameViewportClient * CurrentViewport) { return nullptr; }

	virtual void RemapGamepadControllerIdForPIE(class UGameViewportClient* InGameViewport, int32 &ControllerId) { }

public:

	/**
	 * Gets the engine's default tiny font.
	 *
	 * @return Tiny font.
	 */
	static class UFont* GetTinyFont();

	/**
	 * Gets the engine's default small font
	 *
	 * @return Small font.
	 */
	static class UFont* GetSmallFont();

	/**
	 * Gets the engine's default medium font.
	 *
	 * @return Medium font.
	 */
	static class UFont* GetMediumFont();

	/**
	 * Gets the engine's default large font.
	 *
	 * @return Large font.
	 */
	static class UFont* GetLargeFont();

	/**
	 * Gets the engine's default subtitle font.
	 *
	 * @return Subtitle font.
	 */
	static class UFont* GetSubtitleFont();

	/**
	 * Gets the specified additional font.
	 *
	 * @param AdditionalFontIndex - Index into the AddtionalFonts array.
	 */
	static class UFont* GetAdditionalFont(int32 AdditionalFontIndex);

	/** Makes a strong effort to copy everything possible from and old object to a new object of a different class, used for blueprint to update things after a recompile. */
	struct FCopyPropertiesForUnrelatedObjectsParams
	{
		bool bAggressiveDefaultSubobjectReplacement;
		bool bDoDelta;
		bool bReplaceObjectClassReferences;

		FCopyPropertiesForUnrelatedObjectsParams()
			: bAggressiveDefaultSubobjectReplacement(false)
			, bDoDelta(true)
			, bReplaceObjectClassReferences(true)
		{}
	};
	static void CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, FCopyPropertiesForUnrelatedObjectsParams Params = FCopyPropertiesForUnrelatedObjectsParams());//bool bAggressiveDefaultSubobjectReplacement = false, bool bDoDelta = true);
	virtual void NotifyToolsOfObjectReplacement(const TMap<UObject*, UObject*>& OldToNewInstanceMap) { }

	virtual bool UseSound() const;

	// This should only ever be called for a EditorEngine
	virtual UWorld* CreatePIEWorldByDuplication(FWorldContext &Context, UWorld* InWorld, FString &PlayWorldMapName) { check(false); return nullptr; }


protected:

	/**
	 *	Initialize the audio device
	 *
	 *	@return	true on success, false otherwise.
	 */
	virtual bool InitializeAudioDevice();

	/**
	 *	Detects and initializes any attached HMD devices
	 *
	 *	@return true if there is an initialized device, false otherwise
	 */
	virtual bool InitializeHMDDevice();

	/**
	 *	Record EngineAnalytics information for attached HMD devices
	 */
	virtual void RecordHMDAnalytics();

	/**
	 * Loads all Engine object references from their corresponding config entries.
	 */
	virtual void InitializeObjectReferences();

	/** 
	 * Initializes the running average delta to some good initial framerate 
	 */
	virtual void InitializeRunningAverageDeltaTime();

	float RunningAverageDeltaTime;

	/** Broadcasts when a world is added. */
	FWorldAddedEvent			WorldAddedEvent;

	/** Broadcasts when a world is destroyed. */
	FWorldDestroyedEvent		WorldDestroyedEvent;
private:

#if WITH_EDITOR

	/** Broadcasts whenever a world's actor list changes in a way not specifiable through other LevelActor__Events */
	FLevelActorListChangedEvent LevelActorListChangedEvent;

	/** Broadcasts whenever an actor is added. */
	FLevelActorAddedEvent LevelActorAddedEvent;

	/** Broadcasts whenever an actor is removed. */
	FLevelActorDeletedEvent LevelActorDeletedEvent;

	/** Broadcasts whenever an actor is attached. */
	FLevelActorAttachedEvent LevelActorAttachedEvent;

	/** Broadcasts whenever an actor is detached. */
	FLevelActorDetachedEvent LevelActorDetachedEvent;

	/** Broadcasts whenever an actor's folder has changed. */
	FLevelActorFolderChangedEvent LevelActorFolderChangedEvent;

	/** Broadcasts whenever an actor is being renamed */
	FLevelActorRequestRenameEvent LevelActorRequestRenameEvent;

	/** Broadcasts after an actor has been moved, rotated or scaled */
	FOnActorMovedEvent		OnActorMovedEvent;

#endif // #if WITH_EDITOR

	/** Thread preventing screen saver from kicking. Suspend most of the time. */
	FRunnableThread*		ScreenSaverInhibitor;
	FScreenSaverInhibitor*  ScreenSaverInhibitorRunnable;

	/** If true, the engine tick function will poll FPlatformSurvey for results */
	bool					bPendingHardwareSurveyResults;


public:

	/** A list of named UNetDriver definitions */
	UPROPERTY(Config, transient)
	TArray<FNetDriverDefinition> NetDriverDefinitions;

	/** A configurable list of actors that are automatically spawned upon server startup (just prior to InitGame) */
	UPROPERTY(config)
	TArray<FString> ServerActors;

	/** Runtime-modified list of server actors, allowing plugins to use serveractors, without permanently adding them to config files */
	UPROPERTY()
	TArray<FString> RuntimeServerActors;

	/** Spawns all of the registered server actors */
	virtual void SpawnServerActors(UWorld *World);

	/**
	 * Notification of network error messages, allows the engine to handle the failure
	 *
	 * @param	World associated with failure
	 * @param	FailureType	the type of error
	 * @param	NetDriverName name of the network driver generating the error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Notification of server travel error messages, generally network connection related (package verification, client server handshaking, etc) 
	 * allows the engine to handle the failure
	 *
	 * @param   InWorld     the world we were in when the travel failure occurred
	 * @param	FailureType	the type of error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Shutdown any relevant net drivers
	 */
	void ShutdownWorldNetDriver(UWorld*);

	void ShutdownAllNetDrivers();

	/**
	 * Finds a UNetDriver based on its name.
	 *
	 * @param NetDriverName The name associated with the driver to find.
	 *
	 * @return A pointer to the UNetDriver that was found, or nullptr if it wasn't found.
	 */
	UNetDriver* FindNamedNetDriver(UWorld* InWorld, FName NetDriverName);
	UNetDriver* FindNamedNetDriver(const UPendingNetGame* InPendingNetGame, FName NetDriverName);

	/**
	 * Returns the current netmode
	 * @param 	NetDriverName    Name of the net driver to get mode for
	 * @return current netmode
	 *
	 * Note: if there is no valid net driver, returns NM_StandAlone
	 */
	//virtual ENetMode GetNetMode(FName NetDriverName = NAME_GameNetDriver) const;
	ENetMode GetNetMode(const UWorld *World) const;

	/**
	 * Creates a UNetDriver and associates a name with it.
	 *
	 * @param NetDriverName The name to associate with the driver.
	 * @param NetDriverDefinition The name of the definition to use
	 *
	 * @return True if the driver was created successfully, false if there was an error.
	 */
	bool CreateNamedNetDriver(UWorld *InWorld, FName NetDriverName, FName NetDriverDefinition);
	bool CreateNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName, FName NetDriverDefinition);
	
	/**
	 * Destroys a UNetDriver based on its name.
	 *
	 * @param NetDriverName The name associated with the driver to destroy.
	 */
	void DestroyNamedNetDriver(UWorld *InWorld, FName NetDriverName);
	void DestroyNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName);

	virtual bool NetworkRemapPath( UWorld *InWorld, FString &Str, bool reading=true) { return false; }
	virtual bool NetworkRemapPath( UPendingNetGame *PendingNetGame, FString &Str, bool reading=true) { return false; }

	virtual bool HandleOpenCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld * InWorld );

	virtual bool HandleTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	
	virtual bool HandleStreamMapCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld );

#if WITH_SERVER_CODE
	virtual bool HandleServerTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );

	virtual bool HandleSayCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
#endif

	virtual bool HandleDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld );

	virtual bool HandleReconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld );

	
	/**
	 * The proper way to disconnect a given World and NetDriver. Travels world if necessary, cleans up pending connects if necessary.
	 *	
	 * @param InWorld	The world being disconnected (might be nullptr in case of pending net dupl)
	 * @param NetDriver The net driver being disconnect (will be InWorld's net driver if there is a world)
	 *	
	 */
	void HandleDisconnect( UWorld *InWorld, UNetDriver *NetDriver );

	/**
	 * Makes sure map name is a long package name.
	 *
	 * @param InOutMapName Map name. In non-final builds code will attempt to convert to long package name if short name is provided.
	 * @param true if the map name was valid, false otherwise.
	 */
	bool MakeSureMapNameIsValid(FString& InOutMapName);

	void SetClientTravel( UWorld *InWorld, const TCHAR* NextURL, ETravelType InTravelType );

	void SetClientTravel( UPendingNetGame *PendingNetGame, const TCHAR* NextURL, ETravelType InTravelType );

	void SetClientTravelFromPendingGameNetDriver( UNetDriver *PendingGameNetDriverGame, const TCHAR* NextURL, ETravelType InTravelType );

	/** Browse to a specified URL, relative to the current one. */
	virtual EBrowseReturnVal::Type Browse( FWorldContext& WorldContext, FURL URL, FString& Error );

	bool TickWorldTravel(FWorldContext& WorldContext, float DeltaSeconds);

	void BrowseToDefaultMap( FWorldContext& WorldContext );

	virtual bool LoadMap( FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error );

	virtual void RedrawViewports( bool bShouldPresent = true ) { }

	virtual void TriggerStreamingDataRebuild() { }

	/**
	 * Updates level streaming state using active game players view and blocks until all sub-levels are loaded/ visible/ hidden
	 * so further calls to UpdateLevelStreaming won't do any work unless state changes.
	 *
	 * @param InWorld Target world
	 */
	void BlockTillLevelStreamingCompleted(UWorld* InWorld);

	/**
	 * true if the loading movie was started during LoadMap().
	 */
	UPROPERTY(transient)
	uint32 bStartedLoadMapMovie:1;

	/**
	 * Removes the PerMapPackages from the RootSet
	 *
	 * @param FullyLoadType When to load the packages (based on map, GameMode, etc)
	 * @param Tag Name of the map/game to cleanup packages for
	 */
	void CleanupPackagesToFullyLoad(FWorldContext &Context, EFullyLoadPackageType FullyLoadType, const FString& Tag);

	/**
	 * Called to allow overloading by child engines
	 */
	virtual void LoadMapRedrawViewports(void)
	{
		RedrawViewports();
	}

	void ClearDebugDisplayProperties();

	/**
	 * Loads the PerMapPackages for the given map, and adds them to the RootSet
	 *
	 * @param FullyLoadType When to load the packages (based on map, GameMode, etc)
	 * @param Tag Name of the map/game to load packages for
	 */
	void LoadPackagesFully(UWorld * InWorld, EFullyLoadPackageType FullyLoadType, const FString& Tag);

	void UpdateTransitionType(UWorld *CurrentWorld);

	UPendingNetGame* PendingNetGameFromWorld( UWorld* InWorld );

	/** Cancel pending level. */
	virtual void CancelAllPending();

	virtual void CancelPending(UWorld *InWorld, UPendingNetGame *NewPendingNetGame=nullptr );

	virtual bool WorldIsPIEInNewViewport(UWorld *InWorld);

	FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld);
	FWorldContext* GetWorldContextFromGameViewport(const UGameViewportClient *InViewport);
	FWorldContext* GetWorldContextFromPendingNetGame(const UPendingNetGame *InPendingNetGame);	
	FWorldContext* GetWorldContextFromPendingNetGameNetDriver(const UNetDriver *InPendingNetGame);	
	FWorldContext* GetWorldContextFromHandle(const FName WorldContextHandle);
	FWorldContext* GetWorldContextFromPIEInstance(const int32 PIEInstance);

	const FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld) const;
	const FWorldContext* GetWorldContextFromGameViewport(const UGameViewportClient *InViewport) const;
	const FWorldContext* GetWorldContextFromPendingNetGame(const UPendingNetGame *InPendingNetGame) const;	
	const FWorldContext* GetWorldContextFromPendingNetGameNetDriver(const UNetDriver *InPendingNetGame) const;	
	const FWorldContext* GetWorldContextFromHandle(const FName WorldContextHandle) const;
	const FWorldContext* GetWorldContextFromPIEInstance(const int32 PIEInstance) const;

	FWorldContext& GetWorldContextFromWorldChecked(const UWorld * InWorld);
	FWorldContext& GetWorldContextFromGameViewportChecked(const UGameViewportClient *InViewport);
	FWorldContext& GetWorldContextFromPendingNetGameChecked(const UPendingNetGame *InPendingNetGame);	
	FWorldContext& GetWorldContextFromPendingNetGameNetDriverChecked(const UNetDriver *InPendingNetGame);	
	FWorldContext& GetWorldContextFromHandleChecked(const FName WorldContextHandle);
	FWorldContext& GetWorldContextFromPIEInstanceChecked(const int32 PIEInstance);

	const FWorldContext& GetWorldContextFromWorldChecked(const UWorld * InWorld) const;
	const FWorldContext& GetWorldContextFromGameViewportChecked(const UGameViewportClient *InViewport) const;
	const FWorldContext& GetWorldContextFromPendingNetGameChecked(const UPendingNetGame *InPendingNetGame) const;	
	const FWorldContext& GetWorldContextFromPendingNetGameNetDriverChecked(const UNetDriver *InPendingNetGame) const;	
	const FWorldContext& GetWorldContextFromHandleChecked(const FName WorldContextHandle) const;
	const FWorldContext& GetWorldContextFromPIEInstanceChecked(const int32 PIEInstance) const;

	const TIndirectArray<FWorldContext>& GetWorldContexts() const { return WorldList;	}


	/** Verify any remaining World(s) are valid after ::LoadMap destroys a world */
	virtual void VerifyLoadMapWorldCleanup();

	FWorldContext& CreateNewWorldContext(EWorldType::Type WorldType);
	virtual void DestroyWorldContext(UWorld * InWorld);

	bool ShouldAbsorbAuthorityOnlyEvent();
	bool ShouldAbsorbCosmeticOnlyEvent();

	UGameViewportClient* GameViewportForWorld(const UWorld *InWorld) const;

protected:

	TIndirectArray<FWorldContext>	WorldList;

	UPROPERTY()
	int32	NextWorldContextHandle;


	virtual void CancelPending(FWorldContext& WorldContext);

	virtual void CancelPending(UNetDriver* PendingNetGameDriver);

	virtual void MovePendingLevel(FWorldContext &Context);


	bool WorldHasValidContext(UWorld *InWorld);

protected:

	// Async map change/ persistent level transition code.

	/**
	 * Finalizes the pending map change that was being kicked off by PrepareMapChange.
	 *
	 * @param InCurrentWorld	the world context
	 * @return	true if successful, false if there were errors (use GetMapChangeFailureDescription 
	 *			for error description)
	 */
	bool CommitMapChange( FWorldContext &Context);

	/**
	 * Returns whether the prepared map change is ready for commit having called.
	 *
	 * @return true if we're ready to commit the map change, false otherwise
	 */
	bool IsReadyForMapChange(FWorldContext &Context);

	/**
	 * Returns whether we are currently preparing for a map change or not.
	 *
	 * @return true if we are preparing for a map change, false otherwise
	 */
	bool IsPreparingMapChange(FWorldContext &Context);

	/**
	 * Prepares the engine for a map change by pre-loading level packages in the background.
	 *
	 * @param	LevelNames	Array of levels to load in the background; the first level in this
	 *						list is assumed to be the new "persistent" one.
	 *
	 * @return	true if all packages were in the package file cache and the operation succeeded,
	 *			false otherwise. false as a return value also indicates that the code has given
	 *			up.
	 */
	bool PrepareMapChange(FWorldContext &WorldContext, const TArray<FName>& LevelNames);

	/**
	 * Returns the failure description in case of a failed map change request.
	 *
	 * @return	Human readable failure description in case of failure, empty string otherwise
	 */
	FString GetMapChangeFailureDescription(FWorldContext &Context);

	/** Commit map change if requested and map change is pending. Called every frame.	 */
	void ConditionalCommitMapChange(FWorldContext &WorldContext);

	/** Cancels pending map change.	 */
	void CancelPendingMapChange(FWorldContext &Context);

public:

	// Public interface for async map change functions

	bool CommitMapChange(UWorld* InWorld) { return CommitMapChange(GetWorldContextFromWorldChecked(InWorld)); }
	bool IsReadyForMapChange(UWorld* InWorld) { return IsReadyForMapChange(GetWorldContextFromWorldChecked(InWorld)); }
	bool IsPreparingMapChange(UWorld* InWorld) { return IsPreparingMapChange(GetWorldContextFromWorldChecked(InWorld)); }
	bool PrepareMapChange(UWorld* InWorld, const TArray<FName>& LevelNames) { return PrepareMapChange(GetWorldContextFromWorldChecked(InWorld), LevelNames); }
	void ConditionalCommitMapChange(UWorld* InWorld) { return ConditionalCommitMapChange(GetWorldContextFromWorldChecked(InWorld)); }

	FString GetMapChangeFailureDescription(UWorld *InWorld) { return GetMapChangeFailureDescription(GetWorldContextFromWorldChecked(InWorld)); }

	/** Cancels pending map change.	 */
	void CancelPendingMapChange(UWorld *InWorld) { return CancelPendingMapChange(GetWorldContextFromWorldChecked(InWorld)); }

	void AddNewPendingStreamingLevel(UWorld *InWorld, FName PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, int32 LODIndex);

	bool ShouldCommitPendingMapChange(UWorld *InWorld);
	void SetShouldCommitPendingMapChange(UWorld *InWorld, bool NewShouldCommitPendingMapChange);

	FSeamlessTravelHandler&	SeamlessTravelHandlerForWorld(UWorld *World);

	FURL & LastURLFromWorld(UWorld *World);

	/**
	 * Returns the global instance of the game user settings class.
	 */
	const UGameUserSettings* GetGameUserSettings() const;
	UGameUserSettings* GetGameUserSettings();

	/** Delegate handler for screenshots */
	void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);

private:
	void CreateGameUserSettings();

	/** Allows subclasses to pass the failure to a UGameInstance if possible (mainly for blueprints) */
	virtual void HandleNetworkFailure_NotifyGameInstance(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType);

	/** Allows subclasses to pass the failure to a UGameInstance if possible (mainly for blueprints) */
	virtual void HandleTravelFailure_NotifyGameInstance(UWorld* World, ETravelFailure::Type FailureType);

public:
	/**
	 * Delegate we fire every time a new stat has been registered
	 *
	 * @param FName	- The name of the new stat
	 * @param FName - The category of the new stat
	 * @param FText - The description of the new stat
	 */
	DECLARE_EVENT_ThreeParams(UEngine, FOnNewStatRegistered, const FName&, const FName&, const FText&);
	static FOnNewStatRegistered NewStatDelegate;
	
	/**
	 * Wrapper for firing a simple stat exec
	 *
	 * @param World	- The world to apply the exec to
	 * @param ViewportClient - The viewport to apply the exec to
	 * @param InName - The exec string
	 */
	void ExecEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* InName);

	/**
	 * Check to see if the specified stat name is a simple stat
	 *
	 * @param InName - The name of the stat we're checking
	 * @returns true if the stat is a registered simple stat
	 */
	bool IsEngineStat(const FString& InName);

	/**
	 * Set the state of the specified stat
	 *
	 * @param World	- The world to apply the exec to
	 * @param ViewportClient - The viewport to apply the exec to
	 * @param InName - The stat name
	 * @param bShow - The state we would like the stat to be in
	 */
	void SetEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const FString& InName, const bool bShow);

	/**
	 * Set the state of the specified stats (note: array processed in reverse order when !bShow)
	 *
	 * @param World	- The world to apply the exec to
	 * @param ViewportClient - The viewport to apply the exec to
	 * @param InNames - The stat names
	 * @param bShow - The state we would like the stat to be in
	 */
	void SetEngineStats(UWorld* World, FCommonViewportClient* ViewportClient, const TArray<FString>& InNames, const bool bShow);

	/**
	 * Function to render all the simple stats
	 *
	 * @param World	- The world being drawn to
	 * @param ViewportClient - The viewport being drawn to
	 * @param Canvas - The canvas to use when drawing
	 * @param LHSX - The left hand side X position to start drawing from
	 * @param InOutLHSY - The left hand side Y position to start drawing from
	 * @param RHSX - The right hand side X position to start drawing from
	 * @param InOutRHSY - The right hand side Y position to start drawing from
	 * @param ViewLocation - The world space view location
	 * @param ViewRotation - The world space view rotation
	 */
	void RenderEngineStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 LHSX, int32& InOutLHSY, int32 RHSX, int32& InOutRHSY, const FVector* ViewLocation, const FRotator* ViewRotation);

private:
	/**
	 * Function definition for those stats which have their own render funcsions (or affect another render functions)
	 *
	 * @param World	- The world being drawn to
	 * @param ViewportClient - The viewport being drawn to
	 * @param Canvas - The canvas to use when drawing
	 * @param X - The X position to draw to
	 * @param Y - The Y position to draw to
	 * @param ViewLocation - The world space view location
	 * @param ViewRotation - The world space view rotation
	 */
	typedef int32 (UEngine::*EngineStatRender)(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation);

	/**
	 * Function definition for those stats which have their own toggle funcsions (or toggle other stats)
	 *
	 * @param World	- The world being drawn to
	 * @param ViewportClient - The viewport being drawn to
	 * @param Stream - The remaining characters from the Exec call
	 */
	typedef bool (UEngine::*EngineStatToggle)(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream);

	/** Struct for keeping track off all the info regarding a specific simple stat exec */
	struct FEngineStatFuncs
	{
		/** The name of the command, e.g. STAT FPS would just have FPS as it's CommandName */
		FName CommandName;

		/** The category the command falls into (only used by UI) */
		FName CategoryName;

		/** The description of what this command does (only used by UI) */
		FText DescriptionString;

		/** The function needed to render the stat when it's enabled 
		 *  Note: This is only called when it should be rendered */
		EngineStatRender RenderFunc;

		/** The function we call after the stat has been toggled 
		 *  Note: This is only needed if you need to do something else depending on the state of the stat */
		EngineStatToggle ToggleFunc;

		/** If true, this stat should render on the right side of the viewport, otherwise left */
		bool bIsRHS;

		/** Constructor */
		FEngineStatFuncs(const FName& InCommandName, const FName& InCategoryName, const FText& InDescriptionString, EngineStatRender InRenderFunc = nullptr, EngineStatToggle InToggleFunc = nullptr, const bool bInIsRHS = false)
			: CommandName(InCommandName)
			, CategoryName(InCategoryName)
			, DescriptionString(InDescriptionString)
			, RenderFunc(InRenderFunc)
			, ToggleFunc(InToggleFunc)
			, bIsRHS(bInIsRHS)
		{
		}
	};

	/** A list of all the simple stats functions that have been registered */
	TArray<FEngineStatFuncs> EngineStats;

private:
	/**
	 * Functions for performing other actions when the stat is toggled, should only be used when registering with EngineStats
	 *
	 * @param World	- The world being drawn to
	 * @param ViewportClient - The viewport being drawn to
	 * @param Stream - The remaining characters from the Exec call (optional)
	 */
	bool ToggleStatFPS(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatDetailed(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatHitches(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatNamedEvents(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatUnit(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
#if !UE_BUILD_SHIPPING
	bool ToggleStatUnitMax(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatUnitGraph(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatUnitTime(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
	bool ToggleStatRaw(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);
#endif
	bool ToggleStatSounds(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream = nullptr);

	/**
	 * Functions for rendering the various simple stats, should only be used when registering with EngineStats
	 *
	 * @param World	- The world being drawn to
	 * @param ViewportClient - The viewport being drawn to
	 * @param Canvas - The canvas to use when drawing
	 * @param X - The X position to draw to
	 * @param Y - The Y position to draw to
	 * @param ViewLocation - The world space view location
	 * @param ViewRotation - The world space view rotation
	 */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 RenderStatVersion(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#endif
	int32 RenderStatFPS(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatHitches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatSummary(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatNamedEvents(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatColorList(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatLevels(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatLevelMap(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatUnit(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 RenderStatReverb(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatSoundMixes(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatSoundWaves(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatSoundCues(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#endif
	int32 RenderStatSounds(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
	int32 RenderStatAI(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#if STATS
	int32 RenderStatSlateBatches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation = nullptr, const FRotator* ViewRotation = nullptr);
#endif
};
