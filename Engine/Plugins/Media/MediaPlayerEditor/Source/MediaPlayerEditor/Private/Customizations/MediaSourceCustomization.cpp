// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Customizations/MediaSourceCustomization.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "PlatformInfo.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IMediaPlayerFactory.h"
#include "IMediaModule.h"
#include "MediaSource.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "FMediaSourceCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'Platforms' category
	IDetailCategoryBuilder& OverridesCategory = DetailBuilder.EditCategory("Overrides");
	{
		// PlatformPlayerNames
		PlatformPlayerNamesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMediaSource, PlatformPlayerNames));
		{
			IDetailPropertyRow& PlayerNamesRow = OverridesCategory.AddProperty(PlatformPlayerNamesProperty);

			PlayerNamesRow.CustomWidget()
				.NameContent()
				[
					PlatformPlayerNamesProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(0.0f)
				[
					MakePlatformPlayerNamesValueWidget()
				];
		}
	}
}


/* FMediaSourceCustomization implementation
 *****************************************************************************/

TSharedRef<SWidget> FMediaSourceCustomization::MakePlatformPlayersMenu(const FString& PlatformName, const TArray<IMediaPlayerFactory*>& PlayerFactories)
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AutoPlayer", "Auto"),
		LOCTEXT("AutoPlayerTooltip", "Select a player automatically based on the media source"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { SetPlatformPlayerNamesValue(PlatformName, NAME_None); }),
			FCanExecuteAction()
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		const bool SupportsRunningPlatform = Factory->GetSupportedPlatforms().Contains(*PlatformName);
		const FName PlayerName = Factory->GetName();

		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("PlayerNameFormat", "{0} ({1})"), Factory->GetDisplayName(), FText::FromName(PlayerName)),
			FText::FromString(FString::Join(Factory->GetSupportedPlatforms(), TEXT(", "))),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] { SetPlatformPlayerNamesValue(PlatformName, PlayerName); }),
				FCanExecuteAction::CreateLambda([=] { return SupportsRunningPlatform; })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	return MenuBuilder.MakeWidget();
}


TSharedRef<SWidget> FMediaSourceCustomization::MakePlatformPlayerNamesValueWidget()
{
	// get registered player plug-ins
	auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("NoPlayersAvailableLabel", "No players available"));
	}

	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	// get available platforms
	TArray<const PlatformInfo::FPlatformInfo*> AvailablePlatforms;

	for (const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::EnumeratePlatformInfoArray())
	{
		if (PlatformInfo.IsVanilla() && (PlatformInfo.PlatformType == PlatformInfo::EPlatformType::Game) && (PlatformInfo.PlatformInfoName != TEXT("AllDesktop")))
		{
			AvailablePlatforms.Add(&PlatformInfo);
		}
	}

	// sort available platforms alphabetically
	AvailablePlatforms.Sort([](const PlatformInfo::FPlatformInfo& One, const PlatformInfo::FPlatformInfo& Two) -> bool
	{
		return One.DisplayName.CompareTo(Two.DisplayName) < 0;
	});

	// build value widget
	TSharedRef<SGridPanel> PlatformPanel = SNew(SGridPanel);

	for (int32 PlatformIndex = 0; PlatformIndex < AvailablePlatforms.Num(); ++PlatformIndex)
	{
		const PlatformInfo::FPlatformInfo* Platform = AvailablePlatforms[PlatformIndex];

		// hack: FPlatformInfo does not currently include IniPlatformName,
		// so we're using PlatformInfoName and strip desktop suffixes.
		FString PlatformName = Platform->PlatformInfoName.ToString();
		PlatformName.RemoveFromEnd(TEXT("NoEditor"));

		// platform icon
		PlatformPanel->AddSlot(0, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(Platform->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
			];

		// platform name
		PlatformPanel->AddSlot(1, PlatformIndex)
			.Padding(4.0f, 0.0f, 16.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Platform->DisplayName)
			];

		// player combo box
		PlatformPanel->AddSlot(2, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(this, &FMediaSourceCustomization::HandlePlatformPlayersComboButtonText, PlatformName)
							.ToolTipText(LOCTEXT("PlatformPlayerComboButtonToolTipText", "Choose desired player for this platform"))
					]
					.ContentPadding(FMargin(6.0f, 2.0f))
					.MenuContent()
					[
						MakePlatformPlayersMenu(PlatformName, PlayerFactories)
					]
			];
	}

	return PlatformPanel;
}


void FMediaSourceCustomization::SetPlatformPlayerNamesValue(FString PlatformName, FName PlayerName)
{
	TArray<UObject*> OuterObjects;
	{
		PlatformPlayerNamesProperty->GetOuterObjects(OuterObjects);
	}

	for (auto Object : OuterObjects)
	{
		FName& OldPlayerName = Cast<UMediaSource>(Object)->PlatformPlayerNames.FindOrAdd(PlatformName);;

		if (OldPlayerName != PlayerName)
		{
			Object->Modify(true);
			OldPlayerName = PlayerName;
		}
	}
}


/* FMediaSourceCustomization callbacks
 *****************************************************************************/

FText FMediaSourceCustomization::HandlePlatformPlayersComboButtonText(FString PlatformName) const
{
	TArray<UObject*> OuterObjects;
	{
		PlatformPlayerNamesProperty->GetOuterObjects(OuterObjects);
	}

	if (OuterObjects.Num() == 0)
	{
		return FText::GetEmpty();
	}

	FName PlayerName = Cast<UMediaSource>(OuterObjects[0])->PlatformPlayerNames.FindRef(PlatformName);

	for (int32 ObjectIndex = 1; ObjectIndex < OuterObjects.Num(); ++ObjectIndex)
	{
		if (Cast<UMediaSource>(OuterObjects[ObjectIndex])->PlatformPlayerNames.FindRef(PlatformName) != PlayerName)
		{
			return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
		}
	}

	if (PlayerName == NAME_None)
	{
		return LOCTEXT("AutomaticLabel", "Auto");
	}

	return FText::FromName(PlayerName);
}


#undef LOCTEXT_NAMESPACE
