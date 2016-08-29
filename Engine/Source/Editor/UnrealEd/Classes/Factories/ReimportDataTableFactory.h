// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ReimportDataTableFactory.generated.h"

UCLASS()
class UNREALED_API UReimportDataTableFactory : public UCSVImportFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	//~ Begin FReimportHandler Interface
	virtual bool FactoryCanImport( const FString& Filename ) override;
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
};



