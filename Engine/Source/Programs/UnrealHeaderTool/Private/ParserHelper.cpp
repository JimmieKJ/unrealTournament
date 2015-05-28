// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealHeaderTool.h"
#include "ParserHelper.h"
#include "DefaultValueHelper.h"

/////////////////////////////////////////////////////
// FClassMetaData

/**
 * Finds the metadata for the struct specified
 * 
 * @param	Struct	the struct to search for
 *
 * @return	pointer to the metadata for the struct specified, or NULL
 *			if the struct doesn't exist in the list (for example, if it
 *			is declared in a package that is already compiled and has had its
 *			source stripped)
 */
FStructData* FClassMetaData::FindStructData( UScriptStruct* Struct )
{
	FStructData* Result = NULL;
	
	TScopedPointer<FStructData>* pStructData = StructData.Find(Struct);
	if ( pStructData != NULL )
	{
		Result = pStructData->GetOwnedPointer();
	}

	if ( Result == NULL )
	{
		UClass* OwnerClass = Struct->GetOwnerClass();
		FClassMetaData* OwnerClassData = GScriptHelper.FindClassData(OwnerClass);
		if ( OwnerClassData && OwnerClassData != this )
		{
			Result = OwnerClassData->FindStructData(Struct);
		}
	}

	return Result;
}

/**
 * Finds the metadata for the property specified
 *
 * @param	Prop	the property to search for
 *
 * @return	pointer to the metadata for the property specified, or NULL
 *			if the property doesn't exist in the list (for example, if it
 *			is declared in a package that is already compiled and has had its
 *			source stripped)
 */
FTokenData* FClassMetaData::FindTokenData( UProperty* Prop )
{
	check(Prop);

	FTokenData* Result = nullptr;
	UObject* Outer = Prop->GetOuter();
	UClass* OuterClass = nullptr;
	if (Outer->IsA<UStruct>())
	{
		Result = GlobalPropertyData.Find(Prop);

		if (Result == nullptr)
		{
			OuterClass = Cast<UClass>(Outer);

			if (Result == nullptr && OuterClass != nullptr && OuterClass->GetSuperClass() != OuterClass)
			{
				OuterClass = OuterClass->GetSuperClass();
			}
		}
	}
	else
	{
		UFunction* OuterFunction = Cast<UFunction>(Outer);
		if ( OuterFunction != NULL )
		{
			// function parameter, return, or local property
			FFunctionData* FuncData = nullptr;
			if (FFunctionData::TryFindForFunction(OuterFunction, FuncData))
			{
				FPropertyData& FunctionParameters = FuncData->GetParameterData();
				Result = FunctionParameters.Find(Prop);
				if ( Result == NULL )
				{
					Result = FuncData->GetReturnTokenData();
				}
			}
			else
			{
				OuterClass = OuterFunction->GetOwnerClass();
			}
		}
		else
		{
			// struct property
			UScriptStruct* OuterStruct = Cast<UScriptStruct>(Outer);
			check(OuterStruct != NULL);

			TScopedPointer<FStructData>* pStructInfo = StructData.Find(OuterStruct);
			if ( pStructInfo != NULL )
			{
				FStructData* StructInfo = pStructInfo->GetOwnedPointer();
				check(StructInfo);

				FPropertyData& StructProperties = StructInfo->GetStructPropertyData();
				Result = StructProperties.Find(Prop);
			}
			else
			{
				OuterClass = OuterStruct->GetOwnerClass();
			}
		}
	}

	if (Result == nullptr && OuterClass != nullptr)
	{
		FClassMetaData* SuperClassData = GScriptHelper.FindClassData(OuterClass);
		if (SuperClassData && SuperClassData != this)
		{
			Result = SuperClassData->FindTokenData(Prop);
		}
	}

	return Result;
}

/////////////////////////////////////////////////////
// FPropertyBase

