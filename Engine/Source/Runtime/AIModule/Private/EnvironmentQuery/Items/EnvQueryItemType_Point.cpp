// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

UEnvQueryItemType_Point::UEnvQueryItemType_Point(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FVector);
}

FVector UEnvQueryItemType_Point::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FVector>(RawData);
}

void UEnvQueryItemType_Point::SetValue(uint8* RawData, const FVector& Value)
{
	return SetValueInMemory<FVector>(RawData, Value);
}

FVector UEnvQueryItemType_Point::GetItemLocation(const uint8* RawData) const
{
	return UEnvQueryItemType_Point::GetValue(RawData);
}

void UEnvQueryItemType_Point::SetContextHelper(FEnvQueryContextData& ContextData, const FVector& SinglePoint)
{
	ContextData.ValueType = UEnvQueryItemType_Point::StaticClass();
	ContextData.NumValues = 1;
	ContextData.RawData.Init(sizeof(FVector));

	UEnvQueryItemType_Point::SetValue((uint8*)ContextData.RawData.GetData(), SinglePoint);
}

void UEnvQueryItemType_Point::SetContextHelper(FEnvQueryContextData& ContextData, const TArray<FVector>& MultiplePoints)
{
	ContextData.ValueType = UEnvQueryItemType_Point::StaticClass();
	ContextData.NumValues = MultiplePoints.Num();
	ContextData.RawData.Init(sizeof(FVector) * MultiplePoints.Num());

	uint8* RawData = (uint8*)ContextData.RawData.GetData();
	for (int32 PointIndex = 0; PointIndex < MultiplePoints.Num(); PointIndex++)
	{
		UEnvQueryItemType_Point::SetValue(RawData, MultiplePoints[PointIndex]);
		RawData += sizeof(FVector);
	}
}
