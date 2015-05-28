// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"
#include "SPluginCreatorTabContent.h"

#include "STabButtonList.h"
#include "PluginDescriptorObject.h"

#include "SWidgetSwitcher.h"

#include "SlateExtras.h"
#include "Runtime/Slate/Private/Framework/Docking/DockingPrivate.h"

#include "SLayoutCreator.h"
#include "EditorStyle.h"

#include "GameProjectUtils.h"

#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "PluginCreatorPluginModule"

SPluginCreatorTabContent::SPluginCreatorTabContent()
{
	const FText BlankTemplateName = LOCTEXT("BlankLabel", "Blank");
	const FText BasicTemplateName = LOCTEXT("BasicTemplateTabLabel", "Toolbar Button");
	const FText AdvancedTemplateName = LOCTEXT("AdvancedTemplateTabLabel", "Standalone Window");
	const FText BlankDescription = LOCTEXT("BlankTemplateDesc", "Create a blank plugin with a minimal amount of code.\n\nChoose this if you want to set everything up from scratch or are making a non-visual plugin.\nA plugin created with this template will appear in the Editor's plugin list but will not register any buttons or menu entries.\n\nThis template will generate the following structure at:");
	const FText BasicDescription = LOCTEXT("BasicTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor.\n\nStart by implementing something in the created \"OnButtonClick\" event.\n\nThis template will generate the following structure at:");
	const FText AdvancedDescription = LOCTEXT("AdvancedTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor that summons an empty standalone tab window when clicked.\n\nThis template will generate the following structure at:");

	new (Templates) FPluginTemplateDescription(BlankTemplateName, BlankDescription, TEXT("BlankPluginSource"), TEXT("Blank"), false);
	new (Templates) FPluginTemplateDescription(BasicTemplateName, BasicDescription, TEXT("BasicPluginSource"), TEXT("Basic"), true);
	new (Templates) FPluginTemplateDescription(AdvancedTemplateName, AdvancedDescription, TEXT("BasicPluginSource"), TEXT("Advanced"), true);
}


SPluginCreatorTabContent::~SPluginCreatorTabContent()
{
	if (PropertyView.IsValid())
	{
		PropertyView->SetObject(nullptr, true);
		PropertyView.Reset();
	}

	if (DescriptorObject != nullptr)
	{
		DescriptorObject->RemoveFromRoot();
		DescriptorObject = nullptr;
	}
}

void SPluginCreatorTabContent::Construct(const FArguments& InArgs, TSharedPtr<SDockTab> InOwnerTab)
{
	OwnerTab = InOwnerTab;
	SelectedTemplateIndex = 0;

	bUsePublicPrivateSplit = true;
	bPluginNameIsValid = false;
	bIsEnginePlugin = false;

	OnPluginCreated = InArgs._OnPluginCreated;

	InitDetailsView();


	TemplateWidgetSwitcher = SNew(SWidgetSwitcher)
		.WidgetIndex(0);
	TArray<FText> ButtonLabels;

	for (const FPluginTemplateDescription& Template : Templates)
	{
		ButtonLabels.Add(Template.Name);

		TemplateWidgetSwitcher->AddSlot()
		[
			GetTemplateDescriptionWidget(Template.Description, FPluginCreatorStyle::Get().GetBrush(Template.ImageStylePath))
		];
	}

	PluginEditorIconPath = GetPluginCreatorRootPath() / TEXT("Resources") / TEXT("DefaultIcon128.png");
	PluginButtonIconPath = GetPluginCreatorRootPath() / TEXT("Resources") / TEXT("ButtonIcon_40x.png");

	TSharedPtr<STabButtonList> TabList = SNew(STabButtonList)
		.ButtonLabels(ButtonLabels)
		.OnTabButtonSelectionChanged(FOnTextChanged::CreateRaw(this, &SPluginCreatorTabContent::OnTemplatesTabSwitched));

	FSlateFontInfo PluginNameFont = FPluginCreatorStyle::Get().GetFontStyle(TEXT("PluginNameFont"));


	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(10)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.PhysicalSplitterHandleSize(5)
				+SSplitter::Slot()
				.Value(0.55f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FPluginCreatorStyle::Get().GetBrush(TEXT("DarkGrayBackground")))
						.Padding(15)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							[
								SNew(SBox)
								.HeightOverride(64)
								[
									SAssignNew(PluginNameTextBox, SEditableTextBox)
									.Font(PluginNameFont)
									.HintText(LOCTEXT("PluginNameTextHint", "Plugin name"))
									.OnTextChanged(FOnTextChanged::CreateRaw(this, &SPluginCreatorTabContent::OnPluginNameTextChanged))
								]
								
							]

							// Disabled as it doesn't affect the source hierarchy shown, and isn't super useful
#if UE_PLUGIN_CREATOR_WIP
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Padding(3)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged(this, &SPluginCreatorTabContent::OnUsePrivatePublicSplitChanged)
								.IsChecked(ECheckBoxState::Checked)
								.Content()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("PrivatePublicCheckboxLabel", "Use Private/Public source folder split"))
								]
							]
