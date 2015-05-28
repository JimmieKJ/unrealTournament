// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComboBoxWidgetStyle.h"

#include "ComboBoxString.generated.h"

/**
 * The combobox allows you to display a list of options to the user in a dropdown menu for them to select one.
 */
UCLASS(meta=( DisplayName="ComboBox (String)"))
class UMG_API UComboBoxString : public UWidget
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectionChangedEvent, FString, SelectedItem, ESelectInfo::Type, SelectionType);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOpeningEvent);

private:

	/** The default list of items to be displayed on the combobox. */
	UPROPERTY(EditAnywhere, Category=Content)
	TArray<FString> DefaultOptions;

	/** The item in the combobox to select by default */
	UPROPERTY(EditAnywhere, Category=Content)
	FString SelectedOption;

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FComboBoxStyle WidgetStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	FMargin ContentPadding;

	/** The max height of the combobox list that opens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content, AdvancedDisplay)
	float MaxListHeight;

	/**
	 * When false, the down arrow is not generated and it is up to the API consumer
	 * to make their own visual hint that this is a drop down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content, AdvancedDisplay)
	bool HasDownArrow;

public: // Events

	/** Called when the widget is needed for the item. */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FGenerateWidgetForString OnGenerateWidgetEvent;

	/** Called when a new item is selected in the combobox. */
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnSelectionChangedEvent OnSelectionChanged;

	/** Called when the combobox is opening */
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnOpeningEvent OnOpening;

public:

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void AddOption(const FString& Option);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	bool RemoveOption(const FString& Option);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	int32 FindOptionIndex(const FString& Option) const;

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	FString GetOptionAtIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void ClearOptions();

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void ClearSelection();

	/**
	 * Refreshes the list of options.  If you added new ones, and want to update the list even if it's
	 * currently being displayed use this.
	 */
	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void RefreshOptions();

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void SetSelectedOption(FString Option);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	FString GetSelectedOption() const;

	/** @return The number of options */
	UFUNCTION(BlueprintCallable, Category="ComboBox")
	int32 GetOptionCount() const;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	// Begin UObject interface
	virtual void PostLoad() override;
	// End of UObject interface

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	/** Called by slate when it needs to generate a new item for the combobox */
	virtual TSharedRef<SWidget> HandleGenerateWidget(TSharedPtr<FString> Item) const;

	/** Called by slate when the underlying comobobox selection changes */
	virtual void HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectionType);

	/** Called by slate when the underlying comobobox is opening */
	virtual void HandleOpening();

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	/** The true objects bound to the Slate combobox. */
	TArray< TSharedPtr<FString> > Options;

	/** A shared pointer to the underlying slate combobox */
	TSharedPtr< SComboBox< TSharedPtr<FString> > > MyComboBox;

	/** A shared pointer to a container that holds the comobobox content that is selected */
	TSharedPtr< SBox > ComoboBoxContent;

	/** A shared pointer to the current selected string */
	TSharedPtr<FString> CurrentOptionPtr;
};
