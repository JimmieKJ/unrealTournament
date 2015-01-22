// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NewsFeedPrivatePCH.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "FNewsFeedModule"


/**
 * Implements the NewsFeed module.
 */
class FNewsFeedModule
	: public INewsFeedModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override
	{
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Editor", "LevelEditor", "NewsFeed",
				LOCTEXT("NewsFeedSettingsName", "News Feed"),
				LOCTEXT("NewsFeedSettingsDescription", "Change how you receive and view news updates."),
				GetMutableDefault<UNewsFeedSettings>()
			);
		}
	}

	virtual void ShutdownModule( ) override
	{
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Editor", "LevelEditor", "NewsFeed");
		}
	}

public:

	// INewsFeedModule interface

	virtual TSharedRef<class SWidget> CreateNewsFeedButton( ) override
	{
		FNewsFeedCacheRef NewsFeedCache = MakeShareable(new FNewsFeedCache());

		TSharedRef<STextBlock> UnreadItemsTextBlock = SNew(STextBlock)
			.ColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f))
			.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 7))
			.Text_Raw(this, &FNewsFeedModule::HandleUnreadItemsText, NewsFeedCache);

		return SNew(SBox)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
			.VAlign(VAlign_Bottom)
			[
				SNew(SComboButton)
					.HasDownArrow(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0.0f)
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Bottom)
							.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
							[
								SNew(SImage)
									.Image(FEditorStyle::GetBrush("NewsFeed.ToolbarIcon.Small"))
							]

						+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Top)
							[
								SNew(SBorder)
									.BorderBackgroundColor_Raw(this, &FNewsFeedModule::HandleUnreadCountBackgroundColor)
									.BorderImage(FEditorStyle::GetBrush("NewsFeed.UnreadCountBackground"))
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.Padding(FMargin(2.0f, 0.0f))
									.Visibility_Raw(this, &FNewsFeedModule::HandleUnreadItemsVisibility, UnreadItemsTextBlock)
									[
										UnreadItemsTextBlock
									]
							]
					]
					.MenuContent()
					[
						SNew(SNewsFeed, NewsFeedCache)
					]
			];
	}

private:

	// Callback for getting the background color the unread item count.
	FSlateColor HandleUnreadCountBackgroundColor( ) const
	{
		return GetDefault<UEditorStyleSettings>()->SelectionColor;
	}

	// Callback for getting the text of the unread item count.
	FText HandleUnreadItemsText( FNewsFeedCacheRef NewsFeedCache ) const
	{
		int32 UnreadItems = 0;

		for (auto Item : NewsFeedCache->GetItems())
		{
			if (!Item->Read)
			{
				++UnreadItems;
			}
		}

		if (UnreadItems > 0)
		{
			return FText::AsNumber(UnreadItems);
		}

		return FText::GetEmpty();
	}

	// Callback for getting the visibility of the unread item count.
	EVisibility HandleUnreadItemsVisibility( TSharedRef<STextBlock> UnreadItemsTextBlock ) const
	{
		return UnreadItemsTextBlock->GetText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
	}
};


IMPLEMENT_MODULE(FNewsFeedModule, NewsFeed);


#undef LOCTEXT_NAMESPACE
