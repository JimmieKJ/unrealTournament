// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#include "UnrealEd.h"

#include "Blueprint/BlueprintSupport.h"
#include "BlueprintNativeCodeGenModule.h"
#include "Engine/WorldComposition.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "ISourceControlModule.h"
#include "GlobalShader.h"
#include "TargetPlatform.h"
#include "IConsoleManager.h"
#include "Developer/PackageDependencyInfo/Public/PackageDependencyInfo.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Messaging.h"
#include "NetworkFileSystem.h"
#include "AssetRegistryModule.h"
#include "UnrealEdMessages.h"
#include "GameDelegates.h"
#include "ChunkManifestGenerator.h"
#include "CookerSettings.h"
#include "ShaderCompiler.h"
#include "MemoryMisc.h"
#include "CookStats.h"
#include "DerivedDataCacheUsageStats.h"

DEFINE_LOG_CATEGORY_STATIC(LogCookCommandlet, Log, All);

#if ENABLE_COOK_STATS
#include "ScopedTimers.h"
#include "AnalyticsET.h"
#include "IAnalyticsProviderET.h"

namespace DetailedCookStats
{
	FString CookProject;
	FString TargetPlatforms;
	double CookWallTimeSec = 0.0;
	double StartupWallTimeSec = 0.0;
	double NewCookTimeSec = 0.0;
	double StartCookByTheBookTimeSec = 0.0;
	extern double TickCookOnTheSideTimeSec;
	extern double TickCookOnTheSideLoadPackagesTimeSec;
	extern double TickCookOnTheSideResolveRedirectorsTimeSec;
	extern double TickCookOnTheSideSaveCookedPackageTimeSec;
	extern double TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec;
	extern double TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec;
	double TickLoopGCTimeSec = 0.0;
	double TickLoopRecompileShaderRequestsTimeSec = 0.0;
	double TickLoopShaderProcessAsyncResultsTimeSec = 0.0;
	double TickLoopProcessDeferredCommandsTimeSec = 0.0;
	double TickLoopTickCommandletStatsTimeSec = 0.0;

	FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		const FString StatName(TEXT("Cook.Profile"));
		TArray<FCookStatsManager::StringKeyValue> Attrs;
		#define ADD_COOK_STAT_FLT(Path, Name) AddStat(StatName, FCookStatsManager::CreateKeyValueArray(TEXT("Path"), TEXT(Path), TEXT(#Name), Name))
		ADD_COOK_STAT_FLT(" 0", CookWallTimeSec);
		ADD_COOK_STAT_FLT(" 0. 0", StartupWallTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1", NewCookTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 0", StartCookByTheBookTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1", TickCookOnTheSideTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 0", TickCookOnTheSideLoadPackagesTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 1", TickCookOnTheSideSaveCookedPackageTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 1. 0", TickCookOnTheSideResolveRedirectorsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 2", TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 3", TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 2", TickLoopGCTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 3", TickLoopRecompileShaderRequestsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 4", TickLoopShaderProcessAsyncResultsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 5", TickLoopProcessDeferredCommandsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 6", TickLoopTickCommandletStatsTimeSec);
		#undef ADD_COOK_STAT_FLT
	});

	/** Gathers the cook stats registered with the FCookStatsManager delegate and logs them as a CSV. */
	static void LogCookStats(const FString& CookCmdLine)
	{
		// Optionally create an analytics provider to send stats to for central collection.
		if (GIsBuildMachine || FParse::Param(FCommandLine::Get(), TEXT("SendCookAnalytics")))
		{
			FString APIServerET;
			// This value is set by an INI private to Epic.
			if (GConfig->GetString(TEXT("CookAnalytics"), TEXT("APIServer"), APIServerET, GEngineIni))
			{
				TSharedPtr<IAnalyticsProviderET> CookAnalytics = FAnalyticsET::Get().CreateAnalyticsProvider(FAnalyticsET::Config(TEXT("Cook"), APIServerET, FString(), true));
				if (CookAnalytics.IsValid())
				{
					// start the session
					CookAnalytics->SetUserID(FString(FPlatformProcess::ComputerName()) + FString(TEXT("\\")) + FString(FPlatformProcess::UserName(false)));
					CookAnalytics->StartSession(MakeAnalyticsEventAttributeArray(
						TEXT("Project"), CookProject,
						TEXT("CmdLine"), CookCmdLine,
						TEXT("IsBuildMachine"), GIsBuildMachine,
						TEXT("TargetPlatforms"), TargetPlatforms
					));
					// Sends each cook stat to the analytics provider.
					auto SendCookStatsToAnalytics = [CookAnalytics](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
					{
						// convert all stats directly to an analytics event
						TArray<FAnalyticsEventAttribute> StatAttrs;
						StatAttrs.Reset(StatAttributes.Num());
						for (const auto& Attr : StatAttributes)
						{
							StatAttrs.Emplace(Attr.Key, Attr.Value);
						}
						CookAnalytics->RecordEvent(StatName, StatAttrs);
					};
					FCookStatsManager::LogCookStats(SendCookStatsToAnalytics);
				}
			}
		}

		/** Used for custom logging of DDC Resource usage stats. */
		struct FDDCResourceUsageStat
		{
		public:
			FDDCResourceUsageStat(FString InAssetType, double InTotalTimeSec, bool bIsGameThreadTime, double InSizeMB, int64 InAssetsBuilt) : AssetType(MoveTemp(InAssetType)), TotalTimeSec(InTotalTimeSec), GameThreadTimeSec(bIsGameThreadTime ? InTotalTimeSec : 0.0), SizeMB(InSizeMB), AssetsBuilt(InAssetsBuilt) {}
			void Accumulate(const FDDCResourceUsageStat& OtherStat)
			{
				TotalTimeSec += OtherStat.TotalTimeSec;
				GameThreadTimeSec += OtherStat.GameThreadTimeSec;
				SizeMB += OtherStat.SizeMB;
				AssetsBuilt += OtherStat.AssetsBuilt;
			}
			FString AssetType;
			double TotalTimeSec;
			double GameThreadTimeSec;
			double SizeMB;
			int64 AssetsBuilt;
		};

		/** Used for custom TSet comparison of DDC Resource usage stats. */
		struct FDDCResourceUsageStatKeyFuncs : BaseKeyFuncs<FDDCResourceUsageStat, FString, false>
		{
			static const FString& GetSetKey(const FDDCResourceUsageStat& Element) { return Element.AssetType; }
			static bool Matches(const FString& A, const FString& B) { return A == B; }
			static uint32 GetKeyHash(const FString& Key) { return GetTypeHash(Key); }
		};

		/** Used to store profile data for custom logging. */
		struct FCookProfileData
		{
		public:
			FCookProfileData(FString InPath, FString InKey, FString InValue) : Path(MoveTemp(InPath)), Key(MoveTemp(InKey)), Value(MoveTemp(InValue)) {}
			FString Path;
			FString Key;
			FString Value;
		};

		// instead of printing the usage stats generically, we capture them so we can log a subset of them in an easy-to-read way.
		TSet<FDDCResourceUsageStat, FDDCResourceUsageStatKeyFuncs> DDCResourceUsageStats;
		TArray<FCookStatsManager::StringKeyValue> DDCSummaryStats;
		TArray<FCookProfileData> CookProfileData;

		/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
		auto LogStatsFunc = [&DDCResourceUsageStats, &DDCSummaryStats, &CookProfileData](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
		{
			// Some stats will use custom formatting to make a visibly pleasing summary.
			bool bStatUsedCustomFormatting = false;

			if (StatName == TEXT("DDC.Usage"))
			{
				// Don't even log this detailed DDC data. It's mostly only consumable by ingestion into pivot tools.
				bStatUsedCustomFormatting = true;
			}
			else if (StatName.EndsWith(TEXT(".Usage"), ESearchCase::IgnoreCase))
			{
				// Anything that ends in .Usage is assumed to be an instance of FCookStats.FDDCResourceUsageStats. We'll log that using custom formatting.
				FString AssetType = StatName;
				AssetType.RemoveFromEnd(TEXT(".Usage"), ESearchCase::IgnoreCase);
				// See if the asset has a subtype (found via the "Node" parameter")
				const FCookStatsManager::StringKeyValue* AssetSubType = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Node"); });
				if (AssetSubType && AssetSubType->Value.Len() > 0)
				{
					AssetType += FString::Printf(TEXT(" (%s)"), *AssetSubType->Value);
				}
				// Pull the Time and Size attributes and AddOrAccumulate them into the set of stats. Ugly string/container manipulation code courtesy of UE4/C++.
				const FCookStatsManager::StringKeyValue* AssetTimeSecAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("TimeSec"); });
				double AssetTimeSec = 0.0;
				if (AssetTimeSecAttr)
				{
					LexicalConversion::FromString(AssetTimeSec, *AssetTimeSecAttr->Value);
				}
				const FCookStatsManager::StringKeyValue* AssetSizeMBAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("MB"); });
				double AssetSizeMB = 0.0;
				if (AssetSizeMBAttr)
				{
					LexicalConversion::FromString(AssetSizeMB, *AssetSizeMBAttr->Value);
				}
				const FCookStatsManager::StringKeyValue* ThreadNameAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("ThreadName"); });
				bool bIsGameThreadTime = ThreadNameAttr != nullptr && ThreadNameAttr->Value == TEXT("GameThread");

				const FCookStatsManager::StringKeyValue* HitOrMissAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("HitOrMiss"); });
				bool bWasMiss = HitOrMissAttr != nullptr && HitOrMissAttr->Value == TEXT("Miss");
				int64 AssetsBuilt = 0;
				if (bWasMiss)
				{
					const FCookStatsManager::StringKeyValue* CountAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Count"); });
					if (CountAttr)
					{
						LexicalConversion::FromString(AssetsBuilt, *CountAttr->Value);
					}
				}


				FDDCResourceUsageStat Stat(AssetType, AssetTimeSec, bIsGameThreadTime, AssetSizeMB, AssetsBuilt);
				FDDCResourceUsageStat* ExistingStat = DDCResourceUsageStats.Find(Stat.AssetType);
				if (ExistingStat)
				{
					ExistingStat->Accumulate(Stat);
				}
				else
				{
					DDCResourceUsageStats.Add(Stat);
				}
				bStatUsedCustomFormatting = true;
			}
			else if (StatName == TEXT("DDC.Summary"))
			{
				DDCSummaryStats = StatAttributes;
				bStatUsedCustomFormatting = true;
			}
			else if (StatName == TEXT("Cook.Profile"))
			{
				if (StatAttributes.Num() >= 2)
				{
					CookProfileData.Emplace(StatAttributes[0].Value, StatAttributes[1].Key, StatAttributes[1].Value);
				}
				bStatUsedCustomFormatting = true;
			}

			// if a stat doesn't use custom formatting, just spit out the raw info.
			if (!bStatUsedCustomFormatting)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%s"), *StatName);
				// log each key/value pair, with the equal signs lined up.
				for (const auto& Attr : StatAttributes)
				{
					UE_LOG(LogCookCommandlet, Display, TEXT("    %s=%s"), *Attr.Key, *Attr.Value);
				}
			}
		};

		UE_LOG(LogCookCommandlet, Display, TEXT("Misc Cook Stats"));
		UE_LOG(LogCookCommandlet, Display, TEXT("==============="));
		FCookStatsManager::LogCookStats(LogStatsFunc);

		// DDC Usage stats are custom formatted, and the above code just accumulated them into a TSet. Now log it with our special formatting for readability.
		if (CookProfileData.Num() > 0)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("Cook Profile"));
			UE_LOG(LogCookCommandlet, Display, TEXT("============"));
			for (const auto& ProfileEntry : CookProfileData)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%s.%s=%s"), *ProfileEntry.Path, *ProfileEntry.Key, *ProfileEntry.Value);
			}
		}
		if (DDCSummaryStats.Num() > 0)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("DDC Summary Stats"));
			UE_LOG(LogCookCommandlet, Display, TEXT("================="));
			for (const auto& Attr : DDCSummaryStats)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%-14s=%10s"), *Attr.Key, *Attr.Value);
			}
		}
		if (DDCResourceUsageStats.Num() > 0)
		{
			// sort the list
			TArray<FDDCResourceUsageStat> SortedDDCResourceUsageStats;
			SortedDDCResourceUsageStats.Empty(DDCResourceUsageStats.Num());
			for (const FDDCResourceUsageStat& Stat : DDCResourceUsageStats)
			{
				SortedDDCResourceUsageStats.Emplace(Stat);
			}
			SortedDDCResourceUsageStats.Sort([](const FDDCResourceUsageStat& LHS, const FDDCResourceUsageStat& RHS)
			{
				return LHS.TotalTimeSec > RHS.TotalTimeSec;
			});

			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("DDC Resource Stats"));
			UE_LOG(LogCookCommandlet, Display, TEXT("======================================================================================================="));
			UE_LOG(LogCookCommandlet, Display, TEXT("Asset Type                          Total Time (Sec)  GameThread Time (Sec)  Assets Built  MB Processed"));
			UE_LOG(LogCookCommandlet, Display, TEXT("----------------------------------  ----------------  ---------------------  ------------  ------------"));
			for (const FDDCResourceUsageStat& Stat : SortedDDCResourceUsageStats)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%-34s  %16.2f  %21.2f  %12d  %12.2f"), *Stat.AssetType, Stat.TotalTimeSec, Stat.GameThreadTimeSec, Stat.AssetsBuilt, Stat.SizeMB);
			}
		}
	}
}
#endif

