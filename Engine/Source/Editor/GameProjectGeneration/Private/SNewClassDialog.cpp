// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GameProjectGenerationPrivatePCH.h"
#include "SourceCodeNavigation.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Editor/ClassViewer/Private/SClassViewer.h"
#include "DesktopPlatformModule.h"
#include "Editor/Documentation/Public/IDocumentation.h"
#include "EditorClassUtils.h"
#include "SWizard.h"
#include "SHyperlink.h"
#include "TutorialMetaData.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/BlueprintFunctionLibrary.h"


#define LOCTEXT_NAMESPACE "GameProjectGeneration"


struct FParentClassItem
{
	GameProjectUtils::FNewClassInfo ParentClassInfo;

	FParentClassItem(const GameProjectUtils::FNewClassInfo& InParentClassInfo)
		: ParentClassInfo(InParentClassInfo)
	{}
};

class FNativeClassParentFilter : public IClassViewerFilter
{
public:
	FNativeClassParentFilter(const TSharedPtr<FModuleContextInfo>* InSelectedModuleInfoPtr)
		: SelectedModuleInfoPtr(InSelectedModuleInfoPtr)
	{
	}

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		// You may not make native classes based on blueprint generated classes
		const bool bIsBlueprintClass = (InClass->ClassGeneratedBy != NULL);

		// UObject is special cased to be extensible since it would otherwise not be since it doesn't pass the API check (intrinsic class).
		const bool bIsExplicitlyUObject = (InClass == UObject::StaticClass());

		// Is this class in the same module as our current module?
		const FString ClassModuleName = InClass->GetOutermost()->GetName().RightChop( FString(TEXT("/Script/")).Len() );
		const TSharedPtr<FModuleContextInfo>& ModuleInfo = *SelectedModuleInfoPtr;
		const bool bIsInDestinationModule = (ModuleInfo.IsValid() && ModuleInfo->ModuleName == ClassModuleName);

		// You need API if you are either not UObject itself and you are not in the destination module
		const bool bNeedsAPI = !bIsExplicitlyUObject && !bIsInDestinationModule;

		// You may not make a class that is not DLL exported.
		// MinimalAPI classes aren't compatible with the DLL export macro, but can still be used as a valid base
		const bool bHasAPI = InClass->HasAnyClassFlags(CLASS_RequiredAPI) || InClass->HasAnyClassFlags(CLASS_MinimalAPI);

		// @todo should we support interfaces?
		const bool bIsInterface = InClass->IsChildOf(UInterface::StaticClass());

		return !bIsBlueprintClass && (!bNeedsAPI || bHasAPI) && !bIsInterface;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return false;
	}

private:
	/** Pointer to the currently selected module in the new class dialog */
	const TSharedPtr<FModuleContextInfo>* SelectedModuleInfoPtr;
};

