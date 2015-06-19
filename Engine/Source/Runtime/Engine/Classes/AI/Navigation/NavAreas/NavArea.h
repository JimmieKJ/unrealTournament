// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavArea.generated.h"

/** Class containing definition of a navigation area */
UCLASS(DefaultToInstanced, abstract, Config=Engine, Blueprintable)
class ENGINE_API UNavArea : public UObject
{
	GENERATED_UCLASS_BODY()

	/** travel cost multiplier for path distance */
	UPROPERTY(EditAnywhere, Category=NavArea, config, meta=(ClampMin = "0.0"))
	float DefaultCost;

protected:
	/** entering cost */
	UPROPERTY(EditAnywhere, Category=NavArea, config, meta=(ClampMin = "0.0"))
	float FixedAreaEnteringCost;

public:
	/** area color in navigation view */
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	FColor DrawColor;

#if CPP
	union
	{
		struct
		{
#endif
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent0 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent1 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent2 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent3 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent4 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent5 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent6 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent7 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent8 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent9 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent10 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent11 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent12 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent13 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent14 : 1;
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	uint32 bSupportsAgent15 : 1;
#if CPP
		};
		uint32 SupportedAgentsBits;
	};
#endif

	virtual void PostInitProperties() override;
	virtual void FinishDestroy() override;

	FORCEINLINE uint16 GetAreaFlags() const { return AreaFlags; }
	FORCEINLINE bool HasFlags(uint16 InFlags) const { return (InFlags & AreaFlags) != 0; }

	FORCEINLINE bool IsSupportingAgent(int32 AgentIndex) const { return (AgentIndex >= 0 && AgentIndex < 16) ? !!(SupportedAgentsBits & (1 << AgentIndex)) : false; }

	/** Get the fixed area entering cost. */
	virtual float GetFixedAreaEnteringCost() { return FixedAreaEnteringCost; }

	/** Retrieved color declared for AreaDefinitionClass */
	static FColor GetColor(UClass* AreaDefinitionClass);

#if WITH_EDITOR
	/** setup bSupportsAgentX properties */
	virtual void UpdateAgentConfig();
#endif
	/** copy properties from other area */
	virtual void CopyFrom(TSubclassOf<UNavArea> AreaClass);

protected:

	/** these flags will be applied to navigation data along with AreaID */
	uint16 AreaFlags;
};
