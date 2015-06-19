// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PerfCountersModule.h"
#include "Json.h"

class FSocket;

DECLARE_LOG_CATEGORY_EXTERN(LogPerfCounters, Verbose, All);

class FPerfCounters 
	: public FTickerObjectBase
	, public IPerfCounters
{
public:

	FPerfCounters(const FString& InUniqueInstanceId);
	virtual ~FPerfCounters();

	/** Initializes this instance from JSON config. */
	bool Initialize();

	/** FTickerObjectBase */
	virtual bool Tick(float DeltaTime) override;

	// IPerfCounters interface
	const FString& GetInstanceName() const override { return UniqueInstanceId; }
	virtual void SetNumber(const FString& Name, double Value) override;
	virtual void SetString(const FString& Name, const FString& Value) override;
	virtual void SetJson(const FString& Name, const FProduceJsonCounterValue& Callback) override;
	// IPerfCounters interface end

private:
	/** Convert all perf counters into a json type */
	FString ToJson() const;

private:
	struct FJsonVariant
	{
		enum { Null, String, Number, Callback } Format;
		FString		StringValue;
		double		NumberValue;
		FProduceJsonCounterValue CallbackValue;

		FJsonVariant() : Format(Null), NumberValue(0) {}
	};

	/** Unique name of this instance */
	FString UniqueInstanceId;

	/** Map of all known performance counters */
	TMap<FString, FJsonVariant>  PerfCounterMap;

	/* Listen socket for outputting JSON on request */
	FSocket* Socket;
};

