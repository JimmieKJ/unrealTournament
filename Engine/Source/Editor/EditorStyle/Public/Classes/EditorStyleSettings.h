// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EditorStyleSettings.generated.h"


/**
 * Enumerates color vision deficiency types.
 */
UENUM()
enum EColorVisionDeficiency
{
	CVD_NormalVision UMETA(DisplayName="Normal Vision"),
	CVD_Deuteranomly UMETA(DisplayName="Deuteranomly (6% of males, 0.4% of females)"),
	CVD_Deuteranopia UMETA(DisplayName="Deuteranopia (1% of males)"),
	CVD_Protanomly UMETA(DisplayName="Protanomly (1% of males, 0.01% of females)"),
	CVD_Protanopia UMETA(DisplayName="Protanopia (1% of males)"),
	CVD_Tritanomaly UMETA(DisplayName="Tritanomaly (0.01% of males and females)"),
	CVD_Tritanopia UMETA(DisplayName="Tritanopia (1% of males and females)"),
	CVD_Achromatopsia UMETA(DisplayName="Achromatopsia (Extremely Rare)"),
};


UENUM()
enum class EAssetEditorOpenLocation : uint8
{
	/** Attempts to dock asset editors into either a new window, or the main window if they were docked there. */
	Default,
	/** Docks tabs into new windows. */
	NewWindow,
	/** Docks tabs into the main window. */
	MainWindow,
	/** Docks tabs into the content browser's window. */
	ContentBrowser,
	/** Docks tabs into the last window that was docked into, or a new window if there is no last docked window. */
	LastDockedWindowOrNewWindow,
	/** Docks tabs into the last window that was docked into, or the main window if there is no last docked window. */
	LastDockedWindowOrMainWindow,
	/** Docks tabs into the last window that was docked into, or the content browser window if there is no last docked window. */
	LastDockedWindowOrContentBrowser
};

/**
 * Implements the Editor style settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class EDITORSTYLE_API UEditorStyleSettings : public UObject
{
public:

	GENERATED_UCLASS_BODY()

public:

	/** The color used to represent selection */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Selection Color"))
	FLinearColor SelectionColor;

	/** The color used to represent a pressed item */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Pressed Selection Color"))
	FLinearColor PressedSelectionColor;

	/** The color used to represent selected items that are currently inactive */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Inactive Selection Color"))
	FLinearColor InactiveSelectionColor;

	/** The color used to represent keyboard input selection focus */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Keyboard Focus Color"), AdvancedDisplay)
	FLinearColor KeyboardFocusColor;

	/** Applies a color vision deficiency filter to the entire editor */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	TEnumAsByte<EColorVisionDeficiency> ColorVisionDeficiencyPreviewType;

public:

	/** Whether to use small toolbar icons without labels or not. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bUseSmallToolBarIcons:1;

	/** If true the material editor and blueprint editor will show a grid on it's background. */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Use Grids In The Material And Blueprint Editor"))
	uint32 bUseGrid : 1;

	/** The color used to represent regular grid lines */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid regular Color"))
	FLinearColor RegularColor;

	/** The color used to represent ruler lines in the grid */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid ruler Color"))
	FLinearColor RuleColor;

	/** The color used to represent the center lines in the grid */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid center Color"))
	FLinearColor CenterColor;

	/** Enables animated transitions for certain menus and pop-up windows.  Note that animations may be automatically disabled at low frame rates in order to improve responsiveness. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bEnableWindowAnimations:1;

	/** When enabled, the C++ names for properties and functions will be displayed in a format that is easier to read */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Show Friendly Variable Names"))
	uint32 bShowFriendlyNames:1;

	/** When enabled, the Editor Preferences and Project Settings menu items in the main menu will be expanded with sub-menus for each settings section. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, AdvancedDisplay)
	uint32 bExpandConfigurationMenus:1;

	/** When enabled, the Editor Preferences and Project Settings menu items in the main menu will be expanded with sub-menus for each settings section. */
	UPROPERTY(config)
	uint32 bShowProjectMenus : 1;

	/** When enabled, the Launch menu items will be shown. */
	UPROPERTY(config)
	uint32 bShowLaunchMenus : 1;

	/** The display mode for timestamps in the output log */
	UPROPERTY(EditAnywhere, config, Category=Logging)
	TEnumAsByte<ELogTimes::Type> LogTimestampMode;

	/** Should warnings and errors in the Output Log during "Play in Editor" be promoted to the message log? */
	UPROPERTY(EditAnywhere, config, Category=Logging)
	bool bPromoteOutputLogWarningsDuringPIE;

	/** New asset editor tabs will open at the specified location. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	EAssetEditorOpenLocation AssetEditorOpenLocation;

public:

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorStyleSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

	/** @return A subdued version of the users selection color (for use with inactive selection)*/
	FLinearColor GetSubduedSelectionColor() const;
protected:

	// UObject overrides

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
