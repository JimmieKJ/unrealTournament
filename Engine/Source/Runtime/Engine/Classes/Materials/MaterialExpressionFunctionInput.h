// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFunctionInput.generated.h"

/** Supported input types */
UENUM()
enum EFunctionInputType
{
	FunctionInput_Scalar,
	FunctionInput_Vector2,
	FunctionInput_Vector3,
	FunctionInput_Vector4,
	FunctionInput_Texture2D,
	FunctionInput_TextureCube,
	FunctionInput_StaticBool,
	FunctionInput_MaterialAttributes,
	FunctionInput_MAX,
};

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionFunctionInput : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Used for previewing when editing the function, also temporarily used to connect to the outside material when compiling that material. */
	UPROPERTY(meta=(RequiredInput = "false"))
	FExpressionInput Preview;

	/** The input's name, which will be drawn on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	FString InputName;

	/** The input's description, which will be used as a tooltip on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	FString Description;

	/** Id of this input, used to maintain references through name changes. */
	UPROPERTY()
	FGuid Id;

	/** 
	 * Type of this input.  
	 * Input code chunks will be cast to this type, and a compiler error will be emitted if the cast fails.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	TEnumAsByte<enum EFunctionInputType> InputType;

	/** Value used to preview this input when editing the material function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput, meta=(OverridingInputProperty = "Preview"))
	FVector4 PreviewValue;

	/** Whether to use the preview value or texture as the default value for this input. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	uint32 bUsePreviewValueAsDefault:1;

	/** Controls where the input is displayed relative to the other inputs. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	int32 SortPriority;

	/** 
	 * true when this expression is being compiled in a function preview, 
	 * false when this expression is being compiled into a material that uses the function.
	 * Only valid in Compile()
	 */
	UPROPERTY(transient)
	uint32 bCompilingFunctionPreview:1;


	// Begin UObject interface.
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.

	// Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif

	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;

	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
#endif
	// End UMaterialExpression Interface

	/** Generate the Id for this input. */
	ENGINE_API void ConditionallyGenerateId(bool bForce);

	/** Validate InputName.  Must be called after InputName is changed to prevent duplicate inputs. */
	ENGINE_API void ValidateName();


private:

	/** Helper function which compiles this expression for previewing. */
	int32 CompilePreviewValue(FMaterialCompiler* Compiler, int32 MultiplexIndex=INDEX_NONE);
};



