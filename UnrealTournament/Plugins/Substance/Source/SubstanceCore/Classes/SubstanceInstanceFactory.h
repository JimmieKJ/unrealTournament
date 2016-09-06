// Copyright 2014 Allegorithmic All Rights Reserved.

#pragma once

#include "Engine.h"
#include "SubstanceInstanceFactory.generated.h"

namespace Substance
{
	struct FPackage;
}

UENUM(BlueprintType)
enum ESubstanceGenerationMode
{
	SGM_PlatformDefault      =0,
	SGM_Baked                =1,
	SGM_OnLoadSync           =2,
	SGM_OnLoadSyncAndCache	 =3,
	SGM_OnLoadAsync          =4,
	SGM_OnLoadAsyncAndCache	 =5,
	SGM_MAX                  =6
};

UCLASS(hideCategories=Object, MinimalAPI, BlueprintType)
class USubstanceInstanceFactory : public UObject
{
	GENERATED_UCLASS_BODY()

public:

    struct Substance::FPackage* SubstancePackage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Substance")
	TEnumAsByte<ESubstanceGenerationMode> GenerationMode; //! @todo: allow per-platform setting

	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	// End UObject interface.

	ESubstanceGenerationMode GetGenerationMode() const;

	bool ShouldCacheOutput() const;

	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
};
