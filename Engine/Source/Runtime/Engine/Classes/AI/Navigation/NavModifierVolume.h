// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 
#include "GameFramework/Volume.h"
#include "NavRelevantInterface.h"
#include "NavModifierVolume.generated.h"

class UNavArea;

/** 
 *	Allows applying selected AreaClass to navmesh, using Volume's shape
 */
UCLASS(MinimalAPI, hidecategories=(Navigation))
class ANavModifierVolume : public AVolume, public INavRelevantInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Default)
	TSubclassOf<UNavArea> AreaClass;

public:
	ANavModifierVolume(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetAreaClass(TSubclassOf<UNavArea> NewAreaClass = nullptr);

	TSubclassOf<UNavArea> GetAreaClass() const { return AreaClass; }

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