#endif

							// Disabled because the code in PluginStyle.cpp.template doesn't handle setting the path correctly for engine versus game
#if UE_PLUGIN_CREATOR_WIP
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Padding(3)
							[
								SNew(SCheckBox)
								.OnCheckStateChanged(this, &SPluginCreatorTabContent::OnIsEnginePluginChanged)
								.IsChecked(ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("EngineCheckboxLabel", "Should this plugin be an engine plugin?"))
								]
							]
#endif

							// Disable due to styling and behavior
#if UE_PLUGIN_CREATOR_WIP
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(64)
									.HeightOverride(64)
									[
										SNew(SButton)
										.ToolTipText(LOCTEXT("PluginEditorIconTooltip", "This icon will be displayed in plugin editor list. Click to choose a file."))
										.OnClicked(this, &SPluginCreatorTabContent::OnChangePluginEditorIcon)
										.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
										.ContentPadding(2)
										.Content()
										[
											SNew(SImage)
											.Image(this, &SPluginCreatorTabContent::GetPluginEditorImage)
										]

									]
								]
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(64)
									.HeightOverride(64)
									[
										SNew(SButton)
										.ToolTipText(LOCTEXT("PluginButtonIconTooltip", "This icon will be displayed on editor toolbar and under window menu. Click to choose a file."))
										.OnClicked(this, &SPluginCreatorTabContent::OnChangePluginButtonIcon)
										.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
										.ContentPadding(2)
										.Content()
										[
											SNew(SImage)
											.Image(this, &SPluginCreatorTabContent::GetPluginButtonImage)
										]

									]
								]
							]
#endif
						]
					]
					+ SVerticalBox::Slot()
					.Padding(5)
					[
						PropertyView.ToSharedRef()
					]
				]
				+ SSplitter::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0)
					.AutoHeight()
					[
						TabList.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.Padding(0)
					[
						SNew(SBorder)
						.BorderImage(FPluginCreatorStyle::Get().GetBrush(TEXT("DarkGrayBackground")))
						.Padding(15)
						[
							TemplateWidgetSwitcher.ToSharedRef()
						]
					]
				]
			]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ContentPadding(5)
					.TextStyle(FEditorStyle::Get(), "LargeText")
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
					.IsEnabled(this, &SPluginCreatorTabContent::IsPluginNameValid)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("GoButtonLabel", "Create plugin"))
					.OnClicked(this, &SPluginCreatorTabContent::OnCreatePluginClicked)
				]
		];

		
}

TSharedRef<SWidget> SPluginCreatorTabContent::GetTemplateDescriptionWidget(const FText& TemplateDescription, const FSlateBrush* TemplateImage)
{
	return
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SRichTextBlock)
		.Text(TemplateDescription)
		.AutoWrapText(true)
	]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0, 4)
		[
			GetPathTextWidget()
		]
	+ SVerticalBox::Slot()
	.Padding(20)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SImage)
		.Image(TemplateImage)
	];
}

const FSlateBrush* SPluginCreatorTabContent::GetPluginEditorImage() const
{
	return PluginEditorIconBrush.IsValid() ? PluginEditorIconBrush.Get() : FPluginCreatorStyle::Get().GetBrush("DefaultPluginIcon");
}

const FSlateBrush* SPluginCreatorTabContent::GetPluginButtonImage() const
{
	return PluginButtonIconBrush.IsValid() ? PluginButtonIconBrush.Get() : FPluginCreatorStyle::Get().GetBrush("DefaultPluginIcon");
}

