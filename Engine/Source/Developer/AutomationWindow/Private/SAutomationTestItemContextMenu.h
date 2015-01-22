// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "SAutomationTestItemContextMenu"

class SAutomationTestItemContextMenu
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAutomationTestItemContextMenu) {}
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InSessionManager The session to use.
	 */
	void Construct( const FArguments& InArgs, const FString& InAssetName )
	{
		Assetname = InAssetName;

		ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Content()
				[
					MakeContextMenu( )
				]
		];
	}

protected:

	/**
	 * Builds the context menu widget.
	 *
	 * @return The context menu.
	 */
	TSharedRef<SWidget> MakeContextMenu( )
	{
		FMenuBuilder MenuBuilder(true, NULL);

		MenuBuilder.BeginSection("AutomationOptions", LOCTEXT("MenuHeadingText", "Automation Options"));
		{
			MenuBuilder.AddMenuEntry(LOCTEXT("AutomationMenuEntryLoadText", "Load the asset"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SAutomationTestItemContextMenu::HandleContextItemTerminate)));
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

private:

	/** Handle the context menu closing down. If an asset is selected, request that it gets loaded */
	void HandleContextItemTerminate( )
	{
		if (Assetname.Len() > 0)
		{
			FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController").GetAutomationController()->RequestLoadAsset( Assetname );
		}
	}

private:

	/** Holds the selected asset name. */
	FString Assetname;
};


#undef LOCTEXT_NAMESPACE

#endif //WITH_EDITOR