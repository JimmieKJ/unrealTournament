// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "SPluginListTile.h"
#include "PluginListItem.h"
#include "SPluginsEditor.h"
#include "SPluginList.h"
#include "PluginStyle.h"
#include "GameProjectGenerationModule.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "PluginListTile"


void SPluginListTile::Construct( const FArguments& Args, const TSharedRef<SPluginList> Owner, const TSharedRef<class FPluginListItem>& Item )
{
	OwnerWeak = Owner;
	ItemData = Item;

	const float PaddingAmount = FPluginStyle::Get()->GetFloat( "PluginTile.Padding" );
	const float ThumbnailImageSize = FPluginStyle::Get()->GetFloat( "PluginTile.ThumbnailImageSize" );

	// @todo plugedit: Also display whether plugin is editor-only, runtime-only, developer or a combination?
	//		-> Maybe a filter for this too?  (show only editor plugins, etc.)
	// @todo plugedit: Indicate whether plugin has content?  Filter to show only content plugins, and vice-versa?

	// Plugin thumbnail image
	if( !Item->PluginStatus.Icon128FilePath.IsEmpty() )
	{
		const FName BrushName( *Item->PluginStatus.Icon128FilePath );
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);

		if ((Size.X > 0) && (Size.Y > 0))
		{
			PluginIconDynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(Size.X, Size.Y)));
		}
	}
	else
	{
		// Plugin is missing a thumbnail image
		// @todo plugedit: Should display default image or just omit the thumbnail
	}

	// create documentation link
	TSharedPtr<SWidget> DocumentationWidget;
	{
		if (Item->PluginStatus.DocsURL.IsEmpty())
		{
			DocumentationWidget = SNullWidget::NullWidget;
		}
		else
		{
			FString DocsURL = Item->PluginStatus.DocsURL;
			DocumentationWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("MessageLog.Docs"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHyperlink)
						.Text(LOCTEXT("DocumentationLink", "Documentation"))
						.ToolTipText(LOCTEXT("NavigateToCreatedByURL", "Open the plug-in's online documentation"))
						.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(*DocsURL, nullptr, nullptr); })
				];
		}
	}

	// create vendor link
	TSharedPtr<SWidget> CreatedByWidget;
	{
		if (Item->PluginStatus.CreatedBy.IsEmpty())
		{
			CreatedByWidget = SNullWidget::NullWidget;
		}
		else if (Item->PluginStatus.CreatedByURL.IsEmpty())
		{
			CreatedByWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderDeveloper"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(Item->PluginStatus.CreatedBy))
				];
		}
		else
		{
			FString CreatedByURL = Item->PluginStatus.CreatedByURL;
			CreatedByWidget = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FEditorStyle::GetBrush("MessageLog.Url"))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[				
					SNew(SHyperlink)
						.Text(FText::FromString(Item->PluginStatus.CreatedBy))
						.ToolTipText(FText::Format(LOCTEXT("NavigateToCreatedByURL", "Visit the vendor's web site ({0})"), FText::FromString(CreatedByURL)))
						.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(*CreatedByURL, nullptr, nullptr); })
				];
		}
	}

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(PaddingAmount)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(PaddingAmount)
					[
						SNew(SHorizontalBox)

						// Thumbnail image
						+ SHorizontalBox::Slot()
							.Padding(PaddingAmount)
							.AutoWidth()
							[
								SNew(SBox)
									.WidthOverride(ThumbnailImageSize)
									.HeightOverride(ThumbnailImageSize)
									[
										SNew(SImage)
											.Image(PluginIconDynamicImageBrush.IsValid() ? PluginIconDynamicImageBrush.Get() : nullptr)
									]
							]

						+SHorizontalBox::Slot()
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SHorizontalBox)

										// Friendly name
										+SHorizontalBox::Slot()
										.Padding(PaddingAmount)
										[
											SNew(STextBlock)
												.Text(FText::FromString(Item->PluginStatus.FriendlyName))
												.HighlightText_Raw(&Owner->GetOwner().GetPluginTextFilter(), &FPluginTextFilter::GetRawFilterText)
												.TextStyle(FPluginStyle::Get(), "PluginTile.NameText")
										]

										// Version
										+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.Padding(PaddingAmount)
											.AutoWidth()
											[
												SNew(SHorizontalBox)

												+ SHorizontalBox::Slot()
													.AutoWidth()
													.Padding(0.0f, 0.0f, 0.0f, 1.0f) // Lower padding to align font with version number base
													[
														SNew(SHorizontalBox)

														// beta version icon
														+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Bottom)
															.Padding(0.0f, 0.0f, 3.0f, 2.0f)
															[
																SNew(SImage)
																	.Visibility(Item->PluginStatus.bIsBetaVersion ? EVisibility::Visible : EVisibility::Collapsed)
																	.Image(FPluginStyle::Get()->GetBrush("PluginTile.BetaWarning"))
															]

														// version label
														+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Bottom)
															[
																SNew(STextBlock)
																	.Text(Item->PluginStatus.bIsBetaVersion ? LOCTEXT("PluginBetaVersionLabel", "BETA Version ") : LOCTEXT("PluginVersionLabel", "Version "))
															]
													]

												+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign( VAlign_Bottom )
													.Padding( 0.0f, 0.0f, 2.0f, 0.0f )	// Extra padding from the right edge
													[
														SNew(STextBlock)
															.Text(FText::FromString(Item->PluginStatus.VersionName))
															.TextStyle(FPluginStyle::Get(), "PluginTile.VersionNumberText")
													]
											]
									]
			
								+ SVerticalBox::Slot()
									[
										SNew(SVerticalBox)
				
										// Description
										+ SVerticalBox::Slot()
											.Padding( PaddingAmount )
											[
												SNew(STextBlock)
													.Text(FText::FromString(Item->PluginStatus.Description))
													.AutoWrapText(true)
											]

										+ SVerticalBox::Slot()
											.Padding(PaddingAmount)
											.AutoHeight()
											[
												SNew(SHorizontalBox)

												// Enable checkbox
												+ SHorizontalBox::Slot()
													.Padding(PaddingAmount)
													.AutoWidth()
													[
														SNew(SCheckBox)
															.OnCheckStateChanged(this, &SPluginListTile::OnEnablePluginCheckboxChanged)
															.IsChecked(this, &SPluginListTile::IsPluginEnabled)
															.ToolTipText(LOCTEXT("EnableDisableButtonToolTip", "Toggles whether this plugin is enabled for your current project.  You may need to restart the program for this change to take effect."))
															.Content()
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("EnablePluginCheckbox", "Enabled"))
															]
													]

												// Uninstall button
												+ SHorizontalBox::Slot()
													.Padding(PaddingAmount)
													.AutoWidth()
													[
														SNew(SButton)
															.Text(LOCTEXT("PluginUninstallButtonLabel", "Uninstall..."))

															// Don't show 'Uninstall' for built-in plugins
															.Visibility(EVisibility::Collapsed)
															// @todo plugins: Not supported yet
																// .Visibility( Item->PluginStatus.bIsBuiltIn ? EVisibility::Collapsed : EVisibility::Visible )

															.OnClicked(this, &SPluginListTile::OnUninstallButtonClicked)
															.IsEnabled(false)		// @todo plugedit: Not supported yet
															.ToolTipText(LOCTEXT("UninstallButtonToolTip", "Allows you to uninstall this plugin.  This will delete the plugin from your project entirely.  You may need to restart the program for this change to take effect."))
													]

												// docs link
												+ SHorizontalBox::Slot()
													.Padding(PaddingAmount)
													.HAlign(HAlign_Right)
													[
														DocumentationWidget.ToSharedRef()
													]

												// vendor link
												+ SHorizontalBox::Slot()
													.AutoWidth()
													.Padding(12.0f, PaddingAmount, PaddingAmount, PaddingAmount)
													.HAlign(HAlign_Right)
													[
														CreatedByWidget.ToSharedRef()
													]
											]
									]
							]
					]
			]
	];
}


