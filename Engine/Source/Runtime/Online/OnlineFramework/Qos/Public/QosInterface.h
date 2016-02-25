// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "QosModule.h"

class UQosEvaluator;

namespace QOSConsoleVariables
{
	extern TAutoConsoleVariable<FString> CVarForceRegion;
	extern TAutoConsoleVariable<int32> CVarForceDefaultRegion;
	extern TAutoConsoleVariable<int32> CVarOverrideTimestamp;
	extern TAutoConsoleVariable<int32> CVarForceCached;
}

/**
 * Main Qos interface for actions related to server quality of service
 */
class QOS_API FQosInterface
{

public:

	FQosInterface();
	virtual ~FQosInterface();

	/** @return the Qos interface */
	static FQosInterface* Get()
	{
		static const FName QosModuleName = TEXT("Qos");
		FQosModule& QoSModule = FModuleManager::LoadModuleChecked<FQosModule>(QosModuleName);
		return QoSModule.GetQosInterface();
	}

	/**
	 * Get a Qos evaluator for making datacenter determinations and ping timings against servers
	 *
	 * @return a new instance of a qos evaluator
	 */
	UQosEvaluator* CreateQosEvaluator();

	/**
	 * Get the default region for this instance, checking ini and commandline overrides
	 *
	 * @return the default region identifier
	 */
	static const FString& GetDefaultRegionString();

	/**
	 * Get the datacenter id for this instance, checking ini and commandline overrides
	 *
	 * @return the default datacenter identifier
	 */
	static const FString& GetDatacenterId();
};

