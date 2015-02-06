// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SProjectLauncherBuildConfigurationSelector"


/**
 * Delegate type for build configuration selections.
 *
 * The first parameter is the selected build configuration.
 */
DECLARE_DELEGATE_OneParam(FOnSessionSProjectLauncherBuildConfigurationSelected, EBuildConfigurations::Type)


/**
 * Implements a build configuration selector widget.
 */
class SProjectLauncherBuildConfigurationSelector
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherBuildConfigurationSelector) { }
		SLATE_EVENT(FOnSessionSProjectLauncherBuildConfigurationSelected, OnConfigurationSelected)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct(const FArguments& InArgs)
	{
		OnConfigurationSelected = InArgs._OnConfigurationSelected;

		// create build configurations menu
		FMenuBuilder MenuBuilder(true, NULL);
		{
			if (!FRocketSupport::IsRocket())
			{
				FUIAction DebugAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, EBuildConfigurations::Debug));
				MenuBuilder.AddMenuEntry(LOCTEXT("DebugAction", "Debug"), LOCTEXT("DebugActionHint", "Debug configuration."), FSlateIcon(), DebugAction);
			}

			FUIAction DebugGameAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, EBuildConfigurations::DebugGame));
			MenuBuilder.AddMenuEntry(LOCTEXT("DebugGameAction", "DebugGame"), LOCTEXT("DebugGameActionHint", "DebugGame configuration."), FSlateIcon(), DebugGameAction);

			FUIAction DevelopmentAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, EBuildConfigurations::Development));
			MenuBuilder.AddMenuEntry(LOCTEXT("DevelopmentAction", "Development"), LOCTEXT("DevelopmentActionHint", "Development configuration."), FSlateIcon(), DevelopmentAction);

			FUIAction ShippingAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, EBuildConfigurations::Shipping));
			MenuBuilder.AddMenuEntry(LOCTEXT("ShippingAction", "Shipping"), LOCTEXT("ShippingActionHint", "Shipping configuration."), FSlateIcon(), ShippingAction);

			if (!FRocketSupport::IsRocket())
			{
				FUIAction TestAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, EBuildConfigurations::Test));
				MenuBuilder.AddMenuEntry(LOCTEXT("TestAction", "Test"), LOCTEXT("TestActionHint", "Test configuration."), FSlateIcon(), TestAction);
			}
		}

		FSlateFontInfo TextFont = InArgs._Font.IsSet() ? InArgs._Font.Get() : FCoreStyle::Get().GetFontStyle(TEXT("SmallFont"));
		
		ChildSlot
		[
			// build configuration menu
			SNew(SComboButton)
			.VAlign(VAlign_Center)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(TextFont)
				.Text(InArgs._Text)
			]
			.ContentPadding(FMargin(4.0f, 2.0f))
			.MenuContent()
			[
				MenuBuilder.MakeWidget()
			]
		];
	}

private:

	// Callback for clicking a menu entry.
	void HandleMenuEntryClicked( EBuildConfigurations::Type Configuration )
	{
		OnConfigurationSelected.ExecuteIfBound(Configuration);
	}

private:

	// Holds a delegate to be invoked when a build configuration has been selected.
	FOnSessionSProjectLauncherBuildConfigurationSelected OnConfigurationSelected;
};


#undef LOCTEXT_NAMESPACE
