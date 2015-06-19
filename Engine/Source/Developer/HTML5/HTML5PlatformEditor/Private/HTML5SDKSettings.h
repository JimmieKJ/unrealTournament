// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITargetPlatformManagerModule.h"
#include "EditorStyleSet.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IHTML5TargetPlatformModule.h"
#include "HTML5SDKSettings.generated.h"

class IHTML5TargetPlatformModule;

USTRUCT()
struct FHTML5DeviceMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser name"))
	FString DeviceName;

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser filepath"))
	FFilePath DevicePath;
};

USTRUCT()
struct FHTML5SDKPath
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = SDK_Path)
	FString SDKPath;

	// Note that 0.0.0 is invalid and -1.-1.-1 is latest
	UPROPERTY(EditAnywhere, Category = SDK_Path)
	FString EmscriptenVersion;
};

/**
 * Implements the settings for the HTML5 SDK setup.
 */
UCLASS(config=Engine, globaluserconfig)
class HTML5PLATFORMEDITOR_API UHTML5SDKSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Path to the python executable. Can be blank is python is on the PATH
	UPROPERTY(GlobalConfig, EditAnywhere, Category = HTML5_SDK_Paths, Meta = (DisplayName = "Location of Python exe"))
	FFilePath Python;

	// Available browsers that can be used when launching HTML5 builds.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Available browsers"))
	TArray<FHTML5DeviceMapping> DeviceMap;

	// Path to Emscripten SDK install directory. This is the directory which contains emsdk.bat and emsdk.sh
	UPROPERTY(GlobalConfig, EditAnywhere, Category = HTML5_SDK_Paths, Meta = (DisplayName = "Location of Emscripten SDK"))
	FHTML5SDKPath EmscriptenRoot;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
	void QueryKnownBrowserLocations();

	ITargetPlatformManagerModule * TargetManagerModule;
#endif
private:
	IHTML5TargetPlatformModule* TargetPlatformModule;
};

#if WITH_EDITOR
struct FHTML5Version
{
	// Version text to display
	FText Text;

	FHTML5SDKVersionNumber Version;

	bool operator == (const FHTML5Version& RHS) const
	{
		return Version == RHS.Version;
	}
};

class FHTML5SDKPathCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization interface 
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	FReply OnPickDirectory(TSharedRef<IPropertyHandle> PropertyHandle);// const;

	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FHTML5Version> InItem);

	void OnSelectionChanged(TSharedPtr<FHTML5Version> InItem, ESelectInfo::Type InSeletionInfo);

	FText GetSelectedText() const;

	void UpdateAvailableVersions();

	/** The browse button widget */
	TSharedPtr<SButton> BrowseButton;

	TSharedPtr< SComboBox< TSharedPtr<FHTML5Version> > > SDKSelector;

	TArray< TSharedPtr<FHTML5Version> > Options;

	TSharedPtr< FHTML5Version > SelectedVersionItem;

	TSharedPtr< FHTML5Version > InvalidVersion;

	IHTML5TargetPlatformModule* TargetPlatformModule;

	TSharedPtr<IPropertyHandle> PathProperty;
	TSharedPtr<IPropertyHandle> VersionProperty;
};
#endif // WITH_EDITOR
