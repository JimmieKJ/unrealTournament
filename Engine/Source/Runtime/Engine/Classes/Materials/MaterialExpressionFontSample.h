// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFontSample.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionFontSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** font resource that will be sampled */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFontSample)
	class UFont* Font;

	/** allow access to the various font pages */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFontSample)
	int32 FontTexturePage;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetLabelPadding() override { return 8; }
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;

	/** 
	 * Callback to get any texture reference this expression emits.
	 * This is used to link the compiled uniform expressions with their default texture values. 
	 * Any UMaterialExpression whose compilation creates a texture uniform expression (eg Compiler->Texture, Compiler->TextureParameter) must implement this.
	 */
	virtual UTexture* GetReferencedTexture() override;
	// End UMaterialExpression Interface
};



