// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintMapLibrary.h"

bool UBlueprintMapLibrary::GenericMap_Add(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
{
	if (TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		return MapHelper.AddPair(KeyPtr, ValuePtr);
	}
	return false;
}

bool UBlueprintMapLibrary::GenericMap_Remove(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		return MapHelper.RemovePair(KeyPtr);
	}
	return false;
}

bool UBlueprintMapLibrary::GenericMap_Find(const void* TargetMap, const UMapProperty* MapProperty, const void* KeyPtr, void* ValuePtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		uint8* Result = MapHelper.FindValueFromHash(KeyPtr);
		if(Result && ValuePtr)
		{
			MapProperty->ValueProp->CopyCompleteValueFromScriptVM(ValuePtr, Result);
		}
		return Result != nullptr;
	}
	return false;
}

void UBlueprintMapLibrary::GenericMap_Keys(const void* TargetMap, const UMapProperty* MapProperty, const void* TargetArray, const UArrayProperty* ArrayProperty)
{
	if(TargetMap && TargetArray && ensure(MapProperty->KeyProp->GetID() == ArrayProperty->Inner->GetID()) )
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		ArrayHelper.EmptyValues();

		UProperty* InnerProp = ArrayProperty->Inner;

		int32 Size = MapHelper.Num();
		for( int32 I = 0; Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(I), MapHelper.GetKeyPtr(I));
				--Size;
			}
		}
	}
}

void UBlueprintMapLibrary::GenericMap_Values(const void* TargetMap, const UMapProperty* MapProperty, const void* TargetArray, const UArrayProperty* ArrayProperty)
{	
	if(TargetMap && TargetArray && ensure(MapProperty->ValueProp->GetID() == ArrayProperty->Inner->GetID()))
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		ArrayHelper.EmptyValues();

		UProperty* InnerProp = ArrayProperty->Inner;
		
		int32 Size = MapHelper.Num();
		for( int32 I = 0; I < Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(I), MapHelper.GetValuePtr(I));
				--Size;
			}
		}
	}
}

int32 UBlueprintMapLibrary::GenericMap_Length(const void* TargetMap, const UMapProperty* MapProperty)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		return MapHelper.Num();
	}
	return 0;
}

void UBlueprintMapLibrary::GenericMap_Clear(const void* TargetMap, const UMapProperty* MapProperty)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		MapHelper.EmptyValues();
	}
}

