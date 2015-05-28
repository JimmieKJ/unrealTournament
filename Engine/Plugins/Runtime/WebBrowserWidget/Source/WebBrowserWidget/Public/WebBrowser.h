// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"

#include "WebBrowser.generated.h"

/**
 * 
 */
UCLASS()
class WEBBROWSERWIDGET_API UWebBrowser : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Load the specified URL
	 *
	 * @param NewURL New URL to load
	 */
	UFUNCTION(BlueprintCallable, Category="Web Browser")
	void LoadURL(FString NewURL);

	/**
	 * Load a string as data to create a web page
	 *
	 * @param Contents String to load
	 * @param DummyURL Dummy URL for the page
	 */
	UFUNCTION(BlueprintCallable, Category="Web Browser")
	void LoadString(FString Contents, FString DummyURL);

	/**
	 * Get the current title of the web page
	 */
	UFUNCTION(BlueprintCallable, Category="Web Browser")
	FText GetTitleText() const;

public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category=Appearance)
	FString InitialURL;

	UPROPERTY(EditAnywhere, Category=Appearance)
	bool bSupportsTransparency;

protected:
	TSharedPtr<class SWebBrowser> WebBrowserWidget;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
