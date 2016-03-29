// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Image.generated.h"

class USlateBrushAsset;

/**
 * The image widget allows you to display a Slate Brush, or texture or material in the UI.
 *
 * * No Children
 */
UCLASS()
class UMG_API UImage : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Image to draw */
	UPROPERTY()
	USlateBrushAsset* Image_DEPRECATED;

	/** Image to draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush Brush;

	/** A bindable delegate for the Image. */
	UPROPERTY()
	FGetSlateBrush BrushDelegate;

	/** Color and opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( sRGB="true") )
	FLinearColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ColorAndOpacityDelegate;

public:

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonDownEvent;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FLinearColor InColorAndOpacity);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetOpacity(float InOpacity);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrush(const FSlateBrush& InBrush);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromAsset(USlateBrushAsset* Asset);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize = false);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromMaterial(UMaterialInterface* Material);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicMaterial();

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget Interface
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	/** Translates the bound brush data and assigns it to the cached brush used by this widget. */
	const FSlateBrush* ConvertImage(TAttribute<FSlateBrush> InImageAsset) const;

	FReply HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

protected:
	TSharedPtr<SImage> MyImage;
};
