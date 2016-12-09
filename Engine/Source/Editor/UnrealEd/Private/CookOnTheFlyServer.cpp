// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookOnTheFlyServer.cpp: handles polite cook requests via network ;)
=============================================================================*/

#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Serialization/CustomVersion.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "Misc/PackageName.h"
#include "Misc/RedirectCollector.h"
#include "Engine/Level.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "AssetData.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture.h"
#include "SceneUtils.h"
#include "Settings/ProjectPackagingSettings.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "FileServerMessages.h"
#include "Internationalization/Culture.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "GlobalShader.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IAudioFormat.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITextureFormat.h"
#include "Developer/PackageDependencyInfo/Public/PackageDependencyInfo.h"
#include "IMessageContext.h"
#include "Helpers/MessageEndpoint.h"
#include "Helpers/MessageEndpointBuilder.h"
#include "Interfaces/INetworkFileServer.h"
#include "Interfaces/INetworkFileSystemModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PlatformInfo.h"

#include "AssetRegistryModule.h"
#include "BlueprintNativeCodeGenModule.h"

#include "GameDelegates.h"
#include "IPAddress.h"

// cook by the book requirements
#include "Commandlets/ChunkManifestGenerator.h"
#include "Engine/WorldComposition.h"

// error message log
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

// shader compiler processAsyncResults
#include "ShaderCompiler.h"
#include "ShaderCache.h"
#include "Engine/LevelStreaming.h"
#include "Engine/TextureLODSettings.h"
#include "ProfilingDebugging/CookStats.h"

#include "Misc/NetworkVersion.h"

#include "ParallelFor.h"

#define LOCTEXT_NAMESPACE "Cooker"

DEFINE_LOG_CATEGORY_STATIC(LogCook, Log, All);

#define DEBUG_COOKONTHEFLY 0
#define OUTPUT_TIMING 1

#if OUTPUT_TIMING
#include "ProfilingDebugging/ScopedTimers.h"

#define HEIRARCHICAL_TIMER 1
#define PERPACKAGE_TIMER 0

struct FTimerInfo
{
public:

	FTimerInfo( FTimerInfo &&InTimerInfo )
	{
		Swap( Name, InTimerInfo.Name );
		Length = InTimerInfo.Length;
	}

	FTimerInfo( const FTimerInfo &InTimerInfo )
	{
		Name = InTimerInfo.Name;
		Length = InTimerInfo.Length;
	}

	FTimerInfo( FString &&InName, double InLength ) : Name(MoveTemp(InName)), Length(InLength) { }

	FString Name;
	double Length;
};

#if HEIRARCHICAL_TIMER
struct FHierarchicalTimerInfo : public FTimerInfo
{
public:
	FHierarchicalTimerInfo *Parent;
	TMap<FString, FHierarchicalTimerInfo*> Children;

	FHierarchicalTimerInfo(const FHierarchicalTimerInfo& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Children = InTimerInfo.Children;
		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FHierarchicalTimerInfo&& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Swap(Children, InTimerInfo.Children);

		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FString &&Name) : FTimerInfo(MoveTemp(Name), 0.0)
	{
	}

	~FHierarchicalTimerInfo()
	{
		Parent = NULL;
		ClearChildren();
	}

	void ClearChildren()
	{
		for (auto& Child : Children)
		{
			delete Child.Value;
		}
		Children.Empty();
	}

	FHierarchicalTimerInfo* FindChild(const FString& InName)
	{
		FHierarchicalTimerInfo* Child = (FHierarchicalTimerInfo*)(Children.FindRef(InName));
		if ( !Child )
		{
			FString Temp = InName;
			Child = Children.Add(InName, new FHierarchicalTimerInfo(MoveTemp(Temp)));
			Child->Parent = this;
		}

		check(Child);
		return Child;
	}
};

static TMap<FName, int32> IntStats;
void IncIntStat(const FName& Name, const int32 Amount)
{
	int32* Value = IntStats.Find(Name);

	if (Value == NULL)
	{
		IntStats.Add(Name, Amount);
	}
	else
	{
		*Value += Amount;
	}
}


static FHierarchicalTimerInfo RootTimerInfo(MoveTemp(FString(TEXT("Root"))));
static FHierarchicalTimerInfo* CurrentTimerInfo = &RootTimerInfo;
#endif


static TArray<FTimerInfo> GTimerInfo;



struct FScopeTimer
{
private:
	bool Started;
	bool DecrementScope;
	static int GScopeDepth;
#if HEIRARCHICAL_TIMER
	FHierarchicalTimerInfo* HeirarchyTimerInfo;
#endif
public:

	FScopeTimer( const FScopeTimer &outer )
	{
		Index = outer.Index;
		DecrementScope = false;
		Started = false;
	}

	FScopeTimer( const FString &InName, bool IncrementScope = false )
	{
		DecrementScope = IncrementScope;
		
		FString Name = InName;
		for ( int i =0; i < GScopeDepth; ++i )
		{
			Name = FString(TEXT("  ")) + Name;
		}
		if( DecrementScope)
		{
			++GScopeDepth;
		}

#if HEIRARCHICAL_TIMER
		HeirarchyTimerInfo = CurrentTimerInfo->FindChild(Name);
		CurrentTimerInfo = HeirarchyTimerInfo;
#endif
#if PERPACKAGE_TIMER
		Index = GTimerInfo.Emplace(MoveTemp(Name), 0.0);
#endif
		Started = false;
	}

	void Start()
	{
		if ( !Started )
		{
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length -= FPlatformTime::Seconds();
#endif
			Started = true;
#if HEIRARCHICAL_TIMER
			HeirarchyTimerInfo->Length -= FPlatformTime::Seconds();
#endif
		}
	}

	void Stop()
	{
		if ( Started )
		{
#if HEIRARCHICAL_TIMER
			HeirarchyTimerInfo->Length += FPlatformTime::Seconds();
#endif
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length += FPlatformTime::Seconds();
#endif
			Started = false;
		}
	}

	~FScopeTimer()
	{
		Stop();
#if HEIRARCHICAL_TIMER
		check(CurrentTimerInfo == HeirarchyTimerInfo);
		CurrentTimerInfo = HeirarchyTimerInfo->Parent;
#endif
		if ( DecrementScope )
		{
			--GScopeDepth;
		}
	}

	int Index;
};

int FScopeTimer::GScopeDepth = 0;



void OutputTimers()
{
#if PERPACKAGE_TIMER
	if ( GTimerInfo.Num() <= 0 )
		return;
	
	static FArchive *OutputDevice = NULL;

	static TMap<FString, int> TimerIndexMap;


	if ( OutputDevice == NULL )
	{
		OutputDevice = IFileManager::Get().CreateFileWriter(TEXT("CookOnTheFlyServerTiming.csv") );
	}

	TArray<FString> OutputValues;
	OutputValues.AddZeroed(TimerIndexMap.Num());

	bool OutputTimerIndexMap = false;
	for ( auto TimerInfo : GTimerInfo )
	{
		int *IndexPtr = TimerIndexMap.Find(TimerInfo.Name);
		int Index = 0;
		if (IndexPtr == NULL)
		{
			Index = TimerIndexMap.Num();
			TimerIndexMap.Add( TimerInfo.Name, Index );
			OutputValues.AddZeroed();
			OutputTimerIndexMap = true;
		}
		else
			Index = *IndexPtr;

		OutputValues[Index] = FString::Printf(TEXT("%f"),TimerInfo.Length);
	}
	static FString NewLine = FString(TEXT("\r\n"));

	if (OutputTimerIndexMap)
	{
		TArray<FString> OutputHeader;
		OutputHeader.AddZeroed( TimerIndexMap.Num() );
		for ( auto TimerIndex : TimerIndexMap )
		{
			int LocalIndex = TimerIndex.Value;
			OutputHeader[LocalIndex] = TimerIndex.Key;
		}

		for ( auto OutputString : OutputHeader)
		{
			OutputString.Append(TEXT(", "));
			OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
		}

		
		OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );
	}

	for ( auto OutputString : OutputValues)
	{
		OutputString.Append(TEXT(", "));
		OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
	}
	OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );

	OutputDevice->Flush();

	UE_LOG( LogCook, Display, TEXT("Timing information for cook") );
	UE_LOG( LogCook, Display, TEXT("Name\tlength(ms)") );
	for ( auto TimerInfo : GTimerInfo )
	{
		UE_LOG( LogCook, Display, TEXT("%s\t%.2f"), *TimerInfo.Name, TimerInfo.Length * 1000.0f );
	}

	// first item is the total
	if ( GTimerInfo.Num() > 0 && ( ( GTimerInfo[0].Length * 1000.0f ) > 40.0f ) )
	{
		UE_LOG( LogCook, Display, TEXT("Cook tick exceeded 40ms by  %f"), GTimerInfo[0].Length * 1000.0f  );
	}

	GTimerInfo.Empty();
#endif
}
#if HEIRARCHICAL_TIMER
void OutputHierarchyTimers(const FHierarchicalTimerInfo* TimerInfo, FString& Output, int32& Depth)
{	
	Output += FString::Printf(TEXT("%s: %fms\r\n"), *TimerInfo->Name, TimerInfo->Length * 1000);

	++Depth;
	for (const auto& ChildInfo : TimerInfo->Children)
	{
		OutputHierarchyTimers(ChildInfo.Value, Output, Depth);
	}
	--Depth;
}

void OutputHierarchyTimers()
{
	FHierarchicalTimerInfo* TimerInfo = &RootTimerInfo;

	int32 Depth = 0;
	FString Output = TEXT("Hierarchy timer information\nName:  Length(ms)");
	OutputHierarchyTimers(TimerInfo, Output, Depth);

	Output += FString::Printf(TEXT("IntStats\n"));
	for (const auto& IntStat : IntStats)
	{
		Output += FString::Printf(TEXT("%s=%d\r\n"), *IntStat.Key.ToString(), IntStat.Value);
	}

	UE_LOG(LogCook, Display, TEXT("%s"), *Output);

	RootTimerInfo.ClearChildren();
}
#endif

#define CREATE_TIMER(name, incrementScope) FScopeTimer ScopeTimer##name(#name, incrementScope); 

#define SCOPE_TIMER(name) CREATE_TIMER(name, true); ScopeTimer##name.Start();
#define STOP_TIMER( name ) ScopeTimer##name.Stop();


#define ACCUMULATE_TIMER(name) CREATE_TIMER(name, false);
#define ACCUMULATE_TIMER_SCOPE(name) FScopeTimer ScopeTimerInner##name(ScopeTimer##name); ScopeTimerInner##name.Start();
#define ACCUMULATE_TIMER_START(name) ScopeTimer##name.Start();
#define ACCUMULATE_TIMER_STOP(name) ScopeTimer##name.Stop();
#if HEIRARCHICAL_TIMER
#define INC_INT_STAT( name, amount ) { static FName StaticName##name(#name); IncIntStat(StaticName##name, amount); }
#else
#define INC_INT_STAT( name, amount ) 
#endif

#define OUTPUT_TIMERS() OutputTimers();
#define OUTPUT_HIERARCHYTIMERS() OutputHierarchyTimers();

#else
#define CREATE_TIMER(name)

#define SCOPE_TIMER(name)
#define STOP_TIMER(name)

#define ACCUMULATE_TIMER(name) 
#define ACCUMULATE_TIMER_SCOPE(name) 
#define ACCUMULATE_TIMER_START(name) 
#define ACCUMULATE_TIMER_STOP(name) 
#define INC_INT_STAT( name, amount ) 

#define OUTPUT_TIMERS()
#define OUTPUT_HIERARCHYTIMERS()
#endif

#if ENABLE_COOK_STATS
namespace DetailedCookStats
{
	//Externable so CookCommandlet can pick them up and merge them with it's cook stats
	double TickCookOnTheSideTimeSec = 0.0;
	double TickCookOnTheSideLoadPackagesTimeSec = 0.0;
	double TickCookOnTheSideResolveRedirectorsTimeSec = 0.0;
	double TickCookOnTheSideSaveCookedPackageTimeSec = 0.0;
	double TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec = 0.0;
	double TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec = 0.0;
	double GameCookModificationDelegateTimeSec = 0.0;
}
#endif

//////////////////////////////////////////////////////////////////////////
// FCookTimer
// used as a helper to timeslice cooker functions
//////////////////////////////////////////////////////////////////////////

struct FCookerTimer
{
	const bool bIsRealtimeMode;
	const double StartTime;
	const float &TimeSlice;
	const int MaxNumPackagesToSave; // maximum packages to save before exiting tick (this should never really hit unless we are not using realtime mode)
	int NumPackagesSaved;

	FCookerTimer(const float &InTimeSlice, bool bInIsRealtimeMode, int InMaxNumPackagesToSave = 50) :
		bIsRealtimeMode(bInIsRealtimeMode), StartTime(FPlatformTime::Seconds()), TimeSlice(InTimeSlice),
		MaxNumPackagesToSave(InMaxNumPackagesToSave), NumPackagesSaved(0)
	{
	}
	inline double GetTimeTillNow()
	{
		return FPlatformTime::Seconds() - StartTime;
	}
	bool IsTimeUp()
	{
		if (bIsRealtimeMode)
		{
			if ((FPlatformTime::Seconds() - StartTime) > TimeSlice)
			{
				return true;
			}
		}
		if (NumPackagesSaved >= MaxNumPackagesToSave)
		{
			return true;
		}
		return false;
	}

	inline void SavedPackage()
	{
		++NumPackagesSaved;
	}

	inline double GetTimeRemain()
	{
		return TimeSlice - (FPlatformTime::Seconds() - StartTime);
	}
};

////////////////////////////////////////////////////////////////
/// Cook on the fly server
///////////////////////////////////////////////////////////////


DECLARE_CYCLE_STAT(TEXT("Precache Derived data for platform"), STAT_TickPrecacheCooking, STATGROUP_Cooking);
DECLARE_CYCLE_STAT(TEXT("Tick cooking"), STAT_TickCooker, STATGROUP_Cooking);



/* helper structs functions
 *****************************************************************************/

/** Helper to pass a recompile request to game thread */
struct FRecompileRequest
{
	struct FShaderRecompileData RecompileData;
	bool bComplete;
};


/** Helper to assign to any variable for a scope period */
template<class T>
struct FScopeAssign
{
private:
	T* Setting;
	T OriginalValue;
public:
	FScopeAssign(T& InSetting, const T NewValue)
	{
		Setting = &InSetting;
		OriginalValue = *Setting;
		*Setting = NewValue;
	}
	~FScopeAssign()
	{
		*Setting = OriginalValue;
	}
};


class FPackageSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	TArray<FString>& FoundFiles;
public:
	FPackageSearchVisitor(TArray<FString>& InFoundFiles)
		: FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.EndsWith(TEXT(".uasset")) || Filename.EndsWith(TEXT(".umap")))
			{
				FoundFiles.Add(Filename);
			}
		}
		return true;
	}
};

const FString& GetAssetRegistryPath()
{
	static const FString AssetRegistryPath = FPaths::GameDir();
	return AssetRegistryPath;
}

FString GetChildCookerResultFilename(const FString& ResponseFilename)
{
	FString Result = ResponseFilename + TEXT("Result.txt");
	return Result;
}

FString GetChildCookerManifestFilename(const FString ResponseFilename)
{
	FString Result = ResponseFilename + TEXT("Manifest.txt");
	return Result;
}

/**
 * Return the release asset registry filename for the release version supplied
 */
FString GetReleaseVersionAssetRegistryPath(const FString& ReleaseVersion, const FName& PlatformName )
{
	// cache the part of the path which is static because getting the GameDir is really slow and also string manipulation
	const static FString GameDirectory = FPaths::GameDir() / FString(TEXT("Releases"));
	return  GameDirectory / ReleaseVersion / PlatformName.ToString();
}

const FString& GetAssetRegistryFilename()
{
	static const FString AssetRegistryFilename = FString(TEXT("AssetRegistry.bin"));
	return AssetRegistryFilename;
}

/**
 * Uses the FMessageLog to log a message
 * 
 * @param Message to log
 * @param Severity of the message
 */
void LogCookerMessage( const FString& MessageText, EMessageSeverity::Type Severity)
{
	FMessageLog MessageLog("CookResults");

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(Severity);

	Message->AddToken( FTextToken::Create( FText::FromString(MessageText) ) );
	// Message->AddToken(FTextToken::Create(MessageLogTextDetail)); 
	// Message->AddToken(FDocumentationToken::Create(TEXT("https://docs.unrealengine.com/latest/INT/Platforms/iOS/QuickStart/6/index.html"))); 
	MessageLog.AddMessage(Message);

	MessageLog.Notify(FText(), EMessageSeverity::Warning, false);
}



/* FIlename caching functions
 *****************************************************************************/

FString UCookOnTheFlyServer::GetCachedPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFileFName;
}


FString UCookOnTheFlyServer::GetCachedPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFileFName;
}


bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const FName& PackageName ) const
{
	return PackageFilenameCache.Remove( PackageName ) >= 1;
}

bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const UPackage* Package ) const
{
	return PackageFilenameCache.Remove( Package->GetFName() ) >= 1;
}

const FString& UCookOnTheFlyServer::GetCachedSandboxFilename( const UPackage* Package, TUniquePtr<class FSandboxPlatformFile>& InSandboxFile ) const 
{
	FName PackageFName = Package->GetFName();
	static TMap<FName, FString> CachedSandboxFilenames;
	FString* CachedSandboxFilename = CachedSandboxFilenames.Find(PackageFName);
	if ( CachedSandboxFilename )
		return *CachedSandboxFilename;

	const FString& PackageFilename = GetCachedPackageFilename(Package);
	FString SandboxFilename = ConvertToFullSandboxPath(*PackageFilename, true );

	return CachedSandboxFilenames.Add( PackageFName, MoveTemp(SandboxFilename) );
}

const UCookOnTheFlyServer::FCachedPackageFilename& UCookOnTheFlyServer::Cache(const FName& PackageName) const 
{
	FCachedPackageFilename *Cached = PackageFilenameCache.Find( PackageName );
	if ( Cached != NULL )
	{
		return *Cached;
	}
	// cache all the things, like it's your birthday!

	FString Filename;
	FString PackageFilename;
	FString StandardFilename;
	FName StandardFileFName = NAME_None;
	if (FPackageName::DoesPackageExist(PackageName.ToString(), NULL, &Filename))
	{
		StandardFilename = PackageFilename = FPaths::ConvertRelativePathToFull(Filename);


		FPaths::MakeStandardFilename(StandardFilename);
		StandardFileFName = FName(*StandardFilename);
	}
	PackageFilenameToPackageFNameCache.Add(StandardFileFName, PackageName);
	return PackageFilenameCache.Emplace( PackageName, MoveTemp(FCachedPackageFilename(MoveTemp(PackageFilename),MoveTemp(StandardFilename), StandardFileFName)) );
}


const FName* UCookOnTheFlyServer::GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const
{
	const FName* Result = PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
	if ( Result )
	{
		return Result;
	}

	FName PackageName = StandardPackageFilename;
	FString PotentialLongPackageName = StandardPackageFilename.ToString();
	if (FPackageName::IsValidLongPackageName(PotentialLongPackageName) == false)
	{
		PotentialLongPackageName = FPackageName::FilenameToLongPackageName(PotentialLongPackageName);
		PackageName = FName(*PotentialLongPackageName);
	}

	Cache(PackageName);

	return PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
}

void UCookOnTheFlyServer::ClearPackageFilenameCache() const 
{
	PackageFilenameCache.Empty();
	PackageFilenameToPackageFNameCache.Empty();
}


/* UCookOnTheFlyServer functions
 *****************************************************************************/

UCookOnTheFlyServer::UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	CurrentCookMode(ECookMode::CookOnTheFly),
	CookByTheBookOptions(NULL),
	CookFlags(ECookInitializationFlags::None),
	bIsInitializingSandbox(false),
	bIsSavingPackage(false)
{
}

UCookOnTheFlyServer::~UCookOnTheFlyServer()
{
	FCoreDelegates::OnFConfigCreated.RemoveAll(this);
	FCoreDelegates::OnFConfigDeleted.RemoveAll(this);

	check(TickChildCookers() == true);
	if ( CookByTheBookOptions )
	{		
		delete CookByTheBookOptions;
		CookByTheBookOptions = NULL;
	}
}

// this tick only happens in the editor cook commandlet directly calls tick on the side
void UCookOnTheFlyServer::Tick(float DeltaTime)
{
	check(IsCookingInEditor());

	if (IsCookByTheBookMode() && !IsCookByTheBookRunning() && !GIsSlowTask)
	{
		// if we are in the editor then precache some stuff ;)
		TArray<const ITargetPlatform*> CacheTargetPlatforms;
		const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
		ITargetPlatform* TargetPlatform = nullptr;
		if (PlaySettings && (PlaySettings->LastExecutedLaunchModeType == LaunchMode_OnDevice))
		{
			FString DeviceName = PlaySettings->LastExecutedLaunchDevice.Left(PlaySettings->LastExecutedLaunchDevice.Find(TEXT("@")));
			CacheTargetPlatforms.Add(GetTargetPlatformManager()->FindTargetPlatform(DeviceName));
		}
		if (CacheTargetPlatforms.Num() > 0)
		{
			// early out all the stuff we don't care about 
			if (!IsCookFlagSet(ECookInitializationFlags::BuildDDCInBackground))
			{
				return;
			}
			TickPrecacheObjectsForPlatforms(0.001, CacheTargetPlatforms);
		}
	}

	uint32 CookedPackagesCount = 0;
	const static float CookOnTheSideTimeSlice = 0.1f; // seconds
	TickCookOnTheSide( CookOnTheSideTimeSlice, CookedPackagesCount);
	TickRecompileShaderRequests();
}

bool UCookOnTheFlyServer::IsTickable() const 
{ 
	return IsCookFlagSet(ECookInitializationFlags::AutoTick); 
}

TStatId UCookOnTheFlyServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCookServer, STATGROUP_Tickables);
}

bool UCookOnTheFlyServer::StartNetworkFileServer( const bool BindAnyPort )
{
	check( IsCookOnTheFlyMode() );
	//GetDerivedDataCacheRef().WaitForQuiescence(false);


	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();


	GenerateAssetRegistry();

	InitializeSandbox();

	
	{
		// When cooking on the fly the full registry is saved at the beginning
		// in cook by the book asset registry is saved after the cook is finished
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// write it out to a memory archive
		FArrayWriter SerializedAssetRegistry;
		SerializedAssetRegistry.SetFilterEditorOnly(true);
		AssetRegistry.Serialize(SerializedAssetRegistry);
		UE_LOG(LogCook, Display, TEXT("Generated asset registry size is %5.2fkb"), (float)SerializedAssetRegistry.Num() / 1024.f);

		// now save it in each cooked directory
		FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		FString SandboxFilename = ConvertToFullSandboxPath(*RegistryFilename, true);

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			FString PlatFilename = SandboxFilename.Replace(TEXT("[Platform]"), *Platforms[Index]->PlatformName());
			FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatFilename);
		}
	}

	// start the listening thread
	FFileRequestDelegate FileRequestDelegate(FFileRequestDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerFileRequest));
	FRecompileShadersDelegate RecompileShadersDelegate(FRecompileShadersDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders));

	INetworkFileServer *TcpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, &FileRequestDelegate, &RecompileShadersDelegate, ENetworkFileServerProtocol::NFSP_Tcp);
	if ( TcpFileServer )
	{
		NetworkFileServers.Add(TcpFileServer);
	}

	//only needed for html5 and constantly crashes cookonthefly servers
#if 0
	INetworkFileServer *HttpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, &FileRequestDelegate, &RecompileShadersDelegate, ENetworkFileServerProtocol::NFSP_Http);
	if ( HttpFileServer )
	{
		NetworkFileServers.Add( HttpFileServer );
	}
#endif

	// loop while waiting for requests
	GIsRequestingExit = false;
	return true;
}


bool UCookOnTheFlyServer::BroadcastFileserverPresence( const FGuid &InstanceId )
{
	
	TArray<FString> AddressStringList;

	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		TArray<TSharedPtr<FInternetAddr> > AddressList;
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ((NetworkFileServer == NULL || !NetworkFileServer->IsItReadyToAcceptConnections() || !NetworkFileServer->GetAddressList(AddressList)))
		{
			LogCookerMessage( FString(TEXT("Failed to create network file server")), EMessageSeverity::Error );
			UE_LOG(LogCook, Error, TEXT("Failed to create network file server"));
			continue;
		}

		// broadcast our presence
		if (InstanceId.IsValid())
		{
			for (int32 AddressIndex = 0; AddressIndex < AddressList.Num(); ++AddressIndex)
			{
				AddressStringList.Add(FString::Printf( TEXT("%s://%s"), *NetworkFileServer->GetSupportedProtocol(),  *AddressList[AddressIndex]->ToString(true)));
			}

		}
	}



	FMessageEndpointPtr MessageEndpoint = FMessageEndpoint::Builder("UCookOnTheFlyServer").Build();

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FFileServerReady(AddressStringList, InstanceId), EMessageScope::Network);
	}		
	
	return true;
}

/*----------------------------------------------------------------------------
	FArchiveFindReferences.
----------------------------------------------------------------------------*/
/**
 * Archive for gathering all the object references to other objects
 */
class FArchiveFindReferences : public FArchiveUObject
{
private:
	/**
	 * I/O function.  Called when an object reference is encountered.
	 *
	 * @param	Obj		a pointer to the object that was encountered
	 */
	FArchive& operator<<( UObject*& Obj ) override
	{
		if( Obj )
		{
			FoundObject( Obj );
		}
		return *this;
	}

	virtual FArchive& operator<< (class FAssetPtr& Value) override
	{
		if ( Value.Get() )
		{
			Value.Get()->Serialize( *this );
		}
		return *this;
	}
	virtual FArchive& operator<< (struct FStringAssetReference& Value) override
	{
		if ( Value.ResolveObject() )
		{
			Value.ResolveObject()->Serialize( *this );
		}
		return *this;
	}


	void FoundObject( UObject* Object )
	{
		if ( RootSet.Find(Object) == NULL )
		{
			if ( Exclude.Find(Object) == INDEX_NONE )
			{
				// remove this check later because don't want this happening in development builds
				//check(RootSetArray.Find(Object)==INDEX_NONE);

				RootSetArray.Add( Object );
				RootSet.Add(Object);
				Found.Add(Object);
			}
		}
	}


	/**
	 * list of Outers to ignore;  any objects encountered that have one of
	 * these objects as an Outer will also be ignored
	 */
	TArray<UObject*> &Exclude;

	/** list of objects that have been found */
	TSet<UObject*> &Found;
	
	/** the objects to display references to */
	TArray<UObject*> RootSetArray;
	/** Reflection of the rootsetarray */
	TSet<UObject*> RootSet;

public:

	/**
	 * Constructor
	 * 
	 * @param	inOutputAr		archive to use for logging results
	 * @param	inOuter			only consider objects that do not have this object as its Outer
	 * @param	inSource		object to show references for
	 * @param	inExclude		list of objects that should be ignored if encountered while serializing SourceObject
	 */
	FArchiveFindReferences( TSet<UObject*> InRootSet, TSet<UObject*> &inFound, TArray<UObject*> &inExclude )
		: Exclude(inExclude)
		, Found(inFound)
		, RootSet(InRootSet)
	{
		ArIsObjectReferenceCollector = true;
		ArIsSaving = true;

		for ( const auto& Object : RootSet )
		{
			RootSetArray.Add( Object );
		}
		
		// loop through all the objects in the root set and serialize them
		for ( int RootIndex = 0; RootIndex < RootSetArray.Num(); ++RootIndex )
		{
			UObject* SourceObject = RootSetArray[RootIndex];

			// quick sanity check
			check(SourceObject);
			check(SourceObject->IsValidLowLevel());

			SourceObject->Serialize( *this );
		}

	}

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override { return TEXT("FArchiveFindReferences"); }
};

