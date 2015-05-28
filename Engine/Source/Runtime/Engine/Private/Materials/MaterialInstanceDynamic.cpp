// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MaterialInstanceDynamic.cpp: MaterialInstanceDynamic implementation.
==============================================================================*/

#include "EnginePrivate.h"
#include "MaterialInstanceSupport.h"

UMaterialInstanceDynamic::UMaterialInstanceDynamic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMaterialInstanceDynamic* UMaterialInstanceDynamic::Create(UMaterialInterface* ParentMaterial, UObject* InOuter)
{
	UObject* Outer = InOuter ? InOuter : GetTransientPackage();
	UMaterialInstanceDynamic* MID = NewObject<UMaterialInstanceDynamic>(Outer);
	MID->SetParentInternal(ParentMaterial, false);
	return MID;
}

void UMaterialInstanceDynamic::SetVectorParameterValue(FName ParameterName, FLinearColor Value)
{
	SetVectorParameterValueInternal(ParameterName,Value);
}

FLinearColor UMaterialInstanceDynamic::K2_GetVectorParameterValue(FName ParameterName)
{
	FLinearColor Result(0,0,0);
	Super::GetVectorParameterValue(ParameterName, Result);
	return Result;
}

void UMaterialInstanceDynamic::SetScalarParameterValue(FName ParameterName, float Value)
{
	SetScalarParameterValueInternal(ParameterName,Value);
}

float UMaterialInstanceDynamic::K2_GetScalarParameterValue(FName ParameterName)
{
	float Result = 0.f;
	Super::GetScalarParameterValue(ParameterName, Result);
	return Result;
}

void UMaterialInstanceDynamic::SetTextureParameterValue(FName ParameterName, UTexture* Value)
{
	SetTextureParameterValueInternal(ParameterName,Value);
}

UTexture* UMaterialInstanceDynamic::K2_GetTextureParameterValue(FName ParameterName)
{
	UTexture* Result = NULL;
	Super::GetTextureParameterValue(ParameterName, Result);
	return Result;
}

void UMaterialInstanceDynamic::SetFontParameterValue(FName ParameterName,class UFont* FontValue,int32 FontPage)
{
	SetFontParameterValueInternal(ParameterName,FontValue,FontPage);
}

void UMaterialInstanceDynamic::ClearParameterValues()
{
	ClearParameterValuesInternal();
}

void UMaterialInstanceDynamic::K2_InterpolateMaterialInstanceParams(UMaterialInstance* MaterialA, UMaterialInstance* MaterialB, float Alpha)
{
	if (MaterialA != NULL && MaterialB != NULL)
	{
		// Interpolate the scalar parameters common to both materials
		for( int32 ParamIdx = 0; ParamIdx < MaterialA->ScalarParameterValues.Num(); ParamIdx++ )
		{
			FScalarParameterValue* MaterialAParam = &MaterialA->ScalarParameterValues[ParamIdx];
			FScalarParameterValue* MaterialBParam = GameThread_FindParameterByName(MaterialB->ScalarParameterValues, MaterialAParam->ParameterName);

			// Found a parameter common to both...
			if( MaterialBParam )
			{
				SetScalarParameterValue(MaterialAParam->ParameterName, FMath::Lerp(MaterialAParam->ParameterValue, MaterialBParam->ParameterValue, Alpha));
			}
		}

		// Interpolate the vector parameters common to both
		for( int32 ParamIdx = 0; ParamIdx < MaterialA->VectorParameterValues.Num(); ParamIdx++ )
		{
			FVectorParameterValue* MaterialAParam = &MaterialA->VectorParameterValues[ParamIdx];
			FVectorParameterValue* MaterialBParam = GameThread_FindParameterByName(MaterialB->VectorParameterValues, MaterialAParam->ParameterName);

			// Found a parameter common to both...
			if( MaterialBParam )
			{
				SetVectorParameterValue(MaterialAParam->ParameterName, FMath::Lerp(MaterialAParam->ParameterValue, MaterialBParam->ParameterValue, Alpha));
			}
		}
	}
}

void UMaterialInstanceDynamic::K2_CopyMaterialInstanceParameters(UMaterialInterface* SourceMaterialToCopyFrom)
{
	CopyMaterialInstanceParameters(SourceMaterialToCopyFrom);
}

void UMaterialInstanceDynamic::CopyParameterOverrides(UMaterialInstance* MaterialInstance)
{
	ClearParameterValues();
	VectorParameterValues = MaterialInstance->VectorParameterValues;
	ScalarParameterValues = MaterialInstance->ScalarParameterValues;
	TextureParameterValues = MaterialInstance->TextureParameterValues;
	FontParameterValues = MaterialInstance->FontParameterValues;
	InitResources();
}
