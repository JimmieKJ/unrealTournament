// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ListView.generated.h"

/**
 * Allows thousands of items to be displayed in a list.  Generates widgets dynamically for each item.
 */
UCLASS(Experimental)
class UMG_API UListView : public UTableViewBase
{
	GENERATED_UCLASS_BODY()

public:

	/** The height of each widget */
	UPROPERTY(EditAnywhere, Category=Content)
	float ItemHeight;

	/** The list of items to generate widgets for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Content)
	TArray<UObject*> Items;

	/** The selection method for the list */
	UPROPERTY(EditAnywhere, Category=Content)
	TEnumAsByte<ESelectionMode::Type> SelectionMode;

	/** Called when a widget needs to be generated */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnGenerateRowUObject OnGenerateRowEvent;

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	TSharedPtr< SListView<UObject*> > MyListView;

	TSharedRef<ITableRow> HandleOnGenerateRow(UObject* Item, const TSharedRef< STableViewBase >& OwnerTable) const;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