void UCookOnTheFlyServer::GetDependentPackages(const TSet<UPackage*>& RootPackages, TSet<FName>& FoundPackages)
{
	check(IsChildCooker() == false);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TSet<FName> RootPackageFNames;
	for (const UPackage* RootPackage : RootPackages)
	{
		RootPackageFNames.Add(RootPackage->GetFName());
	}


	GetDependentPackages(RootPackageFNames, FoundPackages);

}


void UCookOnTheFlyServer::GetDependentPackages( const TSet<FName>& RootPackages, TSet<FName>& FoundPackages )
{
	check(IsChildCooker() == false);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FName> FoundPackagesArray;
	for (const auto& RootPackage : RootPackages)
	{
		FoundPackagesArray.Add(RootPackage);
		FoundPackages.Add(RootPackage);
	}

	int FoundPackagesCounter = 0;
	while ( FoundPackagesCounter < FoundPackagesArray.Num() )
	{
		TArray<FName> PackageDependencies;
		if (AssetRegistry.GetDependencies(FoundPackagesArray[FoundPackagesCounter], PackageDependencies) == false)
		{
			// this could happen if we are in the editor and the dependency list is not up to date

			if (IsCookingInEditor() == false)
			{
				UE_LOG(LogCook, Fatal, TEXT("Unable to find package %s in asset registry.  Can't generate cooked asset registry"), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find package %s in asset registry, cooked asset registry information may be invalid "), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
		}
		++FoundPackagesCounter;
		for ( const auto& OriginalPackageDependency : PackageDependencies )
		{
			// check(PackageDependency.ToString().StartsWith(TEXT("/")));
			FName PackageDependency = OriginalPackageDependency;
			FString PackageDepdencyString = PackageDependency.ToString();

			FText OutReason;
			const bool bIncludeReadOnlyRoots = true; // Dependency packages are often script packages (read-only)
			if (!FPackageName::IsValidLongPackageName(PackageDepdencyString, bIncludeReadOnlyRoots, &OutReason))
			{
				const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
					FText::FromString(PackageDepdencyString), OutReason);

				LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("%s"), *( FailMessage.ToString() ));
				continue;
			}
			else if (FPackageName::IsScriptPackage(PackageDepdencyString))
			{
				continue;
			}

			if ( FoundPackages.Contains(PackageDependency) == false )
			{
				FoundPackages.Add(PackageDependency);
				FoundPackagesArray.Add( PackageDependency );
			}
		}
	}

}

void UCookOnTheFlyServer::GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found)
{
	TSet<UObject*> RootSet;

	for (UPackage* Package : Packages)
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true);
		for (UObject* Obj : ObjectsInPackage)
		{
			RootSet.Add(Obj);
			Found.Add(Obj);
		}
	}

	TArray<UObject*> Exclude;
	FArchiveFindReferences ArFindReferences( RootSet, Found, Exclude );
}

bool UCookOnTheFlyServer::ContainsMap(const FName& PackageName) const
{
	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	TArray<FAssetData> Assets;
	ensure( AssetRegistry.GetAssetsByPackageName(PackageName, Assets) );

	for (const auto& Asset : Assets)
	{
		if (Asset.GetClass()->IsChildOf(UWorld::StaticClass()) || Asset.GetClass()->IsChildOf(ULevel::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

bool UCookOnTheFlyServer::IsCookingInEditor() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;
}

bool UCookOnTheFlyServer::IsRealtimeMode() const 
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;
}

bool UCookOnTheFlyServer::IsCookByTheBookMode() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookByTheBook;
}

bool UCookOnTheFlyServer::IsCookOnTheFlyMode() const
{
	return CurrentCookMode == ECookMode::CookOnTheFly || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor; 
}

FString UCookOnTheFlyServer::GetDLCContentPath()
{
	return FPaths::GamePluginsDir() / CookByTheBookOptions->DlcName / FString(TEXT("Content"));
}

COREUOBJECT_API extern bool GOutputCookingWarnings;


/**
 * Callback for handling string asset references being loaded
 */
void UCookOnTheFlyServer::OnStringAssetReferenceLoadedPackage(const FName& PackageFName)
{
	if (IsCookByTheBookMode())
	{
		FName StandardPackageName = GetCachedStandardPackageFileFName(PackageFName);
		if (StandardPackageName != NAME_None)
		{
			RequestPackage(StandardPackageName, true); // force to front of queue because we know this package is now loaded
		}
		else
		{
			if (!FPackageName::IsScriptPackage(PackageFName.ToString()))
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find cached package name for package %s"), *PackageFName.ToString());
			}
		}
	}
}


bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue)
{
	FFilePlatformRequest FileRequest(StandardPackageFName, TargetPlatforms);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}


bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue)
{
	check(IsCookByTheBookMode());
	// need target platforms if we are not in cook by the book mode
	FFilePlatformRequest FileRequest(StandardPackageFName, CookByTheBookOptions->TargetPlatformNames);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}

// should only call this after tickchildcookers returns false
void UCookOnTheFlyServer::CleanUpChildCookers()
{
	if (IsCookByTheBookMode())
	{
		for (auto& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			check(ChildCooker.bFinished == true);
			if (ChildCooker.Thread != nullptr)
			{
				ChildCooker.Thread->WaitForCompletion();
				delete ChildCooker.Thread;
				ChildCooker.Thread = nullptr;
			}

		}

		
	}
}

bool UCookOnTheFlyServer::TickChildCookers()
{
	if (IsCookByTheBookMode())
	{
		for (auto& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			if (ChildCooker.bFinished == false)
			{
				return false;
			}
		}
	}
	return true;
}


// callback just before the garbage collector gets called.
void UCookOnTheFlyServer::PreGarbageCollect()
{
	PackageReentryData.Empty();
}

UCookOnTheFlyServer::FReentryData& UCookOnTheFlyServer::GetReentryData(const UPackage* Package)
{
	auto& CurrentReentryData = PackageReentryData.FindOrAdd(Package->GetFName());

	if ( (CurrentReentryData.bIsValid == false) && (Package->IsFullyLoaded() == true))
	{
		CurrentReentryData.bIsValid = true;
		CurrentReentryData.FileName = Package->GetFName();
		GetObjectsWithOuter(Package, CurrentReentryData.CachedObjectsInOuter);	
}
	return CurrentReentryData;
}


uint32 UCookOnTheFlyServer::TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackageCount, ECookTickFlags TickFlags )
{
	COOK_STAT(FScopedDurationTimer TickTimer(DetailedCookStats::TickCookOnTheSideTimeSec));
	FCookerTimer Timer(TimeSlice, IsRealtimeMode());

	uint32 Result = 0;

	if (IsChildCooker() == false)
	{
		static const FName AssetRegistryName("AssetRegistry");
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		if (AssetRegistry.IsLoadingAssets())
		{
			// early out
			return Result;
		}
	}

	// This is all the target platforms which we needed to process requests for this iteration
	// we use this in the unsolicited packages processing below
	TArray<FName> AllTargetPlatformNames;
	
	if (CurrentCookMode == ECookMode::CookByTheBook)
	{
		if (CookRequests.HasItems() == false)
		{
			SCOPE_TIMER(WaitingForChildCookers);
			// we have nothing left to cook so we are now waiting for child cookers to finish
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
			
		}
		else
		{
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
		}
	}

	while (!GIsRequestingExit || CurrentCookMode == ECookMode::CookByTheBook)
	{
		// if we just cooked a map then don't process anything the rest of this tick
		if ( Result & COSR_RequiresGC )
		{
			break;
		}

		if ( IsCookByTheBookMode() )
		{
			check( CookByTheBookOptions );
			if ( CookByTheBookOptions->bCancel )
			{
				CancelCookByTheBook();
			}
		}

		FFilePlatformRequest ToBuild;

		//bool bIsUnsolicitedRequest = false;
		if ( CookRequests.HasItems() )
		{
			CookRequests.Dequeue( &ToBuild );
		}
		else
		{
			// no more to do this tick break out and do some other stuff
			break;
		}

		// prevent autosave from happening until we are finished cooking
		// causes really bad hitches
		if ( GUnrealEd )
		{
			const static float SecondsWarningTillAutosave = 10.0f;
			GUnrealEd->GetPackageAutoSaver().ForceMinimumTimeTillAutoSave(SecondsWarningTillAutosave);
		}

		if (CookedPackages.Exists(ToBuild))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package for platform already cooked %s, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			continue;
		}

#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Processing package %s"), *ToBuild.GetFilename().ToString());
#endif
		SCOPE_TIMER(TickCookOnTheSide);

		check( ToBuild.IsValid() );
		const TArray<FName> &TargetPlatformNames = ToBuild.GetPlatformnames();

		TArray<UPackage*> PackagesToSave;

#if OUTPUT_TIMING
		//FScopeTimer PackageManualTimer( ToBuild.GetFilename().ToString(), false );
#endif

		for ( const auto &PlatformName : TargetPlatformNames )
		{
			AllTargetPlatformNames.AddUnique(PlatformName);
		}

		for ( const auto &PlatformName : AllTargetPlatformNames )
		{
			if ( ToBuild.HasPlatform(PlatformName) == false )
			{
				ToBuild.AddPlatform(PlatformName);
			}
		}

		bool bWasUpToDate = false;

		bool bLastLoadWasMap = false;
		FString LastLoadedMapName;

		const FString BuildFilename = ToBuild.GetFilename().ToString();

		// if we have no target platforms then we want to cook because this will cook for all target platforms in that case
		bool bShouldCook = TargetPlatformNames.Num() > 0 ? false : ShouldCook( BuildFilename, NAME_None );
		{
			SCOPE_TIMER(ShouldCook);
			for ( int Index = 0; Index < TargetPlatformNames.Num(); ++Index )
			{
				bShouldCook |= ShouldCook( ToBuild.GetFilename().ToString(), TargetPlatformNames[Index] );
			}
		}
		
		if( CookByTheBookOptions && CookByTheBookOptions->bErrorOnEngineContentUse )
		{
			check(IsCookingDLC());
			FString DLCPath = GetDLCContentPath();
			if ( ToBuild.GetFilename().ToString().StartsWith(DLCPath) == false ) // if we don't start with the dlc path then we shouldn't be cooking this data 
			{
				UE_LOG(LogCook, Error, TEXT("Engine content %s is being referenced by DLC!"), *ToBuild.GetFilename().ToString() );
				bShouldCook = false;
			}
		}

		check(IsInGameThread());
		if (NeverCookPackageList.Contains(ToBuild.GetFilename()))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package %s requested but is in the never cook package list, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			bShouldCook = false;
		}


		if ( bShouldCook ) // if we should cook the package then cook it otherwise add it to the list of already cooked packages below
		{
			COOK_STAT(FScopedDurationTimer LoadPackagesTimer(DetailedCookStats::TickCookOnTheSideLoadPackagesTimeSec));
			UPackage *Package = NULL;
			{
				FString PackageName;
				if ( FPackageName::TryConvertFilenameToLongPackageName(BuildFilename, PackageName) )
				{
					Package = FindObject<UPackage>( ANY_PACKAGE, *PackageName );
				}
			}

#if DEBUG_COOKONTHEFLY
			UE_LOG( LogCook, Display, TEXT("Processing request %s"), *BuildFilename);
#endif
			static TSet<FString> CookWarningsList;
			if ( CookWarningsList.Contains(BuildFilename) == false )
			{
				CookWarningsList.Add( BuildFilename );
				GOutputCookingWarnings = IsCookFlagSet(ECookInitializationFlags::OutputVerboseCookerWarnings);
			}
			
			//  if the package is already loaded then try to avoid reloading it :)
			if ( ( Package == NULL ) || ( Package->IsFullyLoaded() == false ) )
			{
				GIsCookerLoadingPackage = true;
				SCOPE_TIMER(LoadPackage);
				Package = LoadPackage( NULL, *BuildFilename, LOAD_None );
				INC_INT_STAT(LoadPackage, 1);

				GIsCookerLoadingPackage = false;
			}
#if DEBUG_COOKONTHEFLY
			else
			{
				UE_LOG(LogCook, Display, TEXT("Package already loaded %s avoiding reload"), *BuildFilename );
			}
#endif

			if (Package == NULL)
			{
				if ((!IsCookOnTheFlyMode()) || (!IsCookingInEditor()))
				{
					LogCookerMessage(FString::Printf(TEXT("Error loading %s!"), *BuildFilename), EMessageSeverity::Error);
					UE_LOG(LogCook, Error, TEXT("Error loading %s!"), *BuildFilename);
				}
				Result |= COSR_ErrorLoadingPackage;
			}
			else
			{
				check(Package);


				if (Package->ContainsMap())
				{
					// load sublevels
					UWorld* World = UWorld::FindWorldInPackage(Package);
					check(World);

					World->PersistentLevel->HandleLegacyMapBuildData();

					// child cookers can't process maps
					// check(IsChildCooker() == false);
					if ( IsCookByTheBookMode() )
					{
						GIsCookerLoadingPackage = true;
						// TArray<FString> PreviouslyCookedPackages;
						if (World->StreamingLevels.Num())
						{
							//World->LoadSecondaryLevels(true, &PreviouslyCookedPackages);
							World->LoadSecondaryLevels(true, NULL);
						}
						GIsCookerLoadingPackage = false;
						TArray<FString> NewPackagesToCook;

						// Collect world composition tile packages to cook
						if (World->WorldComposition)
						{
							World->WorldComposition->CollectTilesToCook(NewPackagesToCook);
						}

						for ( auto PackageName : NewPackagesToCook )
						{
							FString Filename;
							if ( FPackageName::DoesPackageExist(PackageName, NULL, &Filename) )
							{
								FString StandardFilename = FPaths::ConvertRelativePathToFull(Filename);
								FPaths::MakeStandardFilename(StandardFilename);
								FName StandardPackageFName = FName(*StandardFilename);
								if (IsChildCooker())
								{
									check(IsCookByTheBookMode());
									// notify the main cooker that it should make sure this package gets cooked
									CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
								}
								else
								{
									CookRequests.EnqueueUnique(FFilePlatformRequest(StandardPackageFName, TargetPlatformNames));
								}
							}
						}

						LastLoadedMapName = Package->GetName();
						bLastLoadWasMap = true;
					}

				}

				FString Name = Package->GetPathName();
				FString PackageFilename( GetCachedStandardPackageFilename( Package ) );
				if ( PackageFilename != BuildFilename )
				{
					// we have saved something which we didn't mean to load 
					//  sounds unpossible.... but it is due to searching for files and such
					//  mark the original request as processed (if this isn't actually the file they were requesting then it will fail)
					//	and then also save our new request as processed so we don't do it again
					PackagesToSave.AddUnique( Package );
#if DEBUG_COOKONTHEFLY
					UE_LOG( LogCook, Display, TEXT("Request for %s received going to save %s"), *BuildFilename, *PackageFilename );
#endif
					CookedPackages.Add( FFilePlatformCookedPackage(ToBuild,false) );
					
					ToBuild.SetFilename( PackageFilename );
				}
				else
				{
					PackagesToSave.AddUnique( Package );
				}
			}
			GOutputCookingWarnings = false;
		}

		
		if ( PackagesToSave.Num() == 0 )
		{
			// if we are iterative cooking the package might already be cooked
			// so just add the package to the cooked packages list
			// this could also happen if the source file doesn't exist which is often as we request files with different extensions when we are searching for files
			// just return that we processed the cook request
			// the network file manager will then handle the missing file and search somewhere else
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Not cooking package %s"), *ToBuild.GetFilename().ToString());
#endif
			// did not cook this package 
#if DO_CHECK
			// make sure this package doesn't exist
			for (const auto& TargetPlatformName : ToBuild.GetPlatformnames())
			{
				const FString SandboxFilename = ConvertToFullSandboxPath(ToBuild.GetFilename().ToString(), true, TargetPlatformName.ToString());
				if (IFileManager::Get().FileExists(*SandboxFilename))
				{
					// if we find the file this means it was cooked on a previous cook, however source package can't be found now. 
					// this could be because the source package was deleted or renamed, and we are using iterative cooking
					// perhaps in this case we should delete it?
					UE_LOG(LogCook, Warning, TEXT("Found cooked file which shouldn't exist as it failed loading %s"), *SandboxFilename); 
					IFileManager::Get().Delete(*SandboxFilename);
				}
			}
#endif
			CookedPackages.Add( FFilePlatformCookedPackage( ToBuild.GetFilename(), TargetPlatformNames, false) );
			continue;
		}

		bool bIsAllDataCached = true;

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		TArray<const ITargetPlatform*> TargetPlatforms;
		for ( const auto& TargetPlatformName : AllTargetPlatformNames )
		{
			TargetPlatforms.Add( TPM.FindTargetPlatform( TargetPlatformName.ToString() ) );
		}


		auto BeginPackageCacheForCookedPlatformData = [&]( UPackage* Package )
		{
			COOK_STAT(FScopedDurationTimer DurationTimer(DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec));

#if DEBUG_COOKONTHEFLY 
			UE_LOG(LogCook, Display, TEXT("Caching objects for package %s"), *Package->GetFName().ToString());
#endif
			MakePackageFullyLoaded(Package);
			auto& CurrentReentryData = GetReentryData(Package);

			if (CurrentReentryData.bIsValid == false)
				return true;

			if ( CurrentReentryData.bBeginCacheFinished )
				return true;

			for (; CurrentReentryData.BeginCacheCount < CurrentReentryData.CachedObjectsInOuter.Num(); ++CurrentReentryData.BeginCacheCount)
			{
				const auto& Obj = CurrentReentryData.CachedObjectsInOuter[CurrentReentryData.BeginCacheCount];
				for ( const auto& TargetPlatform : TargetPlatforms )
				{
					if ( Obj->IsA(UMaterialInterface::StaticClass() ) )
					{
						if ( GShaderCompilingManager->GetNumRemainingJobs() > MaxConcurrentShaderJobs )
						{
#if DEBUG_COOKONTHEFLY
							UE_LOG(LogCook, Display, TEXT("Delaying shader compilation of material %s"), *Obj->GetFullName());
#endif
							return false;
						}
					}
					Obj->BeginCacheForCookedPlatformData(TargetPlatform);
				}

				if ( Timer.IsTimeUp() )
				{
#if DEBUG_COOKONTHEFLY
					UE_LOG(LogCook, Display, TEXT("Object %s took too long to cache"), *Obj->GetFullName());
#endif
					return false;
				}
			}

				CurrentReentryData.bBeginCacheFinished = true;
				return true;
		};

		auto FinishPackageCacheForCookedPlatformData = [&]( UPackage* Package )
		{
			COOK_STAT(FScopedDurationTimer DurationTimer(DetailedCookStats::TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec));

			MakePackageFullyLoaded(Package);
			auto& CurrentReentryData = GetReentryData(Package);

			if ( CurrentReentryData.bIsValid == false )
				return true;

			if ( CurrentReentryData.bFinishedCacheFinished )
				return true;

			for (const auto& Obj : CurrentReentryData.CachedObjectsInOuter)
			{
				for (const auto& TargetPlatform : TargetPlatforms)
				{
					COOK_STAT(double CookerStatSavedValue = DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec);

					if (Obj->IsA(UMaterialInterface::StaticClass()))
					{
						if ( Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform) == false )
						{
							if (GShaderCompilingManager->GetNumRemainingJobs() > MaxConcurrentShaderJobs)
							{
								return false;
							}
						}
					}

					// These begin cache calls should be quick 
					// because they will just be checking that the data is already cached and kicking off new multithreaded requests if not
					// all sync requests should have been caught in the first begincache call above
					Obj->BeginCacheForCookedPlatformData(TargetPlatform);
					// We want to measure inclusive time for this function, but not accumulate into the BeginXXX timer, so subtract these times out of the BeginTimer.
					COOK_STAT(DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec = CookerStatSavedValue);
					if (Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform) == false)
					{
#if DEBUG_COOKONTHEFLY
						UE_LOG(LogCook, Display, TEXT("Object %s isn't cached yet"), *Obj->GetFullName());
#endif
						/*if ( Obj->IsA(UMaterial::StaticClass()) )
						{
							if (GShaderCompilingManager->HasShaderJobs() == false)
							{
								UE_LOG(LogCook, Warning, TEXT("Shader compiler is in a bad state!  Shader %s is finished compile but shader compiling manager did not notify shader.  "), *Obj->GetPathName());
							}
						}*/
						return false;
					}
				}
			}

			for (const auto& Obj: CurrentReentryData.CachedObjectsInOuter)
			{
				// if this objects data is cached then we can call FinishedCookedPLatformDataCache
				// we can only safely call this when we are finished caching this object completely.
				// this doesn't ever happen for cook in editor or cook on the fly mode
				if (CurrentCookMode == ECookMode::CookByTheBook)
				{
					check( !IsCookingInEditor() );
					// this might be run multiple times for a single object
					Obj->WillNeverCacheCookedPlatformDataAgain();
				}
			}

			CurrentReentryData.bFinishedCacheFinished = true;
			return true;
		};

		GShaderCompilingManager->ProcessAsyncResults(true, false);


		if ( PackagesToSave.Num() )
		{
			SCOPE_TIMER(CallBeginCacheForCookedPlatformData);
			// cache the resources for this package for each platform
			
			bIsAllDataCached &= BeginPackageCacheForCookedPlatformData(PackagesToSave[0]);
			if( bIsAllDataCached )
			{
				bIsAllDataCached &= FinishPackageCacheForCookedPlatformData(PackagesToSave[0]);
			}
		}


		bool ShouldTickPrecache = true;

		// if we are ready to save then don't waste time precaching other stuff
		if ( bIsAllDataCached == true )
		{
			ShouldTickPrecache = false;
		}
		// don't do this if we are in a commandlet because the save section will prefetch 
		if (!IsRealtimeMode())
		{
			ShouldTickPrecache = false;
		}
		else
		{
			// if we are doing no shader compilation right now try and precache something so that we load up the cpu
			if ( GShaderCompilingManager->GetNumRemainingJobs() == 0 )
			{
				ShouldTickPrecache = true;
			}
		}

		// cook on the fly mode we don't want to precache here because save package is going to stall on this package, we don't want to flood the system with precache requests before we stall
		if (IsCookOnTheFlyMode()) 
		{
			ShouldTickPrecache = false;
		}

		// if we are in the cook commandlet then this data will get cached in the save package section
		// if ( (bIsAllDataCached == false) && IsRealtimeMode())
		if (ShouldTickPrecache)
		{
			double PrecacheTimeSlice = Timer.GetTimeRemain();
			if (PrecacheTimeSlice > 0.0f)
			{
				TickPrecacheObjectsForPlatforms(PrecacheTimeSlice, TargetPlatforms);
			}
		}

		int32 FirstUnsolicitedPackage = PackagesToSave.Num();

		// generate a list of other packages which were loaded with this one
		if ( !IsCookByTheBookMode() || (CookByTheBookOptions->bDisableUnsolicitedPackages == false))
		{
			//GRedirectCollector.ResolveStringAssetReference();
			SCOPE_TIMER(UnsolicitedMarkup);
			bool ContainsFullAssetGCClasses = false;
			GetUnsolicitedPackages(PackagesToSave, ContainsFullAssetGCClasses, AllTargetPlatformNames);
			if (ContainsFullAssetGCClasses)
			{
				Result |= COSR_RequiresGC;
			}
		}


		// if ( IsRealtimeMode() )
		{
			if ((bIsAllDataCached == false) && IsCookByTheBookMode() && !IsRealtimeMode())
			{
				// don't load anymore stuff unless we have space and we don't already have enough stuff to save
				if ((!(Result & COSR_RequiresGC)) && 
					(HasExceededMaxMemory()==false) && 
					((Timer.NumPackagesSaved + PackagesToSave.Num()) < Timer.MaxNumPackagesToSave)) // if we don't need to GC and also we have memory then load some more packages ;)
				{
					GShaderCompilingManager->ProcessAsyncResults(true, false); // we can afford to do work here because we are essentually requing this package for processing later
					Timer.SavedPackage();  // this is a special case to prevent infinite loop, if we only have one package we might fall through this and could loop forever.  
					CookRequests.EnqueueUnique(ToBuild, false);
					continue;
				}
				else
				{
					//was gonna quit but didn't
					// UE_LOG(LogCook, Display, TEXT("Was gonna exit out but didn't num packages to save %d"), PackagesToSave.Num());

					// reque the current task and process it next tick
					/*
					CookRequests.EnqueueUnique(ToBuild, true);
					Result |= COSR_WaitingOnCache;
					break; // break out of the package tick loop
					*/
				}
			}
		}

		bool bFinishedSave = true;

		if ( PackagesToSave.Num() )
		{
			int32 OriginalPackagesToSaveCount = PackagesToSave.Num();
			SCOPE_TIMER(SavingPackages);
			for ( int32 I = 0; I < PackagesToSave.Num(); ++I )
			{
				UPackage* Package = PackagesToSave[I];
				if (Package->IsLoadedByEditorPropertiesOnly() && UncookedEditorOnlyPackages.Contains(Package->GetFName()))
				{
					// We already attempted to cook this package and it's still not referenced by any non editor-only properties.
					continue;
				}

				// This package is valid, so make sure it wasn't previously marked as being an uncooked editor only package or it would get removed from the
				// asset registry at the end of the cook
				UncookedEditorOnlyPackages.Remove(Package->GetFName());

				const FName StandardPackageFilename = GetCachedStandardPackageFileFName(Package);
				check(IsInGameThread());
				if (NeverCookPackageList.Contains(StandardPackageFilename))
				{
					// refuse to save this package, it's clearly one of the undesirables
					continue;
				}

				
				FName PackageFName = GetCachedStandardPackageFileFName(Package);
				TArray<FName> SaveTargetPlatformNames = AllTargetPlatformNames;
				TArray<FName> CookedTargetPlatforms;
				if (CookedPackages.GetCookedPlatforms(PackageFName, CookedTargetPlatforms))
				{
					for (auto const &CookedPlatform : CookedTargetPlatforms)
					{
						SaveTargetPlatformNames.Remove(CookedPlatform);
					}
				}

				// we somehow already cooked this package not sure how that can happen because the PackagesToSave list should have already filtered this
				if (SaveTargetPlatformNames.Num() == 0)
				{
					UE_LOG(LogCook, Warning, TEXT("Allready saved this package not sure how this got here!"));
					// already saved this package
					continue;
				}


				// if we are processing unsolicited packages we can optionally not save these right now
				// the unsolicited packages which we missed now will be picked up on next run
				// we want to do this in cook on the fly also, if there is a new network package request instead of saving unsolicited packages we can process the requested package

				bool bShouldFinishTick = false;

				if (IsCookByTheBookMode() && Timer.IsTimeUp())
				{
					bShouldFinishTick = true;
					// our timeslice is up
				}

				// if we are cook the fly then save the package which was requested as fast as we can because the client is waiting on it

				bool ProcessingUnsolicitedPackages = (I >= FirstUnsolicitedPackage);
				bool ForceSavePackage = false;

				if ( IsCookOnTheFlyMode() )
				{
					if (ProcessingUnsolicitedPackages)
					{
						SCOPE_TIMER(WaitingForCachedCookedPlatformData);
						if ( CookRequests.HasItems() )
						{
							bShouldFinishTick = true;
						}
						if (Timer.IsTimeUp())
						{
							bShouldFinishTick = true;
							// our timeslice is up
						}
						bool bFinishedCachingCookedPlatformData = false;
						// if we are in realtime mode then don't wait forever for the package to be ready
						while ( (!Timer.IsTimeUp()) && IsRealtimeMode() && (bShouldFinishTick == false) )
						{
							if ( FinishPackageCacheForCookedPlatformData(Package) == true )
							{
								bFinishedCachingCookedPlatformData = true;
								break;
							}

							GShaderCompilingManager->ProcessAsyncResults(true, false);
							// sleep for a bit
							FPlatformProcess::Sleep(0.0f);
						}
						bShouldFinishTick |= !bFinishedCachingCookedPlatformData;
					}
					else
					{
						ForceSavePackage = true;
					}
				}

				bool AllObjectsCookedDataCached = true;
				bool HasCheckedAllPackagesAreCached = (I >= OriginalPackagesToSaveCount);

				MakePackageFullyLoaded(Package);

				if ( !bShouldFinishTick )
				{
					AllObjectsCookedDataCached = FinishPackageCacheForCookedPlatformData(Package);
					if ( AllObjectsCookedDataCached == false)
					{
						GShaderCompilingManager->ProcessAsyncResults(true, false);
						AllObjectsCookedDataCached = FinishPackageCacheForCookedPlatformData(Package);
					}
				}

				// if we are in realtime mode and this package isn't ready to be saved then we should exit the tick here so we don't save it while in launch on
				if ( IsRealtimeMode() && 
					(!IsCookOnTheFlyMode() || ProcessingUnsolicitedPackages) && // if we are in cook ont eh fly mode, EVEN if we are realtime we should force save if this is not an unsolicited package
					(AllObjectsCookedDataCached == false) && 
					HasCheckedAllPackagesAreCached )
				{
					bShouldFinishTick = true;
				}

				if (bShouldFinishTick && (!ForceSavePackage))
				{
					SCOPE_TIMER(EnqueueUnsavedPackages);
					// enqueue all the packages which we were about to save
					Timer.SavedPackage();  // this is a special case to prevent infinite loop, if we only have one package we might fall through this and could loop forever.  
					if (IsCookByTheBookMode())
					{
						for (int32 RemainingIndex = I; RemainingIndex < PackagesToSave.Num(); ++RemainingIndex)
						{
							FName StandardFilename = GetCachedStandardPackageFileFName(PackagesToSave[RemainingIndex]);
							CookRequests.EnqueueUnique(FFilePlatformRequest(StandardFilename, AllTargetPlatformNames));
						}
					}
					else
					{
						check(ProcessingUnsolicitedPackages == true);
					}
					Result |= COSR_WaitingOnCache;

					// break out of the loop
					bFinishedSave = false;
					break;
				}

				
				// don't precache other packages if our package isn't ready but we are going to save it.   This will fill up the worker threads with extra shaders which we may need to flush on 
				if ( (!IsCookOnTheFlyMode()) &&  
					(!IsRealtimeMode() || AllObjectsCookedDataCached == true) )  
				{
					// precache platform data for next package 
					UPackage *NextPackage = PackagesToSave[FMath::Min(PackagesToSave.Num() - 1, I + 1)];
					UPackage *NextNextPackage = PackagesToSave[FMath::Min(PackagesToSave.Num() - 1, I + 2)];
					if (NextPackage != Package)
					{
						SCOPE_TIMER(PrecachePlatformDataForNextPackage);
						BeginPackageCacheForCookedPlatformData(NextPackage);
					}
					if (NextNextPackage != NextPackage)
					{
						SCOPE_TIMER(PrecachePlatformDataForNextNextPackage);
						BeginPackageCacheForCookedPlatformData(NextNextPackage);
					}
				}

				// if we are running the cook commandlet
				// if we already went through the entire package list then don't keep requeuing requests
				if ( (HasCheckedAllPackagesAreCached == false) && 
					(AllObjectsCookedDataCached == false) && 
					(ForceSavePackage == false) )
				{
					check(IsCookByTheBookMode() || ProcessingUnsolicitedPackages == true);
					// add to back of queue
					PackagesToSave.Add(Package);
					// UE_LOG(LogCook, Display, TEXT("Delaying save for package %s"), *PackageFName.ToString());
					continue;
				}

				if (HasCheckedAllPackagesAreCached)
				{
					UE_LOG(LogCook, Display, TEXT("Forcing save package %s because was already requeued once"), *PackageFName.ToString());
				}


				bool bShouldSaveAsync = true;
				FString Temp;
				if ( FParse::Value( FCommandLine::Get(), TEXT("-diffagainstcookdirectory="), Temp ) || FParse::Value(FCommandLine::Get(), TEXT("-breakonfile="), Temp))
				{
					// async save doesn't work with this flags
					bShouldSaveAsync = false;
				}

				ESavePackageResult SavePackageResult = ESavePackageResult::Error;
				bool bSucceededSavePackage = false;
				{
					COOK_STAT(FScopedDurationTimer TickCookOnTheSideSaveCookedPackageTimer(DetailedCookStats::TickCookOnTheSideSaveCookedPackageTimeSec));
					SCOPE_TIMER(SaveCookedPackage);
					uint32 SaveFlags = SAVE_KeepGUID | (bShouldSaveAsync ? SAVE_Async : SAVE_None) | (IsCookFlagSet(ECookInitializationFlags::Unversioned) ? SAVE_Unversioned : 0);

					bool KeepEditorOnlyPackages = false;
					// removing editor only packages only works when cooking in commandlet and non iterative cooking
					// also doesn't work in multiprocess cooking
					KeepEditorOnlyPackages = !(IsCookByTheBookMode() && !IsCookingInEditor());
					KeepEditorOnlyPackages |= IsCookFlagSet(ECookInitializationFlags::Iterative);
					KeepEditorOnlyPackages |= IsChildCooker() || (CookByTheBookOptions && CookByTheBookOptions->ChildCookers.Num() > 0);
					SaveFlags |= KeepEditorOnlyPackages ? SAVE_KeepEditorOnlyCookedPackages : SAVE_None;

					GOutputCookingWarnings = IsCookFlagSet(ECookInitializationFlags::OutputVerboseCookerWarnings);
					SavePackageResult = SaveCookedPackage(Package, SaveFlags, bWasUpToDate, AllTargetPlatformNames);
					GOutputCookingWarnings = false;

					if (SavePackageResult == ESavePackageResult::Success)
					{
						bSucceededSavePackage = true;
						// Update flags used to determine garbage collection.
						if (Package->ContainsMap())
						{
							Result |= COSR_CookedMap;
						}
						else
						{
							++CookedPackageCount;
							Result |= COSR_CookedPackage;
						}
					}
					Timer.SavedPackage();
				}

				if ( IsCookingInEditor() == false )
				{
					SCOPE_TIMER(ClearAllCachedCookedPlatformData);
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter(Package, ObjectsInPackage);
					for ( const auto& Object : ObjectsInPackage )
					{
						Object->ClearAllCachedCookedPlatformData();
					}
				}

				//@todo ResetLoaders outside of this (ie when Package is NULL) causes problems w/ default materials
				if (Package->IsRooted() == false && ((CurrentCookMode==ECookMode::CookOnTheFly)) )
				{
					SCOPE_TIMER(ResetLoaders);
					ResetLoaders(Package);
				}

				FName StandardFilename = GetCachedStandardPackageFileFName(Package);

				// We always want to mark package as processed unless it wasn't saved because it was referenced by editor-only data
				// in which case we may still need to save it later when new content loads it through non editor-only references
				if (StandardFilename != NAME_None)
				{
					// mark the package as cooked
					FFilePlatformCookedPackage FileRequest(StandardFilename, AllTargetPlatformNames, bSucceededSavePackage);
					if (SavePackageResult != ESavePackageResult::ReferencedOnlyByEditorOnlyData)
					{
						CookedPackages.Add(FileRequest);

						if ((CurrentCookMode == ECookMode::CookOnTheFly) && (I >= FirstUnsolicitedPackage))
						{
							// this is an unsolicited package
							if ((FPaths::FileExists(FileRequest.GetFilename().ToString()) == true) &&
								(bWasUpToDate == false))
							{
								UnsolicitedCookedPackages.AddCookedPackage(FileRequest);
#if DEBUG_COOKONTHEFLY
								UE_LOG(LogCook, Display, TEXT("UnsolicitedCookedPackages: %s"), *FileRequest.GetFilename().ToString());
#endif
							}
						}
					}
					else
					{
						UncookedEditorOnlyPackages.AddUnique(Package->GetFName());
					}
				}
				else
				{
					check( bSucceededSavePackage == false );
				}
			}
		}


		// TODO: Daniel: this is reference code needs to be reimplemented on the callee side.
		//  cooker can't depend on callee being able to garbage collect
		// collect garbage
		if ( CookByTheBookOptions && CookByTheBookOptions->bLeakTest && bFinishedSave )
		{
			check( CurrentCookMode == ECookMode::CookByTheBook);
			UE_LOG(LogCook, Display, TEXT("Full GC..."));

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			for (FObjectIterator It; It; ++It)
			{
				if (!CookByTheBookOptions->LastGCItems.Contains(FWeakObjectPtr(*It)))
				{
					UE_LOG(LogCook, Warning, TEXT("\tLeaked %s"), *(It->GetFullName()));
					CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
				}
			}
		}

		

		if ( Timer.IsTimeUp() )
		{
			break;
		}
	}

	OUTPUT_TIMERS();

	if ( CookByTheBookOptions )
	{
		CookByTheBookOptions->CookTime += Timer.GetTimeTillNow();
	}

	if ( IsCookByTheBookRunning() && 
		( CookRequests.HasItems() == false ) &&
		(!(Result & COSR_WaitingOnChildCookers)) )
	{
		check(IsCookByTheBookMode());
		// UE_LOG(LogCook, Display, TEXT("String asset reference resolve tried %d did %d"), bTriedToRunStringAssetReferenceResolve, bHasRunStringAssetReferenceResolve);
		// if we are out of stuff and we are in cook by the book from the editor mode then we finish up
		CookByTheBookFinished();
	}

	return Result;
}

