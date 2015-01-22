// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Anchors.h"
#include "Components/PanelSlot.h"
#include "Slate/SConstraintCanvas.h"
#include "CanvasPanelSlot.generated.h"

USTRUCT(BlueprintType)
struct FAnchorData
{
public:
	GENERATED_USTRUCT_BODY()

public:

	/** Offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnchorData)
	FMargin Offsets;
	
	/** Anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnchorData)
	FAnchors Anchors;

	/** Alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnchorData)
	FVector2D Alignment;
};

UCLASS()
class UMG_API UCanvasPanelSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The anchoring information for the slot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Canvas Slot)")
	FAnchorData LayoutData;

	/** When AutoSize is true we use the widget's desired size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Canvas Slot)", AdvancedDisplay, meta=( DisplayName="Size To Content" ))
	bool bAutoSize;

	/** The order priority this widget is rendered in.  Higher values are rendered last (and so they will appear to be on top). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout (Canvas Slot)")
	int32 ZOrder;

public:
	/** Sets the layout data of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetLayout(const FAnchorData& InLayoutData);

	/** Gets the layout data of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FAnchorData GetLayout() const;

	/** Sets the position of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetPosition(FVector2D InPosition);

	/** Gets the position of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FVector2D GetPosition() const;

	/** Sets the size of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetSize(FVector2D InSize);

	/** Gets the size of the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FVector2D GetSize() const;

	/** Sets the offset data of the slot, which could be position and size, or margins depending on the anchor points */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetOffsets(FMargin InOffset);

	/** Gets the offset data of the slot, which could be position and size, or margins depending on the anchor points */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FMargin GetOffsets() const;
	
	/** Sets the anchors on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetAnchors(FAnchors InAnchors);

	/** Gets the anchors on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FAnchors GetAnchors() const;

	/** Sets the alignment on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetAlignment(FVector2D InAlignment);

	/** Gets the alignment on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	FVector2D GetAlignment() const;

	/** Sets if the slot to be auto-sized */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetAutoSize(bool InbAutoSize);

	/** Gets if the slot to be auto-sized */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	bool GetAutoSize() const;

	/** Sets the z-order on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	void SetZOrder(int32 InZOrder);

	/** Gets the z-order on the slot */
	UFUNCTION(BlueprintCallable, Category="Layout (Canvas Slot)")
	int32 GetZOrder() const;

public:

	/** Sets the anchors on the slot */
	UFUNCTION()
	void SetMinimum(FVector2D InMinimumAnchors);

	/** Sets the anchors on the slot */
	UFUNCTION()
	void SetMaximum(FVector2D InMaximumAnchors);

public:

	void BuildSlot(TSharedRef<SConstraintCanvas> Canvas);

	virtual void SetDesiredPosition(FVector2D InPosition) override;

	virtual void SetDesiredSize(FVector2D InSize) override;

	virtual void Resize(const FVector2D& Direction, const FVector2D& Amount) override;

	virtual bool CanResize(const FVector2D& Direction) const override;

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	// UObject interface
	virtual void PreEditChange(class FEditPropertyChain& PropertyAboutToChange);
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent);
	// End of UObject interface

	/** Stores the current layout information about the slot and parent canvas. */
	void SaveBaseLayout();

	/** Compares the saved base layout against the current state.  Updates the necessary properties to maintain a stable position. */
	void RebaseLayout(bool PreserveSize = true);
#endif

private:
	SConstraintCanvas::FSlot* Slot;

#if WITH_EDITOR
	FGeometry PreEditGeometry;
	FAnchorData PreEditLayoutData;
#endif
};