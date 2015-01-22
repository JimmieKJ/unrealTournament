// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 *	A material expression that routes particle emitter parameters to the
 *	material.
 */

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDynamicParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionDynamicParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	The names of the parameters.
	 *	These will show up in Cascade when editing a particle system
	 *	that uses the material it is in...
	 */
	UPROPERTY(EditAnywhere, editfixedsize, Category=MaterialExpressionDynamicParameter)
	TArray<FString> ParamNames;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual TArray<FExpressionOutput>& GetOutputs() override;
	virtual int32 GetWidth() const override;
	virtual int32 GetLabelPadding() override { return 8; }
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
	// End UMaterialExpression Interface

	/**
	 * Iterate through all of the expression nodes until we find another 
	 * dynamic parameter we can copy the parameter names from
	 */
	ENGINE_API void UpdateDynamicParameterNames();

	/**
	 * Copy the parameters names from the specified dynamic parameter
	 *
	 * @param	FromParam	The param to copy the names from
	 * @return	true if sucessful
	 */
	bool CopyDynamicParameterNames(const UMaterialExpressionDynamicParameter* FromParam);
};