void UCookOnTheFlyServer::TickPrecacheObjectsForPlatforms(const float TimeSlice, const TArray<const ITargetPlatform*>& TargetPlatforms) 
{
	
	SCOPE_CYCLE_COUNTER(STAT_TickPrecacheCooking);

	
	FCookerTimer Timer(TimeSlice, true);

	++LastUpdateTick;
	if (LastUpdateTick > 50 ||
		((CachedMaterialsToCacheArray.Num() == 0) && (CachedTexturesToCacheArray.Num() == 0)))
	{
		LastUpdateTick = 0;
		TArray<UObject*> Materials;
		GetObjectsOfClass(UMaterial::StaticClass(), Materials, true);
		for (const auto& Material : Materials)
		{
			if ( Material->GetOutermost() == GetTransientPackage())
				continue;

			CachedMaterialsToCacheArray.Add(Material);
		}
		TArray<UObject*> Textures;
		GetObjectsOfClass(UTexture::StaticClass(), Textures, true);
		for (const auto& Texture : Textures)
		{
			if (Texture->GetOutermost() == GetTransientPackage())
				continue;

			CachedTexturesToCacheArray.Add(Texture);
		}
	}

	if (Timer.IsTimeUp())
		return;

	bool AllMaterialsCompiled = true;
	// queue up some shaders for compilation

	while (CachedMaterialsToCacheArray.Num() > 0)
	{
		UMaterial* Material = (UMaterial*)(CachedMaterialsToCacheArray[0].Get());
		CachedMaterialsToCacheArray.RemoveAtSwap(0, 1, false);

		if (Material == nullptr)
		{
			continue;
		}

		for (const auto& TargetPlatform : TargetPlatforms)
		{
			if (!TargetPlatform)
			{
				continue;
			}
			if (!Material->IsCachedCookedPlatformDataLoaded(TargetPlatform))
			{
				Material->BeginCacheForCookedPlatformData(TargetPlatform);
				AllMaterialsCompiled = false;
			}
		}

		if (Timer.IsTimeUp())
			return;

		if (GShaderCompilingManager->GetNumRemainingJobs() > MaxPrecacheShaderJobs)
		{
			return;
		}
	}


	if (!AllMaterialsCompiled)
	{
		return;
	}

	while (CachedTexturesToCacheArray.Num() > 0)
	{
		UTexture* Texture = (UTexture*)(CachedTexturesToCacheArray[0].Get());
		CachedTexturesToCacheArray.RemoveAtSwap(0, 1, false);

		if (Texture == nullptr)
		{
			continue;
		}

		for (const auto& TargetPlatform : TargetPlatforms)
		{
			if (!TargetPlatform)
			{
				continue;
			}
			Texture->BeginCacheForCookedPlatformData(TargetPlatform);
		}
		if (Timer.IsTimeUp())
			return;
	}

}

bool UCookOnTheFlyServer::HasExceededMaxMemory() const
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	//  if we have less emmory free then we should have then gc some stuff
	if ((MemStats.AvailablePhysical < MinFreeMemory) && 
		(MinFreeMemory != 0) )
	{
		UE_LOG(LogCook, Display, TEXT("Available physical memory low %d kb, exceeded max memory"), MemStats.AvailablePhysical / 1024);
		return true;
	}

	// don't gc if we haven't reached our min gc level yet
	if (MemStats.UsedVirtual < MinMemoryBeforeGC)
	{
		return false;
	}

	//uint64 UsedMemory = MemStats.UsedVirtual; 
	uint64 UsedMemory = MemStats.UsedPhysical; //should this be used virtual?
	if ((UsedMemory >= MaxMemoryAllowance) &&
		(MaxMemoryAllowance > 0u))
	{
		UE_LOG(LogCook, Display, TEXT("Used memory high %d kb, exceeded max memory"), MemStats.UsedPhysical / 1024);
		return true;
	}

	

	return false;
}

void UCookOnTheFlyServer::GetUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, bool &ContainsFullGCAssetClasses, const TArray<FName>& TargetPlatformNames) const
{
	TSet<UPackage*> PackagesToSaveSet;
	for (UPackage* Package : PackagesToSave)
	{
		PackagesToSaveSet.Add(Package);
	}
	PackagesToSave.Empty();
			
	TArray<UObject *> ObjectsInOuter;
	{
		SCOPE_TIMER(GetObjectsWithOuter);
		GetObjectsWithOuter(NULL, ObjectsInOuter, false);
	}

	TArray<FName> PackageNames;
	PackageNames.Empty(ObjectsInOuter.Num());
	{
		SCOPE_TIMER(GeneratePackageNames);
		ACCUMULATE_TIMER(UnsolicitedPackageAlreadyCooked);
		ACCUMULATE_TIMER(PackageCast);
		ACCUMULATE_TIMER(FullGCAssetsContains);
		ACCUMULATE_TIMER(AddUnassignedPackageToManifest);
		ACCUMULATE_TIMER(GetCachedName);
		ACCUMULATE_TIMER(AddToPackageList);
		for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
		{
			ACCUMULATE_TIMER_START(PackageCast);
			UPackage* Package = Cast<UPackage>(ObjectsInOuter[Index]);
			ACCUMULATE_TIMER_STOP(PackageCast);

			ACCUMULATE_TIMER_START(FullGCAssetsContains);
			UObject* Object = ObjectsInOuter[Index];
			if (FullGCAssetClasses.Contains(Object->GetClass()))
			{
				ContainsFullGCAssetClasses = true;
			}
			
			ACCUMULATE_TIMER_STOP(FullGCAssetsContains);

			if (Package)
			{
				ACCUMULATE_TIMER_START(GetCachedName);
				FName StandardPackageFName = GetCachedStandardPackageFileFName(Package);
				ACCUMULATE_TIMER_STOP(GetCachedName);
				if (StandardPackageFName == NAME_None)
					continue;

				ACCUMULATE_TIMER_START(UnsolicitedPackageAlreadyCooked);
				// package is already cooked don't care about processing it again here
				/*if ( CookRequests.Exists( StandardPackageFName, AllTargetPlatformNames) )
					continue;*/

				if (CookedPackages.Exists(StandardPackageFName, TargetPlatformNames))
					continue;
				ACCUMULATE_TIMER_STOP(UnsolicitedPackageAlreadyCooked);
				
				ACCUMULATE_TIMER_START(AddToPackageList);

				if (StandardPackageFName != NAME_None) // if we have name none that means we are in core packages or something...
				{
					if (IsChildCooker())
					{
						// notify the main cooker that it should make sure this package gets cooked
						CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
					}
					else
					{
						// check if the package has already been saved
						PackagesToSaveSet.Add(Package);
						continue;
					}
				}
				ACCUMULATE_TIMER_STOP(AddToPackageList);
			}
		}
	}
	
	for (UPackage* Package : PackagesToSaveSet )
	{
		PackagesToSave.Add(Package);
	}
}

void UCookOnTheFlyServer::OnObjectModified( UObject *ObjectMoving )
{
	if (IsGarbageCollecting())
	{
		return;
	}
	OnObjectUpdated( ObjectMoving );
}

void UCookOnTheFlyServer::OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (IsGarbageCollecting())
	{
		return;
	}

	OnObjectUpdated( ObjectBeingModified );
}

void UCookOnTheFlyServer::OnObjectSaved( UObject* ObjectSaved )
{
	OnObjectUpdated( ObjectSaved );
}

void UCookOnTheFlyServer::OnObjectUpdated( UObject *Object )
{
	// get the outer of the object
	UPackage *Package = Object->GetOutermost();

	MarkPackageDirtyForCooker( Package );
}

void UCookOnTheFlyServer::MarkPackageDirtyForCooker( UPackage *Package )
{
	if ( Package->RootPackageHasAnyFlags(PKG_PlayInEditor) )
	{
		return;
	}

	if ( !bIsSavingPackage )
	{
		// could have just cooked a file which we might need to write
		UPackage::WaitForAsyncFileWrites();

		// force that package to be recooked
		const FString Name = Package->GetPathName();

		FName PackageFFileName = GetCachedStandardPackageFileFName(Package);

		if ( PackageFFileName == NAME_None )
		{
			ClearPackageFilenameCacheForPackage( Package );
		}

		// find all packages which depend on this one
		MarkDependentPackagesDirtyForCooker( Package->GetFName() );

#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Modification detected to package %s"), *PackageFFileName.ToString());
#endif

		if ( CurrentCookMode == ECookMode::CookByTheBookFromTheEditor )
		{
			TArray<FName> CookedPlatforms;
			// if we have already cooked this package and we have made changes then recook ;)
			if ( CookedPackages.GetCookedPlatforms(PackageFFileName, CookedPlatforms) )
			{
				if (IsCookByTheBookRunning())
				{
					// if this package was previously cooked and we are doing a cook by the book 
					// we need to recook this package before finishing cook by the book
					CookRequests.EnqueueUnique(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
				}
				else
				{
					CookByTheBookOptions->PreviousCookRequests.Add(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
				}
			}
		}
		CookedPackages.RemoveFile( PackageFFileName );
	}
}


void UCookOnTheFlyServer::MarkDependentPackagesDirtyForCooker( const FName& PackageName)
{
	// mark all packages which are dependent on this package as need recooking
	
}

void UCookOnTheFlyServer::EndNetworkFileServer()
{
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		// shutdown the server
		NetworkFileServer->Shutdown();
		delete NetworkFileServer;
		NetworkFileServer = NULL;
	}
	NetworkFileServers.Empty();
}

void UCookOnTheFlyServer::SetFullGCAssetClasses( const TArray<UClass*>& InFullGCAssetClasses)
{
	FullGCAssetClasses = InFullGCAssetClasses;
}

uint32 UCookOnTheFlyServer::GetPackagesPerGC() const
{
	return PackagesPerGC;
}

uint32 UCookOnTheFlyServer::GetPackagesPerPartialGC() const
{
	return MaxNumPackagesBeforePartialGC;
}


int32 UCookOnTheFlyServer::GetMaxConcurrentShaderJobs() const
{
	return MaxConcurrentShaderJobs;
}

double UCookOnTheFlyServer::GetIdleTimeToGC() const
{
	return IdleTimeToGC;
}

uint64 UCookOnTheFlyServer::GetMaxMemoryAllowance() const
{
	return MaxMemoryAllowance;
}
PRAGMA_DISABLE_OPTIMIZATION;
const TArray<FName>& UCookOnTheFlyServer::GetFullPackageDependencies(const FName& PackageName ) const
{
	TArray<FName>* PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
	if ( !PackageDependencies )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if ( PackageName.ToString().Contains( TEXT("/Game/Sounds/Class/Interior")))
		{
			static int i =0; 
			++i;
		}

		static const FName NAME_CircularReference(TEXT("CircularReference"));
		static int32 UniqueArrayCounter = 0;
		++UniqueArrayCounter;
		FName CircularReferenceArrayName = FName(NAME_CircularReference,UniqueArrayCounter);
		{
			// can't initialize the PackageDependencies array here because we call GetFullPackageDependencies below and that could recurse and resize CachedFullPackageDependencies
			TArray<FName>& TempPackageDependencies = CachedFullPackageDependencies.Add(PackageName); // IMPORTANT READ ABOVE COMMENT
			// initialize temppackagedependencies to a dummy dependency so that we can detect circular references
			TempPackageDependencies.Add(CircularReferenceArrayName);
			// when someone finds the circular reference name they look for this array name in the CachedFullPackageDependencies map
			// and add their own package name to it, so that they can get fixed up 
			CachedFullPackageDependencies.Add(CircularReferenceArrayName);
		}

		TArray<FName> ChildDependencies;
		if ( AssetRegistry.GetDependencies(PackageName, ChildDependencies, EAssetRegistryDependencyType::All) )
		{
			TArray<FName> Dependencies = ChildDependencies;
			Dependencies.AddUnique(PackageName);
			for ( const auto& ChildDependency : ChildDependencies)
			{
				const TArray<FName>& ChildPackageDependencies = GetFullPackageDependencies(ChildDependency);
				for ( const auto& ChildPackageDependency : ChildPackageDependencies )
				{
						if ( ChildPackageDependency == CircularReferenceArrayName )
						{
							continue;
						}
						if ( ChildPackageDependency.GetComparisonIndex() == NAME_CircularReference.GetComparisonIndex() )
						{
							// add our self to the package which we are circular referencing
							TArray<FName>& TempCircularReference = CachedFullPackageDependencies.FindChecked(ChildPackageDependency);
							TempCircularReference.AddUnique(PackageName); // add this package name so that it's dependencies get fixed up when the outer loop returns
						}

					Dependencies.AddUnique(ChildPackageDependency);
				}
			}

			// all these packages referenced us apparently so fix them all up
			const TArray<FName>& PackagesForFixup = CachedFullPackageDependencies.FindChecked(CircularReferenceArrayName);
			for ( const FName FixupPackage : PackagesForFixup )
			{
				TArray<FName> &FixupList = CachedFullPackageDependencies.FindChecked(FixupPackage);
				// check( FixupList.Contains( CircularReferenceArrayName) );
				ensure( FixupList.Remove(CircularReferenceArrayName) == 1 );
				for( const FName AdditionalDependency : Dependencies )
				{
					FixupList.AddUnique(AdditionalDependency);
					if ( AdditionalDependency.GetComparisonIndex() == NAME_CircularReference.GetComparisonIndex() )
					{
						// add our self to the package which we are circular referencing
						TArray<FName>& TempCircularReference = CachedFullPackageDependencies.FindChecked(AdditionalDependency);
						TempCircularReference.AddUnique(FixupPackage); // add this package name so that it's dependencies get fixed up when the outer loop returns
					}
				}
			}
			CachedFullPackageDependencies.Remove(CircularReferenceArrayName);

			PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
			check(PackageDependencies);

			Swap(*PackageDependencies, Dependencies);
		}
		else
		{
			PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
			PackageDependencies->Add(PackageName);
		}
	}
	return *PackageDependencies;
}
PRAGMA_ENABLE_OPTIMIZATION;
void UCookOnTheFlyServer::MarkGCPackagesToKeepForCooker()
{
	// just saved this package will the cooker need this package again this cook?
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Object = *It;
		Object->ClearFlags(RF_KeepForCooker);
	}

	TSet<FName> KeepPackages;
	// first see if the package is in the required to be saved list
	// then see if the package is needed by any of the packages which are required to be saved

	TMap<FName, int32> PackageDependenciesCount;
	for (const auto& QueuedPackage : CookRequests.GetQueue())
	{
		const FName* PackageName = GetCachedPackageFilenameToPackageFName(QueuedPackage);
		if ( !PackageName )
		{
			PackageDependenciesCount.Add(QueuedPackage, 0);
			continue;
		}
		const TArray<FName>& NeededPackages = GetFullPackageDependencies(*PackageName);
		const FName StandardFName = QueuedPackage;
		PackageDependenciesCount.Add(StandardFName, NeededPackages.Num());
		KeepPackages.Append(NeededPackages);
	}

	TSet<FName> LoadedPackages;
	for ( TObjectIterator<UPackage> It; It; ++It )
	{
		UPackage* Package = (UPackage*)(*It);
		if ( KeepPackages.Contains(Package->GetFName()) )
		{
			LoadedPackages.Add( GetCachedStandardPackageFileFName(Package->GetFName()) );
			const auto& ReentryData = GetReentryData(Package);
			Package->SetFlags(RF_KeepForCooker);
			for (auto& Obj : ReentryData.CachedObjectsInOuter)
			{
				Obj->SetFlags(RF_KeepForCooker);
			}
		}
	}

	// sort the cook requests by the packages which are loaded first
	// then sort by the number of dependencies which are referenced by the package
	// we want to process the packages with the highest dependencies so that they can be evicted from memory and are likely to be able to be released on next gc pass
	CookRequests.Sort([=, &PackageDependenciesCount,&LoadedPackages](const FName& A, const FName& B)
		{
			int32 ADependencies = PackageDependenciesCount.FindChecked(A);
			int32 BDependencies = PackageDependenciesCount.FindChecked(B);
			bool ALoaded = LoadedPackages.Contains(A);
			bool BLoaded = LoadedPackages.Contains(B);
			return (ALoaded == BLoaded) ? (ADependencies > BDependencies) : ALoaded > BLoaded;
		}
	);
}

void UCookOnTheFlyServer::BeginDestroy()
{
	EndNetworkFileServer();

	Super::BeginDestroy();

}

void UCookOnTheFlyServer::TickRecompileShaderRequests()
{
	// try to pull off a request
	FRecompileRequest* Request = NULL;

	RecompileRequests.Dequeue(&Request);

	// process it
	if (Request)
	{
		HandleNetworkFileServerRecompileShaders(Request->RecompileData);


		// all done! other thread can unblock now
		Request->bComplete = true;
	}
}


ESavePackageResult UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate)
{
	TArray<FName> TargetPlatformNames; 
	return SaveCookedPackage( Package, SaveFlags, bOutWasUpToDate, TargetPlatformNames );
}

bool UCookOnTheFlyServer::ShouldCook(const FString& InFileName, const FName &InPlatformName)
{
	return true;
}

bool UCookOnTheFlyServer::ShouldConsiderCompressedPackageFileLengthRequirements() const
{
	bool bConsiderCompressedPackageFileLengthRequirements = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bConsiderCompressedPackageFileLengthRequirements"), bConsiderCompressedPackageFileLengthRequirements, GEditorIni);
	return bConsiderCompressedPackageFileLengthRequirements;
}

