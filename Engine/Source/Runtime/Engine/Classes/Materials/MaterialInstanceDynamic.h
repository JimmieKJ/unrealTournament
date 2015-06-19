// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialInstance.h"
#include "MaterialInstanceDynamic.generated.h"

UCLASS(hidecategories=Object, collapsecategories, BlueprintType, MinimalAPI)
class UMaterialInstanceDynamic : public UMaterialInstance
{
	GENERATED_UCLASS_BODY()

	/** Set a MID scalar (float) parameter value */
	UFUNCTION(BlueprintCallable, meta=(Keywords = "SetFloatParameterValue"), Category="Rendering|Material")
	ENGINE_API void SetScalarParameterValue(FName ParameterName, float Value);

	/** Get the current scalar (float) parameter value from an MID */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetScalarParameterValue", Keywords = "GetFloatParameterValue"), Category="Rendering|Material")
	float K2_GetScalarParameterValue(FName ParameterName);

	/** Set an MID texture parameter value */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	ENGINE_API void SetTextureParameterValue(FName ParameterName, class UTexture* Value);

	/** Get the current MID texture parameter value */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetTextureParameterValue"), Category="Rendering|Material")
	class UTexture* K2_GetTextureParameterValue(FName ParameterName);

	/** Set an MID vector parameter value */
	UFUNCTION(BlueprintCallable, meta=(Keywords = "SetColorParameterValue"), Category="Rendering|Material")
	ENGINE_API void SetVectorParameterValue(FName ParameterName, FLinearColor Value);

	/** Get the current MID vector parameter value */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetVectorParameterValue", Keywords = "GetColorParameterValue"), Category="Rendering|Material")
	FLinearColor K2_GetVectorParameterValue(FName ParameterName);
	
	/**
	 * Interpolates the scalar and vector parameters of this material instance based on two other material instances, and an alpha blending factor
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "InterpolateMaterialInstanceParameters"), Category="Rendering|Material")
	void K2_InterpolateMaterialInstanceParams(UMaterialInstance* MaterialA, UMaterialInstance* MaterialB, float Alpha);

	/** Copies over parameters given a material interface */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "CopyMaterialInstanceParameters"), Category="Rendering|Material")
	void K2_CopyMaterialInstanceParameters(UMaterialInterface* SourceMaterialToCopyFrom);

	/**
	 * Create a material instance dynamic parented to the specified material.
	 */
	ENGINE_API static UMaterialInstanceDynamic* Create(class UMaterialInterface* ParentMaterial, class UObject* InOuter);

	/**
	 * Set the value of the given font parameter.  
	 * @param ParameterName - The name of the font parameter
	 * @param OutFontValue - New font value to set for this MIC
	 * @param OutFontPage - New font page value to set for this MIC
	 */
	ENGINE_API void SetFontParameterValue(FName ParameterName, class UFont* FontValue, int32 FontPage);

	/** Remove all parameter values */
	ENGINE_API void ClearParameterValues();

	/**
	 * Copy parameter values from another material instance. This will copy only
	 * parameters explicitly overridden in that material instance!!
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "CopyParameterOverrides"), Category="Rendering|Material")
	ENGINE_API void CopyParameterOverrides(UMaterialInstance* MaterialInstance);

};

