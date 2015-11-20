// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginBrowserPrivatePCH.h"
#include "SNewPluginWizard.h"
#include "SListView.h"
#include "PluginStyle.h"
#include "PluginHelpers.h"
#include "DesktopPlatformModule.h"
#include "SDockTab.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "GameProjectUtils.h"
#include "PluginBrowserModule.h"
#include "SFilePathBlock.h"
#include "IMainFrameModule.h"

DEFINE_LOG_CATEGORY(LogPluginWizard);

#define LOCTEXT_NAMESPACE "NewPluginWizard"

SNewPluginWizard::SNewPluginWizard()
	: bIsPluginPathValid(false)
	, bIsPluginNameValid(false)
	, bIsEnginePlugin(false)
{
	const FText BlankTemplateName = LOCTEXT("BlankLabel", "Blank");
	const FText BasicTemplateName = LOCTEXT("BasicTemplateTabLabel", "Toolbar Button");
	const FText AdvancedTemplateName = LOCTEXT("AdvancedTemplateTabLabel", "Standalone Window");
	const FText BlueprintLibTemplateName = LOCTEXT("BlueprintLibTemplateLabel", "Blueprint Library");
	const FText EditorModeTemplateName = LOCTEXT("EditorModeTemplateLabel", "Editor Mode");
	const FText ThirdPartyTemplateName = LOCTEXT("ThirdPartyTemplateLabel", "Third Party Library");

	const FText BlankDescription = LOCTEXT("BlankTemplateDesc", "Create a blank plugin with a minimal amount of code.\n\nChoose this if you want to set everything up from scratch or are making a non-visual plugin.\nA plugin created with this template will appear in the Editor's plugin list but will not register any buttons or menu entries.");
	const FText BasicDescription = LOCTEXT("BasicTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor.\n\nStart by implementing something in the created \"OnButtonClick\" event.");
	const FText AdvancedDescription = LOCTEXT("AdvancedTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor that summons an empty standalone tab window when clicked.");
	const FText BlueprintLibDescription = LOCTEXT("BPLibTemplateDesc", "Create a plugin that will contain Blueprint Function Library.\n\nChoose this if you want to create static blueprint nodes.");
	const FText EditorModeDescription = LOCTEXT("EditorModeDesc", "Create a plugin that will have an editor mode.\n\nThis will include a toolkit example to specify UI that will appear in \"Modes\" tab (next to Foliage, Landscape etc).\nIt will also include very basic UI that demonstrates editor interaction and undo/redo functions usage.");
	const FText ThirdPartyDescription = LOCTEXT("ThirdPartyDesc", "Create a plugin that uses an included third party library.\n\nThis can be used as an example of how to include, load and use a third party library yourself.");

	Templates.Add(MakeShareable(new FPluginTemplateDescription(BlankTemplateName, BlankDescription, TEXT("Blank"))));
	Templates.Add(MakeShareable(new FPluginTemplateDescription(BasicTemplateName, BasicDescription, TEXT("Basic"))));
	Templates.Add(MakeShareable(new FPluginTemplateDescription(AdvancedTemplateName, AdvancedDescription, TEXT("Advanced"))));
	Templates.Add(MakeShareable(new FPluginTemplateDescription(BlueprintLibTemplateName, BlueprintLibDescription, TEXT("BlueprintLibrary"))));
	Templates.Add(MakeShareable(new FPluginTemplateDescription(EditorModeTemplateName, EditorModeDescription, TEXT("EditorMode"))));
	Templates.Add(MakeShareable(new FPluginTemplateDescription(ThirdPartyTemplateName, ThirdPartyDescription, TEXT("ThirdPartyLibrary"))));

	AbsoluteGamePluginPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::GamePluginsDir());
	FPaths::MakePlatformFilename(AbsoluteGamePluginPath);
	AbsoluteEnginePluginPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::EnginePluginsDir());
	FPaths::MakePlatformFilename(AbsoluteEnginePluginPath);
}

