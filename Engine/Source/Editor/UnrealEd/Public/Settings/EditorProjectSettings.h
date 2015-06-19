// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnitConversion.h"

#include "EditorProjectSettings.generated.h"

/**
 * Editor project appearance settings. Stored in default config, per-project
 */
UCLASS(config=Editor, defaultconfig, meta=(DisplayName="Appearance"))
class UNREALED_API UEditorProjectAppearanceSettings : public UDeveloperSettings
{
public:
	GENERATED_BODY()
	UEditorProjectAppearanceSettings(const FObjectInitializer&);

protected:
	/** Called when a property on this object is changed */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
	virtual void PostInitProperties() override;

private:

	UPROPERTY(EditAnywhere, config, Category=Units, meta=(DisplayName="Display Units on Applicable Properties", Tooltip="Whether to display units on editor properties where the property has units set."))
	bool bDisplayUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Distance/Length", Tooltip="Choose a set of units in which to display distance/length values."))
	TArray<TEnumAsByte<EUnit>> DistanceUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Mass", Tooltip="Choose a set of units in which to display masses."))
	TArray<TEnumAsByte<EUnit>> MassUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Time", Tooltip="Choose the units in which to display time."))
	TArray<TEnumAsByte<EUnit>> TimeUnits;
	
	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Angles", Tooltip="Choose the units in which to display angles.", ValidEnumValues="Degrees, Radians"))
	TEnumAsByte<EUnit> AngleUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Speed/Velocity", Tooltip="Choose the units in which to display speeds and velocities.", ValidEnumValues="MetersPerSecond, KilometersPerHour, MilesPerHour"))
	TEnumAsByte<EUnit> SpeedUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Temperature", Tooltip="Choose the units in which to display temperatures.", ValidEnumValues="Celsius, Farenheit, Kelvin"))
	TEnumAsByte<EUnit> TemperatureUnits;

	UPROPERTY(EditAnywhere, config, Category=Units, AdvancedDisplay, meta=(DisplayName="Force", Tooltip="Choose the units in which to display forces.", ValidEnumValues="Newtons, PoundsForce, KilogramsForce"))
	TEnumAsByte<EUnit> ForceUnits;

public:
	/** Deprecated properties that didn't live very long */

	/** UENUM to define the specific set of allowable unit types */
	UENUM()
	enum class EUnitDisplay : uint8
	{
		None, Metric, Imperial, Invalid
	};

	UPROPERTY(config)
	EUnitDisplay UnitDisplay_DEPRECATED;

	/** UENUM to define the specific set of allowable default units */
	UENUM()
	enum class EDefaultLocationUnit : uint8
	{
		Micrometers, Millimeters, Centimeters, Meters, Kilometers,
		Inches, Feet, Yards, Miles, Invalid
	};

	UPROPERTY(config)
	EDefaultLocationUnit DefaultInputUnits_DEPRECATED;
};
/**
* 2D layer settings
*/
USTRUCT()
struct UNREALED_API FMode2DLayer
{
	GENERATED_USTRUCT_BODY()

	FMode2DLayer()
	: Name(TEXT("Default"))
	, Depth(0)
	{ }

	FMode2DLayer(FString InName, float InDepth)
		: Name(InName)
		, Depth(InDepth)
	{ }

	/** Whether snapping to surfaces in the world is enabled */
	UPROPERTY(EditAnywhere, config, Category = Layer)
	FString Name;

	/** The amount of depth to apply when snapping to surfaces */
	UPROPERTY(EditAnywhere, config, Category = Layer)
	float Depth;
};

UENUM()
enum class ELevelEditor2DAxis : uint8
{
	X,
	Y,
	Z
};

/**
 * Configure settings for the 2D Level Editor
 */
UCLASS(config=Editor, meta=(DisplayName="2D"), defaultconfig)
class UNREALED_API ULevelEditor2DSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	/** If enabled will allow 2D mode */
	UPROPERTY(EditAnywhere, config, Category=General, meta=(DisplayName="Enable 2D combined translate + rotate widget"))
	bool bEnable2DWidget;

	/** If enabled will allow 2D mode */
	UPROPERTY(EditAnywhere, config, Category=LayerSnapping)
	bool bEnableSnapLayers;

	/** Snap axis */
	UPROPERTY(EditAnywhere, config, Category=LayerSnapping, meta=(EditCondition=bEnableSnapLayers))
	ELevelEditor2DAxis SnapAxis;

	/** Snap layers that are displayed in the viewport toolbar */
	UPROPERTY(EditAnywhere, config, Category=LayerSnapping, meta=(EditCondition=bEnableSnapLayers))
	TArray<FMode2DLayer> SnapLayers;

public:
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

};