static void FindPublicEngineHeaderFiles(TArray<FString>& OutFiles, const FString& Path)
{
	TArray<FString> ModuleDirs;
	IFileManager::Get().FindFiles(ModuleDirs, *(Path / TEXT("*")), false, true);
	for (const FString& ModuleDir : ModuleDirs)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *(Path / ModuleDir / TEXT("Classes")), TEXT("*.h"), true, false, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *(Path / ModuleDir / TEXT("Public")), TEXT("*.h"), true, false, false);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SNewClassDialog::Construct( const FArguments& InArgs )
{
	{
		TArray<FModuleContextInfo> CurrentModules = GameProjectUtils::GetCurrentProjectModules();
		check(CurrentModules.Num()); // this should never happen since GetCurrentProjectModules is supposed to add a dummy runtime module if the project currently has no modules

		AvailableModules.Reserve(CurrentModules.Num());
		for(const FModuleContextInfo& ModuleInfo : CurrentModules)
		{
			AvailableModules.Emplace(MakeShareable(new FModuleContextInfo(ModuleInfo)));
		}
	}

	// If we have a runtime module with the same name as our project, then use that
	// Otherwise, set out default target module as the first runtime module in the list
	{
		const FString ProjectName = FApp::GetGameName();
		for(const auto& AvailableModule : AvailableModules)
		{
			if(AvailableModule->ModuleName == ProjectName)
			{
				SelectedModuleInfo = AvailableModule;
				break;
			}
			
			if(AvailableModule->ModuleType == EHostType::Runtime)
			{
				SelectedModuleInfo = AvailableModule;
				// keep going in case we find a better match
			}
		}

		if (!SelectedModuleInfo.IsValid())
		{
			// No runtime modules? Just take the first available module then
			SelectedModuleInfo = AvailableModules[0];
		}
	}

	NewClassPath = SelectedModuleInfo->ModuleSourcePath;
	ClassLocation = GameProjectUtils::EClassLocation::UserDefined; // the first call to UpdateInputValidity will set this correctly based on NewClassPath

	ParentClassInfo = GameProjectUtils::FNewClassInfo(InArgs._Class);

	bShowFullClassTree = false;

	LastPeriodicValidityCheckTime = 0;
	PeriodicValidityCheckFrequency = 4;
	bLastInputValidityCheckSuccessful = true;
	bPreventPeriodicValidityChecksUntilNextChange = false;

	SetupParentClassItems();
	UpdateInputValidity();

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bIsActorsOnly = false;
	Options.bIsPlaceableOnly = false;
	Options.bIsBlueprintBaseOnly = false;
	Options.bShowUnloadedBlueprints = false;
	Options.bShowNoneOption = false;
	Options.bShowObjectRootClass = true;

	// Prevent creating native classes based on blueprint classes
	Options.ClassFilter = MakeShareable(new FNativeClassParentFilter(&SelectedModuleInfo));

	ClassViewer = StaticCastSharedRef<SClassViewer>(FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SNewClassDialog::OnAdvancedClassSelected)));

	TSharedRef<SWidget> DocWidget = IDocumentation::Get()->CreateAnchor(TAttribute<FString>(this, &SNewClassDialog::GetSelectedParentDocLink));
	DocWidget->SetVisibility(TAttribute<EVisibility>(this, &SNewClassDialog::GetDocLinkVisibility));

	const float EditableTextHeight = 26.0f;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(26, 8))
		.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
		[
			SNew(SVerticalBox)
			.AddMetaData<FTutorialMetaData>(TEXT("AddCodeMajorAnchor"))

			+SVerticalBox::Slot()
			[
				SAssignNew( MainWizard, SWizard)
				.ShowPageList(false)
				.CanFinish(this, &SNewClassDialog::CanFinish)
				.FinishButtonText( LOCTEXT("FinishButtonText", "Create Class") )
				.FinishButtonToolTip ( LOCTEXT("FinishButtonToolTip", "Creates the code files to add your new class.") )
				.OnCanceled(this, &SNewClassDialog::CancelClicked)
				.OnFinished(this, &SNewClassDialog::FinishClicked)
				.InitialPageIndex(ParentClassInfo.IsSet() ? 1 : 0)
				
				// Choose parent class
				+SWizard::Page()
				[
					SNew(SVerticalBox)

					// Title
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 20, 0, 0)
					[
						SNew(STextBlock)
						.TextStyle( FEditorStyle::Get(), "NewClassDialog.PageTitle" )
						.Text( LOCTEXT( "ParentClassTitle", "Choose Parent Class" ) )
					]

					// Title spacer
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 2, 0, 8)
					[
						SNew(SSeparator)
					]

					// Page description and view options
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 10)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text( FText::Format( LOCTEXT("ChooseParentClassDescription", "You are about to add a C++ source code file. To compile these files you must have {0} installed."), FSourceCodeNavigation::GetSuggestedSourceCodeIDE() ) )
						]

						// Full tree checkbox
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4, 0, 0, 0)
						[
							SNew(SCheckBox)
							.IsChecked( this, &SNewClassDialog::IsFullClassTreeChecked )
							.OnCheckStateChanged( this, &SNewClassDialog::OnFullClassTreeChanged )
							[
								SNew(STextBlock)
								.Text( LOCTEXT( "FullClassTree", "Show All Classes" ) )
							]
						]
					]

					// Add Code list
					+SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(0, 10)
					[
						SNew(SBorder)
						.AddMetaData<FTutorialMetaData>(TEXT("AddCodeOptions"))
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							[
								// Basic view
								SAssignNew(ParentClassListView, SListView< TSharedPtr<FParentClassItem> >)
								.ListItemsSource(&ParentClassItemsSource)
								.SelectionMode(ESelectionMode::Single)
								.ClearSelectionOnClick(false)
								.OnGenerateRow(this, &SNewClassDialog::MakeParentClassListViewWidget)
								.OnMouseButtonDoubleClick( this, &SNewClassDialog::OnParentClassItemDoubleClicked )
								.OnSelectionChanged(this, &SNewClassDialog::OnClassSelected)
								.Visibility(this, &SNewClassDialog::GetBasicParentClassVisibility)
							]

							+SVerticalBox::Slot()
							[
								// Advanced view
								SNew(SBox)
								.Visibility(this, &SNewClassDialog::GetAdvancedParentClassVisibility)
								[
									ClassViewer.ToSharedRef()
								]
							]
						]
					]

					// Class selection
					+SVerticalBox::Slot()
					.Padding(30, 2)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						
						// Class label
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 12, 0)
							[
								SNew(STextBlock)
								.TextStyle( FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel" )
								.Text( LOCTEXT( "ParentClassLabel", "Selected Class" ) )
							]

							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 12, 0)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel")
								.Text(LOCTEXT("ParentClassSourceLabel", "Selected Class Source"))
							]
						]

						// Class selection preview
						+SHorizontalBox::Slot()
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 12, 0)
							[
								SNew(SHorizontalBox)

								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text( this, &SNewClassDialog::GetSelectedParentClassName )
								]

								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									DocWidget
								]
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Bottom)
							.HAlign(HAlign_Left)
							.Padding(0.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SHyperlink)
								.Style(FCoreStyle::Get(), "Hyperlink")
								.TextStyle(FEditorStyle::Get(), "DetailsView.GoToCodeHyperlinkStyle")
								.OnNavigate(this, &SNewClassDialog::OnEditCodeClicked)
								.Text(this, &SNewClassDialog::GetSelectedParentClassFilename)
								.ToolTipText(LOCTEXT("GoToCode_ToolTip", "Click to open this source file in a text editor"))
								.Visibility(this, &SNewClassDialog::GetSourceHyperlinkVisibility)
							]
						]
					]
				]

				// Name class
				+SWizard::Page()
				.OnEnter(this, &SNewClassDialog::OnNamePageEntered)
				[
					SNew(SVerticalBox)

					// Title
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 20, 0, 0)
					[
						SNew(STextBlock)
						.TextStyle( FEditorStyle::Get(), "NewClassDialog.PageTitle" )
						.Text( this, &SNewClassDialog::GetNameClassTitle )
					]

					// Title spacer
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 2, 0, 8)
					[
						SNew(SSeparator)
					]

					+SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(0, 10)
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(STextBlock)
							.Text( LOCTEXT("ClassNameDescription", "Enter a name for your new class. Class names may only contain alphanumeric characters, and may not contain a space.") )
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 2)
						[
							SNew(STextBlock)
							.Text( LOCTEXT("ClassNameDetails", "When you click the \"Create\" button below, a header (.h) file and a source (.cpp) file will be made using this name.") )
						]

						// Name Error label
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 5)
						[
							// Constant height, whether the label is visible or not
							SNew(SBox).HeightOverride(20)
							[
								SNew(SBorder)
								.Visibility( this, &SNewClassDialog::GetNameErrorLabelVisibility )
								.BorderImage( FEditorStyle::GetBrush("NewClassDialog.ErrorLabelBorder") )
								.Content()
								[
									SNew(STextBlock)
									.Text( this, &SNewClassDialog::GetNameErrorLabelText )
									.TextStyle( FEditorStyle::Get(), "NewClassDialog.ErrorLabelFont" )
								]
							]
						]

						// Properties
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryTop"))
							.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
							.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
							[
								SNew(SVerticalBox)

								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0)
								[
									SNew(SGridPanel)
									.FillColumn(1, 1.0f)

									// Name label
									+SGridPanel::Slot(0, 0)
									.VAlign(VAlign_Center)
									.Padding(0, 0, 12, 0)
									[
										SNew(STextBlock)
										.TextStyle( FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel" )
										.Text( LOCTEXT( "NameLabel", "Name" ) )
									]

									// Name edit box
									+SGridPanel::Slot(1, 0)
									.Padding(0.0f, 3.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.HeightOverride(EditableTextHeight)
										.AddMetaData<FTutorialMetaData>(TEXT("ClassName"))
										[
											SNew(SHorizontalBox)

											+SHorizontalBox::Slot()
											.FillWidth(1.0f)
											[
												SAssignNew( ClassNameEditBox, SEditableTextBox)
												.Text( this, &SNewClassDialog::OnGetClassNameText )
												.OnTextChanged( this, &SNewClassDialog::OnClassNameTextChanged )
											]

											+SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(6.0f, 0.0f, 0.0f, 0.0f)
											[
												SAssignNew(AvailableModulesCombo, SComboBox<TSharedPtr<FModuleContextInfo>>)
												.ToolTipText( LOCTEXT("ModuleComboToolTip", "Choose the target module for your new class") )
												.OptionsSource( &AvailableModules )
												.InitiallySelectedItem( SelectedModuleInfo )
												.OnSelectionChanged( this, &SNewClassDialog::SelectedModuleComboBoxSelectionChanged )
												.OnGenerateWidget( this, &SNewClassDialog::MakeWidgetForSelectedModuleCombo )
												[
													SNew(STextBlock)
													.Text( this, &SNewClassDialog::GetSelectedModuleComboText )
												]
											]

											+SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(6.0f, 0.0f, 0.0f, 0.0f)
											[
												SNew(SHorizontalBox)

												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew(SCheckBox)
													.Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
													.IsEnabled(this, &SNewClassDialog::CanChangeClassLocation)
													.IsChecked(this, &SNewClassDialog::IsClassLocationActive, GameProjectUtils::EClassLocation::Public)
													.OnCheckStateChanged(this, &SNewClassDialog::OnClassLocationChanged, GameProjectUtils::EClassLocation::Public)
													.ToolTipText(this, &SNewClassDialog::GetClassLocationTooltip, GameProjectUtils::EClassLocation::Public)
													[
														SNew(SBox)
														.VAlign(VAlign_Center)
														.HAlign(HAlign_Left)
														.Padding(FMargin(4.0f, 0.0f, 3.0f, 0.0f))
														[
															SNew(STextBlock)
															.Text(LOCTEXT("Public", "Public"))
															.ColorAndOpacity(this, &SNewClassDialog::GetClassLocationTextColor, GameProjectUtils::EClassLocation::Public)
														]
													]
												]

												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew(SCheckBox)
													.Style(FEditorStyle::Get(), "Property.ToggleButton.End")
													.IsEnabled(this, &SNewClassDialog::CanChangeClassLocation)
													.IsChecked(this, &SNewClassDialog::IsClassLocationActive, GameProjectUtils::EClassLocation::Private)
													.OnCheckStateChanged(this, &SNewClassDialog::OnClassLocationChanged, GameProjectUtils::EClassLocation::Private)
													.ToolTipText(this, &SNewClassDialog::GetClassLocationTooltip, GameProjectUtils::EClassLocation::Private)
													[
														SNew(SBox)
														.VAlign(VAlign_Center)
														.HAlign(HAlign_Right)
														.Padding(FMargin(3.0f, 0.0f, 4.0f, 0.0f))
														[
															SNew(STextBlock)
															.Text(LOCTEXT("Private", "Private"))
															.ColorAndOpacity(this, &SNewClassDialog::GetClassLocationTextColor, GameProjectUtils::EClassLocation::Private)
														]
													]
												]
											]
										]
									]

									// Path label
									+SGridPanel::Slot(0, 1)
									.VAlign(VAlign_Center)
									.Padding(0, 0, 12, 0)
									[
										SNew(STextBlock)
										.TextStyle( FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel" )
										.Text( LOCTEXT( "PathLabel", "Path" ).ToString() )
									]

									// Path edit box
									+SGridPanel::Slot(1, 1)
									.Padding(0.0f, 3.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.HeightOverride(EditableTextHeight)
										.AddMetaData<FTutorialMetaData>(TEXT("Path"))
										[
											SNew(SHorizontalBox)

											+SHorizontalBox::Slot()
											.FillWidth(1.0f)
											[
												SNew(SEditableTextBox)
												.Text(this, &SNewClassDialog::OnGetClassPathText)
												.OnTextChanged(this, &SNewClassDialog::OnClassPathTextChanged)
											]

											+SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(6.0f, 1.0f, 0.0f, 0.0f)
											[
												SNew(SButton)
												.VAlign(VAlign_Center)
												.OnClicked(this, &SNewClassDialog::HandleChooseFolderButtonClicked)
												.Text( LOCTEXT( "BrowseButtonText", "Choose Folder" ) )
											]
										]
									]

									// Header output label
									+SGridPanel::Slot(0, 2)
									.VAlign(VAlign_Center)
									.Padding(0, 0, 12, 0)
									[
										SNew(STextBlock)
										.TextStyle( FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel" )
										.Text( LOCTEXT( "HeaderFileLabel", "Header File" ).ToString() )
									]

									// Header output text
									+SGridPanel::Slot(1, 2)
									.Padding(0.0f, 3.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.VAlign(VAlign_Center)
										.HeightOverride(EditableTextHeight)
										[
											SNew(STextBlock)
											.Text(this, &SNewClassDialog::OnGetClassHeaderFileText)
										]
									]

									// Source output label
									+SGridPanel::Slot(0, 3)
									.VAlign(VAlign_Center)
									.Padding(0, 0, 12, 0)
									[
										SNew(STextBlock)
										.TextStyle( FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel" )
										.Text( LOCTEXT( "SourceFileLabel", "Source File" ).ToString() )
									]

									// Source output text
									+SGridPanel::Slot(1, 3)
									.Padding(0.0f, 3.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.VAlign(VAlign_Center)
										.HeightOverride(EditableTextHeight)
										[
											SNew(STextBlock)
											.Text(this, &SNewClassDialog::OnGetClassSourceFileText)
										]
									]
								]
							]
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f)
						[
							SNew(SBorder)
							.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
							.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
							.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
						]
					]
				]
			]

			// IDE download information
			+SVerticalBox::Slot()
			.Padding(0, 5)
			.AutoHeight()
			[
				SNew(SBorder)
				.Visibility( this, &SNewClassDialog::GetGlobalErrorLabelVisibility )
				.BorderImage( FEditorStyle::GetBrush("NewClassDialog.ErrorLabelBorder") )
				.Content()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( this, &SNewClassDialog::GetGlobalErrorLabelText )
						.TextStyle( FEditorStyle::Get(), "NewClassDialog.ErrorLabelFont" )
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SHyperlink)
						.Text( FText::Format( LOCTEXT("IDEDownloadLinkText", "Download {0}"), FSourceCodeNavigation::GetSuggestedSourceCodeIDE() ) )
						.OnNavigate( this, &SNewClassDialog::OnDownloadIDEClicked, FSourceCodeNavigation::GetSuggestedSourceCodeIDEDownloadURL() )
						.Visibility( this, &SNewClassDialog::GetGlobalErrorLabelIDELinkVisibility )
					]
				]
			]
		]
	];

	// Select the first item
	if ( InArgs._Class == NULL && ParentClassItemsSource.Num() > 0 )
	{
		ParentClassListView->SetSelection(ParentClassItemsSource[0], ESelectInfo::Direct);
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SNewClassDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Every few seconds, the class name/path is checked for validity in case the disk contents changed and the location is now valid or invalid.
	// After class creation, periodic checks are disabled to prevent a brief message indicating that the class you created already exists.
	// This feature is re-enabled if the user did not restart and began editing parameters again.
	if ( !bPreventPeriodicValidityChecksUntilNextChange && (InCurrentTime > LastPeriodicValidityCheckTime + PeriodicValidityCheckFrequency) )
	{
		UpdateInputValidity();
	}
}

