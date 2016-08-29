// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SInputKeySelector.h"
#include "InputKeySelector.generated.h"

/** A widget for selecting a single key or a single key with a modifier. */
UCLASS()
class UMG_API UInputKeySelector : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnKeySelected, FInputChord, SelectedKey );
	DECLARE_DYNAMIC_MULTICAST_DELEGATE( FOnIsSelectingKeyChanged );

public:
	/** The currently selected key chord. */
	UPROPERTY( BlueprintReadOnly, Category = "Key Selection" )
	FInputChord SelectedKey;

	/** The font used to display the currently selected key. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = Appearance )
	FSlateFontInfo Font;

	/** The amount of blank space around the text used to display the currently selected key. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = Appearance )
	FMargin Margin;

	/** The color of the text used to display the currently selected key. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = Appearance )
	FLinearColor ColorAndOpacity;

	/** Sets the text which is displayed while selecting keys. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = Appearance )
	FText KeySelectionText;

	/** When true modifier keys such as control and alt are allowed in the */
	/** input chord representing the selected key, if false modifier keys are ignored. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Key Selection" )
	bool bAllowModifierKeys;

public:
	/** Called whenever a new key is selected by the user. */
	UPROPERTY( BlueprintAssignable, Category = "Widget Event" )
	FOnKeySelected OnKeySelected;

	/** Called whenever the key selection mode starts or stops. */
	UPROPERTY( BlueprintAssignable, Category = "Widget Event" )
	FOnIsSelectingKeyChanged OnIsSelectingKeyChanged;

	/** Sets the currently selected key. */
	UFUNCTION( BlueprintCallable, Category = "Widget" )
	void SetSelectedKey(FInputChord InSelectedKey);

	/** Sets the text which is displayed while selecting keys. */
	UFUNCTION( BlueprintCallable, Category = "Widget" )
	void SetKeySelectionText( FText InKeySelectionText );

	/** Sets whether or not modifier keys are allowed in the selected key. */
	UFUNCTION(BlueprintCallable, Category = "Widget" )
	void SetAllowModifierKeys(bool bInAllowModifierKeys );

	/** Returns true if the widget is currently selecting a key, otherwise returns false. */
	UFUNCTION( BlueprintCallable, Category = "Widget" )
	bool GetIsSelectingKey() const;

	/** Sets the style of the button used to start key selection mode. */
	void SetButtonStyle(const FButtonStyle* ButtonStyle);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

private:
	void HandleKeySelected( FInputChord SelectedKey );
	void HandleIsSelectingKeyChanged();

private:

	/** The style for the button used to start key selection mode. */
	const FButtonStyle* ButtonStyle;

	/** The input key selector widget managed by this object. */
	TSharedPtr<SInputKeySelector> MyInputKeySelector;
};