void SNewPluginWizard::Construct(const FArguments& Args, TSharedPtr<SDockTab> InOwnerTab)
{
	OwnerTab = InOwnerTab;

	LastBrowsePath = AbsoluteGamePluginPath;
	PluginFolderPath = AbsoluteGamePluginPath;
	bIsPluginPathValid = true;

	const float PaddingAmount = FPluginStyle::Get()->GetFloat("PluginCreator.Padding");

	TSharedRef<SVerticalBox> MainContent = SNew(SVerticalBox)
	+SVerticalBox::Slot()
	.Padding(PaddingAmount)
	.AutoHeight()
	[
		SNew(SBox)
		.Padding(PaddingAmount)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChoosePluginTemplate", "Choose a template and then specify a name to create a new plugin."))
		]
	]
	+SVerticalBox::Slot()
	.Padding(PaddingAmount)
	[
		// main list of plugins
		SNew( SListView<TSharedRef<FPluginTemplateDescription>> )
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource(&Templates)
		.OnGenerateRow(this, &SNewPluginWizard::OnGenerateTemplateRow)
		.OnSelectionChanged(this, &SNewPluginWizard::OnTemplateSelectionChanged)
	]
	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding(PaddingAmount)
	.HAlign(HAlign_Center)
	[
		SAssignNew(FilePathBlock, SFilePathBlock)
		.OnBrowseForFolder(this, &SNewPluginWizard::OnBrowseButtonClicked)
		.LabelBackgroundBrush(FPluginStyle::Get()->GetBrush("PluginCreator.Background"))
		.LabelBackgroundColor(FLinearColor::White)
		.FolderPath(this, &SNewPluginWizard::GetPluginDestinationPath)
		.Name(this, &SNewPluginWizard::GetCurrentPluginName)
		.NameHint(LOCTEXT("PluginNameTextHint", "Plugin name"))
		.OnFolderChanged(this, &SNewPluginWizard::OnFolderPathTextChanged)
		.OnNameChanged(this, &SNewPluginWizard::OnPluginNameTextChanged)
	];

	// Don't show the option to make an engine plugin in installed builds
	if (!FApp::IsEngineInstalled())
	{
		MainContent->AddSlot()
		.AutoHeight()
		.Padding(PaddingAmount)
		[
			SNew(SBox)
			.Padding(PaddingAmount)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SNewPluginWizard::OnEnginePluginCheckboxChanged)
				.IsChecked(this, &SNewPluginWizard::IsEnginePlugin)
				.ToolTipText(LOCTEXT("EnginePluginButtonToolTip", "Toggles whether this plugin will be created in the current project or the engine directory."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnginePluginCheckbox", "Is Engine Plugin"))
				]
			]
		];
	}

	MainContent->AddSlot()
	.AutoHeight()
	.Padding(5)
	.HAlign(HAlign_Right)
	[
		SNew(SButton)
		.ContentPadding(5)
		.TextStyle(FEditorStyle::Get(), "LargeText")
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
		.IsEnabled(this, &SNewPluginWizard::CanCreatePlugin)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("CreateButtonLabel", "Create plugin"))
		.OnClicked(this, &SNewPluginWizard::OnCreatePluginClicked)
	];

	ChildSlot
	[
		MainContent
	];
}

TSharedRef<ITableRow> SNewPluginWizard::OnGenerateTemplateRow(TSharedRef<FPluginTemplateDescription> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const float PaddingAmount = FPluginStyle::Get()->GetFloat("PluginTile.Padding");
	const float ThumbnailImageSize = FPluginStyle::Get()->GetFloat("PluginTile.ThumbnailImageSize");

	// Plugin thumbnail image
	FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"))->GetBaseDir();
	FString Icon128FilePath = PluginBaseDir / TEXT("Templates") / Item->OnDiskPath / TEXT("Resources/Icon128.png");
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*Icon128FilePath))
	{
		Icon128FilePath = PluginBaseDir / TEXT("Resources/DefaultIcon128.png");
	}

	const FName BrushName(*Icon128FilePath);
	const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
	if ((Size.X > 0) && (Size.Y > 0))
	{
		Item->PluginIconDynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(Size.X, Size.Y)));
	}

	return SNew(STableRow< TSharedRef<FPluginTemplateDescription> >, OwnerTable)
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
						.Image(Item->PluginIconDynamicImageBrush.IsValid() ? Item->PluginIconDynamicImageBrush.Get() : nullptr)
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(PaddingAmount)
					[
						SNew(STextBlock)
						.Text(Item->Name)
						.TextStyle(FPluginStyle::Get(), "PluginTile.NameText")
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(PaddingAmount)
					[
						SNew(SRichTextBlock)
						.Text(Item->Description)
						.TextStyle(FPluginStyle::Get(), "PluginTile.DescriptionText")
						.AutoWrapText(true)
					]
				]
			]
		]
	];
}

