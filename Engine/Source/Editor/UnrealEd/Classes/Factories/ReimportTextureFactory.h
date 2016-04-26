// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// ReimportTextureFactory
//~=============================================================================

#pragma once
#include "ReimportTextureFactory.generated.h"

UCLASS(hidecategories=(LightMap, DitherMipMaps, LODGroup), collapsecategories)
class UReimportTextureFactory : public UTextureFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UTexture* pOriginalTex;


	//~ Begin FReimportHandler Interface
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

private:
	//~ Begin UTextureFactory Interface
	virtual UTexture2D* CreateTexture2D( UObject* InParent, FName Name, EObjectFlags Flags ) override;
	virtual UTextureCube* CreateTextureCube( UObject* InParent, FName Name, EObjectFlags Flags ) override;
	//~ End UTextureFactory Interface
};