TSharedRef<ITableRow> SNewClassDialog::MakeParentClassListViewWidget(TSharedPtr<FParentClassItem> ParentClassItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(ParentClassItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FParentClassItem>>, OwnerTable );
	}

	if ( !ParentClassItem->ParentClassInfo.IsSet() )
	{
		return SNew( STableRow<TSharedPtr<FParentClassItem>>, OwnerTable );
	}

	const FString ClassName = ParentClassItem->ParentClassInfo.GetClassName();
	const FString ClassDescription = ParentClassItem->ParentClassInfo.GetClassDescription();
	const FSlateBrush* const ClassBrush = ParentClassItem->ParentClassInfo.GetClassIcon();
	const UClass* Class = ParentClassItem->ParentClassInfo.BaseClass;

	const int32 ItemHeight = 64;
	const int32 DescriptionIndent = 32;
	return
		SNew( STableRow<TSharedPtr<FParentClassItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "NewClassDialog.ParentClassListView.TableRow")
		.ToolTip(IDocumentation::Get()->CreateToolTip(FText::FromString(ClassDescription), nullptr, FEditorClassUtils::GetDocumentationPage(Class), FEditorClassUtils::GetDocumentationExcerpt(Class)))
		[
			SNew(SBox).HeightOverride(ItemHeight)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.Padding(8)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image( ClassBrush )
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle( FEditorStyle::Get(), "NewClassDialog.ParentClassItemTitle" )
						.Text(ClassName)
					]
				]

				+SVerticalBox::Slot()
				.FillHeight(1.f)
				.Padding(DescriptionIndent, 0, 0, 0)
				[
					SNew(STextBlock)
					//.AutoWrapText(true)
					.Text(ClassDescription)
				]
			]
		];
}

