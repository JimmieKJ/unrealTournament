// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
#include "QosInterface.h"
#include "QosEvaluator.h"

namespace QOSConsoleVariables
{
	// CVars
	TAutoConsoleVariable<FString> CVarForceRegion(
		TEXT("QOS.ForceRegion"),
		TEXT(""),
		TEXT("Force a given region id\n")
		TEXT("String region id"),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarForceDefaultRegion(
		TEXT("QOS.UseDefaultRegion"),
		0,
		TEXT("Forces default region\n")
		TEXT("1 enables default region, 0 disables."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarForceCached(
		TEXT("QOS.ForceCached"),
		0,
		TEXT("Forced cached value\n")
		TEXT("1 Enables, 0 Disables."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarOverrideTimestamp(
		TEXT("QOS.ForceEval"),
		0,
		TEXT("Ignore Qos datacenter caching\n")
		TEXT("1 Enables, 0 Disables."),
		ECVF_Default);
	
}

FQosInterface::FQosInterface()
{

}

FQosInterface::~FQosInterface()
{

}

UQosEvaluator* FQosInterface::CreateQosEvaluator()
{
	return NewObject<UQosEvaluator>();
}