bool UCookOnTheFlyServer::MakePackageFullyLoaded(UPackage* Package)
{
	if ( Package->IsFullyLoaded() )
		return true;

	bool bPackageFullyLoaded = false;
	GIsCookerLoadingPackage = true;
	Package->FullyLoad();
	//LoadPackage(NULL, *Package->GetName(), LOAD_None);
	GIsCookerLoadingPackage = false;
	if (!Package->IsFullyLoaded())
	{
		LogCookerMessage(FString::Printf(TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"),
			*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set")), EMessageSeverity::Warning);

		UE_LOG(LogCook, Warning, TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"),
			*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set"));
	}
	else
	{
		bPackageFullyLoaded = true;
	}
	// If fully loading has caused a blueprint to be regenerated, make sure we eliminate all meta data outside the package
	UMetaData* MetaData = Package->GetMetaData();
	MetaData->RemoveMetaDataOutsidePackage();

	return bPackageFullyLoaded;
}

ESavePackageResult UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FName> &TargetPlatformNames)
{
	ESavePackageResult Result = ESavePackageResult::Success;
	check( bIsSavingPackage == false );
	bIsSavingPackage = true;
	FString Filename(GetCachedPackageFilename(Package));

	if (IsCookByTheBookMode())
	{
		GIsCookerLoadingPackage = true;

		SCOPE_TIMER(ResolveRedirectors);
		COOK_STAT(FScopedDurationTimer ResolveRedirectorsTimer(DetailedCookStats::TickCookOnTheSideResolveRedirectorsTimeSec));
		FString RelativeFilename = Filename;
		FPaths::MakeStandardFilename(RelativeFilename);
		GRedirectCollector.ResolveStringAssetReference(RelativeFilename);

		GIsCookerLoadingPackage = false;
	}

	if (Filename.Len())
	{
		if (Package->HasAnyPackageFlags(PKG_ReloadingForCooker))
		{
			UE_LOG(LogCook, Warning, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
			UE_LOG(LogCook, Fatal, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
		}

		FString Name = Package->GetPathName();

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		Filename = ConvertToFullSandboxPath(*Filename, true);

		uint32 OriginalPackageFlags = Package->GetPackageFlags();
		UWorld* World = NULL;
		EObjectFlags Flags = RF_NoFlags;


		bool bShouldCompressPackage = IsCookFlagSet(ECookInitializationFlags::Compressed);

		if (CookByTheBookOptions)
		{
			bShouldCompressPackage &= (CookByTheBookOptions->bForceDisableCompressedPackages == false);
			bShouldCompressPackage |= CookByTheBookOptions->bForceEnableCompressedPackages;
		}

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

		static TArray<ITargetPlatform*> ActiveStartupPlatforms = TPM.GetCookingTargetPlatforms();

		TArray<ITargetPlatform*> Platforms;

		if ( TargetPlatformNames.Num() )
		{
			const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();

			for (const FName TargetPlatformFName : TargetPlatformNames)
			{
				const FString TargetPlatformName = TargetPlatformFName.ToString();

				for (ITargetPlatform *TargetPlatform  : TargetPlatforms)
				{
					if ( TargetPlatform->PlatformName() == TargetPlatformName )
					{
						Platforms.Add( TargetPlatform );
					}
				}
			}
		}
		else
		{
			Platforms = ActiveStartupPlatforms;

			for (ITargetPlatform *Platform : Platforms)
			{
				TargetPlatformNames.Add(FName(*Platform->PlatformName()));
			}
		}
		
		for (const auto& TargetPlatform : Platforms)
		{
			bShouldCompressPackage &= TargetPlatform->SupportsFeature(ETargetPlatformFeatures::ShouldUseCompressedCookedPackages);
		}

		if (bShouldCompressPackage)
		{
			Package->SetPackageFlags(PKG_StoreCompressed);
		}

		for (ITargetPlatform* Target : Platforms)
		{
			FString PlatFilename = Filename.Replace(TEXT("[Platform]"), *Target->PlatformName());

			bool bCookPackage = true;

			// don't save Editor resources from the Engine if the target doesn't have editoronly data
			if (IsCookFlagSet(ECookInitializationFlags::SkipEditorContent) &&
				(Name.StartsWith(TEXT("/Engine/Editor")) || Name.StartsWith(TEXT("/Engine/VREditor"))) &&
				!Target->HasEditorOnlyData())
			{
				bCookPackage = false;
			}

			if (bCookPackage == true)
			{
				bool bPackageFullyLoaded = false;
				if (bPackageFullyLoaded == false)
				{
					SCOPE_TIMER(LoadPackage);

					bPackageFullyLoaded = MakePackageFullyLoaded(Package);

					// look for a world object in the package (if there is one, there's a map)
					World = UWorld::FindWorldInPackage(Package);
					Flags = World ? RF_NoFlags : RF_Standalone;
				}

				if (bPackageFullyLoaded)
				{
					UE_LOG(LogCook, Display, TEXT("Cooking %s -> %s"), *Package->GetName(), *PlatFilename);

					bool bSwap = (!Target->IsLittleEndian()) ^ (!PLATFORM_LITTLE_ENDIAN);


					if (!Target->HasEditorOnlyData())
					{
						Package->SetPackageFlags(PKG_FilterEditorOnly);
					}
					else
					{
						Package->ClearPackageFlags(PKG_FilterEditorOnly);
					}

					if (World)
					{
						// Fixup legacy lightmaps before saving
						// This should be done after loading, but FRedirectCollector::ResolveStringAssetReference in Core loads UWorlds with LoadObject so there's no opportunity to handle this fixup on load
						World->PersistentLevel->HandleLegacyMapBuildData();
					}

					// need to subtract 32 because the SavePackage code creates temporary files with longer file names then the one we provide
					// projects may ignore this restriction if desired
					static bool bConsiderCompressedPackageFileLengthRequirements = ShouldConsiderCompressedPackageFileLengthRequirements();
					const int32 CompressedPackageFileLengthRequirement = bConsiderCompressedPackageFileLengthRequirements ? 32 : 0;
					const FString FullFilename = FPaths::ConvertRelativePathToFull(PlatFilename);
					if (FullFilename.Len() >= (PLATFORM_MAX_FILEPATH_LENGTH - CompressedPackageFileLengthRequirement))
					{
						LogCookerMessage(FString::Printf(TEXT("Couldn't save package, filename is too long: %s"), *PlatFilename), EMessageSeverity::Error);
						UE_LOG(LogCook, Error, TEXT("Couldn't save package, filename is too long :%s"), *PlatFilename);
						Result = ESavePackageResult::Error;
					}
					else
					{
						SCOPE_TIMER(GEditorSavePackage);
						GIsCookerLoadingPackage = true;
						Result = GEditor->Save(Package, World, Flags, *PlatFilename, GError, NULL, bSwap, false, SaveFlags, Target, FDateTime::MinValue(), false);
						GIsCookerLoadingPackage = false;
						IBlueprintNativeCodeGenModule::Get().Convert(Package, Result, *(Target->PlatformName()));
						INC_INT_STAT(SavedPackage, 1);
					}

					bOutWasUpToDate = false;
				}
				else
				{
					LogCookerMessage(FString::Printf(TEXT("Unable to cook package for platform because it is unable to be loaded: %s"), *PlatFilename), EMessageSeverity::Error);
					UE_LOG(LogCook, Display, TEXT("Unable to cook package for platform because it is unable to be loaded %s -> %s"), *Package->GetName(), *PlatFilename);
					Result = ESavePackageResult::Error;
				}
			}
			else
			{
#if DEBUG_COOKONTHEFLY
				UE_LOG(LogCook, Display, TEXT("Up to date: %s"), *PlatFilename);
#endif

				bOutWasUpToDate = true;
			}
		}

		Package->SetPackageFlagsTo(OriginalPackageFlags);
	}

	check( bIsSavingPackage == true );
	bIsSavingPackage = false;

	// return success
	return Result;
}


void UCookOnTheFlyServer::Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookFlags, const FString &InOutputDirectoryOverride )
{
	OutputDirectoryOverride = InOutputDirectoryOverride;
	CurrentCookMode = DesiredCookMode;
	CookFlags = InCookFlags;

	FCoreUObjectDelegates::PreGarbageCollect.AddUObject(this, &UCookOnTheFlyServer::PreGarbageCollect);

	if (IsCookByTheBookMode() && !IsCookingInEditor())
	{
		FCoreUObjectDelegates::PackageCreatedForLoad.AddUObject(this, &UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded);
	}

	if (IsCookByTheBookMode())
	{
		FCoreUObjectDelegates::PackageLoadedFromStringAssetReference.AddUObject(this, &UCookOnTheFlyServer::OnStringAssetReferenceLoadedPackage);
	}

	if (IsCookingInEditor())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UCookOnTheFlyServer::OnObjectPropertyChanged);
		FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UCookOnTheFlyServer::OnObjectModified);
		FCoreUObjectDelegates::OnObjectSaved.AddUObject(this, &UCookOnTheFlyServer::OnObjectSaved);

		FCoreDelegates::OnTargetPlatformChangedSupportedFormats.AddUObject(this, &UCookOnTheFlyServer::OnTargetPlatformChangedSupportedFormats);
	}

	FCoreDelegates::OnFConfigCreated.AddUObject(this, &UCookOnTheFlyServer::OnFConfigCreated);
	FCoreDelegates::OnFConfigDeleted.AddUObject(this, &UCookOnTheFlyServer::OnFConfigDeleted);

	bool bUseFullGCAssetClassNames = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bUseFullGCAssetClassNames"), bUseFullGCAssetClassNames, GEditorIni);
	
	MaxPrecacheShaderJobs = FPlatformMisc::NumberOfCores() - 1; // number of cores -1 is a good default allows the editor to still be responsive to other shader requests and allows cooker to take advantage of multiple processors while the editor is running
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxPrecacheShaderJobs"), MaxPrecacheShaderJobs, GEditorIni);

	MaxConcurrentShaderJobs = FPlatformMisc::NumberOfCores() * 4; // number of cores -1 is a good default allows the editor to still be responsive to other shader requests and allows cooker to take advantage of multiple processors while the editor is running
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxConcurrentShaderJobs"), MaxConcurrentShaderJobs, GEditorIni);

	if (bUseFullGCAssetClassNames)
	{
		TArray<FString> FullGCAssetClassNames;
		GConfig->GetArray( TEXT("CookSettings"), TEXT("FullGCAssetClassNames"), FullGCAssetClassNames, GEditorIni );
		for ( const auto& FullGCAssetClassName : FullGCAssetClassNames )
		{
			UClass* FullGCAssetClass = FindObject<UClass>(ANY_PACKAGE, *FullGCAssetClassName, true);
			if( FullGCAssetClass == NULL )
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find full gc asset class name %s may result in bad cook"), *FullGCAssetClassName);
			}
			else
			{
				FullGCAssetClasses.Add( FullGCAssetClass );
			}
		}
		if ( FullGCAssetClasses.Num() == 0 )
		{
			// default to UWorld
			FullGCAssetClasses.Add( UWorld::StaticClass() );
		}
	}

	PackagesPerGC = 500;
	int32 ConfigPackagesPerGC = 0;
	if (GConfig->GetInt( TEXT("CookSettings"), TEXT("PackagesPerGC"), ConfigPackagesPerGC, GEditorIni ))
	{
		// Going unsigned. Make negative values 0
		PackagesPerGC = ConfigPackagesPerGC > 0 ? ConfigPackagesPerGC : 0;
	}

	IdleTimeToGC = 20.0;
	GConfig->GetDouble( TEXT("CookSettings"), TEXT("IdleTimeToGC"), IdleTimeToGC, GEditorIni );

	int32 MaxMemoryAllowanceInMB = 8 * 1024;
	GConfig->GetInt( TEXT("CookSettings"), TEXT("MaxMemoryAllowance"), MaxMemoryAllowanceInMB, GEditorIni );
	MaxMemoryAllowanceInMB = FMath::Max(MaxMemoryAllowanceInMB, 0);
	MaxMemoryAllowance = MaxMemoryAllowanceInMB * 1024LL * 1024LL;
	
	int32 MinMemoryBeforeGCInMB = 0; // 6 * 1024;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinMemoryBeforeGC"), MinMemoryBeforeGCInMB, GEditorIni);
	MinMemoryBeforeGCInMB = FMath::Max(MinMemoryBeforeGCInMB, 0);
	MinMemoryBeforeGC = MinMemoryBeforeGCInMB * 1024LL * 1024LL;
	MinMemoryBeforeGC = FMath::Min(MaxMemoryAllowance, MinMemoryBeforeGC);

	int32 MinFreeMemoryInMB = 0;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinFreeMemory"), MinFreeMemoryInMB, GEditorIni);
	MinFreeMemoryInMB = FMath::Max(MinFreeMemoryInMB, 0);
	MinFreeMemory = MinFreeMemoryInMB * 1024LL * 1024LL;

	// check the amount of OS memory and use that number minus the reserved memory nubmer
	int32 MinReservedMemoryInMB = 0;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinReservedMemory"), MinReservedMemoryInMB, GEditorIni);
	MinReservedMemoryInMB = FMath::Max(MinReservedMemoryInMB, 0);
	int64 MinReservedMemory = MinReservedMemoryInMB * 1024LL * 1024LL;
	if ( MinReservedMemory )
	{
		int64 TotalRam = FPlatformMemory::GetPhysicalGBRam() * 1024LL * 1024LL * 1024LL;
		MaxMemoryAllowance = FMath::Min<int64>( MaxMemoryAllowance, TotalRam - MinReservedMemory );
	}

	MaxNumPackagesBeforePartialGC = 400;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxNumPackagesBeforePartialGC"), MaxNumPackagesBeforePartialGC, GEditorIni);
	
	GConfig->GetArray(TEXT("CookSettings"), TEXT("ConfigSettingBlacklist"), ConfigSettingBlacklist, GEditorIni);

	UE_LOG(LogCook, Display, TEXT("Max memory allowance for cook %dmb min free memory %dmb"), MaxMemoryAllowanceInMB, MinFreeMemoryInMB);

	if (IsCookByTheBookMode())
	{
		CookByTheBookOptions = new FCookByTheBookOptions();
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if ((*It) != GetTransientPackage())
			{
				CookByTheBookOptions->StartupPackages.Add(It->GetFName());
			}
		}
	}
	
	UE_LOG(LogCook, Display, TEXT("Mobile HDR setting %d"), IsMobileHDR());
}

bool UCookOnTheFlyServer::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("package")))
	{
		FString PackageName;
		if (!FParse::Value(Cmd, TEXT("name="), PackageName))
		{
			Ar.Logf(TEXT("Required package name for cook package function. \"cook package name=<name> platform=<platform>\""));
			return true;
		}

		FString PlatformName;
		if (!FParse::Value(Cmd, TEXT("platform="), PlatformName))
		{
			Ar.Logf(TEXT("Required package name for cook package function. \"cook package name=<name> platform=<platform>\""));
			return true;
		}

		if (FPackageName::IsShortPackageName(PackageName))
		{
			FString OutFilename;
			if (FPackageName::SearchForPackageOnDisk(PackageName, NULL, &OutFilename))
			{
				PackageName = OutFilename;
			}
		}

		FName RawPackageName(*PackageName);
		TArray<FName> PackageNames;
		PackageNames.Add(RawPackageName);

		GenerateLongPackageNames(PackageNames);
		

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(PlatformName);
		if (TargetPlatform == nullptr)
		{
			Ar.Logf(TEXT("Target platform %s wasn't found."), *PlatformName);
			return true;
		}

		FCookByTheBookStartupOptions StartupOptions;

		StartupOptions.TargetPlatforms.Add(TargetPlatform);
		for (const auto& StandardPackageName : PackageNames)
		{
			FName PackageFileFName = GetCachedStandardPackageFileFName(StandardPackageName);
			StartupOptions.CookMaps.Add(StandardPackageName.ToString());
		}
		StartupOptions.CookOptions = ECookByTheBookOptions::NoAlwaysCookMaps | ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages | ECookByTheBookOptions::NoInputPackages | ECookByTheBookOptions::NoSlatePackages | ECookByTheBookOptions::DisableUnsolicitedPackages | ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
		
		StartCookByTheBook(StartupOptions);
	}
	else if (FParse::Command(&Cmd, TEXT("clearall")))
	{
		StopAndClearCookedData();
	}

	return false;
}



uint32 UCookOnTheFlyServer::NumConnections() const
{
	int Result= 0;
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ( NetworkFileServer )
		{
			Result += NetworkFileServer->NumConnections();
		}
	}
	return Result;
}

FString UCookOnTheFlyServer::GetOutputDirectoryOverride() const
{
	FString OutputDirectory = OutputDirectoryOverride;
	// Output directory override.	
	if (OutputDirectory.Len() <= 0)
	{
		if ( IsCookingDLC() )
		{
			check( IsCookByTheBookMode() );
			OutputDirectory = FPaths::Combine(*FPaths::GamePluginsDir(), *CookByTheBookOptions->DlcName, TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		else if ( IsCookingInEditor() )
		{
			// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
			OutputDirectory = FPaths::Combine(*FPaths::GameDir(), TEXT("Saved"), TEXT("EditorCooked"), TEXT("[Platform]"));
		}
		else
		{
			// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
			OutputDirectory = FPaths::Combine(*FPaths::GameDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		
		OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	}
	else if (!OutputDirectory.Contains(TEXT("[Platform]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) )
	{
		// Output directory needs to contain [Platform] token to be able to cook for multiple targets.
		OutputDirectory = FPaths::Combine(*OutputDirectory, TEXT("[Platform]"));
	}
	FPaths::NormalizeDirectoryName(OutputDirectory);

	return OutputDirectory;
}

template<class T>
void GetVersionFormatNumbersForIniVersionStrings( TArray<FString>& IniVersionStrings, const FString& FormatName, const TArray<T> &FormatArray )
{
	for ( const auto& Format : FormatArray )
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for ( const auto& SupportedFormat : SupportedFormats )
		{
			int32 VersionNumber = Format->GetVersion(SupportedFormat);
			FString IniVersionString = FString::Printf( TEXT("%s:%s:VersionNumber%d"), *FormatName, *SupportedFormat.ToString(), VersionNumber);
			IniVersionStrings.Emplace( IniVersionString );
		}
	}
}




template<class T>
void GetVersionFormatNumbersForIniVersionStrings(TMap<FString, FString>& IniVersionMap, const FString& FormatName, const TArray<T> &FormatArray)
{
	for (const auto& Format : FormatArray)
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for (const auto& SupportedFormat : SupportedFormats)
		{
			int32 VersionNumber = Format->GetVersion(SupportedFormat);
			FString IniVersionString = FString::Printf(TEXT("%s:%s:VersionNumber"), *FormatName, *SupportedFormat.ToString());
			IniVersionMap.Add(IniVersionString, FString::Printf(TEXT("%d"), VersionNumber));
		}
	}
}


void GetAdditionalCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, TMap<FString, FString>& IniVersionMap )
{

	TArray<FString> VersionedRValues;
	GConfig->GetArray(TEXT("CookSettings"), TEXT("VersionedIntRValues"), VersionedRValues, GEditorIni);

	for (const auto& RValue : VersionedRValues)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(*RValue);
		if (CVar)
		{
			IniVersionMap.Add(*RValue, FString::Printf(TEXT("%d"), CVar->GetValueOnGameThread()));
		}
	}


	const UTextureLODSettings& LodSettings = TargetPlatform->GetTextureLODSettings();

	UEnum* TextureGroupEnum = FindObject<UEnum>(NULL, TEXT("Engine.TextureGroup"));
	UEnum* TextureMipGenSettingsEnum = FindObject<UEnum>(NULL, TEXT("Engine.TextureMipGenSettings"));

	for (int I = 0; I < TextureGroup::TEXTUREGROUP_MAX; ++I)
	{
		const TextureMipGenSettings& MipGenSettings = LodSettings.GetTextureMipGenSettings((TextureGroup)(I));
		FString MipGenVersionString = FString::Printf(TEXT("TextureLODGroupMipGenSettings:%s#%s"), *TextureGroupEnum->GetEnumName(I), *TextureMipGenSettingsEnum->GetEnumName((int32)(MipGenSettings)));
		//IniVersionStrings.Emplace(MoveTemp(MipGenVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMipGenSettings:%s"), *TextureGroupEnum->GetEnumName(I)), *TextureMipGenSettingsEnum->GetEnumName((int32)(MipGenSettings)));

		const int32 MinMipCount = LodSettings.GetMinLODMipCount((TextureGroup)(I));
		// FString MinMipVersionString = FString::Printf(TEXT("TextureLODGroupMinMipCount:%s#%d"), *TextureGroupEnum->GetEnumName(I), MinMipCount);
		//IniVersionStrings.Emplace(MoveTemp(MinMipVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMinMipCount:%s"), *TextureGroupEnum->GetEnumName(I)), FString::Printf(TEXT("%d"),MinMipCount));

		const int32 MaxMipCount = LodSettings.GetMaxLODMipCount((TextureGroup)(I));
		// FString MaxMipVersionString = FString::Printf(TEXT("TextureLODGroupMaxMipCount:%s#%d"), *TextureGroupEnum->GetEnumName(I), MaxMipCount);
		//IniVersionStrings.Emplace(MoveTemp(MaxMipVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMaxMipCount:%s"), *TextureGroupEnum->GetEnumName(I)), FString::Printf(TEXT("%d"), MaxMipCount));
	}

	// save off the ddc version numbers also
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);

	GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("AudioFormat"), TPM->GetAudioFormats());
	GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("TextureFormat"), TPM->GetTextureFormats());
	GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("ShaderFormat"), TPM->GetShaderFormats());


	static const FCustomVersionContainer& CustomVersionContainer = FCustomVersionContainer::GetRegistered();
	for (const auto& CustomVersion : CustomVersionContainer.GetAllVersions())
	{
		FString CustomVersionString = FString::Printf(TEXT("%s:%s"), *CustomVersion.GetFriendlyName().ToString(), *CustomVersion.Key.ToString());
		FString CustomVersionValue = FString::Printf(TEXT("%d"), CustomVersion.Version);
		IniVersionMap.Add(CustomVersionString, CustomVersionValue);
	}

	FString UE4Ver = FString::Printf(TEXT("PackageFileVersions:%d"), GPackageFileUE4Version);
	FString UE4Value = FString::Printf(TEXT("%d"), GPackageFileLicenseeUE4Version);
	IniVersionMap.Add(UE4Ver, UE4Value);

	FString UE4NetVer = TEXT("NetworkCompatibleCL");
	FString UE4NetValue = FString::Printf(TEXT("%u"), FNetworkVersion::GetNetworkCompatibleChangelist());
	IniVersionMap.Add(UE4NetVer, UE4NetValue);

	IniVersionMap.Add(TEXT("MaterialShaderMapDDCVersion"), *GetMaterialShaderMapDDCKey());
	IniVersionMap.Add(TEXT("GlobalDDCVersion"), *GetGlobalShaderMapDDCKey());
}



bool UCookOnTheFlyServer::GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings ) const
{
	IniVersionStrings = AccessedIniStrings;

	// this should be called after the cook is finished
	TArray<FString> IniFiles;
	GConfig->GetConfigFilenames(IniFiles);

	TMap<FString, int32> MultiMapCounter;

	for ( const auto& ConfigFilename : IniFiles )
	{
		if ( ConfigFilename.Contains(TEXT("CookedIniVersion.txt")) )
		{
			continue;
		}

		const FConfigFile *ConfigFile = GConfig->FindConfigFile(ConfigFilename);
		ProcessAccessedIniSettings(ConfigFile, IniVersionStrings);
		
	}

	for (const auto& ConfigFile : OpenConfigFiles)
	{
		ProcessAccessedIniSettings(ConfigFile, IniVersionStrings);
	}


	// remove any which are filtered out
	for ( const auto& Filter : ConfigSettingBlacklist )
	{
		TArray<FString> FilterArray;
		Filter.ParseIntoArray( FilterArray, TEXT(":"));

		FString *ConfigFileName = nullptr;
		FString *SectionName = nullptr;
		FString *ValueName = nullptr;
		switch ( FilterArray.Num() )
		{
		case 3:
			ValueName = &FilterArray[2];
		case 2:
			SectionName = &FilterArray[1];
		case 1:
			ConfigFileName = &FilterArray[0];
			break;
		default:
			continue;
		}

		if ( ConfigFileName )
		{
			for ( auto ConfigFile = IniVersionStrings.CreateIterator(); ConfigFile; ++ConfigFile )
			{
				if ( ConfigFile.Key().ToString().MatchesWildcard(*ConfigFileName) )
				{
					if ( SectionName )
					{
						for ( auto Section = ConfigFile.Value().CreateIterator(); Section; ++Section )
						{
							if ( Section.Key().ToString().MatchesWildcard(*SectionName))
							{
								if (ValueName)
								{
									for ( auto Value = Section.Value().CreateIterator(); Value; ++Value )
									{
										if ( Value.Key().ToString().MatchesWildcard(*ValueName))
										{
											Value.RemoveCurrent();
										}
									}
								}
								else
								{
									Section.RemoveCurrent();
								}
							}
						}
					}
					else
					{
						ConfigFile.RemoveCurrent();
					}
				}
			}
		}
	}
	return true;
}


bool UCookOnTheFlyServer::GetCookedIniVersionStrings(const ITargetPlatform* TargetPlatform, FIniSettingContainer& OutIniSettings, TMap<FString,FString>& OutAdditionalSettings) const
{
	const FString EditorIni = FPaths::GameDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxEditorIni = ConvertToFullSandboxPath(*EditorIni, true);


	const FString PlatformSandboxEditorIni = SandboxEditorIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());

	TArray<FString> SavedIniVersionedParams;

	FConfigFile ConfigFile;
	ConfigFile.Read(*PlatformSandboxEditorIni);

	

	const static FName NAME_UsedSettings(TEXT("UsedSettings")); 
	const FConfigSection* UsedSettings = ConfigFile.Find(NAME_UsedSettings.ToString());
	if (UsedSettings == nullptr)
	{
		return false;
	}


	const static FName NAME_AdditionalSettings(TEXT("AdditionalSettings"));
	const FConfigSection* AdditionalSettings = ConfigFile.Find(NAME_AdditionalSettings.ToString());
	if (AdditionalSettings == nullptr)
	{
		return false;
	}


	for (const auto& UsedSetting : *UsedSettings )
	{
		FName Key = UsedSetting.Key;
		const FConfigValue& UsedValue = UsedSetting.Value;

		TArray<FString> SplitString;
		Key.ToString().ParseIntoArray(SplitString, TEXT(":"));

		if (SplitString.Num() != 4)
		{
			UE_LOG(LogCook, Warning, TEXT("Found unparsable ini setting %s for platform %s, invalidating cook."), *Key.ToString(), *TargetPlatform->PlatformName());
			return false;
		}


		check(SplitString.Num() == 4); // We generate this ini file in SaveCurrentIniSettings
		const FString& Filename = SplitString[0];
		const FString& SectionName = SplitString[1];
		const FString& ValueName = SplitString[2];
		const int32 ValueIndex = FCString::Atoi(*SplitString[3]);

		auto& OutFile = OutIniSettings.FindOrAdd(FName(*Filename));
		auto& OutSection = OutFile.FindOrAdd(FName(*SectionName));
		auto& ValueArray = OutSection.FindOrAdd(FName(*ValueName));
		if ( ValueArray.Num() < (ValueIndex+1) )
		{
			ValueArray.AddZeroed( ValueIndex - ValueArray.Num() +1 );
		}
		ValueArray[ValueIndex] = UsedValue.GetSavedValue();
	}



	for (const auto& AdditionalSetting : *AdditionalSettings)
	{
		const FName& Key = AdditionalSetting.Key;
		const FString& Value = AdditionalSetting.Value.GetSavedValue();
		OutAdditionalSettings.Add(Key.ToString(), Value);
	}

	return true;
}



void UCookOnTheFlyServer::OnFConfigCreated(const FConfigFile* Config)
{
	if (IniSettingRecurse)
		return;

	OpenConfigFiles.Add(Config);
}

void UCookOnTheFlyServer::OnFConfigDeleted(const FConfigFile* Config)
{
	if (IniSettingRecurse)
		return;

	ProcessAccessedIniSettings(Config, AccessedIniStrings);


	OpenConfigFiles.Remove(Config);
}


