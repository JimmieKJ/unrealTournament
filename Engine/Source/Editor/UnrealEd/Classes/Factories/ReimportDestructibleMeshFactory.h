// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/** Factory which allows re-import of an NxApexDestructibleAsset */

#pragma once
#include "ReimportDestructibleMeshFactory.generated.h"

UCLASS(hideCategories=Object)
class UReimportDestructibleMeshFactory : public UDestructibleMeshFactory, public FReimportHandler
{
    GENERATED_UCLASS_BODY()
public:
	virtual FText GetDisplayName() const override;
#if WITH_APEX
	// Begin FReimportHandler interface
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	virtual int32 GetPriority() const override;
#else
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) { return false; }
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) {}
	virtual EReimportResult::Type Reimport( UObject* Obj ) override { return EReimportResult::Failed; }
	virtual int32 GetPriority() const override { return -1; }
	// End FReimportHandler interface
#endif // WITH_APEX
};


