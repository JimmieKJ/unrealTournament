// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Misc/ExclusiveLoadPackageTimeTracker.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"

#if WITH_LOADPACKAGE_TIME_TRACKER

#define LOCTEXT_NAMESPACE "ExclusiveLoadPackageTimeTracker"

FExclusiveLoadPackageTimeTracker::FExclusiveLoadPackageTimeTracker()
	: TrackerOverhead(0.f)
	, EndLoadName(FName(TEXT("EndLoad")))
	, UnknownAssetName(FName(TEXT("Unknown")))
	, DumpReportCommand(
		TEXT("LoadTimes.DumpReport"),
		*LOCTEXT("CommandText_DumpReport", "Dumps a report about the amount of time spent loading assets").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FExclusiveLoadPackageTimeTracker::DumpReportCommandHandler))
{
}

FExclusiveLoadPackageTimeTracker& FExclusiveLoadPackageTimeTracker::Get()
{
	static FExclusiveLoadPackageTimeTracker Tracker;
	return Tracker;
}

void FExclusiveLoadPackageTimeTracker::InternalPushLoadPackage(FName PackageName)
{
	FScopeLock Lock(&TimesCritical);
	const double CurrentTime = FPlatformTime::Seconds();

	if (TimeStack.Num() > 0)
	{
		FLoadTime& Time = TimeStack.Last();
		Time.ExclusiveTime += (CurrentTime - Time.LastStartTime);
	}

	TimeStack.Push(FLoadTime(PackageName, CurrentTime));

	TrackerOverhead += (FPlatformTime::Seconds() - CurrentTime);
}

void FExclusiveLoadPackageTimeTracker::InternalPopLoadPackage(UPackage* LoadedPackage, UObject* LoadedAsset)
{
	FScopeLock Lock(&TimesCritical);
	if (ensure(TimeStack.Num() > 0))
	{
		const double CurrentTime = FPlatformTime::Seconds();
		FLoadTime& Time = TimeStack.Last();
		Time.ExclusiveTime += (CurrentTime - Time.LastStartTime);

		FName ClassName = UnknownAssetName;
		if (LoadedAsset)
		{
			ClassName = LoadedAsset->GetClass()->GetFName();
		}
		else if (LoadedPackage)
		{
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithOuter(LoadedPackage, ObjectsInPackage, false);
			for (UObject* Object : ObjectsInPackage)
			{
				if (Object && Object->IsAsset())
				{
					ClassName = Object->GetClass()->GetFName();
					break;
				}
			}
		}

		double InclusiveTime = CurrentTime - Time.OriginalStartTime;
		FLoadTime* LoadTimePtr = LoadTimes.Find(Time.TimeName);
		if ( LoadTimePtr )
		{
			// Use the most recently discovered classname. This may be "Unknown" during async loading but future pops will correct it.
			LoadTimePtr->AssetClass = ClassName;
			LoadTimePtr->ExclusiveTime += Time.ExclusiveTime;
			LoadTimePtr->InclusiveTime += InclusiveTime;
		}
		else
		{
			LoadTimes.Add(Time.TimeName, FLoadTime(Time.TimeName, ClassName, Time.ExclusiveTime, InclusiveTime));
		}

		TimeStack.RemoveAt(TimeStack.Num() - 1, 1, false);

		if (TimeStack.Num() > 0)
		{
			const double NewCurrentTime = FPlatformTime::Seconds();
			FLoadTime& InnerTime = TimeStack.Last();
			InnerTime.LastStartTime = NewCurrentTime;
		}

		TrackerOverhead += (FPlatformTime::Seconds() - CurrentTime);
	}
}

