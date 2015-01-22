// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "GlobalEditorCommonCommands.h"

//////////////////////////////////////////////////////////////////////////
// SGlobalOpenAssetDialog

#include "AssetData.h"
#include "ContentBrowserModule.h"
#include "Toolkits/AssetEditorManager.h"
#include "OutputLogModule.h"

class SGlobalOpenAssetDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGlobalOpenAssetDialog){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FVector2D InSize)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateSP(this, &SGlobalOpenAssetDialog::OnAssetSelectedFromPicker);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SGlobalOpenAssetDialog::OnPressedEnterOnAssetsInPicker);
		AssetPickerConfig.ThumbnailScale = 0.0f;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bCanShowClasses = false;

		ChildSlot
		[
			SNew(SBox)
			.WidthOverride(InSize.X)
			.HeightOverride(InSize.Y)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				]
			]
		];
	}

	// SWidget interface
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	// End of SWidget interface

protected:
	void OnAssetSelectedFromPicker(const FAssetData& AssetData)
	{
		if (UObject* ObjectToEdit = AssetData.GetAsset())
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
		}
	}

	void OnPressedEnterOnAssetsInPicker(const TArray<FAssetData>& SelectedAssets)
	{
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			if (UObject* ObjectToEdit = AssetIt->GetAsset())
			{
				FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
			}
		}
	}
};

FReply SGlobalOpenAssetDialog::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////
// FGlobalEditorCommonCommands

FGlobalEditorCommonCommands::FGlobalEditorCommonCommands()
	: TCommands<FGlobalEditorCommonCommands>(TEXT("SystemWideCommands"), NSLOCTEXT("Contexts", "SystemWideCommands", "System-wide"), NAME_None, FEditorStyle::GetStyleSetName())
{
}

void FGlobalEditorCommonCommands::RegisterCommands()
{
	//UI_COMMAND( SummonControlTabNavigation, "Tab Navigation", "Summons a list of open assets and tabs", EUserInterfaceActionType::Button, FInputGesture(EModifierKey::Control, EKeys::Tab) );
	UI_COMMAND( SummonOpenAssetDialog, "Open Asset...", "Summons an asset picker", EUserInterfaceActionType::Button, FInputGesture(EModifierKey::Control, EKeys::P) );
	UI_COMMAND( FindInContentBrowser, "Find in Content Browser", "Summons the Content Browser and navigates to the selected asset", EUserInterfaceActionType::Button, FInputGesture(EModifierKey::Control, EKeys::B));
	UI_COMMAND( ViewReferences, "View References", "Launches the reference viewer showing the selected assets' references", EUserInterfaceActionType::Button, FInputGesture(EModifierKey::Shift | EModifierKey::Alt, EKeys::R));
	
	UI_COMMAND( OpenConsoleCommandBox, "Open Console Command Box", "Opens an edit box where you can type in a console command", EUserInterfaceActionType::Button, FInputGesture(EKeys::Tilde));

	UI_COMMAND( OpenDocumentation, "Open Documentation...", "Opens documentation for this tool", EUserInterfaceActionType::Button, FInputGesture(EKeys::F1) );
}

void FGlobalEditorCommonCommands::MapActions(TSharedRef<FUICommandList>& ToolkitCommands)
{
	Register();

// 	ToolkitCommands->MapAction(
// 		Get().SummonControlTabNavigation,
// 		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnPressedCtrlTab ) );

	ToolkitCommands->MapAction(
		Get().SummonOpenAssetDialog,
		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnSummonedAssetPicker ) );

	ToolkitCommands->MapAction(
		Get().OpenConsoleCommandBox,
		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnSummonedConsoleCommandBox ) );
}

void FGlobalEditorCommonCommands::OnPressedCtrlTab()
{
}

void FGlobalEditorCommonCommands::OnSummonedAssetPicker()
{
	const FVector2D AssetPickerSize(600.0f, 586.0f);

	FMenuBuilder MenuBuilder(false, NULL);

	// Create the contents of the popup
	TSharedRef<SWidget> ActualWidget = SNew(SGlobalOpenAssetDialog, AssetPickerSize);

	// Wrap the picker widget in a multibox-style menu body
	MenuBuilder.BeginSection("AssetPickerOpenAsset", NSLOCTEXT("GlobalAssetPicker", "WindowTitle", "Open Asset"));
	{
		MenuBuilder.AddWidget(ActualWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	TSharedRef<SWidget> WindowContents = MenuBuilder.MakeWidget();

	// Determine where the pop-up should open
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	FVector2D WindowPosition = FSlateApplication::Get().GetCursorPos();
	if (ParentWindow.IsValid())
	{
		FSlateRect ParentMonitorRect = ParentWindow->GetFullScreenInfo();
		const FVector2D MonitorCenter((ParentMonitorRect.Right + ParentMonitorRect.Left) * 0.5f, (ParentMonitorRect.Top + ParentMonitorRect.Bottom) * 0.5f);
		WindowPosition = MonitorCenter - AssetPickerSize * 0.5f;

		// Open the pop-up
		FPopupTransitionEffect TransitionEffect(FPopupTransitionEffect::None);
		TSharedRef<SWindow> PopupWindow = FSlateApplication::Get().PushMenu(ParentWindow.ToSharedRef(), WindowContents, WindowPosition, TransitionEffect);
	}
}

static void CloseDebugConsole()
{
	FOutputLogModule& OutputLogModule = FModuleManager::LoadModuleChecked< FOutputLogModule >(TEXT("OutputLog"));
	OutputLogModule.CloseDebugConsole();
}

void FGlobalEditorCommonCommands::OnSummonedConsoleCommandBox()
{
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	
	if( ParentWindow.IsValid() )
	{
		TSharedRef<SWindow> WindowRef = ParentWindow.ToSharedRef();
		FOutputLogModule& OutputLogModule = FModuleManager::LoadModuleChecked< FOutputLogModule >(TEXT("OutputLog"));

		FDebugConsoleDelegates Delegates;
		Delegates.OnConsoleCommandExecuted = FSimpleDelegate::CreateStatic( &CloseDebugConsole );

		OutputLogModule.ToggleDebugConsoleForWindow(WindowRef, EDebugConsoleStyle::Compact, Delegates);
	}
}