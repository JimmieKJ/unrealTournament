// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

#include "StatsCollector.h"


namespace BuildPatchServices
{
	const double ToPercentage = 10000;
	const double FromPercentage = 1 / ToPercentage;

	uint64 FStatsCollector::GetCycles()
	{
#if PLATFORM_WINDOWS
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return Cycles.QuadPart;
#elif PLATFORM_MAC
		uint64 Cycles = mach_absolute_time();
		return Cycles;
#else
		return FPlatformTime::Cycles();
#endif
	}

	double FStatsCollector::CyclesToSeconds(uint64 Cycles)
	{
		return FPlatformTime::GetSecondsPerCycle() * Cycles;
	}

	uint64 FStatsCollector::SecondsToCycles(double Seconds)
	{
		return Seconds / FPlatformTime::GetSecondsPerCycle();
	}

	void FStatsCollector::AccumulateTimeBegin(uint64& TempValue)
	{
		TempValue = FStatsCollector::GetCycles();
	}

	void FStatsCollector::AccumulateTimeEnd(volatile int64* Stat, uint64& TempValue)
	{
		FPlatformAtomics::InterlockedAdd(Stat, FStatsCollector::GetCycles() - TempValue);
	}

	void FStatsCollector::Accumulate(volatile int64* Stat, int64 Amount)
	{
		FPlatformAtomics::InterlockedAdd(Stat, Amount);
	}

	void FStatsCollector::Set(volatile int64* Stat, int64 Value)
	{
		FPlatformAtomics::InterlockedExchange(Stat, Value);
	}

	void FStatsCollector::SetAsPercentage(volatile int64* Stat, double Value)
	{
		FPlatformAtomics::InterlockedExchange(Stat, int64(Value * ToPercentage));
	}

	double FStatsCollector::GetAsPercentage(volatile int64* Stat)
	{
		return *Stat * FromPercentage;
	}

	class FStatsCollectorImpl
		: public FStatsCollector
	{
	public:
		FStatsCollectorImpl();
		virtual ~FStatsCollectorImpl();

		virtual volatile int64* CreateStat(const FString& Name, EStatFormat Type, int64 InitialValue = 0) override;
		virtual void LogStats(float TimeBetweenLogs = 0.0f) override;

	private:
		FCriticalSection DataCS;
		TMap<FString, volatile int64*> AddedStats;
		TMap<int64*, FString> AtomicNameMap;
		TMap<int64*, EStatFormat> AtomicFormatMap;
		uint64 LastLogged;
		int32 LongestName;
		FNumberFormattingOptions PercentageFormattingOptions;
	};

	FStatsCollectorImpl::FStatsCollectorImpl()
		: LastLogged(FStatsCollector::GetCycles())
		, LongestName(0)
	{
		PercentageFormattingOptions.MinimumFractionalDigits = 2;
		PercentageFormattingOptions.MaximumFractionalDigits = 2;
	}

	FStatsCollectorImpl::~FStatsCollectorImpl()
	{
		FScopeLock ScopeLock(&DataCS);
		for (auto& Stat : AddedStats)
		{
			delete Stat.Value;
		}
		AddedStats.Empty();
		AtomicNameMap.Empty();
		AtomicFormatMap.Empty();
	}

	volatile int64* FStatsCollectorImpl::CreateStat(const FString& Name, EStatFormat Type, int64 InitialValue)
	{
		FScopeLock ScopeLock(&DataCS);
		if (AddedStats.Contains(Name) == false)
		{
			volatile int64* NewInteger = new volatile int64(InitialValue);
			AddedStats.Add(Name, NewInteger);
			AtomicNameMap.Add((int64*)NewInteger, Name + TEXT(": "));
			AtomicFormatMap.Add((int64*)NewInteger, Type);
			LongestName = FMath::Max<uint32>(LongestName, Name.Len() + 2);
			for (auto& Stat : AtomicNameMap)
			{
				while (Stat.Value.Len() < LongestName)
				{
					Stat.Value += TEXT(" ");
				}
			}
		}
		return AddedStats[Name];
	}

	void FStatsCollectorImpl::LogStats(float TimeBetweenLogs)
	{
		const uint64 Cycles = FStatsCollector::GetCycles();
		if (FStatsCollector::CyclesToSeconds(Cycles - LastLogged) >= TimeBetweenLogs)
		{
			LastLogged = Cycles;
			FScopeLock ScopeLock(&DataCS);
			GLog->Log(TEXT("/-------- FStatsCollector Log ---------------------"));
			for (auto& Stat : AddedStats)
			{
				int64* NameLookup = (int64*)Stat.Value;
				switch (AtomicFormatMap[NameLookup])
				{
				case EStatFormat::Timer:
					GLog->Logf(TEXT("| %s%s"), *AtomicNameMap[NameLookup], *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(*Stat.Value)));
					break;
				case EStatFormat::DataSize:
					GLog->Logf(TEXT("| %s%s"), *AtomicNameMap[NameLookup], *FText::AsMemory(*Stat.Value).ToString());
					break;
				case EStatFormat::DataSpeed:
					GLog->Logf(TEXT("| %s%s/s"), *AtomicNameMap[NameLookup], *FText::AsMemory(*Stat.Value).ToString());
					break;
				case EStatFormat::Percentage:
					GLog->Logf(TEXT("| %s%s"), *AtomicNameMap[NameLookup], *FText::AsPercent(GetAsPercentage(Stat.Value), &PercentageFormattingOptions).ToString());
					break;
				default:
					GLog->Logf(TEXT("| %s%lld"), *AtomicNameMap[NameLookup], *Stat.Value);
					break;
				}
			}
			GLog->Log(TEXT("\\--------------------------------------------------"));
		}
	}

	FStatsCollectorRef FStatsCollectorFactory::Create()
	{
		return MakeShareable(new FStatsCollectorImpl());
	}
}
