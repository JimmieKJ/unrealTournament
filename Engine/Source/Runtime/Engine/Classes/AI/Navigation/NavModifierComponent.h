// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavRelevantComponent.h"
#include "NavModifierComponent.generated.h"

class UNavArea;
struct FCompositeNavModifier;

UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent), hidecategories = (Activation))
class ENGINE_API UNavModifierComponent : public UNavRelevantComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Navigation)
	TSubclassOf<UNavArea> AreaClass;

	/** box extent used ONLY when owning actor doesn't have collision component */
	UPROPERTY(EditAnywhere, Category = Navigation)
	FVector FailsafeExtent;

	virtual void CalcAndCacheBounds() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetAreaClass(TSubclassOf<UNavArea> NewAreaClass);

protected:
	struct FRotatedBox
	{
		FBox Box;
		FQuat Quat;

		FRotatedBox() {}
		FRotatedBox(const FBox& InBox, const FQuat& InQuat) : Box(InBox), Quat(InQuat) {}
	};

	mutable TArray<FRotatedBox> ComponentBounds;
};
