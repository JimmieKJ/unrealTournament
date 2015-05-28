// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionStaticBoolParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionStaticBoolParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticBoolParameter)
	uint32 DefaultValue:1;

public:
	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual uint32 GetOutputType(int32 OutputIndex) override {return MCT_StaticBool;}
#endif
	// End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, bool& OutValue, FGuid& OutExpressionGuid) const;
};



