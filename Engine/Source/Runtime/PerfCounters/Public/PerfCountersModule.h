// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;
typedef TSharedRef< TJsonWriter<TCHAR,TPrettyJsonPrintPolicy<TCHAR> > > FPrettyJsonWriter;

DECLARE_DELEGATE_OneParam(FProduceJsonCounterValue, const FPrettyJsonWriter& /* JsonWriter */);

/**
 * A programming interface for setting/updating performance counters
 */
class IPerfCounters
{
public:

	virtual ~IPerfCounters() {};

	/** Get the unique identifier for this perf counter instance */
	virtual const FString& GetInstanceName() const = 0;

	/** Maps value to a numeric holder */
	virtual void SetNumber(const FString& Name, double Value) = 0;

	/** Maps value to a string holder */
	virtual void SetString(const FString& Name, const FString& Value) = 0;

	/** Make a callback so we can request more extensive types on demand (presumably backed by some struct locally) */
	virtual void SetJson(const FString& Name, const FProduceJsonCounterValue& Callback) = 0;

public:
	/** Set overloads (use these) */

	void Set(const FString& Name, int32 Val)
	{
		SetNumber(Name, (double)Val);
	}

	void Set(const FString& Name, uint32 Val)
	{
		SetNumber(Name, (double)Val);
	}

	void Set(const FString& Name, float Val)
	{
		SetNumber(Name, (double)Val);
	}

	void Set(const FString& Name, double Val)
	{
		SetNumber(Name, Val);
	}

	void Set(const FString& Name, int64 Val)
	{
		SetString(Name, FString::Printf(TEXT("%lld"), Val));
	}

	void Set(const FString& Name, uint64 Val)
	{
		SetString(Name, FString::Printf(TEXT("%llu"), Val));
	}

	void Set(const FString& Name, const FString& Val)
	{
		SetString(Name, Val);
	}

	void Set(const FString& Name, const FProduceJsonCounterValue& Callback)
	{
		SetJson(Name, Callback);
	}
};


/**
 * The public interface to this module
 */
class IPerfCountersModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IPerfCountersModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IPerfCountersModule >("PerfCounters");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("PerfCounters");
	}

	/**
	 * @return the currently initialized / in use perf counters 
	 */
	virtual IPerfCounters* GetPerformanceCounters() const = 0;

	/** 
	 * Creates and initializes the performance counters object
	 *
	 * @param UniqueInstanceId		optional parameter that allows to assign a known name for this set of counters (a default one that will include process id will be provided if not given)
	 *
	 * @return IPerfCounters object (should be explicitly deleted later), or nullptr if failed
	 */
	virtual IPerfCounters * CreatePerformanceCounters(const FString& UniqueInstanceId = TEXT("")) = 0;
};
