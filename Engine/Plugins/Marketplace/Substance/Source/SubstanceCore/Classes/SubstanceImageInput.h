// Copyright 2014 Allegorithmic All Rights Reserved.
#pragma once

#include "Engine.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceImageInput.generated.h"

namespace Substance
{
	struct FImageInputInstance;
}

UCLASS(hideCategories=Object, MinimalAPI, BlueprintType)
class USubstanceImageInput : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	FByteBulkData ImageRGB;

	FByteBulkData ImageA;

	/** Color channels compression (0: disable, 1: auto, 2-99: manual) */
	UPROPERTY(EditAnywhere, Category = General)
	int32 CompressionRGB;

	/** Alpha channel compression (0: disable, 1: auto, 2-99: manual) */
	UPROPERTY(EditAnywhere, Category = General)
	int32 CompressionAlpha;
	
	UPROPERTY(Category = General, VisibleAnywhere)
	int32 SizeX;

	UPROPERTY(Category = General, VisibleAnywhere)
	int32 SizeY;

	UPROPERTY(Category = General, VisibleAnywhere)
	int32 NumComponents;

	UPROPERTY(Category = File, VisibleAnywhere, BlueprintReadOnly)
	FString SourceFilePath;

	UPROPERTY(Category = File, VisibleAnywhere, BlueprintReadOnly)
	FString SourceFileTimestamp;

	UPROPERTY(Transient)
	TArray< USubstanceGraphInstance* > Consumers;

public:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	virtual void Serialize(FArchive& Ar) override;
	virtual FString GetDesc() override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type) override;
};
