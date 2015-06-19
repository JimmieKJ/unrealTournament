// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "LandscapeGrassType.h"

#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionLandscapeGrassOutput.generated.h"

USTRUCT()
struct FGrassInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Grass)
	FName Name;

	UPROPERTY(EditAnywhere, Category = Grass)
	ULandscapeGrassType* GrassType;

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Input;

	FGrassInput()
	: GrassType(nullptr)
	{}
	
	FGrassInput(FName InName)
	: Name(InName)
	, GrassType(nullptr)
	{}
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionLandscapeGrassOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface

	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Float; }
#endif

	virtual int32 GetNumOutputs() const override { return GrassTypes.Num(); }
	virtual FString GetFunctionName() const override { return TEXT("GetGrassWeight"); }

	UPROPERTY(EditAnywhere, Category = UMaterialExpressionLandscapeGrassOutput)
	TArray<FGrassInput> GrassTypes;
};