void SNewPluginWizard::OnTemplateSelectionChanged( TSharedPtr<FPluginTemplateDescription> InItem, ESelectInfo::Type SelectInfo )
{
	CurrentTemplate = InItem;
}

void SNewPluginWizard::OnFolderPathTextChanged(const FText& InText)
{
	PluginFolderPath = InText.ToString();
	FPaths::MakePlatformFilename(PluginFolderPath);
	ValidateFullPluginPath();
}

void SNewPluginWizard::OnPluginNameTextChanged(const FText& InText)
{
	PluginNameText = InText;
	ValidateFullPluginPath();
}

FReply SNewPluginWizard::OnBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		FString FolderName;
		const FString Title = LOCTEXT("NewPluginBrowseTitle", "Choose a plugin location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowWindowHandle,
			Title,
			LastBrowsePath,
			FolderName
			);

		if (bFolderSelected)
		{
			LastBrowsePath = FolderName;
			OnFolderPathTextChanged(FText::FromString(FolderName));
		}
	}

	return FReply::Handled();
}

void SNewPluginWizard::ValidateFullPluginPath()
{
	// Check for issues with path
	bIsPluginPathValid = false;
	bool bIsNewPathValid = true;
	FText FolderPathError;

	if (!FPaths::ValidatePath(GetPluginDestinationPath().ToString(), &FolderPathError))
	{
		bIsNewPathValid = false;
	}

	if (bIsNewPathValid)
	{
		bool bFoundValidPath = false;
		FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GetPluginDestinationPath().ToString());
		FPaths::MakePlatformFilename(AbsolutePath);

		if (AbsolutePath.StartsWith(AbsoluteGamePluginPath))
		{
			bFoundValidPath = true;
			bIsEnginePlugin = false;
		}

		if (!bFoundValidPath && !FApp::IsEngineInstalled())
		{
			if (AbsolutePath.StartsWith(AbsoluteEnginePluginPath))
			{
				bFoundValidPath = true;
				bIsEnginePlugin = true;
			}
		}

		bIsNewPathValid = bFoundValidPath;
		if (!bFoundValidPath)
		{
			if (FApp::IsEngineInstalled())
			{
				FolderPathError = LOCTEXT("InstalledPluginFolderPathError", "Plugins can only be created within your Project's Plugins folder");
			}
			else
			{
				FolderPathError = LOCTEXT("PluginFolderPathError", "Plugins can only be created within the Engine Plugins folder or your Project's Plugins folder");
			}
		}
	}

	bIsPluginPathValid = bIsNewPathValid;
	FilePathBlock->SetFolderPathError(FolderPathError);

	// Check for issues with name
	bIsPluginNameValid = false;
	bool bIsNewNameValid = true;
	FText PluginNameError;

	// Fail silently if text is empty
	if (GetCurrentPluginName().IsEmpty())
	{
		bIsNewNameValid = false;
	}

	// Don't allow commas, dots, etc...
	FString IllegalCharacters;
	if (bIsNewNameValid && !GameProjectUtils::NameContainsOnlyLegalCharacters(GetCurrentPluginName().ToString(), IllegalCharacters))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("IllegalCharacters"), FText::FromString(IllegalCharacters));
		PluginNameError = FText::Format(LOCTEXT("WrongPluginNameErrorText", "Plugin name cannot contain illegal characters like: \"{IllegalCharacters}\""), Args);
		bIsNewNameValid = false;
	}

	if (bIsNewNameValid)
	{
		const FString& TestPluginName = GetCurrentPluginName().ToString();

		// Check to see if a a compiled plugin with this name exists (at any path)
		const TArray< FPluginStatus > Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
		for (auto PluginIt(Plugins.CreateConstIterator()); PluginIt; ++PluginIt)
		{
			const auto& PluginStatus = *PluginIt;

			if (PluginStatus.Name == TestPluginName)
			{
				PluginNameError = LOCTEXT("PluginNameExistsErrorText", "A plugin with this name already exists!");
				bIsNewNameValid = false;
				break;
			}
		}
	}

	// Check to see if a .uplugin exists at this path (in case there is an uncompiled or disabled plugin)
	if (bIsNewNameValid)
	{
		const FString TestPluginPath = GetPluginFilenameWithPath();
		if (!TestPluginPath.IsEmpty())
		{
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*TestPluginPath))
			{
				PluginNameError = LOCTEXT("PluginPathExistsErrorText", "A plugin already exists at this path!");
				bIsNewNameValid = false;
			}
		}
	}

	bIsPluginNameValid = bIsNewNameValid;
	FilePathBlock->SetNameError(PluginNameError);
}