FText SNewClassDialog::GetSelectedParentClassName() const
{
	return ParentClassInfo.IsSet() ? FText::FromString(ParentClassInfo.GetClassName()) : FText::GetEmpty();
}

FString GetClassHeaderPath(const UClass* Class)
{
	if (Class)
	{
		FString ClassHeaderPath;
		if (FSourceCodeNavigation::FindClassHeaderPath(Class, ClassHeaderPath) && IFileManager::Get().FileSize(*ClassHeaderPath) != INDEX_NONE)
		{
			return ClassHeaderPath;
		}
	}
	return FString();
}

EVisibility SNewClassDialog::GetSourceHyperlinkVisibility() const
{
	return (GetClassHeaderPath(ParentClassInfo.BaseClass).Len() > 0 ? EVisibility::Visible : EVisibility::Hidden);
}

FText SNewClassDialog::GetSelectedParentClassFilename() const
{
	const FString ClassHeaderPath = GetClassHeaderPath(ParentClassInfo.BaseClass);
	if (ClassHeaderPath.Len() > 0)
	{
		return FText::FromString(FPaths::GetCleanFilename(*ClassHeaderPath));
	}
	return FText::GetEmpty();
}

EVisibility SNewClassDialog::GetDocLinkVisibility() const
{
	return (ParentClassInfo.BaseClass == nullptr || FEditorClassUtils::GetDocumentationLink(ParentClassInfo.BaseClass).IsEmpty() ? EVisibility::Hidden : EVisibility::Visible);
}

