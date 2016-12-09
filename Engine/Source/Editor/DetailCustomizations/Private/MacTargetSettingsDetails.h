// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
enum class ECheckBoxState : uint8;

/* FMacShaderFormatsPropertyDetails
 *****************************************************************************/

/**
 * Helper which implements details panel customizations for a device profiles parent property
 */
class FMacShaderFormatsPropertyDetails
: public TSharedFromThis<FMacShaderFormatsPropertyDetails>
{
	
public:
	
	/**
	 * Constructor for the parent property details view
	 *
	 * @param InDetailsBuilder - Where we are adding our property view to
	 * @param InProperty - The category name to override
	 * @param InTitle - Title for display
	 */
	FMacShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty, FString InTitle);
	
	
	/**
	 * Create the UI to select which windows shader formats we are targetting
	 */
	void CreateTargetShaderFormatsPropertyView();
	
private:
	
	// Supported/Targeted RHI check boxes
	void OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName);
	ECheckBoxState IsTargetedRHIChecked(FName InRHIName) const;
	
private:
	
	/** A handle to the detail view builder */
	IDetailLayoutBuilder* DetailBuilder;
	
	/** Access to the Parent Property */
	TSharedPtr<IPropertyHandle> ShaderFormatsPropertyHandle;
	
	/** The category name to override */
	FString Property;
	
	/** Title for display */
	FString Title;
};

/**
 * Manages the Transform section of a details view                    
 */
class FMacTargetSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	/** Delegate handler for before an icon is copied */
	bool HandlePreExternalIconCopy(const FString& InChosenImage);

	/** Delegate handler to get the path to start picking from */
	FString GetPickerPath();

	/** Delegate handler to set the path to start picking from */
	bool HandlePostExternalIconCopy(const FString& InChosenImage);
	
private:
	/** Reference to the target shader formats property view */
	TSharedPtr<FMacShaderFormatsPropertyDetails> TargetShaderFormatsDetails;
	
	/** Reference to the cached shader formats property view */
	TSharedPtr<FMacShaderFormatsPropertyDetails> CachedShaderFormatsDetails;
};