bool SNewPluginWizard::CanCreatePlugin() const
{
	return bIsPluginPathValid && bIsPluginNameValid && CurrentTemplate.IsValid();
}

FText SNewPluginWizard::GetPluginDestinationPath() const
{
	return FText::FromString(PluginFolderPath);
}

FText SNewPluginWizard::GetCurrentPluginName() const
{
	return PluginNameText;
}

FString SNewPluginWizard::GetPluginFilenameWithPath() const
{
	if (PluginFolderPath.IsEmpty() || PluginNameText.IsEmpty())
	{
		// Don't even try to assemble the path or else it may be relative to the binaries folder!
		return TEXT("");
	}
	else
	{
		const FString& TestPluginName = PluginNameText.ToString();
		FString TestPluginPath = PluginFolderPath / TestPluginName / (TestPluginName + TEXT(".uplugin"));
		FPaths::MakePlatformFilename(TestPluginPath);
		return TestPluginPath;
	}
}

ECheckBoxState SNewPluginWizard::IsEnginePlugin() const
{
	return bIsEnginePlugin ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewPluginWizard::OnEnginePluginCheckboxChanged(ECheckBoxState NewCheckedState)
{
	bool bNewEnginePluginState = NewCheckedState == ECheckBoxState::Checked;
	if (bIsEnginePlugin != bNewEnginePluginState)
	{
		bIsEnginePlugin = bNewEnginePluginState;
		if (bIsEnginePlugin)
		{
			PluginFolderPath = AbsoluteEnginePluginPath;
		}
		else
		{
			PluginFolderPath = AbsoluteGamePluginPath;
		}
		bIsPluginPathValid = true;
		FilePathBlock->SetFolderPathError(FText::GetEmpty());
	}
}

FReply SNewPluginWizard::OnCreatePluginClicked()
{
	const FString AutoPluginName = PluginNameText.ToString();
	const FPluginTemplateDescription& SelectedTemplate = *CurrentTemplate.Get();

	// Plugin thumbnail image
	bool bRequiresDefaultIcon = false;
	FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"))->GetBaseDir();
	FString TemplateFolderName = PluginBaseDir / TEXT("Templates") / SelectedTemplate.OnDiskPath;
	FString PluginEditorIconPath = TemplateFolderName / TEXT("Resources/Icon128.png");
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*PluginEditorIconPath))
	{
		PluginEditorIconPath = PluginBaseDir / TEXT("Resources/DefaultIcon128.png");
		bRequiresDefaultIcon = true;
	}

	TArray<FString> CreatedFiles;

	FText LocalFailReason;

	bool bSucceeded = true;

	// Save descriptor file as .uplugin file
	const FString UPluginFilePath = GetPluginFilenameWithPath();
	bSucceeded = bSucceeded && WritePluginDescriptor(AutoPluginName, UPluginFilePath);

	// Main plugin dir
	const FString PluginFolder = GetPluginDestinationPath().ToString() / AutoPluginName;

	// Resource folder
	const FString ResourcesFolder = PluginFolder / TEXT("Resources");

	if (bRequiresDefaultIcon)
	{
		// Copy the icon
		bSucceeded = bSucceeded && CopyFile(ResourcesFolder / TEXT("Icon128.png"), PluginEditorIconPath, /*inout*/ CreatedFiles);
	}

	if (bSucceeded && !FPluginHelpers::CopyPluginTemplateFolder(*PluginFolder, *TemplateFolderName, AutoPluginName))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedTemplateCopy", "Failed to copy plugin Template: {0}"), FText::FromString(TemplateFolderName)));
		bSucceeded = false;
	}

	if (bSucceeded)
	{
		// Generate project files if we happen to be using a project file.
		if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
		{
			PopErrorNotification(LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files."));
		}

		// Notify that a new plugin has been created
		FPluginBrowserModule& PluginBrowserModule = FPluginBrowserModule::Get();
		PluginBrowserModule.BroadcastNewPluginCreated();

		// Add game plugins to list of pending enable plugins 
		if (!bIsEnginePlugin)
		{
			PluginBrowserModule.SetPluginPendingEnableState(AutoPluginName, false, true);
		}

		FText DialogTitle = LOCTEXT("PluginCreatedTitle", "New Plugin Created");
		FText DialogText = LOCTEXT("PluginCreatedText", "Restart the editor to activate new Plugin or reopen your code solution to see/edit the newly created source files.");
		FMessageDialog::Open(EAppMsgType::Ok, DialogText, &DialogTitle);

		auto PinnedOwnerTab = OwnerTab.Pin();
		if (PinnedOwnerTab.IsValid())
		{
			PinnedOwnerTab->RequestCloseTab();
		}

		return FReply::Handled();
	}
	else
	{
		DeletePluginDirectory(*PluginFolder);
		return FReply::Unhandled();
	}
}