void UCookOnTheFlyServer::ProcessAccessedIniSettings(const FConfigFile* Config, FIniSettingContainer& OutAccessedIniStrings) const
{	
	if (Config->Name == NAME_None)
	{
		return;
	}
	// try figure out if this config file is for a specific platform 
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetTargetPlatforms();
	FString PlatformName;
	bool bFoundPlatformName = false;
	for (const auto& Platform : Platforms )
	{
		FString CurrentPlatformName = Platform->IniPlatformName();
		for ( const auto& SourceIni : Config->SourceIniHierarchy )
		{
			if ( SourceIni.Value.Filename.Contains(CurrentPlatformName) )
			{
				PlatformName = CurrentPlatformName;
				bFoundPlatformName = true;
				break;
			}
		}
		if ( bFoundPlatformName )
		{
			break;
		}
	}

	


	FString ConfigName = bFoundPlatformName ? FString::Printf(TEXT("%s.%s"),*PlatformName, *Config->Name.ToString()) : Config->Name.ToString();
	const FName& ConfigFName = FName(*ConfigName);
	
	for ( auto& ConfigSection : *Config )
	{
		TSet<FName> ProcessedValues; 
		const FName SectionName = FName(*ConfigSection.Key);

		for ( auto& ConfigValue : ConfigSection.Value )
		{
			const FName& ValueName = ConfigValue.Key;
			if ( ProcessedValues.Contains(ValueName) )
				continue;

			ProcessedValues.Add( ValueName );
			TArray<FConfigValue> ValueArray;
			ConfigSection.Value.MultiFind( ValueName, ValueArray, true );

			bool bHasBeenAccessed = false;
			for (const auto& ValueArrayEntry : ValueArray)
			{
				if (ValueArrayEntry.HasBeenRead())
				{
					bHasBeenAccessed = true;
					break;
				}
			}

			if ( bHasBeenAccessed )
			{
				auto& AccessedConfig = OutAccessedIniStrings.FindOrAdd(ConfigFName);
				auto& AccessedSection = AccessedConfig.FindOrAdd(SectionName);
				auto& AccessedKey = AccessedSection.FindOrAdd(ValueName);
				AccessedKey.Empty();
				for ( const auto& ValueArrayEntry : ValueArray )
				{
					AccessedKey.Add(ValueArrayEntry.GetSavedValue());
				}
			}
			
		}
	}
}




bool UCookOnTheFlyServer::IniSettingsOutOfDate(const ITargetPlatform* TargetPlatform) const
{
	FScopeAssign<bool> A = FScopeAssign<bool>(IniSettingRecurse, true);

	FIniSettingContainer OldIniSettings;
	TMap<FString, FString> OldAdditionalSettings;
	if ( GetCookedIniVersionStrings(TargetPlatform, OldIniSettings, OldAdditionalSettings) == false)
	{
		UE_LOG(LogCook, Display, TEXT("Unable to read previous cook inisettings for platform %s invalidating cook"), *TargetPlatform->PlatformName());
		return true;
	}

	// compare against current settings
	TMap<FString, FString> CurrentAdditionalSettings;
	GetAdditionalCurrentIniVersionStrings(TargetPlatform, CurrentAdditionalSettings);

	for ( const auto& OldIniSetting : OldAdditionalSettings)
	{
		const FString* CurrentValue = CurrentAdditionalSettings.Find(OldIniSetting.Key);
		if ( !CurrentValue )
		{
			UE_LOG(LogCook, Display, TEXT("Missing Additional Setting from previous cook %s"), *OldIniSetting.Key);
			return true;
		}

		if ( *CurrentValue != OldIniSetting.Value )
		{
			UE_LOG(LogCook, Display, TEXT("Additional Setting from previous cook %s doesn't match %s %s"), *OldIniSetting.Key, **CurrentValue, *OldIniSetting.Value );
			return true;
		}
	}

	for ( const auto& OldIniFile : OldIniSettings )
	{
		const FName& ConfigNameKey = OldIniFile.Key;

		TArray<FString> ConfigNameArray;
		ConfigNameKey.ToString().ParseIntoArray(ConfigNameArray, TEXT("."));
		FString Filename;
		FString PlatformName;
		bool bFoundPlatformName = false;
		if ( ConfigNameArray.Num() <= 1 )
		{
			Filename = ConfigNameKey.ToString();
		}
		else if ( ConfigNameArray.Num() == 2 )
		{
			PlatformName = ConfigNameArray[0];
			Filename = ConfigNameArray[1];
			bFoundPlatformName = true;
		}
		else
		{
			UE_LOG( LogCook, Warning, TEXT("Found invalid file name in old ini settings file Filename %s settings file %s"), *ConfigNameKey.ToString(), *TargetPlatform->PlatformName() );
			return true;
		}
		
		const FConfigFile* ConfigFile = nullptr;
		FConfigFile Temp;
		if ( bFoundPlatformName)
		{
			GConfig->LoadLocalIniFile(Temp, *Filename, true, *PlatformName );
			ConfigFile = &Temp;
		}
		else
		{
			ConfigFile = GConfig->Find(Filename, false);
		}
		FName FileFName = FName(*Filename);
		if ( !ConfigFile )
		{
			for( const auto& File : *GConfig )
			{
				if (File.Value.Name == FileFName)
				{
					ConfigFile = &File.Value;
					break;
				}
			}
			if ( !ConfigFile )
			{
				UE_LOG(LogCook, Display, TEXT("Unable to find config file %s invalidating inisettings"), *FString::Printf(TEXT("%s %s"), *PlatformName, *Filename));
				return true;
			}
		}
		for ( const auto& OldIniSection : OldIniFile.Value )
		{
			
			const FName& SectionName = OldIniSection.Key;
			const FConfigSection* IniSection = ConfigFile->Find( SectionName.ToString() );

			if ( IniSection == nullptr )
			{
				UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, Current section doesn't exist"), *FString::Printf(TEXT("%s %s %s"), *PlatformName, *Filename, *SectionName.ToString()));
				return true;
			}

			for ( const auto& OldIniValue : OldIniSection.Value )
			{
				const FName& ValueName = OldIniValue.Key;

				TArray<FConfigValue> CurrentValues;
				IniSection->MultiFind( ValueName, CurrentValues, true );

				if ( CurrentValues.Num() != OldIniValue.Value.Num() )
				{
					UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, missmatched num array elements %d != %d "), *FString::Printf(TEXT("%s %s %s %s"), *PlatformName, *Filename, *SectionName.ToString(), *ValueName.ToString()), CurrentValues.Num(), OldIniValue.Value.Num());
					return true;
				}
				for ( int Index = 0; Index < CurrentValues.Num(); ++Index )
				{
					if ( CurrentValues[Index].GetSavedValue() != OldIniValue.Value[Index] )
					{
						UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, value %s != %s invalidating cook"),  *FString::Printf(TEXT("%s %s %s %s %d"),*PlatformName, *Filename, *SectionName.ToString(), *ValueName.ToString(), Index), *CurrentValues[Index].GetSavedValue(), *OldIniValue.Value[Index] );
						return true;
					}
				}
				
			}
		}
	}

	return false;
}

bool UCookOnTheFlyServer::SaveCurrentIniSettings(const ITargetPlatform* TargetPlatform) const
{
	auto S = FScopeAssign<bool>(IniSettingRecurse, true);
	// IniSettingRecurse = true;

	check(IsChildCooker() == false);

	TMap<FString, FString> AdditionalIniSettings;
	GetAdditionalCurrentIniVersionStrings(TargetPlatform, AdditionalIniSettings);

	FIniSettingContainer CurrentIniSettings;
	GetCurrentIniVersionStrings(TargetPlatform, CurrentIniSettings);

	const FString EditorIni = FPaths::GameDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxEditorIni = ConvertToFullSandboxPath(*EditorIni, true);


	const FString PlatformSandboxEditorIni = SandboxEditorIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());


	FConfigFile ConfigFile;
	// ConfigFile.Read(*PlatformSandboxEditorIni);

	ConfigFile.Dirty = true;
	const static FName NAME_UsedSettings(TEXT("UsedSettings"));
	ConfigFile.Remove(NAME_UsedSettings.ToString());
	FConfigSection& UsedSettings = ConfigFile.FindOrAdd(NAME_UsedSettings.ToString());

	for (const auto& CurrentIniFilename : CurrentIniSettings)
	{
		const FName& Filename = CurrentIniFilename.Key;
		for ( const auto& CurrentSection : CurrentIniFilename.Value )
		{
			const FName& Section = CurrentSection.Key;
			for ( const auto& CurrentValue : CurrentSection.Value )
			{
				const FName& ValueName = CurrentValue.Key;
				const TArray<FString>& Values = CurrentValue.Value;

				for ( int Index = 0; Index < Values.Num(); ++Index )
				{
					FString NewKey = FString::Printf(TEXT("%s:%s:%s:%d"), *Filename.ToString(), *Section.ToString(), *ValueName.ToString(), Index);
					UsedSettings.Add(FName(*NewKey), Values[Index]);
				}
			}
		}
	}


	const static FName NAME_AdditionalSettings(TEXT("AdditionalSettings"));
	ConfigFile.Remove(NAME_AdditionalSettings.ToString());
	FConfigSection& AdditionalSettings = ConfigFile.FindOrAdd(NAME_AdditionalSettings.ToString());

	for (const auto& AdditionalIniSetting : AdditionalIniSettings)
	{
		AdditionalSettings.Add( FName(*AdditionalIniSetting.Key), AdditionalIniSetting.Value );
	}

	ConfigFile.Write(PlatformSandboxEditorIni);


	return true;

}

FString UCookOnTheFlyServer::ConvertCookedPathToUncookedPath(const FString& CookedRelativeFilename) const 
{
	const FString CookedFilename = FPaths::ConvertRelativePathToFull(CookedRelativeFilename);


	FString SandboxDirectory = SandboxFile->GetSandboxDirectory();

	SandboxDirectory.ReplaceInline(TEXT("[PLATFORM]"), TEXT(""));
	SandboxDirectory.ReplaceInline(TEXT("//"), TEXT("/"));

	FString CookedFilenameNoSandbox = CookedFilename;
	CookedFilenameNoSandbox.RemoveFromStart(SandboxDirectory);
	int32 EndOfPlatformIndex = 0;
	
	FString CookedFilenameNoPlatform = CookedFilename;

	if (CookedFilenameNoSandbox.FindChar(TEXT('/'), EndOfPlatformIndex))
	{
		CookedFilenameNoPlatform = SandboxFile->GetSandboxDirectory() / CookedFilenameNoSandbox.Mid(EndOfPlatformIndex);
		CookedFilenameNoPlatform.ReplaceInline(TEXT("//"), TEXT("/"));
	}
	

	FString FullUncookedPath = SandboxFile->ConvertFromSandboxPath(*CookedFilenameNoPlatform);

	FPaths::MakeStandardFilename(FullUncookedPath);
	return FullUncookedPath;


}

struct FCookedAssetPackageInfo
{
	FCookedAssetPackageInfo() : IsEditorPackage(false), SucceededSave(true) { }
	FString RelativeFilename;
	FString SourcePackageName;
	FMD5Hash DependentPackageHash;
	FMD5Hash FullPackageHash;
	bool IsEditorPackage;
	bool SucceededSave;
	TArray<FString> DependentPackages;
};

bool ReadCookedAssetRegistry(const FString& Filename, TArray<FCookedAssetPackageInfo>& CookedPackageInfo )
{
	FString JsonInString;
	bool Succeeded = FFileHelper::LoadFileToString(JsonInString, *Filename);
	if (Succeeded)
	{
		// load up previous package asset registry and fill in any packages which weren't recooked on this run
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonInString);
		TSharedPtr<FJsonObject> JsonObject;
		Succeeded = FJsonSerializer::Deserialize<TCHAR>(Reader, JsonObject); 
		// JsonObject.IsValid() && JsonObject->HasTypedField<EJson::Array>(TEXT("Packages"));
		if (Succeeded)
		{
			TArray<TSharedPtr<FJsonValue>> PackageList = JsonObject->GetArrayField(TEXT("Packages"));
			for (auto PackageListIt = PackageList.CreateConstIterator(); PackageListIt && Succeeded; ++PackageListIt)
			{
				const TSharedPtr<FJsonValue>& JsonValue = *PackageListIt;
				Succeeded = JsonValue->Type == EJson::Object;
				if (Succeeded)
				{
					const TSharedPtr<FJsonObject>& JsonPackage = JsonValue->AsObject();

					// get the package name and see if we have already written it out this run
					int32 Index = CookedPackageInfo.AddDefaulted(1);
					FCookedAssetPackageInfo& PackageInfo = CookedPackageInfo[Index];

					check( PackageInfo.IsEditorPackage == false );
					FString SourcePackageName;
					verify(JsonPackage->TryGetStringField(TEXT("SourcePackageName"), SourcePackageName));
					PackageInfo.SourcePackageName = SourcePackageName;


					FString CookedPackageName;
					verify(JsonPackage->TryGetStringField(TEXT("CookedPackageName"), CookedPackageName));

					JsonPackage->TryGetStringField(TEXT("ResolvedRelativePath"), PackageInfo.RelativeFilename);

					/*FPackageName::TryConvertLongPackageNameToFilename(SourcePackageName, PackageInfo.RelativeFilename);
					PackageInfo.RelativeFilename += FPaths::GetExtension(CookedPackageName, true);*/

					FString DependentPackageHash;
					if (JsonPackage->TryGetStringField(TEXT("DependentPackageHash"), DependentPackageHash))
					{
						LexicalConversion::FromString(PackageInfo.DependentPackageHash, *DependentPackageHash);
					}

					FString FullPackageHash;
					if (JsonPackage->TryGetStringField(TEXT("FullPackageHash"), FullPackageHash))
					{
						LexicalConversion::FromString(PackageInfo.FullPackageHash, *FullPackageHash);
					}

					FString IsEditorPackage;
					if (JsonPackage->TryGetStringField(TEXT("IsEditorOnly"), IsEditorPackage))
					{
						LexicalConversion::FromString(PackageInfo.IsEditorPackage, *IsEditorPackage);
					}

					FString SucceededSave;
					if (JsonPackage->TryGetStringField(TEXT("SucceededSave"), SucceededSave))
					{
						LexicalConversion::FromString(PackageInfo.SucceededSave, *SucceededSave);
					}


					const TArray<TSharedPtr<FJsonValue>>* DependencyArray = nullptr;
					if (JsonPackage->TryGetArrayField(TEXT("DependencyData"), DependencyArray) )
					{
						check( DependencyArray != nullptr );
						for ( const TSharedPtr<FJsonValue>& Dependency : *DependencyArray )
						{
							// if ( Dependency != nullptr )
							{
								PackageInfo.DependentPackages.Add( Dependency->AsString() );
							}
						}
					}


				}
			}
		}
	}
	return Succeeded;
}



void InternalCopyJsonValueToWriter(TSharedRef<TJsonWriter<TCHAR>> &Json, const FString& ValueName, const TSharedPtr<FJsonValue>& JsonValue)
{
	if (JsonValue->Type == EJson::String)
	{
		Json->WriteValue(ValueName, JsonValue->AsString());
	}
	else if (JsonValue->Type == EJson::Array)
	{
		if (ValueName.IsEmpty())
		{
			Json->WriteArrayStart();
		}
		else
		{
			Json->WriteArrayStart(ValueName);
		}

		const TArray<TSharedPtr<FJsonValue>>& Array = JsonValue->AsArray();
		for (const auto& ArrayValue : Array)
		{
			InternalCopyJsonValueToWriter(Json, FString(), ArrayValue);
		}

		Json->WriteArrayEnd();
	}
	else if (JsonValue->Type == EJson::Object)
	{
		if (ValueName.IsEmpty())
		{
			Json->WriteObjectStart();
		}
		else
		{
			Json->WriteObjectStart(ValueName);
		}

		const TSharedPtr<FJsonObject>& Object = JsonValue->AsObject();
		for (const auto& ObjectProperty : Object->Values)
		{
			InternalCopyJsonValueToWriter(Json, ObjectProperty.Key, ObjectProperty.Value);
		}

		Json->WriteObjectEnd();
	}
	else
	{

		UE_LOG(LogCook, Warning, TEXT("Unrecognized json value type %d in object %s"), *UEnum::GetValueAsString(TEXT("Json.EJson"), JsonValue->Type), *ValueName)
	}
}


bool UCookOnTheFlyServer::SaveCookedAssetRegistry(const FString& InCookedAssetRegistryFilename, const TArray<FName>& InAllCookedPackages, const TArray<FName>& UncookedEditorPackages, const TArray<FName>& FailedToSavePackages, const FString& PlatformName, const bool Append) const
{
	bool bSuccess = true;

	const FString CookedAssetRegistryFilename = InCookedAssetRegistryFilename.Replace(TEXT("[Platform]"), *PlatformName);


	FString JsonOutString;
	TSharedRef<TJsonWriter<TCHAR>> Json = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonOutString);

	Json->WriteObjectStart();
	Json->WriteArrayStart(TEXT("Packages"));

	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");



	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	TArray<FName> AllCookedPackages = InAllCookedPackages;
	TSet<FName> FailedToSavePackagesSet;
	for ( const FName& FailedToSavePackage : FailedToSavePackages)
	{
		const FName& FailedStandardName = GetCachedStandardPackageFileFName(FailedToSavePackage);
		AllCookedPackages.Add(FailedStandardName );
		FailedToSavePackagesSet.Add(FailedStandardName);
	}

	TSet<FName> ProcessedCookedPackages; // keep track of which packages are processed
	for (const auto& Package : AllCookedPackages)
	{
		const FString& RelativePath = Package.ToString();

		FName PackageName;
		FString PlatformSandboxPath;
		//FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		if (FPackageName::IsScriptPackage(RelativePath))
		{
			PackageName = Package;
		}
		else
		{
			FString StringPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(RelativePath, StringPackageName) == false)
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to determine extension for file %s skipping asset registry entry"), *PackageName.ToString());
				continue;
			}
			PackageName = FName(*StringPackageName);

			PlatformSandboxPath = SandboxFile->ConvertToSandboxPath(*StringPackageName);
			PlatformSandboxPath.ReplaceInline(TEXT("[Platform]"), *PlatformName);
		}

		if ( UncookedEditorPackages.Contains( PackageName ) )
		{
			continue;
		}

		Json->WriteObjectStart(); // unnamed package start

		ProcessedCookedPackages.Add(PackageName);



		FPackageName::FindPackageFileWithoutExtension(PlatformSandboxPath, PlatformSandboxPath);

		FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*PlatformSandboxPath);
		int64 FileSize = IFileManager::Get().FileSize(*PlatformSandboxPath);

		Json->WriteValue(TEXT("SourcePackageName"), PackageName.ToString());
		Json->WriteValue(TEXT("CookedPackageName"), PlatformSandboxPath);
		Json->WriteValue(TEXT("CookedPackageTimeStamp"), TimeStamp.ToString());
		Json->WriteValue(TEXT("FileSize"), FString::Printf(TEXT("%lld"), FileSize));
		Json->WriteValue(TEXT("ResolvedRelativePath"), *RelativePath);

		Json->WriteValue(TEXT("IsEditorOnly"), TEXT("false"));
		if ( FailedToSavePackagesSet.Contains(Package) )
		{
			Json->WriteValue(TEXT("SucceededSave"), TEXT("false"));
		}
		else
		{
			Json->WriteValue(TEXT("SucceededSave"), TEXT("true"));
		}
		

		{
			FMD5Hash Hash;
			PDInfoModule.DeterminePackageDependentHash(*RelativePath, Hash);
			FString DependentPackageHashString = LexicalConversion::ToString(Hash);
			Json->WriteValue(TEXT("DependentPackageHash"), *DependentPackageHashString);
		}
		{
			FMD5Hash Hash;
			PDInfoModule.DetermineFullPackageHash(*RelativePath, Hash);
			FString FullPackageHashString = LexicalConversion::ToString(Hash);
			Json->WriteValue(TEXT("FullPackageHash"), *FullPackageHashString);
		}

		{
			TArray<FString> DependencyNames;
			if (PDInfoModule.GetPackageDependencies(*RelativePath, DependencyNames))
			{
				Json->WriteArrayStart(TEXT("DependencyData"));

				for (const FString& DependencyName : DependencyNames)
				{
					Json->WriteValue(DependencyName);
				}
				Json->WriteArrayEnd();
			}
		}

		Json->WriteValue(TEXT("FileSize"), FString::Printf(TEXT("%lld"), FileSize));
		Json->WriteArrayStart("AssetData");


		TArray<FAssetData> AssetRegistryData;
		AssetRegistry.GetAssetsByPackageName(PackageName, AssetRegistryData, true);

		for (const auto& AssetData : AssetRegistryData)
		{	// Add only assets that have actually been cooked and belong to any chunk
			if (AssetData.ChunkIDs.Num() > 0 && (AssetData.PackageName == PackageName))
			{
				Json->WriteObjectStart();
				// save all their infos 
				Json->WriteValue(TEXT("ObjectPath"), AssetData.ObjectPath.ToString());
				Json->WriteValue(TEXT("PackageName"), AssetData.PackageName.ToString());
				Json->WriteValue(TEXT("PackagePath"), AssetData.PackagePath.ToString());
				Json->WriteValue(TEXT("GroupNames"), AssetData.GroupNames.ToString());
				Json->WriteValue(TEXT("AssetName"), AssetData.AssetName.ToString());
				Json->WriteValue(TEXT("AssetClass"), AssetData.AssetClass.ToString());
				Json->WriteObjectStart("TagsAndValues");
				for (const auto& Tag : AssetData.TagsAndValues)
				{
					Json->WriteValue(Tag.Key.ToString(), Tag.Value);
				}
				Json->WriteObjectEnd(); // end tags and values object
				Json->WriteObjectEnd(); // end unnamed array object
			}
		}
		Json->WriteArrayEnd();
		Json->WriteObjectEnd(); // unnamed package
	}
	/*
	for (const auto& FailedPackage : FailedToSavePackages)
	{
		const FString& RelativePath = FailedPackage.ToString();

		FName PackageName;
		FString PlatformSandboxPath;
		//FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		if (FPackageName::IsScriptPackage(RelativePath))
		{
			PackageName = FailedPackage;
		}
		else
		{
			FString StringPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(RelativePath, StringPackageName) == false)
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to determine extension for file %s skipping asset registry entry"), *PackageName.ToString());
				continue;
			}
			PackageName = FName(*StringPackageName);

			PlatformSandboxPath = SandboxFile->ConvertToSandboxPath(*StringPackageName);
			PlatformSandboxPath.ReplaceInline(TEXT("[Platform]"), *PlatformName);
		}

		Json->WriteObjectStart(); // unnamed package start

		ProcessedCookedPackages.Add(PackageName);



		FPackageName::FindPackageFileWithoutExtension(PlatformSandboxPath, PlatformSandboxPath);

		FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*PlatformSandboxPath);
		int64 FileSize = IFileManager::Get().FileSize(*PlatformSandboxPath);

		Json->WriteValue(TEXT("SourcePackageName"), PackageName.ToString());
		Json->WriteValue(TEXT("CookedPackageName"), PlatformSandboxPath);
		Json->WriteValue(TEXT("ResolvedRelativePath"), *RelativePath);


		Json->WriteValue(TEXT("IsEditorOnly"), TEXT("false"));
		Json->WriteValue(TEXT("SucceededSave"), TEXT("false"));


		Json->WriteObjectEnd();
	}
	*/

	for (const auto& EditorOnlyPackage : UncookedEditorPackages)
	{
		const FString& RelativePath = EditorOnlyPackage.ToString();

		FName PackageName;
		FString PlatformSandboxPath;
		//FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		if (FPackageName::IsScriptPackage(RelativePath))
		{
			PackageName = EditorOnlyPackage;
		}
		else
		{
			FString StringPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(RelativePath, StringPackageName) == false)
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to determine extension for file %s skipping asset registry entry"), *PackageName.ToString());
				continue;
			}
			PackageName = FName(*StringPackageName);

			PlatformSandboxPath = SandboxFile->ConvertToSandboxPath(*StringPackageName);
			PlatformSandboxPath.ReplaceInline(TEXT("[Platform]"), *PlatformName);
		}

		Json->WriteObjectStart(); // unnamed package start

		ProcessedCookedPackages.Add(PackageName);



		FPackageName::FindPackageFileWithoutExtension(PlatformSandboxPath, PlatformSandboxPath);

		FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*PlatformSandboxPath);
		int64 FileSize = IFileManager::Get().FileSize(*PlatformSandboxPath);

		Json->WriteValue(TEXT("SourcePackageName"), PackageName.ToString());
		Json->WriteValue(TEXT("CookedPackageName"), PlatformSandboxPath);
		Json->WriteValue(TEXT("ResolvedRelativePath"), *RelativePath);

		Json->WriteValue(TEXT("IsEditorOnly"), TEXT("true"));
		Json->WriteValue(TEXT("SucceededSave"), TEXT("false"));

		Json->WriteObjectEnd();
	}


	if (Append)
	{
		FString JsonInString;
		if (FFileHelper::LoadFileToString(JsonInString, *CookedAssetRegistryFilename))
		{
			// load up previous package asset registry and fill in any packages which weren't recooked on this run
			TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonInString);
			TSharedPtr<FJsonObject> JsonObject;
			bool shouldRead = FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid() && JsonObject->HasTypedField<EJson::Array>(TEXT("Packages"));
			if (shouldRead)
			{
				TArray<TSharedPtr<FJsonValue>> PackageList = JsonObject->GetArrayField(TEXT("Packages"));
				for (auto PackageListIt = PackageList.CreateConstIterator(); PackageListIt && shouldRead; ++PackageListIt)
				{
					const TSharedPtr<FJsonValue>& JsonValue = *PackageListIt;
					shouldRead = JsonValue->Type == EJson::Object;
					if (shouldRead)
					{
						const TSharedPtr<FJsonObject>& JsonPackage = JsonValue->AsObject();

						// get the package name and see if we have already written it out this run

						FString CookedPackageName;
						verify(JsonPackage->TryGetStringField(TEXT("SourcePackageName"), CookedPackageName));

						const FName CookedPackageFName(*CookedPackageName);
						if (ProcessedCookedPackages.Contains(CookedPackageFName))
						{
							// don't need to process this package
							continue;
						}


						// check that the on disk version is still valid
						FString SourcePackageName;
						check(JsonPackage->TryGetStringField(TEXT("SourcePackageName"), SourcePackageName));

						// if our timestamp is different then don't copy the information over
						FDateTime CurrentTimeStamp = IFileManager::Get().GetTimeStamp(*CookedPackageName);

						FString SavedTimeString;
						check(JsonPackage->TryGetStringField(TEXT("CookedPackageTimeStamp"), SavedTimeString));
						FDateTime SavedTimeStamp;
						FDateTime::Parse(SavedTimeString, SavedTimeStamp);

						if (SavedTimeStamp != CurrentTimeStamp)
						{
							continue;
						}

						InternalCopyJsonValueToWriter(Json, FString(), JsonValue);
					}

				}
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to read or json is invalid format %s"), *CookedAssetRegistryFilename);
			}
		}
	}


	Json->WriteArrayEnd();
	Json->WriteObjectEnd();

	if (Json->Close())
	{
		FArchive* ItemTemplatesFile = IFileManager::Get().CreateFileWriter(*CookedAssetRegistryFilename);
		if (ItemTemplatesFile)
		{
			// serialize the file contents
			TStringConversion<FTCHARToUTF8_Convert> Convert(*JsonOutString);
			ItemTemplatesFile->Serialize(const_cast<ANSICHAR*>(Convert.Get()), Convert.Length());
			ItemTemplatesFile->Close();
			if (!ItemTemplatesFile->IsError())
			{
				bSuccess = true;
			}
			else
			{
				UE_LOG(LogCook, Error, TEXT("Unable to write to %s"), *CookedAssetRegistryFilename);
			}
			delete ItemTemplatesFile;
		}
		else
		{
			UE_LOG(LogCook, Error, TEXT("Unable to open %s for writing."), *CookedAssetRegistryFilename);
		}
	}
	else
	{
		UE_LOG(LogCook, Error, TEXT("Error closing Json Writer"));
	}
	return bSuccess;
}



