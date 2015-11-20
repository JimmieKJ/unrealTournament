// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "UserInterfaceSettings.generated.h"

/** When to render the Focus Brush for widgets that have user focus. Based on the EFocusCause. */
UENUM()
enum class ERenderFocusRule : uint8
{
	/** Focus Brush will always be rendered for widgets that have user focus. */
	Always,
	/** Focus Brush will be rendered for widgets that have user focus not set based on pointer causes. */
	NonPointer,
	/** Focus Brush will be rendered for widgets that have user focus only if the focus was set by navigation. */
	NavigationOnly,
	/** Focus Brush will not be rendered. */
	Never,
};

/** The Side to use when scaling the UI. */
UENUM()
enum class EUIScalingRule : uint8
{
	/** Evaluates the scale curve based on the shortest side of the viewport. */
	ShortestSide,
	/** Evaluates the scale curve based on the longest side of the viewport. */
	LongestSide,
	/** Evaluates the scale curve based on the X axis of the viewport. */
	Horizontal,
	/** Evaluates the scale curve based on the Y axis of the viewport. */
	Vertical,
	/** Custom - Allows custom rule interpretation. */
	Custom
};

class UWidget;
class UDPICustomScalingRule;

/**
 * User Interface settings that control Slate and UMG.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="User Interface"))
class ENGINE_API UUserInterfaceSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(config, EditAnywhere, Category = "Focus", meta = (ToolTip = "Rule to determine if we should render the Focus Brush for widgets that have user focus."))
	ERenderFocusRule RenderFocusRule;

	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the Default Cursor is requested."))
	FStringClassReference DefaultCursor;

	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the TextEditBeam Cursor is requested."))
	FStringClassReference TextEditBeamCursor;

	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the Crosshairs Cursor is requested."))
	FStringClassReference CrosshairsCursor;

	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the GrabHand Cursor is requested."))
	FStringClassReference GrabHandCursor;
	
	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the GrabHandClosed Cursor is requested."))
	FStringClassReference GrabHandClosedCursor;

	UPROPERTY(config, EditAnywhere, Category = "Cursors", meta = (MetaClass = "UserWidget", ToolTip = "Widget to use when the SlashedCircle Cursor is requested."))
	FStringClassReference SlashedCircleCursor;

	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta = ( ToolTip = "An optional application scale to apply on top of the custom scaling rules.  So if you want to expose a property in your game title, you can modify this underlying value to scale the entire UI." ))
	float ApplicationScale;

	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=(
		DisplayName="DPI Scale Rule",
		ToolTip="The rule used when trying to decide what scale to apply." ))
	TEnumAsByte<EUIScalingRule> UIScaleRule;

	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=( MetaClass="DPICustomScalingRule", ToolTip = "Set DPI Scale Rule to Custom, and this class will be used instead of any of the built-in rules." ))
	FStringClassReference CustomScalingRuleClass;

	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=(
		DisplayName="DPI Curve",
		ToolTip="Controls how the UI is scaled at different resolutions based on the DPI Scale Rule",
		XAxisName="Resolution",
		YAxisName="Scale"))
	FRuntimeFloatCurve UIScaleCurve;

public:

	virtual void PostInitProperties() override;

	void ForceLoadResources();

	/** Gets the current scale of the UI based on the size of a viewport */
	float GetDPIScaleBasedOnSize(FIntPoint Size) const;

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	TArray<UObject*> CursorClasses;

	UPROPERTY(Transient)
	mutable UClass* CustomScalingRuleClassInstance;

	UPROPERTY(Transient)
	mutable UDPICustomScalingRule* CustomScalingRule;
};
