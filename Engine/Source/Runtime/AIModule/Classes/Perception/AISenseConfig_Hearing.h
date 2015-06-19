// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISenseConfig_Hearing.generated.h"

class UAISense_Hearing;

UCLASS(meta = (DisplayName = "AI Hearing config"))
class AIMODULE_API UAISenseConfig_Hearing : public UAISenseConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Hearing> Implementation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	float HearingRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", meta = (EditCondition = "bUseLoSHearing"))
	float LoSHearingRange;

	/** Warning: has significant runtime cost */
	UPROPERTY()
	uint32 bUseLoSHearing : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	FAISenseAffiliationFilter DetectionByAffiliation;

	UAISenseConfig_Hearing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG
	//----------------------------------------------------------------------//
	virtual void DrawDebugInfo(UCanvas& Canvas, UAIPerceptionComponent& PerceptionComponent) const override;
#endif // !UE_BUILD_SHIPPING
};
