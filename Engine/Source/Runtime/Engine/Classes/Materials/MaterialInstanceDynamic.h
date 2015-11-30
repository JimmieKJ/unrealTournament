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
	 * The output is the object itself (this).
	 * Supports the case SourceA==this || SourceB==this
	 * Both material have to be from the same base material
	 * @param SourceA value that is used for Alpha=0, silently ignores the case if 0
	 * @param SourceB value that is used for Alpha=1, silently ignores the case if 0
	 * @param Alpha usually in the range 0..1, values outside the range extrapolate
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "InterpolateMaterialInstanceParameters"), Category="Rendering|Material")
	void K2_InterpolateMaterialInstanceParams(UMaterialInstance* SourceA, UMaterialInstance* SourceB, float Alpha);

	/**
	 * Copies over parameters given a material interface (copy each instance following the hierarchy)
	 * Very slow implementation, avoid using at runtime. Hopefully we can replace ity later with something like CopyInterpParameters()
	 * The output is the object itself (this).
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "CopyMaterialInstanceParameters"), Category="Rendering|Material")
	void K2_CopyMaterialInstanceParameters(UMaterialInterface* Source);

	/**
	 * Copies over parameters given a material instance (only copy from the instance, not following the hierarchy)
	 * much faster than K2_CopyMaterialInstanceParameters(), 
	 * The output is the object itself (this).
	 * @param Source ignores the call if 0
	 */
	UFUNCTION(meta=(DisplayName = "CopyInterpParameters"), Category="Rendering|Material")
	ENGINE_API void CopyInterpParameters(UMaterialInstance* Source);

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
		
	/**
	 * Copy all interpolatable (scalar/vector) parameters from *SourceMaterialToCopyFrom to *this, using the current QualityLevel and given FeatureLevel
	 * For runtime use. More specialized and efficient than CopyMaterialInstanceParameters().
	 */
	ENGINE_API void CopyScalarAndVectorParameters(const UMaterialInterface& SourceMaterialToCopyFrom, ERHIFeatureLevel::Type FeatureLevel);
};

