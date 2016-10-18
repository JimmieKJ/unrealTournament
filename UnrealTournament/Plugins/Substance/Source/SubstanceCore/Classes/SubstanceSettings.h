#pragma once

#include "SubstanceInstanceFactory.h"
#include "SubstanceSettings.generated.h"

#if ANDROID || IPHONE
#define SBS_MIN_MEM_BUDGET (16)
#define SBS_MAX_MEM_BUDGET (256)
#else
#define SBS_MIN_MEM_BUDGET (128)
#define SBS_MAX_MEM_BUDGET (2048)
#endif

UENUM()
enum ESubstanceEngineType
{
	SET_CPU = 0	UMETA(DisplayName = "CPU Engine"),
	SET_GPU = 1 UMETA(DisplayName = "GPU Engine"),
};

/**
 * Implements the settings for the Substance plugin.
 */
UCLASS(config = Engine, defaultconfig)
class SUBSTANCECORE_API USubstanceSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Hardware Budget", meta = (ClampMin = "16", ClampMax = "2048"))
	int32 MemoryBudgetMb;

	UPROPERTY(EditAnywhere, Config, Category = "Hardware Budget", meta = (ClampMin = "1", ClampMax = "32"))
	int32 CPUCores;

	UPROPERTY(EditAnywhere, Config, Category = "Cooking", meta = (ClampMin = "1", ClampMax = "5", DisplayName = "Mip levels count removed during cooking."))
	int32 AsyncLoadMipClip;

	UPROPERTY(EditAnywhere, Config, Category = "Cooking", meta = (DisplayName = "Default generation mode for Substances."))
	TEnumAsByte<ESubstanceGenerationMode> DefaultGenerationMode;

	UPROPERTY(EditAnywhere, Config, Category = "Cooking", meta = (DisplayName = "Substance Engine (requires editor restart to take effect.)"))
	TEnumAsByte<ESubstanceEngineType> SubstanceEngine;
};