FString SNewClassDialog::GetSelectedParentDocLink() const
{
	return FEditorClassUtils::GetDocumentationLink(ParentClassInfo.BaseClass);
}

void SNewClassDialog::OnEditCodeClicked()
{
	const FString ClassHeaderPath = GetClassHeaderPath(ParentClassInfo.BaseClass);
	if (ClassHeaderPath.Len() > 0)
	{
		const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassHeaderPath);
		FSourceCodeNavigation::OpenSourceFile(AbsoluteHeaderPath);
	}
}


void SNewClassDialog::OnParentClassItemDoubleClicked( TSharedPtr<FParentClassItem> TemplateItem )
{
	// Advance to the name page
	const int32 NamePageIdx = 1;
	if ( MainWizard->CanShowPage(NamePageIdx) )
	{
		MainWizard->ShowPage(NamePageIdx);
	}
}

void SNewClassDialog::OnClassSelected(TSharedPtr<FParentClassItem> Item, ESelectInfo::Type SelectInfo)
{
	if ( Item.IsValid() )
	{
		ClassViewer->ClearSelection();
		ParentClassInfo = Item->ParentClassInfo;
	}
	else
	{
		ParentClassInfo = GameProjectUtils::FNewClassInfo();
	}
}

void SNewClassDialog::OnAdvancedClassSelected(UClass* Class)
{
	ParentClassListView->ClearSelection();
	ParentClassInfo = GameProjectUtils::FNewClassInfo(Class);
}

