// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/Visual.h"
#include "PanelSlot.generated.h"

/** The base class for all Slots in UMG. */
UCLASS(BlueprintType)
class UMG_API UPanelSlot : public UVisual
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	class UPanelWidget* Parent;

	UPROPERTY()
	class UWidget* Content;
	
	bool IsDesignTime() const;

	virtual void SetDesiredPosition(FVector2D InPosition);

	virtual void SetDesiredSize(FVector2D InSize);

	virtual void Resize(const FVector2D& Direction, const FVector2D& Amount);

	virtual bool CanResize(const FVector2D& Direction) const;

	virtual void MoveTo(const FVector2D& Location);

	virtual bool CanMove() const;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Applies all properties to the live slot if possible. */
	virtual void SynchronizeProperties()
	{
	}

#if WITH_EDITOR

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		SynchronizeProperties();
	}

#endif
};