bool SNewPluginWizard::CopyFile(const FString& DestinationFile, const FString& SourceFile, TArray<FString>& InOutCreatedFiles)
{
	if (IFileManager::Get().Copy(*DestinationFile, *SourceFile, /*bReplaceExisting=*/ false) != COPY_OK)
	{
		const FText ErrorMessage = FText::Format(LOCTEXT("ErrorCopyingFile", "Error: Couldn't copy file '{0}' to '{1}'"), FText::AsCultureInvariant(SourceFile), FText::AsCultureInvariant(DestinationFile));
		PopErrorNotification(ErrorMessage);
		return false;
	}
	else
	{
		InOutCreatedFiles.Add(DestinationFile);
		return true;
	}
}

bool SNewPluginWizard::WritePluginDescriptor(const FString& PluginModuleName, const FString& UPluginFilePath)
{
	FPluginDescriptor Descriptor;

	Descriptor.FriendlyName = PluginModuleName;
	Descriptor.Version = 1;
	Descriptor.VersionName = TEXT("1.0");
	Descriptor.Category = TEXT("Other");
	Descriptor.Modules.Add(FModuleDescriptor(*PluginModuleName, EHostType::Developer));

	// Save the descriptor using JSon
	FText FailReason;
	if (Descriptor.Save(UPluginFilePath, FailReason))
	{
		return true;
	}
	else
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedToWriteDescriptor", "Couldn't save plugin descriptor under %s"), FText::AsCultureInvariant(UPluginFilePath)));
		return false;
	}
}

void SNewPluginWizard::PopErrorNotification(const FText& ErrorMessage)
{
	UE_LOG(LogPluginWizard, Log, TEXT("%s"), *ErrorMessage.ToString());

	// Create and display a notification about the failure
	FNotificationInfo Info(ErrorMessage);
	Info.ExpireDuration = 2.0f;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

void SNewPluginWizard::DeletePluginDirectory(const FString& InPath)
{
	IFileManager::Get().DeleteDirectory(*InPath, false, true);
}

#undef LOCTEXT_NAMESPACE