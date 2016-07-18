//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceFactory.h"

#include "ReimportSubstanceFactory.generated.h"

UCLASS(hideCategories=Object)
class UReimportSubstanceFactory : public USubstanceFactory, public FReimportHandler
{
public:
	GENERATED_UCLASS_BODY()

	// Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;	
	// End FReimportHandler interface
};
