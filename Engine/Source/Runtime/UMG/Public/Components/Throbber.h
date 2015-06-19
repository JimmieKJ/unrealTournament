// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SThrobber.h"
#include "Throbber.generated.h"

class USlateBrushAsset;

/**
 * A Throbber widget that shows several zooming circles in a row.
 */
UCLASS()
class UMG_API UThrobber : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	
	/** How many pieces there are */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( ClampMin="1", ClampMax="25", UIMin="1", UIMax="25" ))
	int32 NumberOfPieces;

	/** Should the pieces animate horizontally? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool bAnimateHorizontally;

	/** Should the pieces animate vertically? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool bAnimateVertically;

	/** Should the pieces animate their opacity? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool bAnimateOpacity;

	/** Image to use for each segment of the throbber */
	UPROPERTY()
	USlateBrushAsset* PieceImage_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush Image;

public:

	/** Sets how many pieces there are */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetNumberOfPieces(int32 InNumberOfPieces);

	/** Sets whether the pieces animate horizontally. */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimateHorizontally(bool bInAnimateHorizontally);

	/** Sets whether the pieces animate vertically. */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimateVertically(bool bInAnimateVertically);

	/** Sets whether the pieces animate their opacity. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetAnimateOpacity(bool bInAnimateOpacity);

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

	// Begin UObject interface
	virtual void PostLoad() override;
	// End of UObject interface

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

private:

	/** Gets the combined value of the animation properties as a single SThrobber::EAnimation value. */
	SThrobber::EAnimation GetAnimation() const;

private:
	/** The Throbber widget managed by this object. */
	TSharedPtr<SThrobber> MyThrobber;
};
