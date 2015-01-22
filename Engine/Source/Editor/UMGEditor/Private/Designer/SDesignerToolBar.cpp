// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "DesignerCommands.h"
#include "SDesignerToolBar.h"
#include "Settings/WidgetDesignerSettings.h"
#include "SViewportToolBarComboMenu.h"

#define LOCTEXT_NAMESPACE "UMG"

void SDesignerToolBar::Construct( const FArguments& InArgs )
{
	CommandList = InArgs._CommandList;

	ChildSlot
	[
		MakeToolBar(InArgs._Extenders)
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef< SWidget > SDesignerToolBar::MakeToolBar(const TSharedPtr< FExtender > InExtenders)
{
	FToolBarBuilder ToolbarBuilder( CommandList, FMultiBoxCustomization::None, InExtenders );

	// Use a custom style
	FName ToolBarStyle = "ViewportMenu";
	ToolbarBuilder.SetStyle(&FEditorStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable( false );

	ToolbarBuilder.BeginSection("Transform");
	ToolbarBuilder.BeginBlockGroup();
	{
		ToolbarBuilder.AddToolBarButton( FDesignerCommands::Get().LayoutTransform, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), "LayoutTransform" );
		ToolbarBuilder.AddToolBarButton( FDesignerCommands::Get().RenderTransform, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), "RenderTransform" );
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("LocationGridSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FDesignerCommands::Get().LocationGridSnap.Get();

		static FName PositionSnapName = FName(TEXT("PositionSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(SNew(SViewportToolBarComboMenu)
			.Style(ToolBarStyle)
			.BlockLocation(EMultiBlockLocation::Start)
			.Cursor(EMouseCursor::Default)
			.IsChecked(this, &SDesignerToolBar::IsLocationGridSnapChecked)
			.OnCheckStateChanged(this, &SDesignerToolBar::HandleToggleLocationGridSnap)
			.Label(this, &SDesignerToolBar::GetLocationGridLabel)
			.OnGetMenuContent(this, &SDesignerToolBar::FillLocationGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("LocationGridSnap_ToolTip", "Set the Position Grid Snap value"))
			.Icon(Command->GetIcon())
			.ParentToolBar(SharedThis(this))
			, PositionSnapName);
	}
	ToolbarBuilder.EndSection();
	
	return ToolbarBuilder.MakeWidget();
}

ECheckBoxState SDesignerToolBar::IsLocationGridSnapChecked() const
{
	return GetDefault<UWidgetDesignerSettings>()->GridSnapEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDesignerToolBar::HandleToggleLocationGridSnap(ECheckBoxState InState)
{
	UWidgetDesignerSettings* ViewportSettings = GetMutableDefault<UWidgetDesignerSettings>();
	ViewportSettings->GridSnapEnabled = !ViewportSettings->GridSnapEnabled;
}

FText SDesignerToolBar::GetLocationGridLabel() const
{
	const UWidgetDesignerSettings* ViewportSettings = GetDefault<UWidgetDesignerSettings>();
	return FText::AsNumber(ViewportSettings->GridSnapSize);
}

TSharedRef<SWidget> SDesignerToolBar::FillLocationGridSnapMenu()
{
	const UWidgetDesignerSettings* ViewportSettings = GetDefault<UWidgetDesignerSettings>();

	TArray<int32> GridSizes;
	GridSizes.Add(1);
	GridSizes.Add(2);
	GridSizes.Add(3);
	GridSizes.Add(4);
	GridSizes.Add(5);
	GridSizes.Add(10);
	GridSizes.Add(15);
	GridSizes.Add(25);

	return BuildLocationGridCheckBoxList("Snap", LOCTEXT("LocationSnapText", "Snap Sizes"), GridSizes);
}

TSharedRef<SWidget> SDesignerToolBar::BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<int32>& InGridSizes) const
{
	const UWidgetDesignerSettings* ViewportSettings = GetDefault<UWidgetDesignerSettings>();

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder LocationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	LocationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for ( int32 CurGridSizeIndex = 0; CurGridSizeIndex < InGridSizes.Num(); ++CurGridSizeIndex )
	{
		const int32 CurGridSize = InGridSizes[CurGridSizeIndex];

		LocationGridMenuBuilder.AddMenuEntry(
			FText::AsNumber(CurGridSize),
			FText::Format(LOCTEXT("LocationGridSize_ToolTip", "Sets grid size to {0}"), FText::AsNumber(CurGridSize)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&SDesignerToolBar::SetGridSize, CurGridSize),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&SDesignerToolBar::IsGridSizeChecked, CurGridSize)),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	LocationGridMenuBuilder.EndSection();

	return LocationGridMenuBuilder.MakeWidget();
}

void SDesignerToolBar::SetGridSize(int32 InGridSize)
{
	UWidgetDesignerSettings* ViewportSettings = GetMutableDefault<UWidgetDesignerSettings>();
	ViewportSettings->GridSnapSize = InGridSize;
}

bool SDesignerToolBar::IsGridSizeChecked(int32 InGridSnapSize)
{
	const UWidgetDesignerSettings* ViewportSettings = GetDefault<UWidgetDesignerSettings>();
	return ( ViewportSettings->GridSnapSize == InGridSnapSize );
}

#undef LOCTEXT_NAMESPACE