UCookerSettings::UCookerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCompileBlueprintsInDevelopmentMode(true)
{
	SectionName = TEXT("Cooker");
	DefaultPVRTCQuality = 1;
	DefaultASTCQualityBySize = 3;
	DefaultASTCQualityBySpeed = 3;
}


/* Static functions
 *****************************************************************************/

static FString GetPackageFilename( UPackage* Package )
{
	FString Filename;
	if (FPackageName::DoesPackageExist(Package->GetName(), NULL, &Filename))
	{
		Filename = FPaths::ConvertRelativePathToFull(Filename);
		FPaths::RemoveDuplicateSlashes(Filename);
	}
	return Filename;
}


/* UCookCommandlet structors
 *****************************************************************************/

UCookCommandlet::UCookCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

	LogToConsole = false;
}


/* UCookCommandlet interface
 *****************************************************************************/

bool UCookCommandlet::CookOnTheFly( FGuid InstanceId, int32 Timeout, bool bForceClose )
{
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();

	struct FScopeRootObject
	{
		UObject *Object;
		FScopeRootObject( UObject *InObject ) : Object( InObject )
		{
			Object->AddToRoot();
		}

		~FScopeRootObject()
		{
			Object->RemoveFromRoot();
		}
	};

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	FScopeRootObject S(CookOnTheFlyServer);

	ECookInitializationFlags CookFlags = ECookInitializationFlags::None;
	CookFlags |= bCompressed ? ECookInitializationFlags::Compressed : ECookInitializationFlags::None;
	CookFlags |= bIterativeCooking ? ECookInitializationFlags::Iterative : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookOnTheFlyServer->Initialize( ECookMode::CookOnTheFly, CookFlags );

	bool BindAnyPort = InstanceId.IsValid();

	if ( CookOnTheFlyServer->StartNetworkFileServer(BindAnyPort) == false )
	{
		return false;
	}

	if ( InstanceId.IsValid() )
	{
		if ( CookOnTheFlyServer->BroadcastFileserverPresence(InstanceId) == false )
		{
			return false;
		}
	}

	// Garbage collection should happen when either
	//	1. We have cooked a map (configurable asset type)
	//	2. We have cooked non-map packages and...
	//		a. we have accumulated 50 (configurable) of these since the last GC.
	//		b. we have been idle for 20 (configurable) seconds.
	bool bShouldGC = true;

	// megamoth
	uint32 NonMapPackageCountSinceLastGC = 0;
	
	const uint32 PackagesPerGC = CookOnTheFlyServer->GetPackagesPerGC();
	const double IdleTimeToGC = CookOnTheFlyServer->GetIdleTimeToGC();
	const uint64 MaxMemoryAllowance = CookOnTheFlyServer->GetMaxMemoryAllowance();

	double LastCookActionTime = FPlatformTime::Seconds();

	FDateTime LastConnectionTime = FDateTime::UtcNow();
	bool bHadConnection = false;

	bool bCookedAMapSinceLastGC = false;
	while (!GIsRequestingExit)
	{
		uint32 TickResults = 0;
		static const float CookOnTheSideTimeSlice = 10.0f;
		TickResults = CookOnTheFlyServer->TickCookOnTheSide(CookOnTheSideTimeSlice, NonMapPackageCountSinceLastGC);

		bCookedAMapSinceLastGC |= ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC) != 0);
		if ( TickResults & (UCookOnTheFlyServer::COSR_CookedMap | UCookOnTheFlyServer::COSR_CookedPackage | UCookOnTheFlyServer::COSR_WaitingOnCache))
		{
			LastCookActionTime = FPlatformTime::Seconds();
		}

		if (NonMapPackageCountSinceLastGC > 0)
		{
			if ((PackagesPerGC > 0) && (NonMapPackageCountSinceLastGC > PackagesPerGC))
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has exceeded max number of non map packages since last gc"));
				bShouldGC |= true;
			}

			if ((IdleTimeToGC > 0) && ((FPlatformTime::Seconds() - LastCookActionTime) >= IdleTimeToGC))
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has been idle for long time gc"));
				bShouldGC |= true;
			}
		}

		if ( bCookedAMapSinceLastGC )
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Cooker cooked a map since last gc collecting garbage"));
			bShouldGC |= true;
		}

		if ( !bShouldGC && CookOnTheFlyServer->HasExceededMaxMemory() )
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has exceeded max memory usage collecting garbage"));
			bShouldGC |= true;
		}

		// we don't want to gc if we are waiting on cache of objects. this could clean up objects which we will need to reload next frame
		if (bShouldGC && ((TickResults & UCookOnTheFlyServer::COSR_WaitingOnCache)==0) )
		{
			bShouldGC = false;
			bCookedAMapSinceLastGC = false;
			NonMapPackageCountSinceLastGC = 0;

			UE_LOG(LogCookCommandlet, Display, TEXT("GC..."));

			CollectGarbage(RF_NoFlags);
		}


		// force at least a tick shader compilation even if we are requesting stuff
		CookOnTheFlyServer->TickRecompileShaderRequests();
		GShaderCompilingManager->ProcessAsyncResults(true, false);


		while ( (CookOnTheFlyServer->HasCookRequests() == false) && !GIsRequestingExit)
		{
			CookOnTheFlyServer->TickRecompileShaderRequests();

			// Shaders need to be updated
			GShaderCompilingManager->ProcessAsyncResults(true, false);

			ProcessDeferredCommands();

			// handle server timeout
			if (InstanceId.IsValid() || bForceClose)
			{
				if (CookOnTheFlyServer->NumConnections() > 0)
				{
					bHadConnection = true;
					LastConnectionTime = FDateTime::UtcNow();
				}

				if ((FDateTime::UtcNow() - LastConnectionTime) > FTimespan::FromSeconds(Timeout))
				{
					uint32 Result = FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "FileServerIdle", "The file server did not receive any connections in the past 3 minutes. Would you like to shut it down?"));

					if (Result == EAppReturnType::No && !bForceClose)
					{
						LastConnectionTime = FDateTime::UtcNow();
					}
					else
					{
						GIsRequestingExit = true;
					}
				}
				else if (bHadConnection && (CookOnTheFlyServer->NumConnections() == 0) && bForceClose) // immediately shut down if we previously had a connection and now do not
				{
					GIsRequestingExit = true;
				}
			}
		}
	}

	CookOnTheFlyServer->EndNetworkFileServer();
	return true;
}