const TCHAR* FPropertyBase::GetPropertyTypeText( EPropertyType Type )
{
	switch ( Type )
	{
		CASE_TEXT(CPT_None);
		CASE_TEXT(CPT_Byte);
		CASE_TEXT(CPT_Int8);
		CASE_TEXT(CPT_Int16);
		CASE_TEXT(CPT_Int);
		CASE_TEXT(CPT_Int64);
		CASE_TEXT(CPT_UInt16);
		CASE_TEXT(CPT_UInt32);
		CASE_TEXT(CPT_UInt64);
		CASE_TEXT(CPT_Bool);
		CASE_TEXT(CPT_Bool8);
		CASE_TEXT(CPT_Bool16);
		CASE_TEXT(CPT_Bool32);
		CASE_TEXT(CPT_Bool64);
		CASE_TEXT(CPT_Float);
		CASE_TEXT(CPT_Double);
		CASE_TEXT(CPT_ObjectReference);
		CASE_TEXT(CPT_Interface);
		CASE_TEXT(CPT_Name);
		CASE_TEXT(CPT_Delegate);
		CASE_TEXT(CPT_Range);
		CASE_TEXT(CPT_Struct);
		CASE_TEXT(CPT_Vector);
		CASE_TEXT(CPT_Rotation);
		CASE_TEXT(CPT_String);
		CASE_TEXT(CPT_Text);
		CASE_TEXT(CPT_MulticastDelegate);
		CASE_TEXT(CPT_AssetObjectReference);
		CASE_TEXT(CPT_WeakObjectReference);
		CASE_TEXT(CPT_LazyObjectReference);
		CASE_TEXT(CPT_Map);
		CASE_TEXT(CPT_MAX);
	}

	return TEXT("");
}

/////////////////////////////////////////////////////
// FToken

/**
 * Copies the properties from this token into another.
 *
 * @param	Other	the token to copy this token's properties to.
 */
void FToken::Clone( const FToken& Other )
{
	// none of FPropertyBase's members require special handling
	(FPropertyBase&)*this	= (FPropertyBase&)Other;

	TokenType = Other.TokenType;
	TokenName = Other.TokenName;
	StartPos = Other.StartPos;
	StartLine = Other.StartLine;
	TokenProperty = Other.TokenProperty;

	FCString::Strncpy(Identifier, Other.Identifier, NAME_SIZE);
	FMemory::Memcpy(String, Other.String, sizeof(String));
}

/////////////////////////////////////////////////////
// FAdvancedDisplayParameterHandler
FAdvancedDisplayParameterHandler::FAdvancedDisplayParameterHandler(const TMap<FName, FString>* MetaData)
	: NumberLeaveUnmarked(-1), AlreadyLeft(0), bUseNumber(false)
{
	if(MetaData)
	{
		const FString* FoundString = MetaData->Find(FName(TEXT("AdvancedDisplay")));
		if(FoundString)
		{
			FoundString->ParseIntoArray(ParametersNames, TEXT(","), true);
			for(int32 NameIndex = 0; NameIndex < ParametersNames.Num();)
			{
				FString& ParameterName = ParametersNames[NameIndex];
				ParameterName.Trim();
				ParameterName.TrimTrailing();
				if(ParameterName.IsEmpty())
				{
					ParametersNames.RemoveAtSwap(NameIndex);
				}
				else
				{
					++NameIndex;
				}
			}
			if(1 == ParametersNames.Num())
			{
				bUseNumber = FDefaultValueHelper::ParseInt(ParametersNames[0], NumberLeaveUnmarked);
			}
		}
	}
}

bool FAdvancedDisplayParameterHandler::ShouldMarkParameter(const FString& ParameterName)
{
	if(bUseNumber)
	{
		if(NumberLeaveUnmarked < 0)
		{
			return false;
		}
		if(AlreadyLeft < NumberLeaveUnmarked)
		{
			AlreadyLeft++;
			return false;
		}
		return true;
	}
	return ParametersNames.Contains(ParameterName);
}

bool FAdvancedDisplayParameterHandler::CanMarkMore() const
{
	return bUseNumber ? (NumberLeaveUnmarked > 0) : (0 != ParametersNames.Num());
}

TMap<UFunction*, TSharedRef<FFunctionData> > FFunctionData::FunctionDataMap;

FFunctionData* FFunctionData::FindForFunction(UFunction* Function)
{
	auto* Output = FunctionDataMap.Find(Function);

	check(Output);

	return &(*Output).Get();
}

FFunctionData* FFunctionData::Add(UFunction* Function)
{
	auto Output = FunctionDataMap.Add(Function, MakeShareable(new FFunctionData()));

	return &Output.Get();
}

FFunctionData* FFunctionData::Add(const FFuncInfo& FunctionInfo)
{
	auto Output = FunctionDataMap.Add(FunctionInfo.FunctionReference, MakeShareable(new FFunctionData(FunctionInfo)));

	return &Output.Get();
}

bool FFunctionData::TryFindForFunction(UFunction* Function, FFunctionData*& OutData)
{
	auto* Output = FunctionDataMap.Find(Function);

	if (!Output)
	{
		return false;
	}

	OutData = &(*Output).Get();
	return true;
}
