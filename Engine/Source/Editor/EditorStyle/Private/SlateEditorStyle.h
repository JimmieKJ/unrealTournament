// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISettingsModule.h"
#include "ModuleManager.h"


/**
 * Declares the Editor's visual style.
 */
class FSlateEditorStyle
	: public FEditorStyle
{
public:

	static void Initialize() 
	{
		Settings = NULL;

#if WITH_EDITOR
		Settings = GetMutableDefault<UEditorStyleSettings>();
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings( "Editor", "General", "Appearance",
				NSLOCTEXT("EditorStyle", "Appearance_UserSettingsName", "Appearance"),
				NSLOCTEXT("EditorStyle", "Appearance_UserSettingsDescription", "Customize the look of the editor."),
				Settings
			);
		}
#endif

		StyleInstance = Create( Settings );
		SetStyle( StyleInstance.ToSharedRef() );
	}

	static void Shutdown()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings( "Editor", "General", "Appearance");
		}

		ResetToDefault();
		ensure( StyleInstance.IsUnique() );
		StyleInstance.Reset();
	}
	
	static void SyncCustomizations()
	{
		FSlateEditorStyle::StyleInstance->SyncSettings();
	}

	class FStyle : public FSlateStyleSet
	{
	public:
		FStyle( const TWeakObjectPtr< UEditorStyleSettings >& InSettings );

		void Initialize();
		void SetupGeneralStyles();
		void SetupGeneralIcons();
		void SetupWindowStyles();
		void SetupProjectBadgeStyle();
		void SetupDockingStyles();
		void SetupTutorialStyles();
		void SetupTranslationEditorStyles();
		void SetupLocalizationDashboardStyles();
		void SetupPropertyEditorStyles();
		void SetupProfilerStyle();
		void SetupGraphEditorStyles();
		void SetupLevelEditorStyle();
		void SetupPersonaStyle();
		void SetupClassIconsAndThumbnails();
		void SetupContentBrowserStyle();
		void SetupLandscapeEditorStyle();
		void SetupToolkitStyles();
		void SetupMatineeStyle();
		void SetupSourceControlStyles();
		void SetupAutomationStyles();
		void SetupUMGEditorStyles();

		void SettingsChanged( UObject* ChangedObject, FPropertyChangedEvent& PropertyChangedEvent );
		void SyncSettings();

		const FVector2D Icon7x16;
		const FVector2D Icon8x4;
		const FVector2D Icon16x4;
		const FVector2D Icon8x8;
		const FVector2D Icon10x10;
		const FVector2D Icon12x12;
		const FVector2D Icon12x16;
		const FVector2D Icon14x14;
		const FVector2D Icon16x16;
		const FVector2D Icon16x20;
		const FVector2D Icon20x20;
		const FVector2D Icon22x22;
		const FVector2D Icon24x24;
		const FVector2D Icon25x25;
		const FVector2D Icon32x32;
		const FVector2D Icon40x40;
		const FVector2D Icon48x48;
		const FVector2D Icon64x64;
		const FVector2D Icon36x24;
		const FVector2D Icon128x128;

		// These are the colors that are updated by the user style customizations
		const TSharedRef< FLinearColor > DefaultForeground_LinearRef;
		const TSharedRef< FLinearColor > InvertedForeground_LinearRef;
		const TSharedRef< FLinearColor > SelectorColor_LinearRef;
		const TSharedRef< FLinearColor > SelectionColor_LinearRef;
		const TSharedRef< FLinearColor > SelectionColor_Subdued_LinearRef;
		const TSharedRef< FLinearColor > SelectionColor_Inactive_LinearRef;
		const TSharedRef< FLinearColor > SelectionColor_Pressed_LinearRef;

		// These are the Slate colors which reference those above; these are the colors to put into the style
		const FSlateColor DefaultForeground;
		const FSlateColor InvertedForeground;
		const FSlateColor SelectorColor;
		const FSlateColor SelectionColor;
		const FSlateColor SelectionColor_Subdued;
		const FSlateColor SelectionColor_Inactive;
		const FSlateColor SelectionColor_Pressed;

		// These are common colors used thruout the editor in mutliple style elements
		const FSlateColor InheritedFromBlueprintTextColor;

		FTextBlockStyle NormalText;
		FEditableTextBoxStyle NormalEditableTextBoxStyle;
		FTableRowStyle NormalTableRowStyle;
		FButtonStyle Button;
		FButtonStyle HoverHintOnly;

		TWeakObjectPtr< UEditorStyleSettings > Settings;
	};

	static TSharedRef< class FSlateEditorStyle::FStyle > Create( const TWeakObjectPtr< UEditorStyleSettings >& InCustomization )
	{
		TSharedRef< class FSlateEditorStyle::FStyle > NewStyle = MakeShareable( new FSlateEditorStyle::FStyle( InCustomization ) );
		NewStyle->Initialize();

		FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(NewStyle, &FSlateEditorStyle::FStyle::SettingsChanged);

		return NewStyle;
	}

	static TSharedPtr< FSlateEditorStyle::FStyle > StyleInstance;
	static TWeakObjectPtr< UEditorStyleSettings > Settings;
};
