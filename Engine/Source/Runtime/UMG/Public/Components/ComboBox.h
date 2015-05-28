// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComboBox.generated.h"

/**
 * The combobox allows you to display a list of options to the user in a dropdown menu for them to select one.
 */
UCLASS(Experimental, meta=( DisplayName="ComboBox (Object)" ))
class UMG_API UComboBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The list of items to be displayed on the combobox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Content)
	TArray<UObject*> Items;

	/** Called when the widget is needed for the item. */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FGenerateWidgetForObject OnGenerateWidgetEvent;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	TSharedRef<SWidget> HandleGenerateWidget(UObject* Item) const;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:

	TSharedPtr< SComboBox<UObject*> > MyComboBox;
};
