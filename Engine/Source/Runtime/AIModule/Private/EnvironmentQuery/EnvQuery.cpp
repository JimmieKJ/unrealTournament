// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"

UEnvQuery::UEnvQuery(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

static bool HasNamedValue(const FName& ParamName, const TArray<FEnvNamedValue>& NamedValues)
{
	for (int32 ValueIndex = 0; ValueIndex < NamedValues.Num(); ValueIndex++)
	{
		if (NamedValues[ValueIndex].ParamName == ParamName)
		{
			return true;
		}
	}

	return false;
}

static void AddNamedValue(const FName& ParamName, const EEnvQueryParam::Type& ParamType, float Value,
						  TArray<FEnvNamedValue>& NamedValues, TArray<FName>& RequiredParams)
{
	if (ParamName != NAME_None)
	{
		if (!HasNamedValue(ParamName, NamedValues))
		{
			FEnvNamedValue NewValue;
			NewValue.ParamName = ParamName;
			NewValue.ParamType = ParamType;
			NewValue.Value = Value;
			NamedValues.Add(NewValue);
		}

		RequiredParams.AddUnique(ParamName);
	}
}

#define GET_STRUCT_NAME_CHECKED(StructName) \
	((void)sizeof(StructName), TEXT(#StructName))

static void AddNamedValuesFromObject(const UObject* Ob, TArray<FEnvNamedValue>& NamedValues, TArray<FName>& RequiredParams)
{
	if (Ob == NULL)
	{
		return;
	}

	for (UProperty* TestProperty = Ob->GetClass()->PropertyLink; TestProperty; TestProperty = TestProperty->PropertyLinkNext)
	{
		UStructProperty* TestStruct = Cast<UStructProperty>(TestProperty);
		if (TestStruct == NULL)
		{
			continue;
		}

		FString TypeDesc = TestStruct->GetCPPType(NULL, CPPF_None);
		if (TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FEnvIntParam)))
		{
			const FEnvIntParam* PropertyValue = TestStruct->ContainerPtrToValuePtr<FEnvIntParam>(Ob);
			AddNamedValue(PropertyValue->ParamName, EEnvQueryParam::Int, *((float*)&PropertyValue->Value), NamedValues, RequiredParams);
		}
		else if (TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FEnvFloatParam)))
		{
			const FEnvFloatParam* PropertyValue = TestStruct->ContainerPtrToValuePtr<FEnvFloatParam>(Ob);
			AddNamedValue(PropertyValue->ParamName, EEnvQueryParam::Float, PropertyValue->Value, NamedValues, RequiredParams);
		}
		else if (TypeDesc.Contains(GET_STRUCT_NAME_CHECKED(FEnvBoolParam)))
		{
			const FEnvBoolParam* PropertyValue = TestStruct->ContainerPtrToValuePtr<FEnvBoolParam>(Ob);
			AddNamedValue(PropertyValue->ParamName, EEnvQueryParam::Bool, PropertyValue->Value ? 1.0f : -1.0f, NamedValues, RequiredParams);
		}
	}
}

#undef GET_STRUCT_NAME_CHECKED

void UEnvQuery::CollectQueryParams(TArray<FEnvNamedValue>& NamedValues) const
{
	TArray<FName> RequiredParams;

	// collect all params
	for (int32 OptionIndex = 0; OptionIndex < Options.Num(); OptionIndex++)
	{
		const UEnvQueryOption* Option = Options[OptionIndex];
		AddNamedValuesFromObject(Option->Generator, NamedValues, RequiredParams);

		for (int32 TestIndex = 0; TestIndex < Option->Tests.Num(); TestIndex++)
		{
			const UEnvQueryTest* TestOb = Option->Tests[TestIndex];
			AddNamedValuesFromObject(TestOb, NamedValues, RequiredParams);
		}
	}

	// remove unnecessary params
	for (int32 ValueIndex = NamedValues.Num() - 1; ValueIndex >= 0; ValueIndex--)
	{
		if (!RequiredParams.Contains(NamedValues[ValueIndex].ParamName))
		{
			NamedValues.RemoveAt(ValueIndex);
		}
	}
}