FReply SPluginCreatorTabContent::OnChangePluginEditorIcon()
{
	TArray<FString> OutFiles;

	bool bOpened = OpenImageDialog(OutFiles);


	if (bOpened && OutFiles.Num() > 0)
	{
		FString Filename = OutFiles[0];

		FName BrushName(*Filename);

		PluginEditorIconPath = Filename;

		PluginEditorIconBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(64, 64)));
	}

	return FReply::Handled();
}

FReply SPluginCreatorTabContent::OnChangePluginButtonIcon()
{
	TArray<FString> OutFiles;

	bool bOpened = OpenImageDialog(OutFiles);


	if (bOpened && OutFiles.Num() > 0)
	{
		FString Filename = OutFiles[0];

		FName BrushName(*Filename);

		PluginButtonIconPath = Filename;

		PluginButtonIconBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(64, 64)));
	}

	return FReply::Handled();
}

bool SPluginCreatorTabContent::OpenImageDialog(TArray<FString>& OutFiles)
{
	// TODO: store recent path so the next time user opens a dialog it opens in last loaded path
	void* ParentWindowPtr = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();

	if (ParentWindowPtr)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform)
		{
			FString FileTypes = TEXT("Images (*.png)|*.png|All Files (*.*)|*.*");

			return DesktopPlatform->OpenFileDialog(
				ParentWindowPtr,
				"ChoosePluginEditorIcon",
				TEXT(""),
				TEXT(""),
				FileTypes,
				EFileDialogFlags::None,
				OutFiles
				);
		}
	}

	return false;
}

void SPluginCreatorTabContent::OnTemplatesTabSwitched(const FText& InNewTab)
{
	if (TemplateWidgetSwitcher.IsValid())
	{
		SelectedTemplateIndex = 0;
		for (int32 TemplateIndex = 0; TemplateIndex < Templates.Num(); ++TemplateIndex)
		{
			if (Templates[TemplateIndex].Name.EqualToCaseIgnored(InNewTab))
			{
				SelectedTemplateIndex = TemplateIndex;
				break;
			}
		}

		TemplateWidgetSwitcher->SetActiveWidgetIndex(SelectedTemplateIndex);
	}
}

void SPluginCreatorTabContent::InitDetailsView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs( 
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		/*const ENameAreaSettings InNameAreaSettings =*/ FDetailsViewArgs::ENameAreaSettings::ActorsUseNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ NULL,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	DescriptorObject = NewObject<UPluginDescriptorObject>();
	DescriptorObject->AddToRoot();

	PropertyView->SetObject(DescriptorObject, true);
}

void SPluginCreatorTabContent::OnUsePrivatePublicSplitChanged(ECheckBoxState InState)
{
	bUsePublicPrivateSplit = InState == ECheckBoxState::Checked;
}


void SPluginCreatorTabContent::OnIsEnginePluginChanged(ECheckBoxState InState)
{
	bIsEnginePlugin = InState == ECheckBoxState::Checked;
}

void SPluginCreatorTabContent::OnPluginNameTextChanged(const FText& InText)
{
	// Early exit if text is empty
	if (InText.IsEmpty())
	{
		bPluginNameIsValid = false;
		PluginNameText = FText();
		PluginNameTextBox->SetError(FText::GetEmpty());

		if (DescriptorObject)
		{
			DescriptorObject->FriendlyName = InText.ToString();
		}
		return;
	}

	// Don't allow commas, dots, etc...
	FString IllegalCharacters;
	if (!GameProjectUtils::NameContainsOnlyLegalCharacters(InText.ToString(), IllegalCharacters))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("IllegalCharacters"), FText::FromString(IllegalCharacters));
		FText ErrorText = FText::Format(LOCTEXT("WrongPluginNameErrorText", "Plugin name cannot contain illegal characters like: \"{IllegalCharacters}\""), Args);

		PluginNameTextBox->SetError(ErrorText);
		bPluginNameIsValid = false;
		return;
	}

	const FString TestPluginName = InText.ToString();

	// Check to see if a a compiled plugin with this name exists (at any path)
	const TArray< FPluginStatus > Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
	for (auto PluginIt(Plugins.CreateConstIterator()); PluginIt; ++PluginIt)
	{
		const auto& PluginStatus = *PluginIt;

		if (PluginStatus.Name == TestPluginName)
		{
			PluginNameTextBox->SetError(LOCTEXT("PluginNameExistsErrorText", "A plugin with this name already exists!"));
			bPluginNameIsValid = false;

			return;
		}
	}

	// Check to see if a .uplugin exists at this path (in case there is an uncompiled or disabled plugin)
	const FString TestPluginPath = GetPluginDestinationPath().ToString() / TestPluginName / (TestPluginName + TEXT(".uplugin"));
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*TestPluginPath))
	{
		PluginNameTextBox->SetError(LOCTEXT("PluginPathExistsErrorText", "A plugin already exists at this path!"));
		bPluginNameIsValid = false;

		return;
	}


	PluginNameTextBox->SetError(FText::GetEmpty());
	bPluginNameIsValid = true;

	PluginNameText = InText;

	if (DescriptorObject)
	{
		DescriptorObject->FriendlyName = InText.ToString();
	}
}

