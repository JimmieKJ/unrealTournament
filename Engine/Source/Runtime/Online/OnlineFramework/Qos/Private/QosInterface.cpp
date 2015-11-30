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

const FString& FQosInterface::GetDefaultRegionString()
{
	static FString RegionString = TEXT("");
	if (RegionString.IsEmpty())
	{
		FString OverrideRegion;
		if (FParse::Value(FCommandLine::Get(), TEXT("McpRegion="), OverrideRegion))
		{
			// Region specified on command line
			RegionString = OverrideRegion.ToUpper();
		}
		else
		{
			FString DefaultRegion;
			if (GConfig->GetString(TEXT("Qos"), TEXT("DefaultRegion"), DefaultRegion, GGameIni))
			{
				// Region specified in ini file
				RegionString = DefaultRegion.ToUpper();
			}
			else
			{
				// No Region specified. Assume USA.
				RegionString = TEXT("USA");
			}
		}
	}

	return RegionString;
}

const FString& FQosInterface::GetDatacenterId()
{
	static bool bDCIDCheck = false;
	static FString DCIDString = TEXT("");
	if (!bDCIDCheck)
	{
		FString OverrideDCID;
		if (FParse::Value(FCommandLine::Get(), TEXT("DCID="), OverrideDCID))
		{
			// Region specified on command line
			DCIDString = OverrideDCID.ToUpper();
		}
		else
		{
			FString DefaultDCID;
			if (GConfig->GetString(TEXT("Qos"), TEXT("DCID"), DefaultDCID, GGameIni))
			{
				// Region specified in ini file
				DCIDString = DefaultDCID.ToUpper();
			}
		}

		bDCIDCheck = true;
	}

	return DCIDString;
}