void UCookOnTheFlyServer::PopulateCookedPackagesFromDisk(const TArray<ITargetPlatform*>& Platforms)
{
	check(IsChildCooker() == false);

	// check each package to see if it's out of date
	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");


	// See what files are out of date in the sandbox folder
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		TArray<FString> CookedPackagesToDelete;

		ITargetPlatform* Target = Platforms[Index];
		FString SandboxPath = GetSandboxDirectory(Target->PlatformName());
		FName PlatformFName(*Target->PlatformName());

		FString EngineSandboxPath = SandboxFile->ConvertToSandboxPath(*FPaths::EngineDir()) + TEXT("/");
		EngineSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString GameSandboxPath = SandboxFile->ConvertToSandboxPath(*(FPaths::GameDir() + TEXT("a.txt")));
		GameSandboxPath.ReplaceInline(TEXT("a.txt"), TEXT(""));
		GameSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString LocalGamePath = FPaths::GameDir();
		if (FPaths::IsProjectFilePathSet())
		{
			LocalGamePath = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/");
		}

		FString LocalEnginePath = FPaths::EngineDir();

		static bool bFindCulprit = false;

		if (IsCookFlagSet(ECookInitializationFlags::IterateOnHash))
		{
			// load the platform cooked asset registry
			const FString CookedAssetRegistry = FPaths::GameDir() / TEXT("CookedAssetRegistry.json");
			const FString SandboxCookedAssetRegistryFilename = ConvertToFullSandboxPath(*CookedAssetRegistry, true, Target->PlatformName());

			TArray<FCookedAssetPackageInfo> CookedPackagesInfo;
			ReadCookedAssetRegistry(SandboxCookedAssetRegistryFilename, CookedPackagesInfo);
			TMap<FName, FCookedAssetPackageInfo*> CookedPackageInfoLookup;
			for (auto& PackageInfo : CookedPackagesInfo)
			{
				CookedPackageInfoLookup.Add(FName(*PackageInfo.RelativeFilename), &PackageInfo);
			}

			// check the files on disk 



			TMap<FName, FName> CookedPathToUncookedPath;
			TArray<FName> AllUncookedFNames;
			{
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				TArray<FString> CookedFiles;
				FPackageSearchVisitor PackageSearch(CookedFiles);
				PlatformFile.IterateDirectoryRecursively(*SandboxPath, PackageSearch);
				TArray<FString> AllUncookedFilenames;
				for (const FString& CookedFile : CookedFiles)
				{
					const FName CookedFName(*CookedFile);

					const FString CookedFullPath = FPaths::ConvertRelativePathToFull(CookedFile);
					const FString UncookedFilename = ConvertCookedPathToUncookedPath(CookedFullPath);

					const FName UncookedFName(*UncookedFilename);
					CookedPathToUncookedPath.Add(CookedFName, UncookedFName);
					if (CookedPackageInfoLookup.Contains(UncookedFName))
					{
						AllUncookedFilenames.Add(UncookedFilename);
						AllUncookedFNames.Add(UncookedFName);
					}
				}
				{
#if OUTPUT_TIMING
					SCOPE_TIMER(DetermineAllDependentPackageInfoTime);
#endif
					//AllUncookedFilenames.Empty();

					int32 NumThreadsToDeterminePackageInfo = GLargeThreadPool->GetNumThreads() - 2;
					PDInfoModule.AsyncDetermineAllDependentPackageInfo(AllUncookedFilenames, NumThreadsToDeterminePackageInfo);
				}
			}

			uint32 NumPackagesConsidered = CookedPathToUncookedPath.Num();
			uint32 NumPackagesUnableToDetermineCurrentDependencyInfo = 0;
			uint32 NumPackagesUnableToFindCookedPackageInfo = 0;
			uint32 NumPackagesFileHashMissmatch = 0;
			uint32 NumPackagesDependentHashMissmatch = 0;
			uint32 NumPackagesKept = 0;

			for (const auto& CookedPaths : CookedPathToUncookedPath)
			{
				const FName CookedFile = CookedPaths.Key;
				const FName UncookedFilename = CookedPaths.Value;
				const FString UncookedFilenameString = UncookedFilename.ToString();
				bool bShouldKeep = true;


				FCookedAssetPackageInfo* CookedPackageInfo = CookedPackageInfoLookup.FindRef(UncookedFilename);

				if (bShouldKeep && CookedPackageInfo == nullptr)
				{
					++NumPackagesUnableToFindCookedPackageInfo;
					bShouldKeep = false;
				}

				if ( bShouldKeep && CookedPackageInfo->SucceededSave == false )
				{
					// get rid of this package
					bShouldKeep = false;
				}
				
				FMD5Hash FullPackageHash;
				if (bShouldKeep && PDInfoModule.DetermineFullPackageHash(*UncookedFilenameString, FullPackageHash) == false)
				{
					++NumPackagesUnableToDetermineCurrentDependencyInfo;
					bShouldKeep = false;
				}

				FMD5Hash PackageDependentHash;
				if (bShouldKeep && PDInfoModule.DeterminePackageDependentHash(*UncookedFilenameString, PackageDependentHash) == false)
				{
					UE_LOG(LogCook, Warning, TEXT("Should never happen, above function should have failed and not gone in here, Package %s"), *UncookedFilenameString);
					bShouldKeep = false;
				}

				// find the package dependent hash in the previously cooked asset registry
				if (bShouldKeep && CookedPackageInfo->FullPackageHash != FullPackageHash)
				{
					++NumPackagesFileHashMissmatch;
					bShouldKeep = false;
				}

				if (bShouldKeep && CookedPackageInfo->DependentPackageHash != PackageDependentHash)
				{
					++NumPackagesDependentHashMissmatch;
					if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
					{
						UE_LOG(LogCook, Display, TEXT("Tracing package dependency which caused iterative cook to fail for: %s could be slow"), *UncookedFilenameString);
						TMap<FString, FMD5Hash> CurrentPackageHashes;
						PDInfoModule.RecursiveGetDependentPackageHashes(*UncookedFilenameString, CurrentPackageHashes);
						for (const auto& CurrentPackageHash : CurrentPackageHashes)
						{
							const FName PackageName(*CurrentPackageHash.Key);
							FCookedAssetPackageInfo* CurrentCookedPackageInfo = CookedPackageInfoLookup.FindRef(PackageName);
							if (CurrentCookedPackageInfo && CurrentCookedPackageInfo->FullPackageHash != CurrentPackageHash.Value)
							{
								UE_LOG(LogCook, Display, TEXT("\tinvalidated because full package hash for package: %s doesn't match"), *CurrentPackageHash.Key);
							}
						}

						TArray<FString> CurrentPackageDependencies;
						PDInfoModule.GetPackageDependencies(*UncookedFilenameString, CurrentPackageDependencies);

						FString DependencyString = TEXT("CookedPackageInfo:");
						for (const FString& Dependency : CookedPackageInfo->DependentPackages)
						{
							DependencyString += TEXT("\n");
							DependencyString += Dependency;
						}
						DependencyString += TEXT("\nCurrentPackageInfo");
						for (const FString& Dependency : CurrentPackageDependencies)
						{
							DependencyString += TEXT("\n");
							DependencyString += Dependency;
						}

						UE_LOG(LogCook, Display, TEXT("Verifing package dependency count and order %d != %d %s"), CookedPackageInfo->DependentPackages.Num(), CurrentPackageDependencies.Num(), *DependencyString);

						
						UE_LOG(LogCook, Display, TEXT("Finish trace of: %s"), *UncookedFilenameString);
					}
					bShouldKeep = false;
				}

				if (bShouldKeep)
				{
					// Add this package to the CookedPackages list so that we don't try cook it again
					check( IFileManager::Get().FileExists(*CookedFile.ToString()));
					TArray<FName> PlatformNames;
					PlatformNames.Add(PlatformFName);
					CookedPackages.Add(FFilePlatformCookedPackage(UncookedFilename, MoveTemp(PlatformNames), true));
					++NumPackagesKept;
				}
				else
				{
					// delete the old package 
					const FString CookedFullPath = FPaths::ConvertRelativePathToFull(CookedFile.ToString());
#if DEBUG_COOKONTHEFLY
					UE_LOG(LogCook, Display, TEXT("Deleting cooked package %s failed filehash test"), *CookedFullPath);
#endif
					CookedPackagesToDelete.Add(CookedFullPath); 
				}
			}

			uint32 NumMarkedEditorOnly = 0;
			uint32 NumMarkedFailedSave = 0;
			uint32 NumMarkedFailedSaveKept = 0;
			uint32 NumMarkedFailedSaveMismatchHash = 0;

			// additionally we also need to populate the editor packages which were not cooked
			for (const auto& CookedPackageInfo : CookedPackagesInfo)
			{
				if (CookedPackageInfo.SucceededSave == false)
				{
					// if we failed to save then we shouldn't be in the cooked package lists

					const FName StandardName = GetCachedStandardPackageFileFName(FName(*CookedPackageInfo.SourcePackageName));

					++NumMarkedFailedSave;

					TArray<FName> PlatformNames;
					PlatformNames.Add(PlatformFName);


					ensure(CookedPackages.Exists(StandardName, PlatformNames, false) == false);

					bool bShouldKeep = true;
					FMD5Hash FullPackageHash;
					if (bShouldKeep && PDInfoModule.DetermineFullPackageHash(*StandardName.ToString(), FullPackageHash) == false)
					{
						++NumPackagesUnableToDetermineCurrentDependencyInfo;
						// UE_LOG(LogCook, Display, TEXT("Unable to find package dependency information for: %s, unable to iterative cook"), *UncookedFilenameString);
						bShouldKeep = false;
					}

					// test against the saved hash for the source packages
					// if a package changes so it can now be saved it may need to be recooked
					// failed packages only need their package hash processed as their dependencies wouldn't have been saved the first time around
					if (bShouldKeep && CookedPackageInfo.FullPackageHash != FullPackageHash)
					{
						++NumMarkedFailedSaveMismatchHash;
						UE_LOG(LogCook, Display, TEXT("Full package hash for: %s doesn't match, force recook"), *StandardName.ToString());
						bShouldKeep = false;
					}


					if ( bShouldKeep )
					{
						++NumMarkedFailedSaveKept;
						CookedPackages.Add(MoveTemp(FFilePlatformCookedPackage(StandardName, MoveTemp(PlatformNames), false)));
					}
					
				}
			}

			// additionally we also need to populate the editor packages which were not cooked
			for (const auto& CookedPackageInfo : CookedPackagesInfo)
			{
				if (CookedPackageInfo.IsEditorPackage)
				{
					++NumMarkedEditorOnly;
					UncookedEditorOnlyPackages.Add(FName(*CookedPackageInfo.SourcePackageName));
				}
			}

			UE_LOG(LogCook, Display, TEXT("Iterative cooking summary for %s, \nConsidered: %d, \nFile Hash missmatch: %d, \nDependency Hash Missmatch: %d, \nPackages marked editor only: %d, \nPackages failed save: %d, \nPackages failed save hash mismatch: %d, \nPackages Kept: %d, \nPackages failed save kept: %d, \nMissing Cooked Info(expected 0): %d, \nFailed Determining Package Info(expected 0): %d"),
				*Target->PlatformName(),
				NumPackagesConsidered, NumPackagesFileHashMissmatch, NumPackagesDependentHashMissmatch, 
				NumMarkedEditorOnly, NumMarkedFailedSave, NumMarkedFailedSaveMismatchHash,
				NumPackagesKept, NumMarkedFailedSaveKept,
				NumPackagesUnableToFindCookedPackageInfo, NumPackagesUnableToDetermineCurrentDependencyInfo);

			
			
			auto DeletePackageLambda = [&CookedPackagesToDelete](int32 PackageIndex)
				{ 
					const FString& CookedFullPath = CookedPackagesToDelete[PackageIndex];
					IFileManager::Get().Delete(*CookedFullPath, true, true, true);
				};
			ParallelFor( CookedPackagesToDelete.Num(), DeletePackageLambda );

			/*for ( const FString& CookedFullPath : CookedPackagesToDelete )
			{
				IFileManager::Get().Delete(*CookedFullPath, true, true, true);
			}*/

		}
		else
		{
			UE_LOG(LogCook, Display, TEXT("Used time stamps to iteratively cook"));
			// list of directories to skip
			TArray<FString> DirectoriesToSkip;
			TArray<FString> DirectoriesToNotRecurse;

			// use the timestamp grabbing visitor
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);

			PlatformFile.IterateDirectory(*SandboxPath, Visitor);

			for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
			{
				FString CookedFilename = TimestampIt.Key();
				if ((CookedFilename.EndsWith(TEXT(".uasset")) == false) &&
					(CookedFilename.EndsWith(TEXT(".umap")) == false))
				{
					// don't care if it's not a map or an asset
					continue;
				}
				FDateTime CookedTimestamp = TimestampIt.Value();
				CookedFilename = FPaths::ConvertRelativePathToFull(CookedFilename);

				FString UncookedFilename = ConvertCookedPathToUncookedPath(CookedFilename);

				FDateTime DependentTimestamp;
				if (PDInfoModule.DeterminePackageDependentTimeStamp(*UncookedFilename, DependentTimestamp) == true)
				{
					double Diff = (CookedTimestamp - DependentTimestamp).GetTotalSeconds();

					if (Diff >= 0.0)
					{
						TArray<FName> PlatformNames;
						PlatformNames.Add(PlatformFName);
						CookedPackages.Add(FFilePlatformCookedPackage(FName(*UncookedFilename), MoveTemp(PlatformNames), true));
					}
				}
			}
		}
	}
}

const FString ExtractPackageNameFromObjectPath( const FString ObjectPath )
{
	// get the path 
	int32 Beginning = ObjectPath.Find(TEXT("'"), ESearchCase::CaseSensitive);
	if ( Beginning == INDEX_NONE )
	{
		return ObjectPath;
	}
	int32 End = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, Beginning + 1);
	if (End == INDEX_NONE )
	{
		End = ObjectPath.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Beginning + 1);
	}
	if ( End == INDEX_NONE )
	{
		// one more use case is that the path is "Class'Path" example "OrionBoostItemDefinition'/Game/Misc/Boosts/XP_1Win" dunno why but this is actually dumb
		if ( ObjectPath[Beginning+1] == '/' )
		{
			return ObjectPath.Mid(Beginning+1);
		}
		return ObjectPath;
	}
	return ObjectPath.Mid(Beginning + 1, End - Beginning - 1);
}

void UCookOnTheFlyServer::VerifyCookedPackagesAreUptodate(const TArray<ITargetPlatform*>& Platforms)
{
	// we remove cooked packages which are updated using the "MarkObjectDirty" callbacks from the editor
	// here we process the dependencies by removing any packages which awere dependent on a package which was removed

	uint32 CookedPackagesMissingOnDisk = 0;

	// loop through all packages on disk and remove any packages from the cooked packages array which don't exist
	// this shouldn't ever happen because the cooker was not shutdown since it cooked these packages so unless someone manually went in and deleted the cooked packages this won't happen
	for (const auto& TargetPlatform : Platforms)
	{
		FName PlatformFName(*TargetPlatform->PlatformName());
		TArray<FName> PlatformNamesArray;

		TArray<FName> CookedFiles;
		CookedPackages.GetCookedFilesForPlatform(PlatformFName, CookedFiles, false);

		for (const FName& CookedFile : CookedFiles)
		{
			const FString SandboxPath = SandboxFile->ConvertToSandboxPath(*CookedFile.ToString()).Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());

			if (!IFileManager::Get().FileExists(*SandboxPath))
			{
				++CookedPackagesMissingOnDisk;
				// removing cooked file because it doesn't exist
				if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
				{
					UE_LOG(LogCook, Display, TEXT("Cooked file invalidated because it doesn't exist %s"), *SandboxPath);
				}
				CookedPackages.RemoveFileForPlatform(CookedFile, PlatformFName);
			}
		}
	}



	// loop through all cooked packages and remove any which are missing their dependencies
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	uint32 CookedPackagesConsidered = 0;
	uint32 CookedPackagesFailedDependencyCheck = 0;
	uint32 FoundRedirectors = 0;
	uint32 CookedPackagesKept = 0;
	


	for (const auto& TargetPlatform : Platforms)
	{
		FName PlatformFName = FName(*TargetPlatform->PlatformName());


		TArray<FName> PlatformNamesArray;
		PlatformNamesArray.Add(PlatformFName);

		TArray<FName> CookedFiles;
		CookedPackages.GetCookedFilesForPlatform(PlatformFName, CookedFiles, false);
		for (const auto& CookedFile : CookedFiles)
		{
			bool bShouldRemovePackage = false;

			const FName* PackageNamePtr = GetCachedPackageFilenameToPackageFName(CookedFile);
			check(PackageNamePtr != nullptr);
			const FName& PackageName = *PackageNamePtr;

			TArray<FName> Dependencies;
			Dependencies.Add(PackageName);

			++CookedPackagesConsidered;

			for (int32 I = 0; I < Dependencies.Num(); ++I)
			{
				FName DependencyPackageName = Dependencies[I];

				if (FPackageName::IsScriptPackage(DependencyPackageName.ToString()))
				{
					// don't care about script packages we are assuming the scripts can't be invalidated after the cooker has started
					continue;
				}

				if (!FPackageName::IsValidLongPackageName(*DependencyPackageName.ToString()))
				{
					// this is probably a soft reference to a UObject rather then a package anem.
					const FString FinalPackageNameString = ExtractPackageNameFromObjectPath(DependencyPackageName.ToString());
					if (FPackageName::IsValidLongPackageName(FinalPackageNameString))
					{
						DependencyPackageName = FName(*FinalPackageNameString);
					}
					else
					{
						UE_LOG(LogCook, Warning, TEXT("Unable resolve dependency package name %s, not able to iterative cook %s"), *DependencyPackageName.ToString(), *CookedFile.ToString());
						bShouldRemovePackage = true;
						break;
					}
				}

				if (UncookedEditorOnlyPackages.Contains(DependencyPackageName))
				{
					// don't need to process this package, it was editor only so unless some other package comes along and needs it, it won't need to be resaved (and also shouldn't invalidate other packages)
					continue;
				}

				bool bContainsObjectRedirector = false;
				TArray<FAssetData> AssetData;
				if (AssetRegistry.GetAssetsByPackageName(DependencyPackageName, AssetData, true))
				{
					for ( const auto& Asset : AssetData )
					{
						if (Asset.IsRedirector())
						{
							FString DestinationObjectPath;
							if (Asset.GetTagValue("DestinationObject", DestinationObjectPath))
							{
								FString RedirectPackageName = ExtractPackageNameFromObjectPath(DestinationObjectPath);
								if ( !FPackageName::IsValidLongPackageName(RedirectPackageName) )
								{
									UE_LOG(LogCook, Warning, TEXT("Unable to redirector object path from string %s, potentially malformed"), *DestinationObjectPath);
									continue;
								}
								Dependencies.AddUnique( FName(*RedirectPackageName) );
							}
							bContainsObjectRedirector = true;
						}
					}
					if (bContainsObjectRedirector)
					{
						++FoundRedirectors;
						// don't process this dependency anymore 
						continue;
					}
				}

				const FName StandardDependencyName = GetCachedStandardPackageFileFName(DependencyPackageName);
				if ( StandardDependencyName == NAME_None )
				{
					UE_LOG(LogCook, Warning, TEXT("Unable to resolve package name %s not able to iterative cook"), *DependencyPackageName.ToString());
					bShouldRemovePackage = true;
					break;
				}

				// check this package is cooked for the platform
				if (!CookedPackages.Exists(StandardDependencyName, PlatformNamesArray))
				{
					if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
					{
						UE_LOG(LogCook, Display, TEXT("Invalidating cooked package %s because %s was considered out of date"), *CookedFile.ToString(), *StandardDependencyName.ToString());
					}
					++CookedPackagesFailedDependencyCheck;
					bShouldRemovePackage = true;
					break;
				}

				if (CookedPackages.Exists(StandardDependencyName, PlatformNamesArray,false))
				{
					TArray<FName> NewDependencies;
					AssetRegistry.GetDependencies(DependencyPackageName, NewDependencies, EAssetRegistryDependencyType::All);
					for (const auto& NewDependency : NewDependencies)
					{
						Dependencies.AddUnique(NewDependency);
					}
				}
			}

			if (bShouldRemovePackage)
			{
				CookedPackages.RemoveFileForPlatform(CookedFile, PlatformFName);
			}
			else
			{
				++CookedPackagesKept;
			}
		}
	}


	uint32 CookedPackagesDeleted = 0; // this should be the same as the cooked packages removed above but in cases where people tamper with the cooked directory this will be different

	// delete any packages on disk which don't exist in the CookedPackages array 
	for (const auto& TargetPlatform : Platforms)
	{
		FString SandboxPath = GetSandboxDirectory(TargetPlatform->PlatformName());
		FName PlatformFName(*TargetPlatform->PlatformName());
		TArray<FName> PlatformNamesArray;
		PlatformNamesArray.Add(PlatformFName);


		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		TArray<FString> CookedFiles;
		FPackageSearchVisitor PackageSearch(CookedFiles);
		PlatformFile.IterateDirectoryRecursively(*SandboxPath, PackageSearch);

		for (const FString& CookedFile : CookedFiles)
		{
			const FString UncookedFilename = ConvertCookedPathToUncookedPath(CookedFile);
			const FName UncookedFName(*UncookedFilename);
			if (!CookedPackages.Exists(UncookedFName, PlatformNamesArray))
			{
				IFileManager::Get().Delete(*CookedFile, true, true, true);
				if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
				{
					UE_LOG(LogCook, Display, TEXT("Deleted cooked package %s because it wasn't in the cooked package list"), *CookedFile);
				}
			}
		}
	}

	// CookedPackagesConsidered are packages which passed the missing test and had their dependencies tested against
	// FoundRedirectors are packages which were referencing redirectors
	// CookedPackagesKept are packages which were kept after their dependencies were checked that they still exist
	// CookedPackagesMissingOnDisk are packages which were in the CookedPackages list but weren't actually on disk in the "Cooked\" directory
	// CookedPackagesDeleted are packages which were deleted as a result of them not being kept or being in the cooked directory and not on the CookedPackages list 
	//		this can happen when something removed them from the cooked packages list either because it was modified in memory or package information couldn't be found about this package
	UE_LOG(LogCook, Display, TEXT("Verify cooked packages: \nPackages considered for dependency checks: %d, \nPackages failed dependency checks %d, \nFound redirectors: %d, \nPackages Kept: %d, \nPackages Missing: %d, \nPackages Deleted: %d"),
		CookedPackagesConsidered, 
		CookedPackagesFailedDependencyCheck,
		FoundRedirectors, 
		CookedPackagesKept, 
		CookedPackagesMissingOnDisk,
		CookedPackagesDeleted );

}

void UCookOnTheFlyServer::CleanSandbox(const bool bIterative)
{
	//child cookers shouldn't clean sandbox we would be deleting the master cookers / other cookers data
	check(IsChildCooker() == false);

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();

	// before we can delete any cooked files we need to make sure that we have finished writing them
	UPackage::WaitForAsyncFileWrites();

#if OUTPUT_TIMING
	double SandboxCleanTime = 0.0;
#endif
	{
#if OUTPUT_TIMING
		SCOPE_SECONDS_COUNTER(SandboxCleanTime);
		SCOPE_TIMER(CleanSandboxTime);
#endif
		if (bIterative == false)
		{
			// for now we are going to wipe the cooked directory
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				UE_LOG(LogCook, Display, TEXT("Cooked content cleared for platform %s"), *Target->PlatformName());

				FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName()); // GetOutputDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);

				ClearPlatformCookedData(FName(*Target->PlatformName()));

				IniSettingsOutOfDate(Target);
				SaveCurrentIniSettings(Target);
			}

		}
		else
		{
			for (const auto& Target : Platforms)
			{
				if (IniSettingsOutOfDate(Target))
				{
					UE_LOG(LogCook, Display, TEXT("Cook invalidated for platform %s ini settings don't match from last cook, clearing all cooked content"), *Target->PlatformName());

					ClearPlatformCookedData(FName(*Target->PlatformName()));

					FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName());
					IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);


					SaveCurrentIniSettings(Target);
				}
			}

			if (bIsInitializingSandbox)
			{
				// if we are initializing sandbox that means that the sandbox has been recreated and could be pointing at something different
				// need to hit the disk and populate our cooked packages form there
				PopulateCookedPackagesFromDisk(Platforms);

				VerifyCookedPackagesAreUptodate(Platforms);
			}
			else
			{
				// do a fast check and see if anything has changed since last cook
				// we know that the sandbox is the same so the cookedpackagelist will be populated with what's on disk
				// note this can only happen in editor
				VerifyCookedPackagesAreUptodate(Platforms);
			}


			// Collect garbage to ensure we don't have any packages hanging around from dependent time stamp determination
			CollectGarbage(RF_NoFlags);
		}
	}
#if OUTPUT_TIMING
	FString PlatformNames;
	for (const auto& Target : Platforms)
	{
		PlatformNames += Target->PlatformName() + TEXT(" ");
	}
	UE_LOG(LogCook, Display, TEXT("Sandbox cleanup took %5.3f seconds for platforms %s iterative %s"), SandboxCleanTime, *PlatformNames, bIterative ? TEXT("true") : TEXT("false"));
#endif
}

void UCookOnTheFlyServer::GenerateAssetRegistry()
{
	if (!!(CookFlags & ECookInitializationFlags::GeneratedAssetRegistry))
	{
		return;
	}
	CookFlags |= ECookInitializationFlags::GeneratedAssetRegistry;

	if (IsChildCooker())
	{
		// don't generate the asset registry
		return;
	}

	if (IsCookingInEditor() == false)
	{
		// we want to register the temporary save directory if we are cooking outside the editor.  
		// If we are cooking inside the editor we never use this directory so don't worry about registring it
		FPackageName::RegisterMountPoint(TEXT("/TempAutosave/"), FPaths::GameSavedDir() / GEngine->PlayOnConsoleSaveDir);
	}

	// load the interface
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	double GenerateAssetRegistryTime = 0.0;
	{
		SCOPE_TIMER(GenerateAssetRegistryTime);
#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Creating asset registry [is editor: %d]"), GIsEditor);
#endif
		// Perform a synchronous search of any .ini based asset paths (note that the per-game delegate may
		// have already scanned paths on its own)
		// We want the registry to be fully initialized when generating streaming manifests too.
		bool bEditor = IsRealtimeMode();

		// editor will scan asset registry automagically 
		if ( !bEditor )
		{
			TArray<FString> ScanPaths;
			if (GConfig->GetArray(TEXT("AssetRegistry"), TEXT("PathsToScanForCook"), ScanPaths, GEngineIni) > 0)
			{
				AssetRegistry.ScanPathsSynchronous(ScanPaths);
			}
			else
			{
				AssetRegistry.SearchAllAssets(true);
			}
		}
	}
}

void UCookOnTheFlyServer::GenerateLongPackageNames(TArray<FName>& FilesInPath)
{
	TArray<FName> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());

	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1].ToString();
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			const FName FileInPathFName(*FileInPath);
			FilesInPathReverse.AddUnique(FileInPathFName);
		}
		else
		{
			FString LongPackageName;
			FString FailureReason;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName, &FailureReason))
			{
				const FName LongPackageFName(*LongPackageName);
				FilesInPathReverse.AddUnique(LongPackageFName);
			}
			else
			{
				LogCookerMessage(FString::Printf(TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason);
			}
		}
	}
	// Exchange(FilesInPathReverse, FilesInPath);
	FilesInPath.Empty(FilesInPathReverse.Num());
	for ( const auto& Files : FilesInPathReverse )
	{
		FilesInPath.Add(Files);
	}
}