ECheckBoxState SPluginListTile::IsPluginEnabled() const
{
	return ItemData->PluginStatus.bIsEnabled
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}


void SPluginListTile::OnEnablePluginCheckboxChanged(ECheckBoxState NewCheckedState)
{
	const bool bNewEnabledState = (NewCheckedState == ECheckBoxState::Checked);

	if (bNewEnabledState && ItemData->PluginStatus.bIsBetaVersion)
	{
		FText WarningMessage = FText::Format(
			LOCTEXT("Warning_EnablingBetaPlugin", "Plugin '{0}' is a beta version and might be unstable or removed without notice. Please use with caution. Are you sure you want to enable the plugin?"),
			FText::FromString(ItemData->PluginStatus.FriendlyName));

		if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
		{
			// user chose to keep beta plug-in disabled
			return;
		}
	}

	ItemData->PluginStatus.bIsEnabled = bNewEnabledState;
	FGameProjectGenerationModule::Get().TryMakeProjectFileWriteable(FPaths::GetProjectFilePath());
	FText FailMessage;

	if (!IProjectManager::Get().SetPluginEnabled(ItemData->PluginStatus.Name, bNewEnabledState, FailMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
	}
}


FReply SPluginListTile::OnUninstallButtonClicked()
{
	// not expecting to deal with engine plugins here (they can't be uninstalled)
	if (ensure(!ItemData->PluginStatus.bIsBuiltIn))
	{
		// @todo plugedit: Make this work!
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
