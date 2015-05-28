// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The name of the parameter */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionParameter)
	FName ParameterName;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionParameter)
	FName Group;

	// Begin UMaterialExpression Interface
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
#if WITH_EDITOR
	virtual bool CanRenameNode() const override { return true; }
	virtual FString GetEditableName() const override;
	virtual void SetEditableName(const FString& NewName) override;
#endif
	// End UMaterialExpression Interface

	ENGINE_API virtual FGuid& GetParameterExpressionId() override
	{
		return ExpressionGUID;
	}

	/**
	 * Get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};



