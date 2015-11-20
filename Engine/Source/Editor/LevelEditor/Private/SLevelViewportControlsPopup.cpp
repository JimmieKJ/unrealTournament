// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "LevelEditor.h"
#include "SLevelViewportControlsPopup.h"
#include "SWebBrowser.h"

void SLevelViewportControlsPopup::Construct(const FArguments& InArgs)
{
	PopupSize.Set(252, 558);
#if PLATFORM_WINDOWS
	PopupPath = FString(TEXT("file:///")) + FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("Documentation/Source/Shared/Editor/Overlays/ViewportControlsWindows.html")));
#elif PLATFORM_LINUX
	PopupPath = FString(TEXT("file://")) + FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("Documentation/Source/Shared/Editor/Overlays/ViewportControlsWindows.html")));
#elif PLATFORM_MAC
	PopupPath = FString(TEXT("file://")) + FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("Documentation/Source/Shared/Editor/Overlays/ViewportControlsMac.html")));
#endif

	if (!PopupPath.IsEmpty())
	{
		TAttribute<FText> ToolTipText = NSLOCTEXT("LevelViewportControlsPopup", "ViewportControlsToolTip", "Click to show Viewport Controls");
		Default = FEditorStyle::GetBrush("HelpIcon");
		Hovered = FEditorStyle::GetBrush("HelpIcon.Hovered");
		Pressed = FEditorStyle::GetBrush("HelpIcon.Pressed");

		ChildSlot
		[
			SAssignNew(MenuAnchor, SMenuAnchor)
			.Method(EPopupMethod::UseCurrentWindow)
			.Placement(MenuPlacement_AboveAnchor)
			.OnGetMenuContent(this, &SLevelViewportControlsPopup::OnGetMenuContent)
			[
				SAssignNew(Button, SButton)
				.ContentPadding(5)
				.ButtonStyle(FEditorStyle::Get(), "HelpButton")
				.OnClicked(this, &SLevelViewportControlsPopup::OnClicked)
				.ClickMethod(EButtonClickMethod::MouseDown)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(ToolTipText)
				[
					SAssignNew(ButtonImage, SImage)
					.Image(this, &SLevelViewportControlsPopup::GetButtonImage)
				]
			]
		];
	}
}

const FSlateBrush* SLevelViewportControlsPopup::GetButtonImage() const
{
	if (Button->IsPressed())
	{
		return Pressed;
	}

	if (ButtonImage->IsHovered())
	{
		return Hovered;
	}

	return Default;
}

FReply SLevelViewportControlsPopup::OnClicked() const
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	if (MenuAnchor->ShouldOpenDueToClick())
	{
		MenuAnchor->SetIsOpen(true);
	}
	else
	{
		MenuAnchor->SetIsOpen(false);
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SLevelViewportControlsPopup::OnGetMenuContent()
{
	if (!PopupPath.IsEmpty())
	{
		// The size of the web browser is a hack to ensure that the browser shrinks,
		// otherwise it seems to show a scroll bar if the size never changes.
		return SNew(SBox)
			.WidthOverride(PopupSize.X)
			.HeightOverride(PopupSize.Y)
			[
				SNew(SWebBrowser)
				.InitialURL(PopupPath)
				.ShowControls(false)
				.SupportsTransparency(true)
				.ViewportSize(PopupSize + 1)
			];
	}

	return SNullWidget::NullWidget;
}