ECheckBoxState SNewClassDialog::IsFullClassTreeChecked() const
{
	return bShowFullClassTree ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewClassDialog::OnFullClassTreeChanged(ECheckBoxState NewCheckedState)
{
	bShowFullClassTree = (NewCheckedState == ECheckBoxState::Checked);
}

EVisibility SNewClassDialog::GetBasicParentClassVisibility() const
{
	return bShowFullClassTree ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SNewClassDialog::GetAdvancedParentClassVisibility() const
{
	return bShowFullClassTree ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNewClassDialog::GetNameErrorLabelVisibility() const
{
	return GetNameErrorLabelText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

FText SNewClassDialog::GetNameErrorLabelText() const
{
	if ( !bLastInputValidityCheckSuccessful )
	{
		return LastInputValidityErrorText;
	}

	return FText::GetEmpty();
}

EVisibility SNewClassDialog::GetGlobalErrorLabelVisibility() const
{
	return GetGlobalErrorLabelText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

EVisibility SNewClassDialog::GetGlobalErrorLabelIDELinkVisibility() const
{
	return FSourceCodeNavigation::IsCompilerAvailable() ? EVisibility::Collapsed : EVisibility::Visible;
}

FString SNewClassDialog::GetGlobalErrorLabelText() const
{
	if ( !FSourceCodeNavigation::IsCompilerAvailable() )
	{
		return FText::Format( LOCTEXT("NoCompilerFound", "No compiler was found. In order to use C++ code, you must first install {0}."), FSourceCodeNavigation::GetSuggestedSourceCodeIDE() ).ToString();
	}

	return TEXT("");
}

void SNewClassDialog::OnNamePageEntered()
{
	// Set the default class name based on the selected parent class, eg MyActor
	const FString ParentClassName = ParentClassInfo.GetClassNameCPP();
	const FString PotentialNewClassName = FString::Printf(TEXT("My%s"), ParentClassName.IsEmpty() ? TEXT("Class") : *ParentClassName);

	// Only set the default if the user hasn't changed the class name from the previous default
	if(LastAutoGeneratedClassName.IsEmpty() || NewClassName == LastAutoGeneratedClassName)
	{
		NewClassName = PotentialNewClassName;
		LastAutoGeneratedClassName = PotentialNewClassName;
	}

	UpdateInputValidity();

	// Steal keyboard focus to accelerate name entering
	FSlateApplication::Get().SetKeyboardFocus(ClassNameEditBox, EFocusCause::SetDirectly);
}

FText SNewClassDialog::GetNameClassTitle() const
{
	static const FString NoneString = TEXT("None");

	const FText ParentClassName = GetSelectedParentClassName();
	if(!ParentClassName.IsEmpty() && ParentClassName.ToString() != NoneString)
	{
		return FText::Format( LOCTEXT( "NameClassTitle", "Name Your New {0}" ), ParentClassName );
	}

	return LOCTEXT( "NameClassGenericTitle", "Name Your New Class" );
}

FText SNewClassDialog::OnGetClassNameText() const
{
	return FText::FromString(NewClassName);
}

void SNewClassDialog::OnClassNameTextChanged(const FText& NewText)
{
	NewClassName = NewText.ToString();
	UpdateInputValidity();
}

FText SNewClassDialog::OnGetClassPathText() const
{
	return FText::FromString(NewClassPath);
}

void SNewClassDialog::OnClassPathTextChanged(const FText& NewText)
{
	NewClassPath = NewText.ToString();

	// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
	for(const auto& AvailableModule : AvailableModules)
	{
		if(NewClassPath.StartsWith(AvailableModule->ModuleSourcePath))
		{
			SelectedModuleInfo = AvailableModule;
			AvailableModulesCombo->SetSelectedItem(SelectedModuleInfo);
			break;
		}
	}

	UpdateInputValidity();
}

FText SNewClassDialog::OnGetClassHeaderFileText() const
{
	return FText::FromString(CalculatedClassHeaderName);
}

FText SNewClassDialog::OnGetClassSourceFileText() const
{
	return FText::FromString(CalculatedClassSourceName);
}

void SNewClassDialog::CancelClicked()
{
	CloseContainingWindow();
}

bool SNewClassDialog::CanFinish() const
{
	return bLastInputValidityCheckSuccessful && ParentClassInfo.IsSet() && FSourceCodeNavigation::IsCompilerAvailable();
}

void SNewClassDialog::FinishClicked()
{
	check(CanFinish());

	FString HeaderFilePath;
	FString CppFilePath;

	FText FailReason;
	const TSet<FString>& DisallowedHeaderNames = FSourceCodeNavigation::GetSourceFileDatabase().GetDisallowedHeaderNames();
	if (GameProjectUtils::AddCodeToProject(NewClassName, NewClassPath, *SelectedModuleInfo, ParentClassInfo, DisallowedHeaderNames, HeaderFilePath, CppFilePath, FailReason))
	{
		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		if ( HeaderFilePath.IsEmpty() || CppFilePath.IsEmpty() || !FSlateApplication::Get().SupportsSourceAccess() )
		{
			// Code successfully added, notify the user. We are either running on a platform that does not support source access or a file was not given so don't ask about editing the file
			const FText Message = FText::Format( LOCTEXT("AddCodeSuccess", "Successfully added class {0}."), FText::FromString(NewClassName) );
			FMessageDialog::Open(EAppMsgType::Ok, Message);
		}
		else
		{
			// Code successfully added, notify the user and ask about opening the IDE now
			const FText Message = FText::Format( LOCTEXT("AddCodeSuccessWithSync", "Successfully added class {0}. Would you like to edit the code now?"), FText::FromString(NewClassName) );
			if ( FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes )
			{
				TArray<FString> SourceFiles;
				SourceFiles.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*HeaderFilePath));
				SourceFiles.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CppFilePath));

				FSourceCodeNavigation::OpenSourceFiles(SourceFiles);
			}
		}

		// Successfully created the code and potentially opened the IDE. Close the dialog.
		CloseContainingWindow();
	}
	else
	{
		// @todo show fail reason in error label
		// Failed to add code
		const FText Message = FText::Format( LOCTEXT("AddCodeFailed", "Failed to add class {0}. {1}"), FText::FromString(NewClassName), FailReason );
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
}

void SNewClassDialog::OnDownloadIDEClicked(FString URL)
{
	FPlatformProcess::LaunchURL( *URL, NULL, NULL );
}

FReply SNewClassDialog::HandleChooseFolderButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( DesktopPlatform )
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowWindowHandle = (ParentWindow.IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString FolderName;
		const FString Title = LOCTEXT("NewClassBrowseTitle", "Choose a source location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowWindowHandle,
			Title,
			NewClassPath,
			FolderName
			);

		if ( bFolderSelected )
		{
			if ( !FolderName.EndsWith(TEXT("/")) )
			{
				FolderName += TEXT("/");
			}

			NewClassPath = FolderName;

			// If the user has selected a path which matches the root of a known module, then update our selected module to be that module
			for(const auto& AvailableModule : AvailableModules)
			{
				if(NewClassPath.StartsWith(AvailableModule->ModuleSourcePath))
				{
					SelectedModuleInfo = AvailableModule;
					AvailableModulesCombo->SetSelectedItem(SelectedModuleInfo);
					break;
				}
			}

			UpdateInputValidity();
		}
	}

	return FReply::Handled();
}

FText SNewClassDialog::GetSelectedModuleComboText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(SelectedModuleInfo->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(SelectedModuleInfo->ModuleType)));
	return FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args);
}