FString UCookCommandlet::GetOutputDirectory( const FString& PlatformName ) const
{
	// Use SandboxFile to get the correct sandbox directory.
	FString OutputDirectory = SandboxFile->GetSandboxDirectory();

	return OutputDirectory.Replace(TEXT("[Platform]"), *PlatformName);
}


bool UCookCommandlet::GetPackageTimestamp( const FString& InFilename, FDateTime& OutDateTime )
{
	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
	FDateTime DependentTime;

	if (PDInfoModule.DeterminePackageDependentTimeStamp(*InFilename, DependentTime) == true)
	{
		OutDateTime = DependentTime;

		return true;
	}

	return false;
}

bool UCookCommandlet::ShouldCook(const FString& InFileName, const FString &InPlatformName)
{
	bool bDoCook = false;


	FString PkgFile;
	FString PkgFilename;
	FDateTime DependentTimeStamp = FDateTime::MinValue();

	if (bIterativeCooking && FPackageName::DoesPackageExist(InFileName, NULL, &PkgFile))
	{
		PkgFilename = PkgFile;

		if (GetPackageTimestamp(FPaths::GetBaseFilename(PkgFilename, false), DependentTimeStamp) == false)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Failed to find dependency timestamp for: %s"), *PkgFilename);
		}
	}

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	PkgFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*PkgFilename);

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

	static const TArray<ITargetPlatform*> &ActiveTargetPlatforms = TPM.GetActiveTargetPlatforms();

	TArray<ITargetPlatform*> Platforms;

	if ( InPlatformName.Len() > 0 )
	{
		Platforms.Add( TPM.FindTargetPlatform( InPlatformName ) );
	}
	else 
	{
		Platforms = ActiveTargetPlatforms;
	}

	for (int32 Index = 0; Index < Platforms.Num() && !bDoCook; Index++)
	{
		ITargetPlatform* Target = Platforms[Index];
		FString PlatFilename = PkgFilename.Replace(TEXT("[Platform]"), *Target->PlatformName());

		// If we are not iterative cooking, then cook the package
		bool bCookPackage = (bIterativeCooking == false);

		if (bCookPackage == false)
		{
			// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
			FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
			int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
			bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
		}
		bDoCook |= bCookPackage;
	}

	return bDoCook;
}


bool UCookCommandlet::SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate ) 
{
	TArray<FString> TargetPlatformNames; 
	return SaveCookedPackage( Package, SaveFlags, bOutWasUpToDate, TargetPlatformNames );
}

