// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "DataProviders/AIDataProvider_QueryParams.h"
#include "EnvironmentQuery/EnvQueryManager.h"

UAIDataProvider_QueryParams::UAIDataProvider_QueryParams(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAIDataProvider_QueryParams::BindData(UObject* Owner, int32 RequestId)
{
	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(Owner);
	if (QueryManager)
	{
		FloatValue = QueryManager->FindNamedParam(RequestId, ParamName);
		IntValue = *((int32*)&FloatValue);
		BoolValue = *((bool*)&FloatValue);
	}
	else
	{
		IntValue = 0;
		FloatValue = 0.0f;
		BoolValue = false;
	}
}

FString UAIDataProvider_QueryParams::ToString(FName PropName) const
{
	return FString::Printf(TEXT("QueryParam.%s"), *ParamName.ToString());
}