void UCookOnTheFlyServer::AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const
{ 
	if (!FPackageName::IsScriptPackage(InFilename))
	{
		FName InFilenameName = FName(*InFilename );
		if ( InFilenameName == NAME_None)
		{
			return;
		}
		InOutFilesToCook.AddUnique(InFilenameName);
	}
}

void UCookOnTheFlyServer::CollectFilesToCook(TArray<FName>& FilesInPath, const TArray<FString>& CookMaps, const TArray<FString>& InCookDirectories, const TArray<FString> &CookCultures, const TArray<FString> &IniMapSections, ECookByTheBookOptions FilesToCookFlags)
{
#if OUTPUT_TIMING
	SCOPE_TIMER(StartupPackagesResolveStringAssetReferences);
#endif
	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());

	bool bCookAll = (!!(FilesToCookFlags & ECookByTheBookOptions::CookAll)) || PackagingSettings->bCookAll;
	bool bMapsOnly = (!!(FilesToCookFlags & ECookByTheBookOptions::MapsOnly)) || PackagingSettings->bCookMapsOnly;
	bool bNoDev = !!(FilesToCookFlags & ECookByTheBookOptions::NoDevContent);

	if (IsChildCooker())
	{
		const FString ChildCookFilename = CookByTheBookOptions->ChildCookFilename;
		check(ChildCookFilename.Len());
		FString ChildCookString;
		ensure(FFileHelper::LoadFileToString(ChildCookString, *ChildCookFilename));
	
		TArray<FString> ChildCookArray;
		ChildCookString.ParseIntoArrayLines(ChildCookArray);

		for (const FString& ChildFile : ChildCookArray)
		{
			AddFileToCook(FilesInPath, ChildFile);
		}
		// if we are a child cooker then just add the files for the child cooker and early out 
		return;
	}

	
	TArray<FString> CookDirectories = InCookDirectories;
	
	if (!IsCookingDLC() && 
		!(FilesToCookFlags & ECookByTheBookOptions::NoAlwaysCookMaps) )
	{
		TArray<FString> MapList;
		// Add the default map section
		GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), MapList);

		for (const auto &IniMapSection : IniMapSections)
		{
			GEditor->LoadMapListFromIni(*IniMapSection, MapList);
		}

		for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
		{
			AddFileToCook(FilesInPath, MapList[MapIdx]);
		}


		// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
		{
			const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
			for (const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
			{
				CookDirectories.Add(AbsoluteGameContentDir / DirToCook.Path);
			}
		}


		// If we didn't find any maps look in the project settings for maps
		for (const auto& MapToCook : PackagingSettings->MapsToCook)
		{
			FilesInPath.Add(FName(*MapToCook.FilePath));
		}
	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::NoGameAlwaysCookPackages))
	{
		COOK_STAT(FScopedDurationTimer TickTimer(DetailedCookStats::GameCookModificationDelegateTimeSec));

#define DEBUG_COOKMODIFICATIONDELEGATE 0
#if DEBUG_COOKMODIFICATIONDELEGATE
		TSet<UPackage*> LoadedPackages;
		for ( TObjectIterator<UPackage> It; It; ++It)
		{
			LoadedPackages.Add(*It);
		}
#endif

		// allow the game to fill out the asset registry, as well as get a list of objects to always cook
		TArray<FString> FilesInPathStrings;
		FGameDelegates::Get().GetCookModificationDelegate().ExecuteIfBound(FilesInPathStrings);
		for (const auto& FileString : FilesInPathStrings)
		{
			FilesInPath.Add(FName(*FileString));
		}
#if DEBUG_COOKMODIFICATIONDELEGATE
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if ( !LoadedPackages.Contains(*It) )
			{
				UE_LOG(LogCook, Display, TEXT("CookModificationDelegate loaded %s"), *It->GetName());
			}
		}
#endif
	}

	for ( const auto &CurrEntry : CookMaps )
	{
		if (FPackageName::IsShortPackageName(CurrEntry))
		{
			FString OutFilename;
			if (FPackageName::SearchForPackageOnDisk(CurrEntry, NULL, &OutFilename) == false)
			{
				LogCookerMessage( FString::Printf(TEXT("Unable to find package for map %s."), *CurrEntry), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to find package for map %s."), *CurrEntry);
			}
			else
			{
				AddFileToCook( FilesInPath, OutFilename);
			}
		}
		else
		{
			AddFileToCook( FilesInPath,CurrEntry);
		}
	}

	const FString ExternalMountPointName(TEXT("/Game/"));
	for ( const auto &CurrEntry : CookDirectories )
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CurrEntry, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			AddFileToCook( FilesInPath,StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, CurrEntry);
			}
		}
	}

	// Add any files of the desired cultures localized assets to cook.
	for (const FString& CultureToCookName : CookCultures)
	{
		FCulturePtr CultureToCook = FInternationalization::Get().GetCulture(CultureToCookName);
		if (!CultureToCook.IsValid())
		{
			continue;
		}

		const TArray<FString> CultureNamesToSearchFor = CultureToCook->GetPrioritizedParentCultureNames();

		for (const FString& L10NSubdirectoryName : CultureNamesToSearchFor)
		{
			TArray<FString> Files;
			const FString DirectoryToSearch = FPaths::Combine(*FPaths::GameContentDir(), *FString::Printf(TEXT("L10N/%s"), *L10NSubdirectoryName));
			IFileManager::Get().FindFilesRecursive(Files, *DirectoryToSearch, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
			for (FString StdFile : Files)
			{
				FPaths::MakeStandardFilename(StdFile);
				AddFileToCook(FilesInPath, StdFile);
			}
		}			
	}

	if ( IsCookingDLC() )
	{
		// get the dlc and make sure we cook that directory 
		FString DLCPath = FPaths::GamePluginsDir() / CookByTheBookOptions->DlcName / FString(TEXT("Content"));

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetMapPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			AddFileToCook( FilesInPath,StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, DLCPath);
			}
		}
	}

	if ((FilesInPath.Num() == 0) || bCookAll)
	{
		TArray<FString> Tokens;
		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());

		uint8 PackageFilter = NORMALIZE_DefaultFlags | NORMALIZE_ExcludeEnginePackages;
		if ( bMapsOnly )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( bNoDev )
		{
			PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
		}

		// assume the first token is the map wildcard/pathname
		TArray<FString> Unused;
		for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			TArray<FString> TokenFiles;
			if ( !NormalizePackageNames( Unused, TokenFiles, Tokens[TokenIndex], PackageFilter) )
			{
				UE_LOG(LogCook, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			for (int32 TokenFileIndex = 0; TokenFileIndex < TokenFiles.Num(); ++TokenFileIndex)
			{
				AddFileToCook( FilesInPath, TokenFiles[TokenFileIndex]);
			}
		}
	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::NoDefaultMaps))
	{
		// make sure we cook the default maps
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		static const TArray<ITargetPlatform*>& Platforms = TPM.GetTargetPlatforms();
		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			// load the platform specific ini to get its DefaultMap
			FConfigFile PlatformEngineIni;
			FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, *Platforms[Index]->IniPlatformName());

			// get the server and game default maps and cook them
			FString Obj;
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (IsCookFlagSet(ECookInitializationFlags::IncludeServerMaps))
			{
				if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), Obj))
				{
					if (Obj != FName(NAME_None).ToString())
					{
						AddFileToCook(FilesInPath, Obj);
					}
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultServerGameMode"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameInstanceClass"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
		}
	}


	if (!(FilesToCookFlags & ECookByTheBookOptions::NoInputPackages))
	{
		// make sure we cook any extra assets for the default touch interface
		// @todo need a better approach to cooking assets which are dynamically loaded by engine code based on settings
		FConfigFile InputIni;
		FString InterfaceFile;
		FConfigCacheIni::LoadLocalIniFile(InputIni, TEXT("Input"), true);
		if (InputIni.GetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultTouchInterface"), InterfaceFile))
		{
			if (InterfaceFile != TEXT("None") && InterfaceFile != TEXT(""))
			{
				AddFileToCook(FilesInPath, InterfaceFile);
			}
		}
	}
	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	if ( !(FilesToCookFlags & ECookByTheBookOptions::NoSlatePackages))
	{
		TArray<FString> UIContentPaths;
		TSet <FName> ContentDirectoryAssets; 
		if (GConfig->GetArray(TEXT("UI"), TEXT("ContentDirectories"), UIContentPaths, GEditorIni) > 0)
		{
			for (int32 DirIdx = 0; DirIdx < UIContentPaths.Num(); DirIdx++)
			{
				FString ContentPath = FPackageName::LongPackageNameToFilename(UIContentPaths[DirIdx]);

				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *ContentPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (int32 Index = 0; Index < Files.Num(); Index++)
				{
					FString StdFile = Files[Index];
					FName PackageName = FName(*FPackageName::FilenameToLongPackageName(StdFile));
					ContentDirectoryAssets.Add(PackageName);
					FPaths::MakeStandardFilename(StdFile);
					AddFileToCook( FilesInPath, StdFile);
				}
			}
		}

		if (CookByTheBookOptions && CookByTheBookOptions->bGenerateDependenciesForMaps) 
		{
			for (auto& MapDependencyGraph : CookByTheBookOptions->MapDependencyGraphs)
			{
				MapDependencyGraph.Value.Add(FName(TEXT("ContentDirectoryAssets")), ContentDirectoryAssets);
			}
		}
	}
}

bool UCookOnTheFlyServer::IsCookByTheBookRunning() const
{
	return CookByTheBookOptions && CookByTheBookOptions->bRunning;
}


void UCookOnTheFlyServer::SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms)
{
	// we don't support this behavior
	check( !IsCookingDLC() );
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// make sure global shaders are up to date!
		TArray<FString> Files;
		FShaderRecompileData RecompileData;
		RecompileData.PlatformName = Platforms[Index]->PlatformName();
		// Compile for all platforms
		RecompileData.ShaderPlatform = -1;
		RecompileData.ModifiedFiles = &Files;
		RecompileData.MeshMaterialMaps = NULL;

		check( IsInGameThread() );

		FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

		RecompileShadersForRemote
			(RecompileData.PlatformName, 
			RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
			OutputDir, 
			RecompileData.MaterialsToLoad, 
			RecompileData.SerializedShaderResources, 
			RecompileData.MeshMaterialMaps, 
			RecompileData.ModifiedFiles);
	}
}

FString UCookOnTheFlyServer::GetSandboxDirectory( const FString& PlatformName ) const
{
	FString Result;
	Result = SandboxFile->GetSandboxDirectory();

	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite ) const
{
	check( SandboxFile );

	FString Result;
	if ( bForWrite)
	{
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FileName);
	}
	else
	{
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForRead(*FileName);
	}

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const
{
	FString Result = ConvertToFullSandboxPath( FileName, bForWrite );
	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);
	return Result;
}

const FString UCookOnTheFlyServer::GetSandboxAssetRegistryFilename()
{
	static const FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
	const FString SandboxRegistryFilename = ConvertToFullSandboxPath(*RegistryFilename, true);
	return SandboxRegistryFilename;
}

const FString UCookOnTheFlyServer::GetCookedAssetRegistryFilename(const FString& PlatformName )
{
	const FString CookedAssetRegistryFilename = GetSandboxAssetRegistryFilename().Replace(TEXT("[Platform]"), *PlatformName);
	return CookedAssetRegistryFilename;
}

void UCookOnTheFlyServer::CookByTheBookFinished()
{
	check( IsInGameThread() );
	check( IsCookByTheBookMode() );
	check( CookByTheBookOptions->bRunning == true );


	UPackage::WaitForAsyncFileWrites();

	GetDerivedDataCacheRef().WaitForQuiescence(true);
	
	GRedirectCollector.LogTimers();

	if (IsChildCooker())
	{
		// if we are the child cooker create a list of all the packages which we think wes hould have cooked but didn't
		FString UncookedPackageList;
		for (const auto& UncookedPackage : CookByTheBookOptions->ChildUnsolicitedPackages)
		{
			UncookedPackageList.Append(UncookedPackage.ToString() + TEXT("\n\r"));
		}
		FFileHelper::SaveStringToFile(UncookedPackageList, *GetChildCookerResultFilename(CookByTheBookOptions->ChildCookFilename));
		if (IBlueprintNativeCodeGenModule::IsNativeCodeGenModuleLoaded())
		{
			IBlueprintNativeCodeGenModule::Get().SaveManifest(CookByTheBookOptions->ChildCookIdentifier);
		}
	}
	else
	{
		CleanUpChildCookers();

		if (IBlueprintNativeCodeGenModule::IsNativeCodeGenModuleLoaded())
		{
			IBlueprintNativeCodeGenModule& CodeGenModule = IBlueprintNativeCodeGenModule::Get();

			CodeGenModule.GenerateFullyConvertedClasses(); // While generating fully converted classes the list of necessary stubs is created.
			CodeGenModule.GenerateStubs();

			// merge the manifest for the blueprint code generator:
			for (int32 I = 0; I < CookByTheBookOptions->ChildCookers.Num(); ++I )
			{
				CodeGenModule.MergeManifest(I);
			}

			CodeGenModule.FinalizeManifest();
		}

		check(CookByTheBookOptions->ChildUnsolicitedPackages.Num() == 0);
		SCOPE_TIMER(SavingAssetRegistry);
		// Save modified asset registry with all streaming chunk info generated during cook
		const FString& SandboxRegistryFilename = GetSandboxAssetRegistryFilename();
		// the registry filename will be modified when we call save asset registry
		
		const FString CookedAssetRegistry = FPaths::GameDir() / TEXT("CookedAssetRegistry.json");
		const FString SandboxCookedAssetRegistryFilename = ConvertToFullSandboxPath(*CookedAssetRegistry, true);

		if(!IsCookFlagSet(ECookInitializationFlags::Iterative) && CookByTheBookOptions->ChildCookers.Num() == 0 && !IsCookingInEditor())
		{
			FString OutputDir = GetOutputDirectoryOverride();
			
			for(auto PlatformName : CookByTheBookOptions->TargetPlatformNames)
			{
				FString PlatformDir = (OutputDir.Replace(TEXT("[Platform]"), *PlatformName.ToString()) / FApp::GetGameName()) / TEXT("Content");
				FShaderCache::SaveBinaryCache(PlatformDir, PlatformName);
			}
			
			FShaderCache::ShutdownShaderCache();
		}
		
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		for ( const auto& TargetPlatformName : CookByTheBookOptions->TargetPlatformNames )
		{
			const ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(TargetPlatformName.ToString());
			SaveCurrentIniSettings(TargetPlatform);
		}

		for ( auto& Manifest : CookByTheBookOptions->ManifestGenerators )
		{
			TArray<FName> CookedPackagesForPlatform;
			CookedPackages.GetCookedFilesForPlatform(Manifest.Key, CookedPackagesForPlatform, false, true);

			TArray<FName> LongCookedPackageNames;
			for (const auto& CookedPackageFileName : CookedPackagesForPlatform)
			{
				const FName *LongPackageFName = GetCachedPackageFilenameToPackageFName(CookedPackageFileName);
				if ( LongPackageFName && (*LongPackageFName != NAME_None)) // shal we do this the easy way or the hard way?
				{
					LongCookedPackageNames.Add(*LongPackageFName);
					continue;
				}
				// the hard way 
				FString LongPackageName;
				verify(FPackageName::TryConvertFilenameToLongPackageName(CookedPackageFileName.ToString(), LongPackageName));
				LongCookedPackageNames.Add(FName(*LongPackageName));
			}

			Manifest.Value->BuildChunkManifest(LongCookedPackageNames, SandboxFile.Get(), CookByTheBookOptions->bGenerateStreamingInstallManifests);
			// Always try to save the manifests, this is required to make the asset registry work, but doesn't necessarily write a file
			Manifest.Value->SaveManifests(SandboxFile.Get());

			const FName& PlatformName = Manifest.Key;
			TArray<FName> IgnorePackageFilenames;

			// ignore any packages which failed to cook
			CookedPackages.GetCookedFilesForPlatform(PlatformName, IgnorePackageFilenames, true, false);
			// ignore any packages which were in a previous release of cooked content
			IgnorePackageFilenames.Append( CookByTheBookOptions->BasedOnReleaseCookedPackages.FindRef(PlatformName) );

			TArray<FName> IgnorePackageNames;
			IgnorePackageNames.Empty(IgnorePackageFilenames.Num());
			for (const auto& PackageFilename : IgnorePackageFilenames)
			{
				FString LongPackageName;
				verify(FPackageName::TryConvertFilenameToLongPackageName(PackageFilename.ToString(), LongPackageName));
				IgnorePackageNames.Add(FName(*LongPackageName));
			}

			// Ignore packages that weren't cooked because they were only referenced by editor-only properties
			TSet<FName> UncookedEditorOnlyPackageNames;
			UncookedEditorOnlyPackages.GetNames(UncookedEditorOnlyPackageNames);
			for (FName UncookedEditorOnlyPackage : UncookedEditorOnlyPackageNames)
			{
				IgnorePackageNames.Add(UncookedEditorOnlyPackage);
			}
			
			Manifest.Value->SaveAssetRegistry(SandboxRegistryFilename, &IgnorePackageNames);

			TArray<FName> AllCookedPackages;
			AllCookedPackages.Append(CookedPackagesForPlatform);
			TArray<FName> UncookedEditorOnlyPackageNamesArray;
			for (FName UncookedEditorOnlyPackage : UncookedEditorOnlyPackageNames)
			{
				UncookedEditorOnlyPackageNamesArray.Add(UncookedEditorOnlyPackage);
			}
			AllCookedPackages.Sort();
			UncookedEditorOnlyPackageNamesArray.Sort();
			SaveCookedAssetRegistry(SandboxCookedAssetRegistryFilename, AllCookedPackages, UncookedEditorOnlyPackageNamesArray, IgnorePackageNames, PlatformName.ToString(), false);

			if ( IsCreatingReleaseVersion() )
			{
				const FString VersionedRegistryPath = GetReleaseVersionAssetRegistryPath( CookByTheBookOptions->CreateReleaseVersion, Manifest.Key ); 
				IFileManager::Get().MakeDirectory( *VersionedRegistryPath ,true );
				const FString VersionedRegistryFilename = VersionedRegistryPath / GetAssetRegistryFilename();
				const FString CookedAssetRegistryFilename = SandboxRegistryFilename.Replace(TEXT("[Platform]"), *Manifest.Key.ToString());
				IFileManager::Get().Copy(*VersionedRegistryFilename, *CookedAssetRegistryFilename, true, true);
			}
		}
	}

	if (CookByTheBookOptions->bGenerateDependenciesForMaps && (IsChildCooker() == false))
	{
		for (auto& MapDependencyGraphIt : CookByTheBookOptions->MapDependencyGraphs)
		{
			BuildMapDependencyGraph(MapDependencyGraphIt.Key);
			WriteMapDependencyGraph(MapDependencyGraphIt.Key);
		}
	}

	CookByTheBookOptions->LastGCItems.Empty();
	const float TotalCookTime = (float)(FPlatformTime::Seconds() - CookByTheBookOptions->CookStartTime);
	UE_LOG(LogCook, Display, TEXT("Cook by the book total time in tick %fs total time %f"), CookByTheBookOptions->CookTime, TotalCookTime);

	CookByTheBookOptions->BasedOnReleaseCookedPackages.Empty();

	CookByTheBookOptions->bRunning = false;

	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();


	UE_LOG(LogCook, Display, TEXT("Peak Used virtual %u Peak Used phsical %u"), MemStats.PeakUsedVirtual / 1024 / 1024, MemStats.PeakUsedPhysical / 1024 / 1024 );

	OUTPUT_HIERARCHYTIMERS();
}

void UCookOnTheFlyServer::BuildMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);



	TArray<FName> PlatformCookedPackages;
	CookedPackages.GetCookedFilesForPlatform(PlatformName, PlatformCookedPackages);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// assign chunks for all the map packages
	for (const auto& CookedPackage : PlatformCookedPackages)
	{
		TArray<FAssetData> PackageAssets;
		FName Name = FName(*FPackageName::FilenameToLongPackageName(CookedPackage.ToString()));

		if (!ContainsMap(Name))
			continue;

		TSet<FName> DependentPackages;
		TSet<FName> Roots; 

		Roots.Add(Name);

		GetDependentPackages(Roots, DependentPackages);

		MapDependencyGraph.Add(Name, DependentPackages);
	}
}

void UCookOnTheFlyServer::WriteMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);

	FString MapDependencyGraphFile = FPaths::GameDir() / TEXT("MapDependencyGraph.json");
	// dump dependency graph. 
	FString DependencyString;
	DependencyString += "{";
	for (auto& Ele : MapDependencyGraph)
	{
		TSet <FName>& Deps = Ele.Value;
		FName MapName = Ele.Key;
		DependencyString += TEXT("\t\"") + MapName.ToString() + TEXT("\" : \n\t[\n ");
		for (auto& Val : Deps)
		{
			DependencyString += TEXT("\t\t\"") + Val.ToString() + TEXT("\",\n");
		}
		DependencyString.RemoveFromEnd(TEXT(",\n"));
		DependencyString += TEXT("\n\t],\n");
	}
	DependencyString.RemoveFromEnd(TEXT(",\n"));
	DependencyString += "\n}";

	FString CookedMapDependencyGraphFilePlatform = ConvertToFullSandboxPath(MapDependencyGraphFile, true).Replace(TEXT("[Platform]"), *PlatformName.ToString());
	FFileHelper::SaveStringToFile(DependencyString, *CookedMapDependencyGraphFilePlatform, FFileHelper::EEncodingOptions::ForceUnicode);
}

void UCookOnTheFlyServer::QueueCancelCookByTheBook()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		CookByTheBookOptions->bCancel = true;
	}
}

void UCookOnTheFlyServer::CancelCookByTheBook()
{
	if ( IsCookByTheBookMode() && CookByTheBookOptions->bRunning )
	{
		check(CookByTheBookOptions);
		check( IsInGameThread() );

		// save the cook requests 
		CookRequests.DequeueAllRequests(CookByTheBookOptions->PreviousCookRequests);
		CookByTheBookOptions->bRunning = false;

		SandboxFile = nullptr;
	}	
}

void UCookOnTheFlyServer::StopAndClearCookedData()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		check( CookByTheBookOptions->bRunning == false );
		CancelCookByTheBook();
		CookByTheBookOptions->PreviousCookRequests.Empty();
	}

	RecompileRequests.Empty();
	CookRequests.Empty();
	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearAllCookedData()
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearPlatformCookedData( const FName& PlatformName )
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	CookedPackages.RemoveAllFilesForPlatform( PlatformName );
	TArray<FName> PackageNames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformName, PackageNames);
}

void UCookOnTheFlyServer::ClearCachedCookedPlatformDataForPlatform( const FName& PlatformName )
{

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(PlatformName.ToString());
	if ( TargetPlatform )
	{
		for ( TObjectIterator<UObject> It; It; ++It )
		{
			It->ClearCachedCookedPlatformData(TargetPlatform);
		}
	}
}


void UCookOnTheFlyServer::OnTargetPlatformChangedSupportedFormats(const ITargetPlatform* TargetPlatform)
{
	for (TObjectIterator<UObject> It; It; ++It)
	{
		It->ClearCachedCookedPlatformData(TargetPlatform);
	}
}

void UCookOnTheFlyServer::CreateSandboxFile()
{

	// initialize the sandbox file after determining if we are cooking dlc
	// Local sandbox file wrapper. This will be used to handle path conversions,
	// but will not be used to actually write/read files so we can safely
	// use [Platform] token in the sandbox directory name and then replace it
	// with the actual platform name.
	check( SandboxFile == nullptr );
	SandboxFile = MakeUnique<FSandboxPlatformFile>(false);

	// Output directory override.	
	FString OutputDirectory = GetOutputDirectoryOverride();

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	SandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
}

void UCookOnTheFlyServer::InitializeSandbox()
{
	if ( SandboxFile == nullptr )
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetCookingTargetPlatforms();

		CreateSandboxFile();

		if (!IsChildCooker())
		{
			bIsInitializingSandbox = true;
			CleanSandbox(IsCookFlagSet(ECookInitializationFlags::Iterative));
			bIsInitializingSandbox = false;

		}
	}
}

void UCookOnTheFlyServer::TermSandbox()
{
	ClearAllCookedData();
	ClearPackageFilenameCache();
	SandboxFile = nullptr;
}

void UCookOnTheFlyServer::ValidateCookByTheBookSettings() const
{
	if (IsChildCooker())
	{
		// should never be generating dependency maps / streaming install manifests for child cookers
		check(CookByTheBookOptions->bGenerateDependenciesForMaps == false);
		check(CookByTheBookOptions->bGenerateStreamingInstallManifests == false);
	}
}