void FExclusiveLoadPackageTimeTracker::InternalDumpReport() const
{
	FScopeLock Lock(&TimesCritical);

	/** Struct to assemble times for assets loaded by class */
	struct FTimeCount
	{
		FName AssetClass;
		double ExclusiveTime;
		int32 Count;

		FTimeCount() : ExclusiveTime(0), Count(0) {}
	};

	double LongestLoadTime = 0;
	double SlowAssetTime = 0;
	FName LongestLoadName;
	double TotalLoadTime = 0;
	const double SlowAssetThreshold = 0.10f;
	TMap<FName, FTimeCount> AssetTypeLoadTimes;
	for (const auto& LoadTimeIt : LoadTimes)
	{
		const FLoadTime& Time = LoadTimeIt.Value;
		if (IsPackageLoadTime(Time))
		{
			if (Time.ExclusiveTime > LongestLoadTime)
			{
				LongestLoadName = LoadTimeIt.Key;
				LongestLoadTime = Time.ExclusiveTime;
			}

			if (Time.ExclusiveTime > SlowAssetThreshold)
			{
				SlowAssetTime += Time.ExclusiveTime;
			}

			FTimeCount& TypeTimeCount = AssetTypeLoadTimes.FindOrAdd(Time.AssetClass);
			TypeTimeCount.AssetClass = Time.AssetClass;
			TypeTimeCount.ExclusiveTime += Time.ExclusiveTime;
			TypeTimeCount.Count++;
		}

		TotalLoadTime += Time.ExclusiveTime;
	}

	const FLoadTime* EndLoadTime = LoadTimes.Find(EndLoadName);
	const int32 NumNonAssetTimes = EndLoadTime ? 1 : 0;
	UE_LOG(LogLoad, Log, TEXT("Loaded: %d packages"), LoadTimes.Num() - NumNonAssetTimes);
	UE_LOG(LogLoad, Log, TEXT("Total time loading packages: %.3f seconds"), TotalLoadTime);
	UE_LOG(LogLoad, Log, TEXT("Time spent loading assets slower than %.1fms: %.3f seconds"), SlowAssetThreshold * 1000, SlowAssetTime);
	UE_LOG(LogLoad, Log, TEXT("Slowest asset: %s (%.1fms)"), *LongestLoadName.ToString(), LongestLoadTime * 1000);
	UE_LOG(LogLoad, Log, TEXT("Time spent in EndLoad: %.3f seconds"), EndLoadTime ? EndLoadTime->ExclusiveTime : 0.f);
	UE_LOG(LogLoad, Log, TEXT("Time spent in overhead tracking asset load times: %.6f seconds"), TrackerOverhead);

	UE_LOG(LogLoad, Log, TEXT("Dumping asset type load times sorted by exclusive time:"));
	TArray<FTimeCount> SortedAssetTypeLoadTimes;
	AssetTypeLoadTimes.GenerateValueArray(SortedAssetTypeLoadTimes);
	SortedAssetTypeLoadTimes.Sort([](const FTimeCount& A, const FTimeCount& B){ return A.ExclusiveTime >= B.ExclusiveTime; });
	for (const FTimeCount& TimeCount : SortedAssetTypeLoadTimes)
	{
		UE_LOG(LogLoad, Log, TEXT("    %.3f: %s (%d packages, %.1fms per package)"), TimeCount.ExclusiveTime, *TimeCount.AssetClass.ToString(), TimeCount.Count, TimeCount.ExclusiveTime / TimeCount.Count * 1000);
	}

	// Assets faster than this will be excluded
	const double LowTimeThreshold = 0.05;

	TArray<FLoadTime> SortedLoadTimes;
	LoadTimes.GenerateValueArray(SortedLoadTimes);
	
	{
		UE_LOG(LogLoad, Log, TEXT("Dumping all loaded assets by exclusive load time:"));
		SortedLoadTimes.Sort([](const FLoadTime& A, const FLoadTime& B){ return A.ExclusiveTime >= B.ExclusiveTime; });
		int32 LowThresholdCount = 0;
		double TotalLowTime = 0;
		for (int32 LoadTimeIdx = 0; LoadTimeIdx < SortedLoadTimes.Num(); ++LoadTimeIdx)
		{
			const FLoadTime& LoadTime = SortedLoadTimes[LoadTimeIdx];
			if (IsPackageLoadTime(LoadTime))
			{
				if (LoadTime.ExclusiveTime > LowTimeThreshold)
				{
					UE_LOG(LogLoad, Log, TEXT("    %.1fms: %s"), LoadTime.ExclusiveTime * 1000, *LoadTime.TimeName.ToString());
				}
				else
				{
					LowThresholdCount++;
					TotalLowTime += LoadTime.ExclusiveTime;
				}
			}
		}

		if (LowThresholdCount > 0)
		{
			UE_LOG(LogLoad, Log, TEXT("    ... skipped %d assets slower than %.1fms totaling %.1fms"), LowThresholdCount, LowTimeThreshold * 1000, TotalLowTime * 1000);
		}
	}

	{
		UE_LOG(LogLoad, Log, TEXT("Dumping all loaded assets by inclusive load time:"));
		SortedLoadTimes.Sort([](const FLoadTime& A, const FLoadTime& B){ return A.InclusiveTime >= B.InclusiveTime; });
		int32 LowThresholdCount = 0;
		double TotalLowTime = 0;
		for (int32 LoadTimeIdx = 0; LoadTimeIdx < SortedLoadTimes.Num(); ++LoadTimeIdx)
		{
			const FLoadTime& LoadTime = SortedLoadTimes[LoadTimeIdx];
			if (IsPackageLoadTime(LoadTime))
			{
				if (LoadTime.InclusiveTime > LowTimeThreshold)
				{
					UE_LOG(LogLoad, Log, TEXT("    %.1fms: %s"), LoadTime.InclusiveTime * 1000, *LoadTime.TimeName.ToString());
				}
				else
				{
					LowThresholdCount++;
					TotalLowTime += LoadTime.InclusiveTime;
				}
			}
		}

		if (LowThresholdCount > 0)
		{
			UE_LOG(LogLoad, Log, TEXT("    ... skipped %d assets slower than %.1fms totaling %.1fms"), LowThresholdCount, LowTimeThreshold * 1000, TotalLowTime * 1000);
		}
	}
}

double FExclusiveLoadPackageTimeTracker::InternalGetExclusiveLoadTime(FName PackageName) const
{
	FScopeLock Lock(&TimesCritical);
	const FLoadTime* LoadTime = LoadTimes.Find(PackageName);
	if (LoadTime)
	{
		return LoadTime->ExclusiveTime;
	}
	
	return -1;
}

double FExclusiveLoadPackageTimeTracker::InternalGetInclusiveLoadTime(FName PackageName) const
{
	FScopeLock Lock(&TimesCritical);
	const FLoadTime* LoadTime = LoadTimes.Find(PackageName);
	if (LoadTime)
	{
		return LoadTime->InclusiveTime;
	}

	return -1;
}

bool FExclusiveLoadPackageTimeTracker::IsPackageLoadTime(const FLoadTime& Time) const
{
	return Time.TimeName != EndLoadName;
}

void FExclusiveLoadPackageTimeTracker::DumpReportCommandHandler(const TArray<FString>& Args)
{
	DumpReport();
}

#undef LOCTEXT_NAMESPACE

#endif //WITH_LOADPACKAGE_TIME_TRACKER