bool UCookCommandlet::SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FString> &TargetPlatformNames )
{
	bool bSavedCorrectly = true;

	FString Filename(GetPackageFilename(Package));

	if (Filename.Len())
	{
		FString PkgFilename;
		FDateTime DependentTimeStamp = FDateTime::MinValue();

		// We always want to use the dependent time stamp when saving a cooked package...
		// Iterative or not!
		FString PkgFile;
		FString Name = Package->GetPathName();

		if (bIterativeCooking && FPackageName::DoesPackageExist(Name, NULL, &PkgFile))
		{
			PkgFilename = PkgFile;

			if (GetPackageTimestamp(FPaths::GetBaseFilename(PkgFilename, false), DependentTimeStamp) == false)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Failed to find dependency timestamp for: %s"), *PkgFilename);
			}
		}

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		Filename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*Filename);

		uint32 OriginalPackageFlags = Package->GetPackageFlags();
		UWorld* World = NULL;
		EObjectFlags Flags = RF_NoFlags;
		bool bPackageFullyLoaded = false;

		if (bCompressed)
		{
			Package->SetPackageFlags(PKG_StoreCompressed);
		}

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

		static TArray<ITargetPlatform*> ActiveStartupPlatforms = TPM.GetActiveTargetPlatforms();

		TArray<ITargetPlatform*> Platforms;

		if ( TargetPlatformNames.Num() )
		{
			const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();

			for (const FString &TargetPlatformName : TargetPlatformNames)
			{
				for (ITargetPlatform *TargetPlatform : TargetPlatforms)
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

			for ( int Index = 0; Index < Platforms.Num(); ++Index )
			{
				TargetPlatformNames.Add( Platforms[Index]->PlatformName() );
			}
		}
		

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			ITargetPlatform* Target = Platforms[Index];
			FString PlatFilename = Filename.Replace(TEXT("[Platform]"), *Target->PlatformName());

			// If we are not iterative cooking, then cook the package
			bool bCookPackage = (bIterativeCooking == false);

			if (bCookPackage == false)
			{
				// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
				FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
				int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
				bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
			}

			// don't save Editor resources from the Engine if the target doesn't have editoronly data
			if (bSkipEditorContent && 
				( Name.StartsWith(TEXT("/Engine/Editor")) || Name.StartsWith(TEXT("/Engine/VREditor"))) && 
				!Target->HasEditorOnlyData())
			{
				bCookPackage = false;
			}

			if (bCookPackage == true)
			{
				if (bPackageFullyLoaded == false)
				{
					Package->FullyLoad();
					if (!Package->IsFullyLoaded())
					{
						UE_LOG(LogCookCommandlet, Warning, TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"), 
							*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set"));
					}
					bPackageFullyLoaded = true;

					// If fully loading has caused a blueprint to be regenerated, make sure we eliminate all meta data outside the package
					UMetaData* MetaData = Package->GetMetaData();
					MetaData->RemoveMetaDataOutsidePackage();

					// look for a world object in the package (if there is one, there's a map)
					World = UWorld::FindWorldInPackage(Package);
					Flags = World ? RF_NoFlags : RF_Standalone;
				}

				UE_LOG(LogCookCommandlet, Display, TEXT("Cooking %s -> %s"), *Package->GetName(), *PlatFilename);

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
					World->PersistentLevel->OwningWorld = World;
				}

				const FString FullFilename = FPaths::ConvertRelativePathToFull( PlatFilename );
				if( FullFilename.Len() >= PLATFORM_MAX_FILEPATH_LENGTH )
				{
					UE_LOG( LogCookCommandlet, Error, TEXT( "Couldn't save package, filename is too long :%s" ), *FullFilename );
					bSavedCorrectly = false;
				}
				else
				{
					ESavePackageResult Result = GEditor->Save(Package, World, Flags, *PlatFilename, GError, NULL, bSwap, false, SaveFlags, Target, FDateTime::MinValue());
					IBlueprintNativeCodeGenModule::Get().Convert(Package, Result, *(Target->PlatformName()));
					bSavedCorrectly &= (Result == ESavePackageResult::ReplaceCompletely || Result == ESavePackageResult::GenerateStub || Result == ESavePackageResult::Success);
				}
				
				bOutWasUpToDate = false;
			}
			else
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Up to date: %s"), *PlatFilename);

				bOutWasUpToDate = true;
			}
		}

		Package->SetPackageFlagsTo(OriginalPackageFlags);
	}

	// return success
	return bSavedCorrectly;
}

void UCookCommandlet::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	FString Name = Package->GetName();
	if (PackagesToNotReload.Contains(Name))
	{
		UE_LOG(LogCookCommandlet, Verbose, TEXT("Marking %s already loaded."), *Name);
		Package->SetPackageFlags(PKG_ReloadingForCooker);
	}
}

/* UCommandlet interface
 *****************************************************************************/

int32 UCookCommandlet::Main(const FString& CmdLineParams)
{
	COOK_STAT(double CookStartTime = FPlatformTime::Seconds());
	Params = CmdLineParams;
	ParseCommandLine(*Params, Tokens, Switches);

	bCookOnTheFly = Switches.Contains(TEXT("COOKONTHEFLY"));   // Prototype cook-on-the-fly server
	bCookAll = Switches.Contains(TEXT("COOKALL"));   // Cook everything
	bLeakTest = Switches.Contains(TEXT("LEAKTEST"));   // Test for UObject leaks
	bUnversioned = Switches.Contains(TEXT("UNVERSIONED"));   // Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes.
	bGenerateStreamingInstallManifests = Switches.Contains(TEXT("MANIFESTS"));   // Generate manifests for building streaming install packages
	bCompressed = Switches.Contains(TEXT("COMPRESSED"));
	bIterativeCooking = Switches.Contains(TEXT("ITERATE"));
	bSkipEditorContent = Switches.Contains(TEXT("SKIPEDITORCONTENT")); // This won't save out any packages in Engine/COntent/Editor*
	bErrorOnEngineContentUse = Switches.Contains(TEXT("ERRORONENGINECONTENTUSE"));
	bUseSerializationForGeneratingPackageDependencies = Switches.Contains(TEXT("UseSerializationForGeneratingPackageDependencies"));
	bCookSinglePackage = Switches.Contains(TEXT("cooksinglepackage"));
	bVerboseCookerWarnings = Switches.Contains(TEXT("verbosecookerwarnings"));
	bPartialGC = Switches.Contains(TEXT("Partialgc"));
	if (bLeakTest)
	{
		for (FObjectIterator It; It; ++It)
		{
			LastGCItems.Add(FWeakObjectPtr(*It));
		}
	}

	COOK_STAT(DetailedCookStats::CookProject = FApp::GetGameName());

	if ( bCookOnTheFly )
	{
		// parse instance identifier
		FString InstanceIdString;
		bool bForceClose = Switches.Contains(TEXT("FORCECLOSE"));

		FGuid InstanceId;
		if (FParse::Value(*Params, TEXT("InstanceId="), InstanceIdString))
		{
			if (!FGuid::Parse(InstanceIdString, InstanceId))
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
			}
		}

		int32 Timeout = 180;
		if (!FParse::Value(*Params, TEXT("timeout="), Timeout))
		{
			Timeout = 180;
		}

		CookOnTheFly( InstanceId, Timeout, bForceClose);
	}
	else
	{
		
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const TArray<ITargetPlatform*>& Platforms = TPM.GetActiveTargetPlatforms();

		TArray<FString> FilesInPath;
		// new cook is better 
		if ( Switches.Contains(TEXT("OLDCOOK")))
		{
			Cook(Platforms, FilesInPath);	
		}
		else
		{
			NewCook(Platforms, FilesInPath );
		}

		// Use -LogCookStats to log the results to the command line after the cook (happens automatically on a build machine)
		COOK_STAT(
		{
			double Now = FPlatformTime::Seconds();
			DetailedCookStats::CookWallTimeSec = Now - GStartTime;
			DetailedCookStats::StartupWallTimeSec = CookStartTime - GStartTime;
			DetailedCookStats::LogCookStats(CmdLineParams);
		});
	}
	
	return 0;
}


/* UCookCommandlet implementation
 *****************************************************************************/

FString UCookCommandlet::GetOutputDirectoryOverride() const
{
	FString OutputDirectory;
	// Output directory override.	
	if (!FParse::Value(*Params, TEXT("Output="), OutputDirectory))
	{
		// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
		OutputDirectory = FPaths::Combine(*FPaths::GameDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
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

void UCookCommandlet::CleanSandbox(const TArray<ITargetPlatform*>& Platforms)
{
	double SandboxCleanTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(SandboxCleanTime);

		if (bIterativeCooking == false)
		{
			// for now we are going to wipe the cooked directory
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				FString SandboxDirectory = GetOutputDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);
			}
		}
		else
		{
			FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
			
			// list of directories to skip
			TArray<FString> DirectoriesToSkip;
			TArray<FString> DirectoriesToNotRecurse;

			// See what files are out of date in the sandbox folder
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				FString SandboxDirectory = GetOutputDirectory(Target->PlatformName());

				// use the timestamp grabbing visitor
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);
			
				PlatformFile.IterateDirectory(*SandboxDirectory, Visitor);

				for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
				{
					FString CookedFilename = TimestampIt.Key();
					FDateTime CookedTimestamp = TimestampIt.Value();
					FString StandardCookedFilename = CookedFilename.Replace(*SandboxDirectory, *(FPaths::GetRelativePathToRoot()));
					FDateTime DependentTimestamp;

					if (PDInfoModule.DeterminePackageDependentTimeStamp(*(FPaths::GetBaseFilename(StandardCookedFilename, false)), DependentTimestamp) == true)
					{
						double Diff = (CookedTimestamp - DependentTimestamp).GetTotalSeconds();

						if (Diff < 0.0)
						{
							UE_LOG(LogCookCommandlet, Display, TEXT("Deleting out of date cooked file: %s"), *CookedFilename);

							IFileManager::Get().Delete(*CookedFilename);
						}
					}
				}
			}

			// Collect garbage to ensure we don't have any packages hanging around from dependent time stamp determination
			CollectGarbage(RF_NoFlags);
		}
	}

	UE_LOG(LogCookCommandlet, Display, TEXT("Sandbox cleanup took %5.3f seconds"), SandboxCleanTime);
}