void SNewClassDialog::SelectedModuleComboBoxSelectionChanged(TSharedPtr<FModuleContextInfo> Value, ESelectInfo::Type SelectInfo)
{
	const FString& OldModulePath = SelectedModuleInfo->ModuleSourcePath;
	const FString& NewModulePath = Value->ModuleSourcePath;

	SelectedModuleInfo = Value;

	// Update the class path to be rooted to the new module location
	const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(NewClassPath) / ""; // Ensure trailing /
	if(AbsoluteClassPath.StartsWith(OldModulePath))
	{
		NewClassPath = AbsoluteClassPath.Replace(*OldModulePath, *NewModulePath);
	}

	UpdateInputValidity();
}

TSharedRef<SWidget> SNewClassDialog::MakeWidgetForSelectedModuleCombo(TSharedPtr<FModuleContextInfo> Value)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ModuleName"), FText::FromString(Value->ModuleName));
	Args.Add(TEXT("ModuleType"), FText::FromString(EHostType::ToString(Value->ModuleType)));
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ModuleComboEntry", "{ModuleName} ({ModuleType})"), Args));
}

FSlateColor SNewClassDialog::GetClassLocationTextColor(GameProjectUtils::EClassLocation InLocation) const
{
	return (ClassLocation == InLocation) ? FSlateColor(FLinearColor(0, 0, 0)) : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
}

FText SNewClassDialog::GetClassLocationTooltip(GameProjectUtils::EClassLocation InLocation) const
{
	if(CanChangeClassLocation())
	{
		switch(InLocation)
		{
		case GameProjectUtils::EClassLocation::Public:
			return LOCTEXT("ClassLocation_Public", "A public class can be included and used inside other modules in addition to the module it resides in");

		case GameProjectUtils::EClassLocation::Private:
			return LOCTEXT("ClassLocation_Private", "A private class can only be included and used within the module it resides in");

		default:
			break;
		}
	}

	return LOCTEXT("ClassLocation_UserDefined", "Your project is either not using a Public and Private source layout, or you're explicitly creating your class outside of the Public or Private folder");
}

