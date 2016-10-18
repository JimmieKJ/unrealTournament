// Copyright 2014 Allegorithmic All Rights Reserved.

#pragma once

#include "SubstanceCoreTypedefs.h"
#include "SubstanceTexture2D.generated.h"

UCLASS(hideCategories=Object, MinimalAPI)
class USubstanceTexture2D : public UTexture2DDynamic
{
	GENERATED_UCLASS_BODY()

	FGuid OutputGuid;

    struct Substance::FOutputInstance* OutputCopy;
	
	UPROPERTY(VisibleAnywhere, Category="Substance")
	class USubstanceGraphInstance* ParentInstance;

	/** The addressing mode to use for the X axis.								*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (DisplayName = "X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis.								*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (DisplayName = "Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressY;

	TIndirectArray<struct FTexture2DMipMap> Mips;

	// Begin UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	// End UObject interface.

	bool CanEditChange(const UProperty* InProperty) const;

	// Begin UTexture interface.
	virtual FString GetDesc() override;
	virtual void UpdateResource() override;
	virtual FTextureResource* CreateResource() override;
	// End UTexture interface.
};