void UCookCommandlet::GenerateAssetRegistry(const TArray<ITargetPlatform*>& Platforms)
{
	// load the interface
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	double GenerateAssetRegistryTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(GenerateAssetRegistryTime);
		UE_LOG(LogCookCommandlet, Display, TEXT("Populating asset registry."));

		// Perform a synchronous search of any .ini based asset paths (note that the per-game delegate may
		// have already scanned paths on its own)
		// We want the registry to be fully initialized when generating streaming manifests too.
		TArray<FString> ScanPaths;
		if (GConfig->GetArray(TEXT("AssetRegistry"), TEXT("PathsToScanForCook"), ScanPaths, GEngineIni) > 0)
		{
			AssetRegistry.ScanPathsSynchronous(ScanPaths);
		}
		else
		{
			AssetRegistry.SearchAllAssets(true);
		}
		
		// When not cooking on the fly the registry will be saved after the cooker has finished
		if (bCookOnTheFly)
		{
			// write it out to a memory archive
			FArrayWriter SerializedAssetRegistry;
			SerializedAssetRegistry.SetFilterEditorOnly(true);
			AssetRegistry.Serialize(SerializedAssetRegistry);
			UE_LOG(LogCookCommandlet, Display, TEXT("Generated asset registry size is %5.2fkb"), (float)SerializedAssetRegistry.Num() / 1024.f);

			// now save it in each cooked directory
			FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
			// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
			FString SandboxFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*RegistryFilename);

			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				FString PlatFilename = SandboxFilename.Replace(TEXT("[Platform]"), *Platforms[Index]->PlatformName());
				FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatFilename);
			}
		}
	}
	UE_LOG(LogCookCommandlet, Display, TEXT("Done populating registry. It took %5.2fs."), GenerateAssetRegistryTime);
	
}

void UCookCommandlet::SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms)
{
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

		FString OutputDir = GetOutputDirectory(RecompileData.PlatformName);

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

void UCookCommandlet::CollectFilesToCook(TArray<FString>& FilesInPath)
{
	TArray<FString> MapList;

	// Add the default map section
	GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), MapList);

	// Add any map sections specified on command line
	GEditor->ParseMapSectionIni(*Params, MapList);
	for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
	{
		AddFileToCook(FilesInPath, MapList[MapIdx]);
	}

	TArray<FString> CmdLineMapEntries;
	TArray<FString> CmdLineDirEntries;
	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		const FString& Switch = Switches[SwitchIdx];

		auto GetSwitchValueElements = [&Switch](const FString SwitchKey) -> TArray<FString>
		{
			TArray<FString> ValueElements;
			if (Switch.StartsWith(SwitchKey + TEXT("=")) == true)
			{
				FString ValuesList = Switch.Right(Switch.Len() - (SwitchKey + TEXT("=")).Len());

				// Allow support for -KEY=Value1+Value2+Value3 as well as -KEY=Value1 -KEY=Value2
				for (int32 PlusIdx = ValuesList.Find(TEXT("+")); PlusIdx != INDEX_NONE; PlusIdx = ValuesList.Find(TEXT("+")))
				{
					const FString ValueElement = ValuesList.Left(PlusIdx);
					ValueElements.Add(ValueElement);

					ValuesList = ValuesList.Right(ValuesList.Len() - (PlusIdx + 1));
				}
				ValueElements.Add(ValuesList);
			}
			return ValueElements;
		};

		// Check for -MAP=<name of map> entries
		CmdLineMapEntries += GetSwitchValueElements(TEXT("MAP"));

		// Check for -COOKDIR=<path to directory> entries
		CmdLineDirEntries += GetSwitchValueElements(TEXT("COOKDIR"));
		for(FString& Entry : CmdLineDirEntries)
		{
			Entry = Entry.TrimQuotes();
			FPaths::NormalizeDirectoryName(Entry);
		}
	}

	// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
		for(const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
		{
			CmdLineDirEntries.Add(AbsoluteGameContentDir / DirToCook.Path);
		}
	}

	for (int32 CmdLineMapIdx = 0; CmdLineMapIdx < CmdLineMapEntries.Num(); CmdLineMapIdx++)
	{
		FString CurrEntry = CmdLineMapEntries[CmdLineMapIdx];
		
		if (FPackageName::IsShortPackageName(CurrEntry))
		{
			if (FPackageName::SearchForPackageOnDisk(CurrEntry, NULL, &CurrEntry) == false)
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to find package for map %s."), *CurrEntry);
			}
			else
			{
				AddFileToCook(FilesInPath, CurrEntry);
			}
		}
		else if (FPackageName::IsValidLongPackageName(CurrEntry))
		{
			CurrEntry = FPackageName::LongPackageNameToFilename(CurrEntry, TEXT(".umap"));
			AddFileToCook(FilesInPath, CurrEntry);
		}
		else
		{
			AddFileToCook(FilesInPath, CurrEntry);
		}
	}

	const FString ExternalMountPointName(TEXT("/Game/"));
	for (int32 CmdLineDirIdx = 0; CmdLineDirIdx < CmdLineDirEntries.Num(); CmdLineDirIdx++)
	{
		FString CurrEntry = CmdLineDirEntries[CmdLineDirIdx];

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CurrEntry, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			AddFileToCook(FilesInPath, StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, CurrEntry);
			}
		}
	}

	if (FilesInPath.Num() == 0 || bCookAll)
	{
		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());

		uint8 PackageFilter = NORMALIZE_DefaultFlags | NORMALIZE_ExcludeEnginePackages;
		if ( Switches.Contains(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( Switches.Contains(TEXT("NODEV")) )
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
				UE_LOG(LogCookCommandlet, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			for (int32 TokenFileIndex = 0; TokenFileIndex < TokenFiles.Num(); ++TokenFileIndex)
			{
				AddFileToCook(FilesInPath, TokenFiles[TokenFileIndex]);
			}
		}
	}

	// make sure we cook the default maps
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	static const TArray<ITargetPlatform*>& Platforms =  TPM.GetTargetPlatforms();
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
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), Obj))
		{
			if (Obj != FName(NAME_None).ToString())
			{
				AddFileToCook(FilesInPath, Obj);
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

	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	{
		TArray<FString> UIContentPaths;
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
					FPaths::MakeStandardFilename(StdFile);
					AddFileToCook(FilesInPath, StdFile);
				}
			}
		}
	}
}

void UCookCommandlet::GenerateLongPackageNames(TArray<FString>& FilesInPath)
{
	TArray<FString> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());
	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1];
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			AddFileToCook(FilesInPathReverse, FileInPath);
		}
		else
		{
			FString LongPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName))
			{
				AddFileToCook(FilesInPathReverse, LongPackageName);
			}
			else
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to generate long package name for %s"), *FileInPath);
			}
		}
	}
	Exchange(FilesInPathReverse, FilesInPath);
}