bool SPluginCreatorTabContent::MakeDirectory(const FString& InPath)
{
	if (!IFileManager::Get().MakeDirectory(*InPath))
	{
		PopErrorNotification(FText::Format(LOCTEXT("CreateDirectoryFailed", "Failed to create directory '{0}'"), FText::AsCultureInvariant(InPath)));
		return false;
	}

	return true;
}

void SPluginCreatorTabContent::DeletePluginDirectory(const FString& InPath)
{
	IFileManager::Get().DeleteDirectory(*InPath, false, true);
}

void SPluginCreatorTabContent::PopErrorNotification(const FText& ErrorMessage)
{
	UE_LOG(PluginCreatorPluginLog, Log, TEXT("%s"), *ErrorMessage.ToString());

	// Create and display a notification about the failure
	FNotificationInfo Info(ErrorMessage);
	Info.ExpireDuration = 2.0f;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

bool SPluginCreatorTabContent::CopyFile(const FString& DestinationFile, const FString& SourceFile, TArray<FString>& InOutCreatedFiles)
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

bool SPluginCreatorTabContent::WritePluginDescriptor(const FString& PluginModuleName, const FString& UPluginFilePath)
{
	FPluginDescriptor Descriptor;

	if (DescriptorObject)
	{
		DescriptorObject->FillDescriptor(Descriptor);
	}

	// TODO: let user choose what kind of modules he/she wants to include in new plugin
	TArray<FString> StartupModuleNames;
	StartupModuleNames.Add(PluginModuleName);

	for (int32 Idx = 0; Idx < StartupModuleNames.Num(); Idx++)
	{
		Descriptor.Modules.Add(FModuleDescriptor(*StartupModuleNames[Idx], EHostType::Developer));
	}

	// Save the descriptor using JSon
	if (FPluginHelpers::SavePluginDescriptor(UPluginFilePath, Descriptor))
	{
		return true;
	}
	else
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedToWriteDescriptor", "Couldn't save plugin descriptor under %s"), FText::AsCultureInvariant(UPluginFilePath)));
		return false;
	}
}

