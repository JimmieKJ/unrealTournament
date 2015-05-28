// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

template<>
void FEnvQueryInstance::AddItemData<UEnvQueryItemType_Point, FNavLocation>(FNavLocation ItemValue)
{
	DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());

	const int32 DataOffset = RawData.AddUninitialized(ValueSize);
	UEnvQueryItemType_Point::SetNavValue(RawData.GetData() + DataOffset, ItemValue);
	Items.Add(FEnvQueryItem(DataOffset));

	INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());
}

UEnvQueryItemType_Point::UEnvQueryItemType_Point(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FNavLocation);
}

FVector UEnvQueryItemType_Point::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FNavLocation>(RawData).Location;
}

void UEnvQueryItemType_Point::SetValue(uint8* RawData, const FVector& Value)
{
	return SetValueInMemory<FNavLocation>(RawData, FNavLocation(Value));
}

FNavLocation UEnvQueryItemType_Point::GetNavValue(const uint8* RawData)
{
	return GetValueFromMemory<FNavLocation>(RawData);
}

void UEnvQueryItemType_Point::SetNavValue(uint8* RawData, const FNavLocation& Value)
{
	return SetValueInMemory<FNavLocation>(RawData, Value);
}

FVector UEnvQueryItemType_Point::GetItemLocation(const uint8* RawData) const
{
	return UEnvQueryItemType_Point::GetValue(RawData);
}

FNavLocation UEnvQueryItemType_Point::GetItemNavLocation(const uint8* RawData) const
{
	return UEnvQueryItemType_Point::GetNavValue(RawData);
}

void UEnvQueryItemType_Point::SetContextHelper(FEnvQueryContextData& ContextData, const FVector& SinglePoint)
{
	ContextData.ValueType = UEnvQueryItemType_Point::StaticClass();
	ContextData.NumValues = 1;
	ContextData.RawData.SetNumUninitialized(sizeof(FNavLocation));

	UEnvQueryItemType_Point::SetValue((uint8*)ContextData.RawData.GetData(), SinglePoint);
}

void UEnvQueryItemType_Point::SetContextHelper(FEnvQueryContextData& ContextData, const TArray<FVector>& MultiplePoints)
{
	ContextData.ValueType = UEnvQueryItemType_Point::StaticClass();
	ContextData.NumValues = MultiplePoints.Num();
	ContextData.RawData.SetNumUninitialized(sizeof(FNavLocation)* MultiplePoints.Num());

	uint8* RawData = (uint8*)ContextData.RawData.GetData();
	for (int32 PointIndex = 0; PointIndex < MultiplePoints.Num(); PointIndex++)
	{
		UEnvQueryItemType_Point::SetValue(RawData, MultiplePoints[PointIndex]);
		RawData += sizeof(FNavLocation);
	}
}
