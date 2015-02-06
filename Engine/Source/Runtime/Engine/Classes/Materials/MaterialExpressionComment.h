// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionComment.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionComment : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 SizeX;

	UPROPERTY()
	int32 SizeY;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComment, meta=(MultiLine=true))
	FString Text;

	/** Color to style comment with */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionComment)
	FLinearColor CommentColor;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual bool Modify( bool bAlwaysMarkDirty=true ) override;
	// End UObject Interface

	// Begin UMaterialExpression Interface
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
	// End UMaterialExpression Interface
};