FReply SPluginCreatorTabContent::OnCreatePluginClicked()
{
	const FString AutoPluginName = PluginNameText.ToString();
	const FPluginTemplateDescription& SelectedTemplate = Templates[SelectedTemplateIndex];

	TArray<FString> CreatedFiles;

	FText LocalFailReason;

	FReply ReturnReply = FReply::Unhandled();

	// If we're creating a game plugin, GameDir\Plugins folder may not exist so we have to create it
	if (!bIsEnginePlugin && !IFileManager::Get().DirectoryExists(*GetPluginDestinationPath().ToString()))
	{
		if (!MakeDirectory(*GetPluginDestinationPath().ToString()))
		{
			return FReply::Unhandled();
		}
	}

	// Create main plugin dir
	const FString PluginFolder = GetPluginDestinationPath().ToString() / AutoPluginName;
	if (!MakeDirectory(PluginFolder))
	{
		return FReply::Unhandled();
	}

	bool bSucceeded = true;

	// Create resource folder
	const FString ResourcesFolder = PluginFolder / TEXT("Resources");
	bSucceeded = bSucceeded && MakeDirectory(ResourcesFolder);

	// Copy the icons
	bSucceeded = bSucceeded && CopyFile(ResourcesFolder / TEXT("Icon128.png"), PluginEditorIconPath, /*inout*/ CreatedFiles);

	if (SelectedTemplate.bIncludeUI)
	{
		bSucceeded = bSucceeded && CopyFile(ResourcesFolder / TEXT("ButtonIcon_40x.png"), PluginButtonIconPath, /*inout*/ CreatedFiles);
	}

	// Create source folder
	const FString SourceFolder = PluginFolder / TEXT("Source");
	bSucceeded = bSucceeded && MakeDirectory(SourceFolder);

	// Save descriptor file as .uplugin file
	const FString UPluginFilePath = PluginFolder / AutoPluginName + TEXT(".uplugin");
	bSucceeded = bSucceeded && WritePluginDescriptor(AutoPluginName, UPluginFilePath);

	//
	const FString PluginSourceFolder = SourceFolder / AutoPluginName;
	bSucceeded = bSucceeded && MakeDirectory(PluginSourceFolder);

	FString PrivateSourceFolder = PluginSourceFolder;
	FString PublicSourceFolder = PluginSourceFolder;

	// Create Private/Public folder split if user wants to
	if (bUsePublicPrivateSplit)
	{
		PrivateSourceFolder = PluginSourceFolder / TEXT("Private");
		bSucceeded = bSucceeded && MakeDirectory(PrivateSourceFolder);

		PublicSourceFolder = PluginSourceFolder / TEXT("Public");
		bSucceeded = bSucceeded && MakeDirectory(PublicSourceFolder);
	}

	// Based on chosen template create build, and other source files
	if (bSucceeded && !FPluginHelpers::CreatePluginBuildFile(PluginSourceFolder / AutoPluginName + TEXT(".Build.cs"), AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedBuild", "Failed to create plugin build file. {0}"), LocalFailReason));
		bSucceeded = false;
	}

	if (bSucceeded && !FPluginHelpers::CreatePluginHeaderFile(PublicSourceFolder, AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedHeader", "Failed to create plugin header file. {0}"), LocalFailReason));
		bSucceeded = false;
	}

	if (bSucceeded && !FPluginHelpers::CreatePrivatePCHFile(PrivateSourceFolder, AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedPCH", "Failed to create plugin PCH file. {0}"), LocalFailReason));
		bSucceeded = false;
	}

	if (bSucceeded && !FPluginHelpers::CreatePluginCPPFile(PrivateSourceFolder, AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedCppFile", "Failed to create plugin cpp file. {0}"), LocalFailReason));
		bSucceeded = false;
	}

	if (SelectedTemplate.bIncludeUI)
	{
		if (bSucceeded && !FPluginHelpers::CreatePluginStyleFiles(PrivateSourceFolder, PublicSourceFolder, AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
		{
			PopErrorNotification(FText::Format(LOCTEXT("FailedStylesFile", "Failed to create plugin styles files. {0}"), LocalFailReason));
			bSucceeded = false;
		}

		if (bSucceeded && !FPluginHelpers::CreateCommandsFiles(PublicSourceFolder, AutoPluginName, LocalFailReason, SelectedTemplate.OnDiskPath))
		{
			PopErrorNotification(FText::Format(LOCTEXT("FailedStylesFile", "Failed to create plugin commands files. {0}"), LocalFailReason));
			bSucceeded = false;
		}
	}

	if (bSucceeded)
	{
		const bool bShowFolder = (DescriptorObject != nullptr) && (DescriptorObject->bShowPluginFolder);
		if (bShowFolder)
		{
			// Show the plugin folder in the file system
			FPlatformProcess::ExploreFolder(*UPluginFilePath);
		}

		const bool bRegenerateProjectFiles = (DescriptorObject != nullptr) && (DescriptorObject->bRegenerateProjectFiles);
		if (bRegenerateProjectFiles)
		{
			// Generate project files if we happen to be using a project file.
			if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
			{
				PopErrorNotification(LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files."));
			}
		}

		OnPluginCreated.ExecuteIfBound();
		if (SDockTab* StrongOwner = OwnerTab.Pin().Get())
		{
			StrongOwner->RequestCloseTab();
		}

		return FReply::Handled();
	}
	else
	{
		DeletePluginDirectory(*PluginFolder);
		return FReply::Unhandled();
	}
}

FText SPluginCreatorTabContent::GetPluginDestinationPath() const
{
	const FString EnginePath = FPaths::EnginePluginsDir();
	const FString GamePath = FPaths::GamePluginsDir();

	return bIsEnginePlugin ? FText::FromString(IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*EnginePath)) 
		: FText::FromString(IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GamePath));
}

TSharedRef<SWidget> SPluginCreatorTabContent::GetPathTextWidget()
{
	return SNew(STextBlock)
		.Text(this, &SPluginCreatorTabContent::GetPluginDestinationPath);
}



#undef LOCTEXT_NAMESPACE