bool UCookCommandlet::NewCook( const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath )
{
	COOK_STAT(FScopedDurationTimer NewCookTimer(DetailedCookStats::NewCookTimeSec));
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();

	struct FScopeRootObject
	{
		UObject *Object;
		FScopeRootObject( UObject *InObject ) : Object( InObject )
		{
			Object->AddToRoot();
		}

		~FScopeRootObject()
		{
			Object->RemoveFromRoot();
		}
	};

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	FScopeRootObject S(CookOnTheFlyServer);

	ECookInitializationFlags CookFlags = ECookInitializationFlags::IncludeServerMaps;
	CookFlags |= bCompressed ? ECookInitializationFlags::Compressed : ECookInitializationFlags::None;
	CookFlags |= bIterativeCooking ? ECookInitializationFlags::Iterative : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;	
	CookFlags |= bUseSerializationForGeneratingPackageDependencies ? ECookInitializationFlags::UseSerializationForPackageDependencies : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookFlags |= bVerboseCookerWarnings ? ECookInitializationFlags::OutputVerboseCookerWarnings : ECookInitializationFlags::None;
	CookFlags |= bPartialGC ? ECookInitializationFlags::MarkupInUsePackages : ECookInitializationFlags::None;
	bool bTestCook = FParse::Param(*Params, TEXT("Testcook"));
	CookFlags |= bTestCook ? ECookInitializationFlags::TestCook : ECookInitializationFlags::None;

	TArray<UClass*> FullGCAssetClasses;
	if (FullGCAssetClassNames.Num())
	{
		for (const auto& ClassName : FullGCAssetClassNames)
		{
			UClass* ClassToForceFullGC = FindObject<UClass>(nullptr, *ClassName);
			if (ClassToForceFullGC)
			{
				FullGCAssetClasses.Add(ClassToForceFullGC);
			}
			else
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Configured to force full GC for assets of type (%s) but that class does not exist."), *ClassName);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// parse commandline options 

	FString DLCName;
	FParse::Value( *Params, TEXT("DLCNAME="), DLCName);

	FString ChildCookFile;
	FParse::Value(*Params, TEXT("cookchild="), ChildCookFile);

	int32 ChildCookIdentifier = -1;
	FParse::Value(*Params, TEXT("childIdentifier="), ChildCookIdentifier);

	int32 NumProcesses = 0;
	FParse::Value(*Params, TEXT("numcookerstospawn="), NumProcesses);

	FString BasedOnReleaseVersion;
	FParse::Value( *Params, TEXT("BasedOnReleaseVersion="), BasedOnReleaseVersion);

	FString CreateReleaseVersion;
	FParse::Value( *Params, TEXT("CreateReleaseVersion="), CreateReleaseVersion);

	TArray<FString> CmdLineMapEntries;
	TArray<FString> CmdLineDirEntries;
	TArray<FString> CmdLineCultEntries;
	TArray<FString> CmdLineNeverCookDirEntries;
	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		const FString& Switch = Switches[SwitchIdx];

		auto GetSwitchValueElements = [&Switch](const FString SwitchKey) -> TArray<FString>
		{
			TArray<FString> ValueElements;
			if (Switch.StartsWith(SwitchKey + TEXT("=")) == true)
			{
				FString ValuesList = Switch.Right(Switch.Len() - (SwitchKey + TEXT("=")).Len());

				// Allow support for -KEY=Value1+Value2+Value3 as well as -KEY=Value1 -KEY=Value2
				for (int32 PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive); PlusIdx != INDEX_NONE; PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive))
				{
					const FString ValueElement = ValuesList.Left(PlusIdx);
					ValueElements.Add(ValueElement);

					ValuesList = ValuesList.Right(ValuesList.Len() - (PlusIdx + 1));
				}
				ValueElements.Add(ValuesList);
			}
			return ValueElements;
		};

		// Check for -MAP=<name of map> entries
		CmdLineMapEntries += GetSwitchValueElements(TEXT("MAP"));

		// Check for -COOKDIR=<path to directory> entries
		CmdLineDirEntries += GetSwitchValueElements(TEXT("COOKDIR"));
		for(FString& Entry : CmdLineDirEntries)
		{
			Entry = Entry.TrimQuotes();
			FPaths::NormalizeDirectoryName(Entry);
		}

		// Check for -COOKCULTURES=<culture name> entries
		CmdLineCultEntries += GetSwitchValueElements(TEXT("COOKCULTURES"));
	}

	// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
		for (const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
		{
			CmdLineDirEntries.Add(AbsoluteGameContentDir / DirToCook.Path);
		}

	}

	CookOnTheFlyServer->Initialize(ECookMode::CookByTheBook, CookFlags);

	// for backwards compat use the FullGCAssetClasses that we got from the cook commandlet ini section
	if (FullGCAssetClasses.Num() > 0)
	{
		CookOnTheFlyServer->SetFullGCAssetClasses(FullGCAssetClasses);
	}

	// Add any map sections specified on command line
	TArray<FString> AlwaysCookMapList;

	// Add the default map section
	//GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), AlwaysCookMapList);

	TArray<FString> MapList;
	// Add any map sections specified on command line
	/*GEditor->ParseMapSectionIni(*Params, MapList);

	if (MapList.Num() == 0 && !bCookSinglePackage)
	{
		// If we didn't find any maps look in the project settings for maps

		UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());

		for (const auto& MapToCook : PackagingSettings->MapsToCook)
		{
			MapList.Add(MapToCook.FilePath);
		}
	}*/

	// Add any map specified on the command line.
	for (const auto& MapName : CmdLineMapEntries)
	{
		MapList.Add(MapName);
	}

	// If we still don't have any mapsList check if the allmaps ini section is filled out
	// this is for backwards compatibility
	if (MapList.Num() == 0)
	{
		GEditor->ParseMapSectionIni(TEXT("-MAPINISECTION=AllMaps"), MapList);
	}

	if (!bCookSinglePackage)
	{
		// Put the always cook map list at the front of the map list
		AlwaysCookMapList.Append(MapList);
		Swap(MapList, AlwaysCookMapList);
	}

	// Set the list of cultures to cook as those on the commandline, if specified.
	// Otherwise, use the project packaging settings.
	TArray<FString> CookCultures;
	if (Switches.ContainsByPredicate([](const FString& Switch) -> bool
		{
			return Switch.StartsWith("COOKCULTURES=");
		}))
	{
		CookCultures = CmdLineCultEntries;
	}
	else
	{
		UProjectPackagingSettings* const PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
		CookCultures = PackagingSettings->CulturesToStage;
	}

	//////////////////////////////////////////////////////////////////////////
	// start cook by the book 
	ECookByTheBookOptions CookOptions = ECookByTheBookOptions::None;

	CookOptions |= bLeakTest ? ECookByTheBookOptions::LeakTest : ECookByTheBookOptions::None; 
	CookOptions |= bCookAll ? ECookByTheBookOptions::CookAll : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("MAPSONLY")) ? ECookByTheBookOptions::MapsOnly : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("NODEV")) ? ECookByTheBookOptions::NoDevContent : ECookByTheBookOptions::None;

	const ECookByTheBookOptions SinglePackageFlags = ECookByTheBookOptions::NoAlwaysCookMaps | ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages | ECookByTheBookOptions::NoInputPackages | ECookByTheBookOptions::NoSlatePackages | ECookByTheBookOptions::DisableUnsolicitedPackages | ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
	CookOptions |= bCookSinglePackage ? SinglePackageFlags : ECookByTheBookOptions::None;

	UCookOnTheFlyServer::FCookByTheBookStartupOptions StartupOptions;

	StartupOptions.TargetPlatforms = Platforms;
	Swap( StartupOptions.CookMaps, MapList );
	Swap( StartupOptions.CookDirectories, CmdLineDirEntries );
	Swap( StartupOptions.NeverCookDirectories, CmdLineNeverCookDirEntries);
	Swap( StartupOptions.CookCultures, CookCultures );
	Swap( StartupOptions.DLCName, DLCName );
	Swap( StartupOptions.BasedOnReleaseVersion, BasedOnReleaseVersion );
	Swap( StartupOptions.CreateReleaseVersion, CreateReleaseVersion );
	StartupOptions.CookOptions = CookOptions;
	StartupOptions.bErrorOnEngineContentUse = bErrorOnEngineContentUse;
	StartupOptions.bGenerateDependenciesForMaps = Switches.Contains(TEXT("GenerateDependenciesForMaps"));
	StartupOptions.bGenerateStreamingInstallManifests = bGenerateStreamingInstallManifests;
	StartupOptions.ChildCookFileName = ChildCookFile;
	StartupOptions.ChildCookIdentifier = ChildCookIdentifier;
	StartupOptions.NumProcesses = NumProcesses;

	COOK_STAT(
	{
		for (const auto& Platform : Platforms)
		{
			DetailedCookStats::TargetPlatforms += Platform->PlatformName() + TEXT("+");
		}
		if (!DetailedCookStats::TargetPlatforms.IsEmpty())
		{
			DetailedCookStats::TargetPlatforms.RemoveFromEnd(TEXT("+"));
		}
	});
	
	do
	{
		{
			COOK_STAT(FScopedDurationTimer StartCookByTheBookTimer(DetailedCookStats::StartCookByTheBookTimeSec));
			CookOnTheFlyServer->StartCookByTheBook(StartupOptions);
		}

		// Garbage collection should happen when either
		//	1. We have cooked a map (configurable asset type)
		//	2. We have cooked non-map packages and...
		//		a. we have accumulated 50 (configurable) of these since the last GC.
		//		b. we have been idle for 20 (configurable) seconds.
		bool bShouldGC = false;
		FString GCReason;

		// megamoth
		uint32 NonMapPackageCountSinceLastGC = 0;

		const uint32 PackagesPerGC = CookOnTheFlyServer->GetPackagesPerGC();
		const double IdleTimeToGC = CookOnTheFlyServer->GetIdleTimeToGC();
		const uint64 MaxMemoryAllowance = CookOnTheFlyServer->GetMaxMemoryAllowance();

		double LastCookActionTime = FPlatformTime::Seconds();

		FDateTime LastConnectionTime = FDateTime::UtcNow();
		bool bHadConnection = false;

		while (CookOnTheFlyServer->IsCookByTheBookRunning())
		{
			DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "NewCook.MainLoop" ), STAT_CookOnTheFly_MainLoop, STATGROUP_LoadTime );
			{
				uint32 TickResults = 0;
				static const float CookOnTheSideTimeSlice = 10.0f;
				TickResults = CookOnTheFlyServer->TickCookOnTheSide( CookOnTheSideTimeSlice, NonMapPackageCountSinceLastGC );

				{
					COOK_STAT(FScopedDurationTimer ShaderProcessAsyncTimer(DetailedCookStats::TickLoopShaderProcessAsyncResultsTimeSec));
					GShaderCompilingManager->ProcessAsyncResults(true, false);
				}

				const bool bHasExceededMaxMemory = CookOnTheFlyServer->HasExceededMaxMemory();
				// We should GC if we have packages to collect and we've been idle for some time.
				const bool bExceededPackagesPerGC = (PackagesPerGC > 0) && (NonMapPackageCountSinceLastGC > PackagesPerGC);
				const bool bWaitingOnObjectCache = ((TickResults & UCookOnTheFlyServer::COSR_WaitingOnCache) == 0);


				if ( !bWaitingOnObjectCache && bExceededPackagesPerGC ) // if we are waiting on things to cache then ignore the exceeded packages per gc
				{
					bShouldGC = true;
					GCReason = TEXT("Exceeded packages per GC");
				}
				else if ( bHasExceededMaxMemory ) // if we are exceeding memory then we need to gc (this can cause thrashing if the cooker loads the same stuff into memory next tick
				{
					bShouldGC = true;
					GCReason = TEXT("Exceeded Max Memory");
				}
				else if ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC) != 0) // cooker loaded some object which needs to be cleaned up before the cooker can proceed so force gc
				{
					GCReason = TEXT("COSR_RequiresGC");
					bShouldGC = true;
				}


				bShouldGC |= bTestCook; // testing cooking / gc path
				
				auto DumpMemStats = []()
				{
					FGenericMemoryStats MemStats;
					GMalloc->GetAllocatorStats(MemStats);
					for (const auto& Item : MemStats.Data)
					{
						UE_LOG(LogCookCommandlet, Display, TEXT("Item %s = %d"), *Item.Key, Item.Value);
					}
				};

				if ( bPartialGC && (TickResults & UCookOnTheFlyServer::COSR_MarkedUpKeepPackages))
				{
					COOK_STAT(FScopedDurationTimer GCTimer(DetailedCookStats::TickLoopGCTimeSec));
					UE_LOG(LogCookCommandlet, Display, TEXT("GarbageCollection... partial gc"));

					DumpMemStats();

					int32 NumObjectsBeforeGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
					int32 NumObjectsAvailableBeforeGC = GUObjectArray.GetObjectArrayNum();
					CollectGarbage(RF_KeepForCooker, true);

					int32 NumObjectsAfterGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
					int32 NumObjectsAvailableAfterGC = GUObjectArray.GetObjectArrayNum();
					UE_LOG(LogCookCommandlet, Display, TEXT("Partial GC before %d available %d after %d available %d"), NumObjectsBeforeGC, NumObjectsAvailableBeforeGC, NumObjectsAfterGC, NumObjectsAvailableAfterGC);

					DumpMemStats();
				
				}
				else if (bShouldGC) // don't clean up if we are waiting on cache of cooked data
				{
					bShouldGC = false;
					NonMapPackageCountSinceLastGC = 0;

					int32 NumObjectsBeforeGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
					int32 NumObjectsAvailableBeforeGC = GUObjectArray.GetObjectArrayNum();

					UE_LOG( LogCookCommandlet, Display, TEXT( "GarbageCollection... (%s)" ), *GCReason);
					GCReason = FString();


					DumpMemStats();

					COOK_STAT(FScopedDurationTimer GCTimer(DetailedCookStats::TickLoopGCTimeSec));
					CollectGarbage(RF_NoFlags);

					int32 NumObjectsAfterGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
					int32 NumObjectsAvailableAfterGC = GUObjectArray.GetObjectArrayNum();
					UE_LOG(LogCookCommandlet, Display, TEXT("Full GC before %d available %d after %d available %d"), NumObjectsBeforeGC, NumObjectsAvailableBeforeGC, NumObjectsAfterGC, NumObjectsAvailableAfterGC);

					DumpMemStats();
				}
				else
				{
					COOK_STAT(FScopedDurationTimer RecompileTimer(DetailedCookStats::TickLoopRecompileShaderRequestsTimeSec));
					CookOnTheFlyServer->TickRecompileShaderRequests();

					FPlatformProcess::Sleep( 0.0f );
				}

				{
					COOK_STAT(FScopedDurationTimer ProcessDeferredCommandsTimer(DetailedCookStats::TickLoopProcessDeferredCommandsTimeSec));
					ProcessDeferredCommands();
				}
			}

			{
				COOK_STAT(FScopedDurationTimer TickCommandletStatsTimer(DetailedCookStats::TickLoopTickCommandletStatsTimeSec));
				FStats::TickCommandletStats();
			}
		}
	} while (bTestCook);

	return true;
}

