// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFunctionOutput.generated.h"

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionFunctionOutput : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The output's name, which will be drawn on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionOutput)
	FString OutputName;

	/** The output's description, which will be used as a tooltip on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionOutput, meta=(MultiLine=true))
	FString Description;

	/** Controls where the output is displayed relative to the other outputs. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionOutput)
	int32 SortPriority;

	/** Stores the expression in the material function connected to this output. */
	UPROPERTY()
	FExpressionInput A;

	/** Whether this output was previewed the last time this function was edited. */
	UPROPERTY()
	uint32 bLastPreviewed:1;

	/** Id of this input, used to maintain references through name changes. */
	UPROPERTY()
	FGuid Id;


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
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetInputName(int32 InputIndex) const override
	{
		return TEXT("");
	}
#if WITH_EDITOR
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	// End UMaterialExpression Interface


	/** Generate the Id for this input. */
	ENGINE_API void ConditionallyGenerateId(bool bForce);

	/** Validate OutputName.  Must be called after OutputName is changed to prevent duplicate outputs. */
	ENGINE_API void ValidateName();

};