ECheckBoxState SNewClassDialog::IsClassLocationActive(GameProjectUtils::EClassLocation InLocation) const
{
	return (ClassLocation == InLocation) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewClassDialog::OnClassLocationChanged(ECheckBoxState InCheckedState, GameProjectUtils::EClassLocation InLocation)
{
	if(InCheckedState == ECheckBoxState::Checked)
	{
		const FString AbsoluteClassPath = FPaths::ConvertRelativePathToFull(NewClassPath) / ""; // Ensure trailing /

		GameProjectUtils::EClassLocation TmpClassLocation = GameProjectUtils::EClassLocation::UserDefined;
		GameProjectUtils::GetClassLocation(AbsoluteClassPath, *SelectedModuleInfo, TmpClassLocation);

		const FString RootPath = SelectedModuleInfo->ModuleSourcePath;
		const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
		const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /

		// Update the class path to be rooted to the Public or Private folder based on InVisibility
		switch(InLocation)
		{
		case GameProjectUtils::EClassLocation::Public:
			if(AbsoluteClassPath.StartsWith(PrivatePath))
			{
				NewClassPath = AbsoluteClassPath.Replace(*PrivatePath, *PublicPath);
			}
			else if(AbsoluteClassPath.StartsWith(RootPath))
			{
				NewClassPath = AbsoluteClassPath.Replace(*RootPath, *PublicPath);
			}
			break;

		case GameProjectUtils::EClassLocation::Private:
			if(AbsoluteClassPath.StartsWith(PublicPath))
			{
				NewClassPath = AbsoluteClassPath.Replace(*PublicPath, *PrivatePath);
			}
			else if(AbsoluteClassPath.StartsWith(RootPath))
			{
				NewClassPath = AbsoluteClassPath.Replace(*RootPath, *PrivatePath);
			}
			break;

		default:
			break;
		}

		// Will update ClassVisibility correctly
		UpdateInputValidity();
	}
}

bool SNewClassDialog::CanChangeClassLocation() const
{
	return ClassLocation != GameProjectUtils::EClassLocation::UserDefined;
}

void SNewClassDialog::UpdateInputValidity()
{
	bLastInputValidityCheckSuccessful = true;

	// Validate the path first since this has the side effect of updating the UI
	bLastInputValidityCheckSuccessful = GameProjectUtils::CalculateSourcePaths(NewClassPath, *SelectedModuleInfo, CalculatedClassHeaderName, CalculatedClassSourceName, &LastInputValidityErrorText);
	CalculatedClassHeaderName /= ParentClassInfo.GetHeaderFilename(NewClassName);
	CalculatedClassSourceName /= ParentClassInfo.GetSourceFilename(NewClassName);

	// If the source paths check as succeeded, check to see if we're using a Public/Private class
	if(bLastInputValidityCheckSuccessful)
	{
		GameProjectUtils::GetClassLocation(NewClassPath, *SelectedModuleInfo, ClassLocation);

		// We only care about the Public and Private folders
		if(ClassLocation != GameProjectUtils::EClassLocation::Public && ClassLocation != GameProjectUtils::EClassLocation::Private)
		{
			ClassLocation = GameProjectUtils::EClassLocation::UserDefined;
		}
	}
	else
	{
		ClassLocation = GameProjectUtils::EClassLocation::UserDefined;
	}

	// Validate the class name only if the path is valid
	if ( bLastInputValidityCheckSuccessful )
	{
		const TSet<FString>& DisallowedHeaderNames = FSourceCodeNavigation::GetSourceFileDatabase().GetDisallowedHeaderNames();
		bLastInputValidityCheckSuccessful = GameProjectUtils::IsValidClassNameForCreation(NewClassName, *SelectedModuleInfo, DisallowedHeaderNames, LastInputValidityErrorText);
	}

	LastPeriodicValidityCheckTime = FSlateApplication::Get().GetCurrentTime();

	// Since this function was invoked, periodic validity checks should be re-enabled if they were disabled.
	bPreventPeriodicValidityChecksUntilNextChange = false;
}

const GameProjectUtils::FNewClassInfo& SNewClassDialog::GetSelectedParentClassInfo() const
{
	return ParentClassInfo;
}

void SNewClassDialog::SetupParentClassItems()
{
	TArray<GameProjectUtils::FNewClassInfo> FeaturedClasses;
	
	// Add an empty class
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(GameProjectUtils::FNewClassInfo::EClassType::EmptyCpp));

	// @todo make this ini configurable
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(ACharacter::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APawn::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AActor::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerCameraManager::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerController::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AGameMode::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AWorldSettings::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AHUD::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerState::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AGameState::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(UBlueprintFunctionLibrary::StaticClass()));

	// Add the extra non-UObject classes
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(GameProjectUtils::FNewClassInfo::EClassType::SlateWidget));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(GameProjectUtils::FNewClassInfo::EClassType::SlateWidgetStyle));

	for ( auto ClassIt = FeaturedClasses.CreateConstIterator(); ClassIt; ++ClassIt )
	{
		ParentClassItemsSource.Add( MakeShareable( new FParentClassItem(*ClassIt) ) );
	}
}

void SNewClassDialog::CloseContainingWindow()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow( AsShared(), WidgetPath);

	if ( ContainingWindow.IsValid() )
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
