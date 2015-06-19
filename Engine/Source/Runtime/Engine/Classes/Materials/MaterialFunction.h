// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MaterialFunction.generated.h"

/**
 * A Material Function is a collection of material expressions that can be reused in different materials
 */
UCLASS(hidecategories=object, MinimalAPI)
class UMaterialFunction : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Used by materials using this function to know when to recompile. */
	UPROPERTY(duplicatetransient)
	FGuid StateId;

#if WITH_EDITORONLY_DATA
	/** Used in the material editor, points to the function asset being edited, which this function is just a preview for. */
	UPROPERTY(transient)
	class UMaterialFunction* ParentFunction;

#endif // WITH_EDITORONLY_DATA
	/** Description of the function which will be displayed as a tooltip wherever the function is used. */
	UPROPERTY(EditAnywhere, Category=MaterialFunction, AssetRegistrySearchable)
	FString Description;

	/** Whether to list this function in the material function library, which is a window in the material editor that lists categorized functions. */
	UPROPERTY(EditAnywhere, Category=MaterialFunction, AssetRegistrySearchable)
	uint32 bExposeToLibrary:1;

	/** 
	 * Categories that this function belongs to in the material function library.  
	 * Ideally categories should be chosen carefully so that there are not too many.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialFunction, AssetRegistrySearchable)
	TArray<FString> LibraryCategories;

	/** Array of material expressions, excluding Comments.  Used by the material editor. */
	UPROPERTY()
	TArray<class UMaterialExpression*> FunctionExpressions;

#if WITH_EDITORONLY_DATA
	/** Array of comments associated with this material; viewed in the material editor. */
	UPROPERTY()
	TArray<class UMaterialExpressionComment*> FunctionEditorComments;

	UPROPERTY(transient)
	UMaterial* PreviewMaterial;
#endif // WITH_EDITORONLY_DATA
private:
	/** Transient flag used to track re-entrance in recursive functions like IsDependent. */
	UPROPERTY(transient)
	uint32 bReentrantFlag:1;

public:

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	// End UObject interface.

	/** Recursively update all function call expressions in this function, or in nested functions. */
	void UpdateFromFunctionResource();

	/** Get the inputs and outputs that this function exposes, for a function call expression to use. */
	void GetInputsAndOutputs(TArray<struct FFunctionExpressionInput>& OutInputs, TArray<struct FFunctionExpressionOutput>& OutOutputs) const;

	int32 Compile(class FMaterialCompiler* Compiler, const struct FFunctionExpressionOutput& Output, int32 MultiplexIndex, const TArray<struct FFunctionExpressionInput>& Inputs);

	/** @return true if this function is dependent on the passed in function, directly or indirectly. */
	ENGINE_API bool IsDependent(UMaterialFunction* OtherFunction);

	/** Returns an array of the functions that this function is dependent on, directly or indirectly. */
	ENGINE_API void GetDependentFunctions(TArray<UMaterialFunction*>& DependentFunctions) const;

	/** Appends textures referenced by the expressions in this function. */
	ENGINE_API void AppendReferencedTextures(TArray<UTexture*>& InOutTextures) const;

	template<typename ExpressionType>
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
	{
		for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
		{
			const UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<const UMaterialExpressionMaterialFunctionCall>(FunctionExpressions[ExpressionIndex]);

			if (FunctionExpression)
			{
				if (FunctionExpression->MaterialFunction)
				{
					FunctionExpression->MaterialFunction->GetAllParameterNames<const ExpressionType>(OutParameterNames, OutParameterIds);
				}
			}
			else
			{
				const ExpressionType* ParameterExpression = Cast<const ExpressionType>(FunctionExpressions[ExpressionIndex]);

				if (ParameterExpression)
				{
					ParameterExpression->GetAllParameterNames(OutParameterNames, OutParameterIds);
				}
			}
		}

		check(OutParameterNames.Num() == OutParameterIds.Num());
	}

#if WITH_EDITOR
	ENGINE_API UMaterial* GetPreviewMaterial();

	void UpdateInputOutputTypes();

	/**
	 * Checks whether a Material Function is arranged in the old style, with inputs flowing from right to left
	 */
	bool HasFlippedCoordinates() const;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(AssetRegistrySearchable)
	uint32 CombinedInputTypes;

	UPROPERTY(AssetRegistrySearchable)
	uint32 CombinedOutputTypes;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;
#endif
};



