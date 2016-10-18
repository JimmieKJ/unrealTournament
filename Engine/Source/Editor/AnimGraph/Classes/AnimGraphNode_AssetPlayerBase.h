// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AssetPlayerBase.generated.h"

class UAnimSequenceBase;

ANIMGRAPH_API UClass* GetNodeClassForAsset(const UClass* AssetClass);
ANIMGRAPH_API bool SupportNodeClassForAsset(const UClass* AssetClass, const UClass* NodeClass);


/** Helper / intermediate for asset player graphical nodes */
UCLASS(Abstract, MinimalAPI)
class UAnimGraphNode_AssetPlayerBase : public UAnimGraphNode_Base
{
	GENERATED_BODY()
public:
	// Sync group settings for this player.  Sync groups keep related animations with different lengths synchronized.
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimationGroupReference SyncGroup;

	/** UEdGraphNode interface */
	ANIMGRAPH_API virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	ANIMGRAPH_API virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;

	virtual void SetAnimationAsset(UAnimationAsset* Asset) { check(false); /*Base function called*/ }

	/** Store off a string asset reference for later restoration */
	void SetAssetReferenceForPinRestoration(UObject* InAsset);

	/** Find an asset reference when restoring pins */
	UObject* GetAssetReferenceForPinRestoration();

private:
	/** Non-concrete reference to asset that was being used, now potentially coming from a pin */
	UPROPERTY()
	FStringAssetReference AssetReferenceForPinRestoration;
};