void UCookOnTheFlyServer::StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions )
{
	const TArray<ITargetPlatform*>& TargetPlatforms = CookByTheBookStartupOptions.TargetPlatforms;
	const TArray<FString>& CookMaps = CookByTheBookStartupOptions.CookMaps;
	const TArray<FString>& CookDirectories = CookByTheBookStartupOptions.CookDirectories;
	const TArray<FString>& CookCultures = CookByTheBookStartupOptions.CookCultures;
	const TArray<FString>& IniMapSections = CookByTheBookStartupOptions.IniMapSections;
	const ECookByTheBookOptions& CookOptions = CookByTheBookStartupOptions.CookOptions;
	const FString& DLCName = CookByTheBookStartupOptions.DLCName;

	const FString& CreateReleaseVersion = CookByTheBookStartupOptions.CreateReleaseVersion;
	const FString& BasedOnReleaseVersion = CookByTheBookStartupOptions.BasedOnReleaseVersion;

	check( IsInGameThread() );
	check( IsCookByTheBookMode() );

	CookByTheBookOptions->bRunning = true;
	CookByTheBookOptions->bCancel = false;
	CookByTheBookOptions->CookTime = 0.0f;
	CookByTheBookOptions->CookStartTime = FPlatformTime::Seconds();
	CookByTheBookOptions->bGenerateStreamingInstallManifests = CookByTheBookStartupOptions.bGenerateStreamingInstallManifests;
	CookByTheBookOptions->bGenerateDependenciesForMaps = CookByTheBookStartupOptions.bGenerateDependenciesForMaps;
	CookByTheBookOptions->CreateReleaseVersion = CreateReleaseVersion;
	CookByTheBookOptions->bForceEnableCompressedPackages = !!(CookOptions & ECookByTheBookOptions::ForceEnableCompressed);
	CookByTheBookOptions->bForceDisableCompressedPackages = !!(CookOptions & ECookByTheBookOptions::ForceDisableCompressed);
	CookByTheBookOptions->ChildCookFilename = CookByTheBookStartupOptions.ChildCookFileName;
	CookByTheBookOptions->bDisableUnsolicitedPackages = !!(CookOptions & ECookByTheBookOptions::DisableUnsolicitedPackages);
	CookByTheBookOptions->ChildCookIdentifier = CookByTheBookStartupOptions.ChildCookIdentifier;

	GenerateAssetRegistry();


	NeverCookPackageList.Empty();
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

		TArray<FString> NeverCookDirectories = CookByTheBookStartupOptions.NeverCookDirectories;

		for (const auto& DirToNotCook : PackagingSettings->DirectoriesToNeverCook)
		{
			NeverCookDirectories.Add(AbsoluteGameContentDir / DirToNotCook.Path);
		}

		for (const auto& NeverCookDirectory : NeverCookDirectories)
		{
			// add the packages to the never cook package list
			struct FNeverCookDirectoryWalker : public IPlatformFile::FDirectoryVisitor
			{
			private:
				FThreadSafeNameSet &NeverCookPackageList;
			public:
				FNeverCookDirectoryWalker(FThreadSafeNameSet &InNeverCookPackageList) : NeverCookPackageList(InNeverCookPackageList) { }

				// IPlatformFile::FDirectoryVisitor interface
				virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
				{
					if (bIsDirectory)
					{
						return true;
					}
					FString StandardFilename = FString(FilenameOrDirectory);
					FPaths::MakeStandardFilename(StandardFilename);

					NeverCookPackageList.Add(FName(*StandardFilename));
					return true;
				}

			} NeverCookDirectoryWalker(NeverCookPackageList);

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			PlatformFile.IterateDirectoryRecursively(*NeverCookDirectory, NeverCookDirectoryWalker);
		}

	}
	

	CookByTheBookOptions->TargetPlatformNames.Empty();
	for (const auto &Platform : TargetPlatforms)
	{
		FName PlatformName = FName(*Platform->PlatformName());
		CookByTheBookOptions->TargetPlatformNames.Add(PlatformName); // build list of all target platform names
	}
	const TArray<FName>& TargetPlatformNames = CookByTheBookOptions->TargetPlatformNames;

	ValidateCookByTheBookSettings();

	if ( CookByTheBookOptions->DlcName != DLCName )
	{
		// we are going to change the state of dlc we need to clean out our package filename cache (the generated filename cache is dependent on this key)
		CookByTheBookOptions->DlcName = DLCName;
		check( OutputDirectoryOverride.Len() == 0 );

		TermSandbox();
	}

	InitializeSandbox();

	bool bRunConversion = FParse::Param(FCommandLine::Get(), TEXT("NativizeAssets"));
	if (bRunConversion)
	{
		FNativeCodeGenInitData CodeGenData;
		for (auto Entry : CookByTheBookStartupOptions.TargetPlatforms)
		{
			// If you change this target path you must also update logic in CookCommand.Automation.CS. Passing a single directory around is cumbersome for testing, so I have hard coded it.
			CodeGenData.CodegenTargets.Push(TPairInitializer<FString, FString>(Entry->PlatformName(), FString(FPaths::Combine( *FPaths::GameIntermediateDir(), *(Entry->PlatformName())))));
		}
		CodeGenData.ManifestIdentifier = CookByTheBookStartupOptions.ChildCookIdentifier;
		IBlueprintNativeCodeGenModule::InitializeModule(CodeGenData);
	}

	// need to test this out
	/*if (RecompileChangedShaders(TargetPlatformNames))
	{
		// clean everything :(
		CleanSandbox(false);
	}*/

	CookByTheBookOptions->bLeakTest = (CookOptions & ECookByTheBookOptions::LeakTest) != ECookByTheBookOptions::None; // this won't work from the editor this needs to be standalone
	check(!CookByTheBookOptions->bLeakTest || CurrentCookMode == ECookMode::CookByTheBook);

	CookByTheBookOptions->LastGCItems.Empty();
	if (CookByTheBookOptions->bLeakTest)
	{
		for (FObjectIterator It; It; ++It)
		{
			CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
		}
	}

	if (!IsChildCooker())
	{
		// iteratively clean any old files out of the sandbox (check if ini settings are out of date and clean out any stale files)
		CleanSandbox(true);

		bool const bShaderCodeCacheCook = (!IsCookFlagSet(ECookInitializationFlags::Iterative) && CookByTheBookStartupOptions.NumProcesses == 0 && CookByTheBookOptions->ChildCookers.Num() == 0 && !IsCookingInEditor());
		
		TArray<FName> CachedShaderFormats;
		for (const auto &Platform : TargetPlatforms)
		{
			FName PlatformName = FName(*Platform->PlatformName());

			// make sure we have a manifest for all the platforms 
			// we want a seperate manifest for each platform because they can all be in different states of cooked content
			FChunkManifestGenerator* ManifestGenerator = CookByTheBookOptions->ManifestGenerators.FindRef(PlatformName);
			if (ManifestGenerator == NULL)
			{
				TArray<ITargetPlatform*> Platforms;
				Platforms.Add(Platform);
				ManifestGenerator = new FChunkManifestGenerator(Platforms);
				ManifestGenerator->CleanManifestDirectories();
				ManifestGenerator->Initialize(CookByTheBookOptions->StartupPackages);
				CookByTheBookOptions->ManifestGenerators.Add(PlatformName, ManifestGenerator);
			}

			if (CookByTheBookOptions->bGenerateDependenciesForMaps)
			{
				CookByTheBookOptions->MapDependencyGraphs.Add(PlatformName);
			}
			
			if(bShaderCodeCacheCook)
			{
				CachedShaderFormats.Reset();
				Platform->GetAllCachedShaderFormats(CachedShaderFormats);
				if(!FShaderCache::GetShaderCache() && CachedShaderFormats.Num() > 0)
				{
					FShaderCache::InitShaderCache(SCO_Cooking);
				}
				FShaderCache::CacheCookedShaderPlatforms(PlatformName, CachedShaderFormats);
			}
		}
	}

	if ( IsCookingDLC() )
	{
		// if we are cooking dlc we must be based of a release version cook
		check( !BasedOnReleaseVersion.IsEmpty() );

		for ( const auto& PlatformName : TargetPlatformNames )
		{
			FString OriginalSandboxRegistryFilename = GetReleaseVersionAssetRegistryPath(BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

			TArray<FName> PackageList;
			// if this check fails probably because the asset registry can't be found or read
			bool bSucceeded = GetAllPackagesFromAssetRegistry(OriginalSandboxRegistryFilename, PackageList);
			if (!bSucceeded)
			{
				using namespace PlatformInfo;
				// Check all possible flavors 
				// For example release version could be cooked as Android_ETC1 flavor, but DLC can be made as Android_ETC2
				FVanillaPlatformEntry VanillaPlatfromEntry = BuildPlatformHierarchy(PlatformName, EPlatformFilter::CookFlavor);
				for (const FPlatformInfo* PlatformFlaworInfo : VanillaPlatfromEntry.PlatformFlavors)
				{
					OriginalSandboxRegistryFilename = GetReleaseVersionAssetRegistryPath(BasedOnReleaseVersion, PlatformFlaworInfo->PlatformInfoName) / GetAssetRegistryFilename();
					bSucceeded = GetAllPackagesFromAssetRegistry(OriginalSandboxRegistryFilename, PackageList);
					if (bSucceeded)
					{
						break;
					}
				}
			}
			check( bSucceeded );

			if ( bSucceeded )
			{
				TArray<FName> PlatformNames;
				PlatformNames.Add(PlatformName);
				for (const auto& PackageFilename : PackageList)
				{
					CookedPackages.Add( MoveTemp( FFilePlatformCookedPackage( PackageFilename, PlatformNames, true ) ) );
				}
			}
			CookByTheBookOptions->BasedOnReleaseCookedPackages.Add(PlatformName, MoveTemp(PackageList));
		}
	}
	
	TArray<FName> FilesInPath;

	{
		GIsCookerLoadingPackage = true;
#if OUTPUT_TIMING
		SCOPE_TIMER(StartupPackagesResolveStringAssetReferences);
#endif
		for (const auto& StartupPackage : CookByTheBookOptions->StartupPackages)
		{
			GRedirectCollector.ResolveStringAssetReference(StartupPackage.ToString());
		}
		GIsCookerLoadingPackage = false;
	}

	// don't resave the global shader map files in dlc
	if ( !IsCookingDLC() && !IsChildCooker() && !(CookByTheBookStartupOptions.CookOptions & ECookByTheBookOptions::ForceDisableSaveGlobalShaders) )
	{
		SaveGlobalShaderMapFiles(TargetPlatforms);
	}
	
	CollectFilesToCook(FilesInPath, CookMaps, CookDirectories, CookCultures, IniMapSections, CookOptions);
	
	if (FilesInPath.Num() == 0)
	{
		LogCookerMessage(FString::Printf(TEXT("No files found to cook.")), EMessageSeverity::Warning);
		UE_LOG(LogCook, Warning, TEXT("No files found."));
	}

	{
#if OUTPUT_TIMING
		SCOPE_TIMER(GenerateLongPackageName);
#endif
		GenerateLongPackageNames(FilesInPath);
	}
	// add all the files for the requested platform to the cook list
	for ( const FName& FileFName : FilesInPath )
	{
		const FName PackageFileFName = GetCachedStandardPackageFileFName(FileFName);
		
		if (PackageFileFName != NAME_None)
		{
			CookRequests.EnqueueUnique( MoveTemp( FFilePlatformRequest( PackageFileFName, TargetPlatformNames ) ) );
		}
		else if (!FLinkerLoad::IsKnownMissingPackage(FileFName))
		{
			const FString FileName = FileFName.ToString();
			LogCookerMessage( FString::Printf(TEXT("Unable to find package for cooking %s"), *FileName), EMessageSeverity::Warning );
			UE_LOG(LogCook, Warning, TEXT("Unable to find package for cooking %s"), *FileName)
		}	
	}


	if ( !IsCookingDLC() && !IsChildCooker() )
	{
		// if we are not cooking dlc then basedOnRelease version just needs to make sure that we cook all the packages which are in the previous release (as well as the new ones)
		if ( !BasedOnReleaseVersion.IsEmpty() )
		{
			// if we are based of a release and we are not cooking dlc then we should always be creating a new one (note that we could be creating the same one we are based of).
			// note that we might erroneously enter here if we are generating a patch instead and we accidentally passed in BasedOnReleaseVersion to the cooker instead of to unrealpak
			check( !CreateReleaseVersion.IsEmpty() );

			for ( const auto& PlatformName : TargetPlatformNames )
			{
				TArray<FName> PlatformArray;
				PlatformArray.Add( PlatformName );

				// if we are based of a cook and we are creating a new one we need to make sure that at least all the old packages are cooked as well as the new ones
				FString OriginalAssetRegistryPath = GetReleaseVersionAssetRegistryPath( BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

				TArray<FName> PackageFiles;
				verify( !GetAllPackagesFromAssetRegistry(OriginalAssetRegistryPath, PackageFiles) );

				for ( const auto& PackageFilename : PackageFiles )
				{
					CookRequests.EnqueueUnique( MoveTemp( FFilePlatformRequest( PackageFilename, PlatformArray) ) );
				}

			}

		}
	}


	// this is to support canceling cooks from the editor
	// this is required to make sure that the cooker is in a good state after cancel occurs
	// if too many packages are being recooked after resume then we may need to figure out a different way to do this
	for ( const auto& PreviousRequest : CookByTheBookOptions->PreviousCookRequests )
	{
		CookRequests.EnqueueUnique( MoveTemp( PreviousRequest ) );
	}
	CookByTheBookOptions->PreviousCookRequests.Empty();
	
	if (CookByTheBookStartupOptions.NumProcesses)
	{
		FString ExtraCommandLine;
		StartChildCookers(CookByTheBookStartupOptions.NumProcesses, TargetPlatformNames, ExtraCommandLine);
	}
}


bool UCookOnTheFlyServer::RecompileChangedShaders(const TArray<FName>& TargetPlatforms)
{
	bool bShadersRecompiled = false;
	for (const auto& TargetPlatform : TargetPlatforms)
	{
		bShadersRecompiled |= RecompileChangedShadersForPlatform(TargetPlatform.ToString());
	}
	return bShadersRecompiled;
}




class FChildCookerRunnable : public FRunnable
{
private:
	UCookOnTheFlyServer* CookServer;
	UCookOnTheFlyServer::FChildCooker* ChildCooker;

	void ProcessChildLogOutput()
	{
		// process the log output from the child cooker even if we just finished to ensure that we get the end of the log
		FString PipeContents = FPlatformProcess::ReadPipe(ChildCooker->ReadPipe);
		if (PipeContents.Len())
		{
			TArray<FString> PipeLines;
			PipeContents.ParseIntoArrayLines(PipeLines);

			for (const auto& Line : PipeLines)
			{
				UE_LOG(LogCook, Display, TEXT("Cooker output %s: %s"), *ChildCooker->BaseResponseFileName, *Line);
				if (ChildCooker->bFinished == false)
				{
					FPlatformProcess::Sleep(0.0f); // don't be greedy, log output isn't important compared to cooking performance
				}
			}
		}
	}

public:
	FChildCookerRunnable(UCookOnTheFlyServer::FChildCooker* InChildCooker, UCookOnTheFlyServer* InCookServer) : FRunnable(), CookServer(InCookServer), ChildCooker(InChildCooker)
	{ }

	
	virtual uint32 Run() override
	{
		while (true)
		{
			check(ChildCooker != nullptr);
			check(ChildCooker->bFinished == false); 

			int32 ReturnCode;
			if (FPlatformProcess::GetProcReturnCode(ChildCooker->ProcessHandle, &ReturnCode))
			{
				if (ReturnCode != 0)
				{
					UE_LOG(LogCook, Error, TEXT("Child cooker %s returned error code %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}
				else
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s returned %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}


				// if the child cooker completed successfully then it would have output a list of files which the main cooker needs to process
				const FString AdditionalPackagesFileName = GetChildCookerResultFilename(ChildCooker->ResponseFileName);

				FString AdditionalPackages;
				if (FFileHelper::LoadFileToString(AdditionalPackages, *AdditionalPackagesFileName) == false)
				{
					UE_LOG(LogCook, Fatal, TEXT("ChildCooker failed to write out additional packages file to location %s"), *AdditionalPackagesFileName);
				}
				TArray<FString> AdditionalPackageList;
				AdditionalPackages.ParseIntoArrayLines(AdditionalPackageList);

				for (const auto& AdditionalPackage : AdditionalPackageList)
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s requested additional package %s to be cooked"), *ChildCooker->BaseResponseFileName, *AdditionalPackage);
					FName Filename = FName(*AdditionalPackage);
					CookServer->RequestPackage(Filename, false);
				}
				ProcessChildLogOutput();

				ChildCooker->ReturnCode = ReturnCode;
				ChildCooker->bFinished = true;
				ChildCooker = nullptr;
				return 1;
			}

			ProcessChildLogOutput();

			FPlatformProcess::Sleep(0.05f);
		}
		return 1;
	}

};

// sue chefs away!!
void UCookOnTheFlyServer::StartChildCookers(int32 NumCookersToSpawn, const TArray<FName>& TargetPlatformNames, const FString& ExtraCmdParams)
{
	SCOPE_TIMER(StartingChildCookers);
	// create a comprehensive list of all the files we need to cook
	// then get the packages with least dependencies and give them to some sue chefs to handle
	
	check(!IsChildCooker());

	// PackageNames contains a sorted list of the packages we want to distribute to child cookers
	TArray<FName> PackageNames;
	// PackageNamesSet is a set of the same information for quick lookup into packagenames array
	TSet<FName> PackageNamesSet;
	PackageNames.Empty(CookRequests.Num());

	// TArray<FFilePlatformRequest>;

	

	for (const auto& CookRequest : CookRequests.GetQueue())
	{
		FString LongPackageName = FPackageName::FilenameToLongPackageName(CookRequest.ToString());

		const FName PackageFName = FName(*LongPackageName);
		PackageNames.Add(PackageFName);
		PackageNamesSet.Add(PackageFName);
	}


	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	int32 PackageCounter = 0;
	while (PackageCounter < PackageNames.Num())
	{
		const auto& PackageName = PackageNames[PackageCounter];
		++PackageCounter;
		TArray<FName> UnfilteredDependencies;
		AssetRegistry.GetDependencies(PackageName, UnfilteredDependencies);

		for (const auto& Dependency : UnfilteredDependencies)
		{
			if (FPackageName::IsScriptPackage(Dependency.ToString()) == false)
			{
				if (PackageNamesSet.Contains(Dependency) == false)
				{
					PackageNamesSet.Add(Dependency);
					PackageNames.Insert(Dependency, PackageCounter);
				}
			}
		}
	}

	TArray<FName> DistributeStandardFilenames;
	for (const auto& DistributeCandidate : PackageNames)
	{
		FText OutReason;
		FString LongPackageName = DistributeCandidate.ToString();
		if (!FPackageName::IsValidLongPackageName(LongPackageName, true, &OutReason))
		{
			const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
				FText::FromString(LongPackageName), OutReason);

			LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
			UE_LOG(LogCook, Warning, TEXT("%s"), *(FailMessage.ToString()));
			continue;
		}
		else if (FPackageName::IsScriptPackage(LongPackageName))
		{
			continue;
		}
		DistributeStandardFilenames.Add(FName(*LongPackageName));
	}

	UE_LOG(LogCook, Display, TEXT("Distributing %d packages to %d cookers for processing"), DistributeStandardFilenames.Num(), NumCookersToSpawn);

	FString TargetPlatformString;
	for (const auto& TargetPlatformName : TargetPlatformNames)
	{
		if (TargetPlatformString.Len() != 0)
		{
			TargetPlatformString += TEXT("+");
		}
		TargetPlatformString += TargetPlatformName.ToString();
	}

	// allocate the memory here, this can't change while we are running the child threads which are handling input because they have a ptr to the childcookers array
	CookByTheBookOptions->ChildCookers.Empty(NumCookersToSpawn);

	// start the child cookers and give them each some distribution candidates
	for (int32 CookerCounter = 0; CookerCounter < NumCookersToSpawn; ++CookerCounter )
	{
		// count our selves as a cooker
		int32 NumFilesForCooker = DistributeStandardFilenames.Num() / ((NumCookersToSpawn+1) - CookerCounter);

		// don't spawn a cooker unless it has a minimum amount of files to do
		if (NumFilesForCooker < 5)
		{
			continue;
		}


		int32 ChildCookerIndex = CookByTheBookOptions->ChildCookers.AddDefaulted(1);
		auto& ChildCooker = CookByTheBookOptions->ChildCookers[ChildCookerIndex];

		ChildCooker.ResponseFileName = FPaths::CreateTempFilename(*(FPaths::GameSavedDir() / TEXT("CookingTemp")));
		ChildCooker.BaseResponseFileName = FPaths::GetBaseFilename(ChildCooker.ResponseFileName);

		// FArchive* ResponseFile = IFileManager::CreateFileWriter(ChildCooker.UniqueTempName);
		FString ResponseFileText;

		for (int32 I = 0; I < NumFilesForCooker; ++I)
		{
			FName PackageFName = DistributeStandardFilenames[I];
			FString PackageName = PackageFName.ToString();

			ResponseFileText += FString::Printf(TEXT("%s%s"), *PackageName, LINE_TERMINATOR);

			// these are long package names
			FName StandardPackageName = GetCachedStandardPackageFileFName(PackageFName);
			if (StandardPackageName == NAME_None)
				continue;
#if 0 // validation code can be disabled
			PackageName = FPackageName::LongPackageNameToFilename(PackageName,FPaths::GetExtension(PackageName));
			FPaths::MakeStandardFilename(PackageName);
			FName PackageFName = FName(*PackageName);
			check(PackageFName== StandardPackageName);
#endif	
			CookedPackages.Add(FFilePlatformCookedPackage(StandardPackageName, TargetPlatformNames, true));
		}
		DistributeStandardFilenames.RemoveAt(0, NumFilesForCooker);

		UE_LOG(LogCook, Display, TEXT("Child cooker %d working on %d files"), CookerCounter, NumFilesForCooker);

		FFileHelper::SaveStringToFile(ResponseFileText, *ChildCooker.ResponseFileName);

		// default commands
		// multiprocess tells unreal in general we shouldn't do things like save ddc, clean shader working directory, and other various multiprocess unsafe things
		FString CommandLine = FString::Printf(TEXT("\"%s\" -run=cook -multiprocess -targetplatform=%s -cookchild=\"%s\" -abslog=\"%sLog.txt\" -childIdentifier=%d %s"), 
			*FPaths::GetProjectFilePath(), *TargetPlatformString, *ChildCooker.ResponseFileName, *ChildCooker.ResponseFileName, CookerCounter, *ExtraCmdParams);

		auto KeepCommandlineValue = [&](const TCHAR* CommandlineToKeep)
		{
			FString CommandlineValue;
			if (FParse::Value(FCommandLine::Get(), CommandlineToKeep, CommandlineValue ))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
				CommandLine += TEXT("=");
				CommandLine += *CommandlineValue;
			}
		};

		auto KeepCommandlineParam = [&](const TCHAR* CommandlineToKeep)
		{
			if (FParse::Param(FCommandLine::Get(), CommandlineToKeep))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
			}
		};


		KeepCommandlineParam(TEXT("NativizeAssets"));
		KeepCommandlineValue(TEXT("ddc="));
		KeepCommandlineParam(TEXT("SkipEditorContent"));
		KeepCommandlineParam(TEXT("compressed"));
		KeepCommandlineParam(TEXT("Unversioned"));
		KeepCommandlineParam(TEXT("buildmachine"));
		KeepCommandlineParam(TEXT("fileopenlog"));
		KeepCommandlineParam(TEXT("stdout"));
		KeepCommandlineParam(TEXT("FORCELOGFLUSH"));
		KeepCommandlineParam(TEXT("CrashForUAT"));
		KeepCommandlineParam(TEXT("AllowStdOutLogVerbosity"));
		KeepCommandlineParam(TEXT("UTF8Output"));

		FString ExecutablePath = FPlatformProcess::ExecutableName(true);

		UE_LOG(LogCook, Display, TEXT("Launching cooker using commandline %s %s"), *ExecutablePath, *CommandLine);

		void* ReadPipe = NULL;
		void* WritePipe = NULL;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		ChildCooker.ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, false, true, true, NULL, 0, NULL, WritePipe);
		ChildCooker.ReadPipe = ReadPipe;

		// start threads to monitor output and finished state
		ChildCooker.Thread = FRunnableThread::Create(new FChildCookerRunnable(&ChildCooker, this), *FString::Printf(TEXT("ChildCookerInputHandleThreadFor:%s"), *ChildCooker.BaseResponseFileName));
	}

}



/* UCookOnTheFlyServer callbacks
 *****************************************************************************/

void UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	// can't use this optimization while cooking in editor
	check(IsCookingInEditor()==false);
	check(IsCookByTheBookMode());

	if (bIsInitializingSandbox)
	{
		return;
	}


	// if the package is already fully loaded then we are not going to mark it up anyway
	if ( Package->IsFullyLoaded() )
	{
		return;
	}

	FName StandardName = GetCachedStandardPackageFileFName(Package);

	// UE_LOG(LogCook, Display, TEXT("Loading package %s"), *StandardName.ToString());

	bool bShouldMarkAsAlreadyProcessed = false;

	TArray<FName> CookedPlatforms;
	if (CookedPackages.GetCookedPlatforms(StandardName, CookedPlatforms))
	{
		bShouldMarkAsAlreadyProcessed = true;
		for (const auto& TargetPlatform : CookByTheBookOptions->TargetPlatformNames)
		{
			if (!CookedPlatforms.Contains(TargetPlatform))
			{
				bShouldMarkAsAlreadyProcessed = false;
				break;
			}
		}


		FString Platforms;
		for (const auto& CookedPlatform : CookedPlatforms)
		{
			Platforms += TEXT(" ");
			Platforms += CookedPlatform.ToString();
		}
		if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
		{
			if (!bShouldMarkAsAlreadyProcessed)
			{
				UE_LOG(LogCook, Display, TEXT("Reloading package %s slowly because it wasn't cooked for all platforms %s."), *StandardName.ToString(), *Platforms);
			}
			else
			{
				UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it's been cooked for platforms %s."), *StandardName.ToString(), *Platforms);
			}
		}
	}

	check(IsInGameThread());
	if (NeverCookPackageList.Contains(StandardName))
	{
		bShouldMarkAsAlreadyProcessed = true;
		UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it was requested as never cook package."), *StandardName.ToString());
	}

	if (bShouldMarkAsAlreadyProcessed)
	{
		if (Package->IsFullyLoaded() == false)
		{
			Package->SetPackageFlags(PKG_ReloadingForCooker);
		}
	}
}


void UCookOnTheFlyServer::HandleNetworkFileServerFileRequest( const FString& Filename, const FString &Platformname, TArray<FString>& UnsolicitedFiles )
{
	check( IsCookOnTheFlyMode() );	
	
	bool bIsCookable = FPackageName::IsPackageExtension(*FPaths::GetExtension(Filename, true));


	FName PlatformFname = FName( *Platformname );

	if (!bIsCookable)
	{
		TArray<FName> UnsolicitedFilenames;
		UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformFname, UnsolicitedFilenames);

		for ( const auto &UnsolicitedFile : UnsolicitedFilenames )
		{	
			FString StandardFilename = UnsolicitedFile.ToString();
			FPaths::MakeStandardFilename( StandardFilename );
			UnsolicitedFiles.Add( StandardFilename );
		}
		UPackage::WaitForAsyncFileWrites();
		return;
	}

	FString StandardFileName = Filename;
	FPaths::MakeStandardFilename( StandardFileName );

	FName StandardFileFname = FName( *StandardFileName );
	TArray<FName> Platforms;
	Platforms.Add( PlatformFname );
	FFilePlatformRequest FileRequest( StandardFileFname, Platforms);
	CookRequests.EnqueueUnique(FileRequest, true);

	do
	{
		FPlatformProcess::Sleep(0.0f);
	}
	while (!CookedPackages.Exists(FileRequest));

	UE_LOG( LogCook, Display, TEXT("Cook complete %s"), *FileRequest.GetFilename().ToString())

	TArray<FName> UnsolicitedFilenames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformFname, UnsolicitedFilenames);
	UnsolicitedFilenames.Remove(FileRequest.GetFilename());

	for ( const auto &UnsolicitedFile : UnsolicitedFilenames )
	{	
		FString StandardFilename = UnsolicitedFile.ToString();
		FPaths::MakeStandardFilename( StandardFilename );
		UnsolicitedFiles.Add( StandardFilename );
	}

	UPackage::WaitForAsyncFileWrites();


#if DEBUG_COOKONTHEFLY
	UE_LOG( LogCook, Display, TEXT("Processed file request %s"), *Filename );
#endif

}


void UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders(const FShaderRecompileData& RecompileData)
{
	// shouldn't receive network requests unless we are in cook on the fly mode
	check( IsCookOnTheFlyMode() );
	check( !IsCookingDLC() );
	// if we aren't in the game thread, we need to push this over to the game thread and wait for it to finish
	if (!IsInGameThread())
	{
		UE_LOG(LogCook, Display, TEXT("Got a recompile request on non-game thread"));

		// make a new request
		FRecompileRequest* Request = new FRecompileRequest;
		Request->RecompileData = RecompileData;
		Request->bComplete = false;

		// push the request for the game thread to process
		RecompileRequests.Enqueue(Request);

		// wait for it to complete (the game thread will pull it out of the TArray, but I will delete it)
		while (!Request->bComplete)
		{
			FPlatformProcess::Sleep(0);
		}
		delete Request;
		UE_LOG(LogCook, Display, TEXT("Completed recompile..."));

		// at this point, we are done on the game thread, and ModifiedFiles will have been filled out
		return;
	}

	FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

	RecompileShadersForRemote
		(RecompileData.PlatformName, 
		RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
		OutputDir, 
		RecompileData.MaterialsToLoad, 
		RecompileData.SerializedShaderResources, 
		RecompileData.MeshMaterialMaps, 
		RecompileData.ModifiedFiles,
		RecompileData.bCompileChangedShaders);
}

bool UCookOnTheFlyServer::GetAllPackagesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageNames ) const
	{

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *AssetRegistryPath))
		{
		TMap<FName, FAssetData*> RegistryDataMap;
		AssetRegistry.LoadRegistryData(SerializedAssetData, RegistryDataMap);


		for (const auto& RegistryData : RegistryDataMap)
		{
			const auto& NewAssetData = RegistryData.Value;
			FName CachedPackageFileFName = GetCachedStandardPackageFileFName(NewAssetData->ObjectPath);
			if (CachedPackageFileFName != NAME_None)
			{
				OutPackageNames.Add(CachedPackageFileFName);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Could not resolve package %s from %s"), *NewAssetData->ObjectPath.ToString(), *AssetRegistryPath);
			}
		}


		// clean up memory
		for (auto RegistryData : RegistryDataMap)
		{
			delete RegistryData.Value;
		}
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

