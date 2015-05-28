// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISenseConfig_Sight.generated.h"

class UAISense_Sight;

UCLASS(meta = (DisplayName = "AI Sight config"))
class AIMODULE_API UAISenseConfig_Sight : public UAISenseConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Sight> Implementation;

	/** Maximum sight distance to notice a target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	float SightRadius;

	/** Maximum sight distance to see target that has been already seen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	float LoseSightRadius;

	/** How far to the side AI can see, in degrees. Use SetPeripheralVisionAngle to change the value at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	float PeripheralVisionAngleDegrees;

	/** */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	FAISenseAffiliationFilter DetectionByAffiliation;

	/** If not an InvalidRange (which is the default), we will always be able to see the target that has already been seen if they are within this range of their last seen location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	float AutoSuccessRangeFromLastSeenLocation;
		
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG
	//----------------------------------------------------------------------//
	virtual void DrawDebugInfo(UCanvas& Canvas, UAIPerceptionComponent& PerceptionComponent) const override;
#endif // !UE_BUILD_SHIPPING
};