bool UCookCommandlet::HasExceededMaxMemory(uint64 MaxMemoryAllowance) const
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	uint64 UsedMemory = MemStats.UsedPhysical;
	if ( (UsedMemory >= MaxMemoryAllowance) && 
		(MaxMemoryAllowance > 0u) )
	{
		return true;
	}
	return false;
}

void UCookCommandlet::ProcessDeferredCommands()
{
#if PLATFORM_MAC
	// On Mac we need to process Cocoa events so that the console window for CookOnTheFlyServer is interactive
	FPlatformMisc::PumpMessages(true);
#endif

	// update task graph
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	// execute deferred commands
	for (int32 DeferredCommandsIndex = 0; DeferredCommandsIndex<GEngine->DeferredCommands.Num(); ++DeferredCommandsIndex)
	{
		GEngine->Exec( GWorld, *GEngine->DeferredCommands[DeferredCommandsIndex], *GLog);
	}

	GEngine->DeferredCommands.Empty();
}

bool UCookCommandlet::Cook(const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath)
{
	// Local sandbox file wrapper. This will be used to handle path conversions,
	// but will not be used to actually write/read files so we can safely
	// use [Platform] token in the sandbox directory name and then replace it
	// with the actual platform name.
	SandboxFile = new FSandboxPlatformFile(false);

	// Output directory override.	
	FString OutputDirectory = GetOutputDirectoryOverride();

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	SandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));

	CleanSandbox(Platforms);

	// allow the game to fill out the asset registry, as well as get a list of objects to always cook
	FGameDelegates::Get().GetCookModificationDelegate().ExecuteIfBound(FilesInPath);

	// always generate the asset registry before starting to cook, for either method
	GenerateAssetRegistry(Platforms);


	// Subsets for parallel processing
	uint32 SubsetMod = 0;
	uint32 SubsetTarget = MAX_uint32;
	FParse::Value(*Params, TEXT("SubsetMod="), SubsetMod);
	FParse::Value(*Params, TEXT("SubsetTarget="), SubsetTarget);
	bool bDoSubset = SubsetMod > 0 && SubsetTarget < SubsetMod;

	FCoreUObjectDelegates::PackageCreatedForLoad.AddUObject(this, &UCookCommandlet::MaybeMarkPackageAsAlreadyLoaded);
	
	SaveGlobalShaderMapFiles(Platforms);

	CollectFilesToCook(FilesInPath);
	if (FilesInPath.Num() == 0)
	{
		UE_LOG(LogCookCommandlet, Warning, TEXT("No files found."));
	}

	GenerateLongPackageNames(FilesInPath);

	TSet<UClass*> ClassesToForceFullGC;
	for ( const FString& ClassName : FullGCAssetClassNames )
	{
		UClass* ClassToForceFullGC = FindObject<UClass>(nullptr, *ClassName);
		if ( ClassToForceFullGC )
		{
			ClassesToForceFullGC.Add(ClassToForceFullGC);
		}
		else
		{
			UE_LOG(LogCookCommandlet, Warning, TEXT("Configured to force full GC for assets of type (%s) but that class does not exist."), *ClassName);
		}
	}

	const int32 GCInterval = bLeakTest ? 1: 500;
	int32 NumProcessedSinceLastGC = GCInterval;
	bool bForceGC = false;
	TSet<FString> CookedPackages;
	FString LastLoadedMapName;

	FChunkManifestGenerator ManifestGenerator(Platforms);
	// Always clean manifest directories so that there's no stale data
	ManifestGenerator.CleanManifestDirectories();
	// ManifestGenerator.Initialize(bGenerateStreamingInstallManifests);

	for( int32 FileIndex = 0; ; FileIndex++ )
	{
		if (NumProcessedSinceLastGC >= GCInterval || bForceGC || FileIndex < 0 || FileIndex >= FilesInPath.Num())
		{
			// since we are about to save, we need to resolve all string asset references now
			GRedirectCollector.ResolveStringAssetReference();
			TArray<UObject *> ObjectsInOuter;
			GetObjectsWithOuter(NULL, ObjectsInOuter, false);
			// save the cooked packages before collect garbage
			for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
			{
				UPackage* Pkg = Cast<UPackage>(ObjectsInOuter[Index]);
				if (!Pkg)
				{
					continue;
				}

				FString Name = Pkg->GetPathName();
				FString Filename(GetPackageFilename(Pkg));

				if (!Filename.IsEmpty())
				{
					// Populate streaming install manifests
					FString SandboxFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*Filename);
					UE_LOG(LogCookCommandlet, Display, TEXT("Adding package to manifest %s, %s, %s"), *Pkg->GetName(), *SandboxFilename, *LastLoadedMapName);
					//ManifestGenerator.AddPackageToChunkManifest(Pkg, SandboxFilename, LastLoadedMapName, SandboxFile.GetOwnedPointer());
				}
					
				if (!CookedPackages.Contains(Filename))
				{
					CookedPackages.Add(Filename);

					bool bWasUpToDate = false;

					SaveCookedPackage(Pkg, SAVE_KeepGUID | SAVE_Async | (bUnversioned ? SAVE_Unversioned : 0), bWasUpToDate);

					PackagesToNotReload.Add(Pkg->GetName());
					Pkg->SetPackageFlags(PKG_ReloadingForCooker);
					{
						TArray<UObject *> ObjectsInPackage;
						GetObjectsWithOuter(Pkg, ObjectsInPackage, true);
						for( int32 IndexPackage = 0; IndexPackage < ObjectsInPackage.Num(); IndexPackage++ )
						{
							ObjectsInPackage[IndexPackage]->WillNeverCacheCookedPlatformDataAgain();
							ObjectsInPackage[IndexPackage]->ClearAllCachedCookedPlatformData();
						}
					}
				}
			}

			if (bForceGC || NumProcessedSinceLastGC >= GCInterval)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Full GC..."));

				CollectGarbage(RF_NoFlags);
				NumProcessedSinceLastGC = 0;

				if (bLeakTest)
				{
					for (FObjectIterator It; It; ++It)
					{
						if (!LastGCItems.Contains(FWeakObjectPtr(*It)))
						{
							UE_LOG(LogCookCommandlet, Warning, TEXT("\tLeaked %s"), *(It->GetFullName()));
							LastGCItems.Add(FWeakObjectPtr(*It));
						}
					}
				}

				bForceGC = false;
			}
		}

		if (FileIndex < 0 || FileIndex >= FilesInPath.Num())
		{
			break;
		}
		// Attempt to find file for package name. THis is to make sure no short package
		// names are passed to LoadPackage.
		FString Filename;
		if (FPackageName::DoesPackageExist(FilesInPath[FileIndex], NULL, &Filename) == false)
		{
			UE_LOG(LogCookCommandlet, Warning, TEXT("Unable to find package file for: %s"), *FilesInPath[FileIndex]);
			
			continue;
		}
		
		UE_LOG(LogCookCommandlet, Display, TEXT("Processing package %s"), *Filename);
		Filename = FPaths::ConvertRelativePathToFull(Filename);

		if (bDoSubset)
		{
			const FString& PackageName = FPackageName::PackageFromPath(*Filename);
			if (FCrc::StrCrc_DEPRECATED(*PackageName.ToUpper()) % SubsetMod != SubsetTarget)
			{
				continue;
			}
		}

		if (CookedPackages.Contains(Filename))
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("\tskipping %s, already cooked."), *Filename);
			continue;
		}

		if (!ShouldCook(Filename))
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Up To Date: %s"), *Filename);
			NumProcessedSinceLastGC++;
			continue;
		}

		UE_LOG(LogCookCommandlet, Display, TEXT("Loading %s"), *Filename );

		UPackage* Package = LoadPackage( NULL, *Filename, LOAD_None );

		if( Package == NULL )
		{
			UE_LOG(LogCookCommandlet, Warning, TEXT("Could not load %s!"), *Filename );
		}
		else
		{
			NumProcessedSinceLastGC++;
			if (Package->ContainsMap())
			{
				// load sublevels
				UWorld* World = UWorld::FindWorldInPackage(Package);
				check(Package);

				if (World->StreamingLevels.Num())
				{
					World->LoadSecondaryLevels(true, &CookedPackages);
				}

				// Collect world composition tile packages to cook
				if (World->WorldComposition)
				{
					World->WorldComposition->CollectTilesToCook(FilesInPath);
				}

				LastLoadedMapName = Package->GetName();
			}
			else
			{
				LastLoadedMapName.Empty();
			}

			if ( !bForceGC && ClassesToForceFullGC.Num() > 0 )
			{
				const bool bIncludeNestedObjects = false;
				TArray<UObject*> RootLevelObjects;
				GetObjectsWithOuter(Package, RootLevelObjects, bIncludeNestedObjects);
				for ( auto* RootObject : RootLevelObjects )
				{
					if ( ClassesToForceFullGC.Contains(RootObject->GetClass()) )
					{
						bForceGC = true;
						break;
					}
				}
			}
		}
	}

	UPackage::WaitForAsyncFileWrites();

	GetDerivedDataCacheRef().WaitForQuiescence(true);

	{
		// Always try to save the manifests, this is required to make the asset registry work, but doesn't necessarily write a file
		ManifestGenerator.SaveManifests(SandboxFile.GetOwnedPointer());

		// Save modified asset registry with all streaming chunk info generated during cook
		FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
		FString SandboxRegistryFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*RegistryFilename);
		ManifestGenerator.SaveAssetRegistry(SandboxRegistryFilename);

		FString CookedAssetRegistry = FPaths::GameDir() / TEXT("CookedAssetRegistry.json");
		FString SandboxCookedAssetRegistryFilename = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*CookedAssetRegistry);

		ManifestGenerator.SaveCookedPackageAssetRegistry(SandboxCookedAssetRegistryFilename, true);
	}

	return true;
}


