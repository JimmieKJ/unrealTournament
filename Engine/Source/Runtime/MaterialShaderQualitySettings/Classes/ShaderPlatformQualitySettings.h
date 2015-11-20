// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"
#include "ShaderPlatformQualitySettings.generated.h"

/**
* 
*/

// FMaterialQualityOverrides represents the full set of possible material overrides per quality level.
USTRUCT()
struct FMaterialQualityOverrides
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Config, Meta = (DisplayName = "Enable Quality Override"))
	bool bEnableOverride;

	UPROPERTY(Config, Meta = (DisplayName = "Force Fully Rough"))
	bool bForceFullyRough;

	UPROPERTY(Config, Meta = (DisplayName = "Force Non-metal"))
	bool bForceNonMetal;

	UPROPERTY(Config, Meta = (DisplayName = "Disable Lightmap directionality"))
	bool bForceDisableLMDirectionality;
};


UCLASS(config = Engine, defaultconfig, perObjectConfig)
class MATERIALSHADERQUALITYSETTINGS_API UShaderPlatformQualitySettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY(Config)
	FMaterialQualityOverrides QualityOverrides[EMaterialQualityLevel::Num];

	FMaterialQualityOverrides& GetQualityOverrides(EMaterialQualityLevel::Type QualityLevel)
	{
		check(QualityLevel<EMaterialQualityLevel::Num);
		return QualityOverrides[(int32)QualityLevel];
	}

	const FMaterialQualityOverrides& GetQualityOverrides(EMaterialQualityLevel::Type QualityLevel) const
	{
		check(QualityLevel < EMaterialQualityLevel::Num);
		return QualityOverrides[(int32)QualityLevel];
	}

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	// End of UObject interface
#endif

	void BuildHash(EMaterialQualityLevel::Type QualityLevel, class FSHAHash& OutHash) const;
	void AppendToHashState(EMaterialQualityLevel::Type QualityLevel, class FSHA1& HashState) const;
};
