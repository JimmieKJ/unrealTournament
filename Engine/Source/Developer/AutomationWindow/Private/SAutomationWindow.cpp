// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationWindowPrivatePCH.h"

#include "AutomationController.h"
#include "AutomationPresetManager.h"
#include "SAutomationWindow.h"
#include "SSearchBox.h"
#include "SNotificationList.h"
#include "SThrobber.h"
#include "SHyperlink.h"
#include "Internationalization/Regex.h"


#define LOCTEXT_NAMESPACE "AutomationTest"


//////////////////////////////////////////////////////////////////////////
// FAutomationWindowCommands

class FAutomationWindowCommands : public TCommands<FAutomationWindowCommands>
{
public:
	FAutomationWindowCommands()
		: TCommands<FAutomationWindowCommands>(
		TEXT("AutomationWindow"),
		NSLOCTEXT("Contexts", "AutomationWindow", "Automation Window"),
		NAME_None, FEditorStyle::GetStyleSetName()
		)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND( RefreshTests, "Refresh Tests", "Refresh Tests", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( FindWorkers, "Find Workers", "Find Workers", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( ErrorFilter, "Errors", "Toggle Error Filter", EUserInterfaceActionType::ToggleButton, FInputChord() );
		UI_COMMAND( WarningFilter, "Warnings", "Toggle Warning Filter", EUserInterfaceActionType::ToggleButton, FInputChord() );
		UI_COMMAND( DeveloperDirectoryContent, "Dev Content", "Developer Directory Content Filter (when enabled, developer directories are also included)", EUserInterfaceActionType::ToggleButton, FInputChord() );
		
#if WITH_EDITOR
		// Added button for running the currently open level test.
		UI_COMMAND(RunLevelTest, "Run Level Test", "Run Level Test", EUserInterfaceActionType::Button, FInputGesture());
#endif
	}
public:
	TSharedPtr<FUICommandInfo> RefreshTests;
	TSharedPtr<FUICommandInfo> FindWorkers;
	TSharedPtr<FUICommandInfo> ErrorFilter;
	TSharedPtr<FUICommandInfo> WarningFilter;
	TSharedPtr<FUICommandInfo> DeveloperDirectoryContent;

#if WITH_EDITOR
	TSharedPtr<FUICommandInfo> RunLevelTest;
#endif
};

//////////////////////////////////////////////////////////////////////////
// SAutomationWindow

SAutomationWindow::SAutomationWindow() 
	: ColumnWidth(50.0f)
{

}

SAutomationWindow::~SAutomationWindow()
{
	// @todo PeterMcW: is there an actual delegate missing here?
	//give the controller a way to indicate it requires a UI update
	//AutomationController->SetRefreshTestCallback(FOnAutomationControllerTestsRefreshed());

	// Remove ourselves from the session manager
	if( SessionManager.IsValid( ) )
	{
		SessionManager->OnCanSelectSession().RemoveAll(this);
		SessionManager->OnSelectedSessionChanged().RemoveAll(this);
		SessionManager->OnSessionInstanceUpdated().RemoveAll(this);
	}

	if (AutomationController.IsValid())
	{
		AutomationController->RemoveCallbacks();

		AutomationController->OnControllerReset().RemoveAll(this);
		AutomationController->OnTestsRefreshed().RemoveAll(this);
		AutomationController->OnTestsAvailable().RemoveAll(this);
		AutomationController->OnTestsComplete().RemoveAll(this);
	}

#if WITH_EDITOR
	if ( FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")) )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFileLoadProgressUpdated().RemoveAll(this);
	}
#endif

	TSharedPtr<IAutomationReport> PreviousSelectionLock = PreviousSelection.Pin();
	if ( PreviousSelectionLock.IsValid() )
	{
		PreviousSelectionLock->OnSetResults.Unbind();
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAutomationWindow::Construct( const FArguments& InArgs, const IAutomationControllerManagerRef& InAutomationController, const ISessionManagerRef& InSessionManager )
{
	FAutomationWindowCommands::Register();
	CreateCommands();

#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFileLoadProgressUpdated().AddSP(this, &SAutomationWindow::OnAssetRegistryFileLoadProgress);
#endif

	TestPresetManager = MakeShareable(new FAutomationTestPresetManager());
	TestPresetManager->LoadPresets();
	bAddingTestPreset = false;

	bHasChildTestSelected = false;

	SessionManager = InSessionManager;
	AutomationController = InAutomationController;

	AutomationController->OnControllerReset().AddSP(this, &SAutomationWindow::OnRefreshTestCallback);
	AutomationController->OnTestsRefreshed().AddSP(this, &SAutomationWindow::OnRefreshTestCallback);
	AutomationController->OnTestsAvailable().AddSP(this, &SAutomationWindow::OnTestAvailableCallback);
	AutomationController->OnTestsComplete().AddSP(this, &SAutomationWindow::OnTestsCompleteCallback);

	AutomationControllerState = AutomationController->GetTestState();
	
	//cache off reference to filtered reports
	TArray <TSharedPtr <IAutomationReport> >& TestReports = AutomationController->GetReports();

	// Create the search filter and set criteria
	AutomationTextFilter = MakeShareable( new AutomationReportTextFilter( AutomationReportTextFilter::FItemToStringArray::CreateSP( this, &SAutomationWindow::PopulateReportSearchStrings ) ) );
	AutomationGeneralFilter = MakeShareable( new FAutomationFilter() ); 
	AutomationFilters = MakeShareable( new AutomationFilterCollection() );
	AutomationFilters->Add( AutomationTextFilter );
	AutomationFilters->Add( AutomationGeneralFilter );

	bIsRequestingTests = false;

	// Test history tracking
	bIsTrackingHistory = AutomationController->IsTrackingHistory();
	NumHistoryElementsToTrack = AutomationController->GetNumberHistoryItemsTracking();
	
	//make the widget for platforms
	PlatformsHBox = SNew (SHorizontalBox);

	TestTable = SNew(SAutomationTestTreeView< TSharedPtr< IAutomationReport > >)
		.SelectionMode(ESelectionMode::Multi)
		.TreeItemsSource( &TestReports )
		// Generates the actual widget for a tree item
		.OnGenerateRow( this, &SAutomationWindow::OnGenerateWidgetForTest )
		// Gets children
		.OnGetChildren(this, &SAutomationWindow::OnGetChildren)
		// on recursive expansion (shift + click)
		.OnSetExpansionRecursive(this, &SAutomationWindow::OnTestExpansionRecursive)
		//on selection
		.OnSelectionChanged(this, &SAutomationWindow::OnTestSelectionChanged)
		// Allow for some spacing between items with a larger item height.
		.ItemHeight(20.0f)
#if WITH_EDITOR
		// If in editor - add a context menu for opening assets when in editor
		.OnContextMenuOpening(this, &SAutomationWindow::HandleAutomationListContextMenuOpening)
#endif
		.HeaderRow
		(
		SAssignNew(TestTableHeaderRow,SHeaderRow)
		+ SHeaderRow::Column( AutomationTestWindowConstants::Title )
		.FillWidth(0.80f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			[
				//global enable/disable check box
				SAssignNew(HeaderCheckbox, SCheckBox)
				.OnCheckStateChanged( this, &SAutomationWindow::HeaderCheckboxStateChange)
				.ToolTipText( LOCTEXT( "Enable Disable Test", "Enable / Disable  Test" ) )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("TestName_Header", "Test Name") )
			]
		]

		+ SHeaderRow::Column( AutomationTestWindowConstants::SmokeTest )
		.FixedWidth( 50.0f )
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		[
			//icon for the smoke test column
			SNew(SImage)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.4f))
			.ToolTipText( LOCTEXT( "Smoke Test", "Smoke Test" ) )
			.Image(FEditorStyle::GetBrush("Automation.SmokeTest"))
		]

		+ SHeaderRow::Column( AutomationTestWindowConstants::RequiredDeviceCount )
		.FixedWidth(50.0f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush("Automation.ParticipantsWarning") )
			.ToolTipText( LOCTEXT( "RequiredDeviceCountWarningToolTip", "Number of devices required." ) )
		]

		+ SHeaderRow::Column(AutomationTestWindowConstants::Timing)
		.FixedWidth(100.0f)
		.DefaultLabel(LOCTEXT("TestDurationRange", "Duration"))

		+ SHeaderRow::Column( AutomationTestWindowConstants::Status )
		.FillWidth(0.20f)
		[
			//platform header placeholder
			PlatformsHBox.ToSharedRef()
		]

		);

	if(bIsTrackingHistory)
	{
		TestTableHeaderRow->AddColumn(
			SHeaderRow::Column(AutomationTestWindowConstants::History)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			.FixedWidth(100.0f)
			.DefaultLabel(LOCTEXT("TestHistory", "Test History"))
		);
	}

	TSharedRef<SNotificationList> NotificationList = SNew(SNotificationList) .Visibility( EVisibility::HitTestInvisible );

	//build the actual guts of the window
	this->ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				SNew( SSplitter )
				.IsEnabled(this, &SAutomationWindow::HandleMainContentIsEnabled)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.66f)
				[
					//automation test panel
					SAssignNew( MenuBar, SVerticalBox )

					//ACTIONS
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew( SHorizontalBox )

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SAutomationWindow::MakeAutomationWindowToolBar( AutomationWindowActions.ToSharedRef(), SharedThis(this) )
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SBorder)
							.BorderImage(this, &SAutomationWindow::GetTestBackgroundBorderImage)
							.Padding(3)
							[
								SNew(SBox)
								.Padding(4)
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 4.0f)
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(SBox)
											.MinDesiredWidth(130.0f)
											[
												SAssignNew(RequestedFilterComboBox, SComboBox< TSharedPtr<FString> >)
												.OptionsSource(&RequestedFilterComboList)
												.OnGenerateWidget(this, &SAutomationWindow::GenerateRequestedFilterComboItem)
												.OnSelectionChanged(this, &SAutomationWindow::HandleRequesteFilterChanged)
												.ContentPadding(FMargin(4.0, 1.0f))
												[
													SNew(STextBlock)
													.Text(this, &SAutomationWindow::GetRequestedFilterComboText)
												]
											]
										]

										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.VAlign(VAlign_Center)
										.Padding(2.0f, 0, 0, 0)
										[
											SAssignNew(AutomationSearchBox, SSearchBox)
											.ToolTipText(LOCTEXT("Search Tests", "Search Tests"))
											.OnTextChanged(this, &SAutomationWindow::OnFilterTextChanged)
											.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
										]
									]

									+ SVerticalBox::Slot()
									.FillHeight(1.0f)
									[
										//the actual table full of tests
										TestTable.ToSharedRef()
									]
								]
							]
						]

						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SThrobber)	
							.Visibility(this, &SAutomationWindow::GetTestsUpdatingThrobberVisibility)
						]
					]
				]

				+ SSplitter::Slot()
					.Value(0.33f)
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
							[
								SNew(SBox)
									.Visibility(this, &SAutomationWindow::GetTestGraphVisibility)
									[
										//Graphical Results Panel
										SNew( SVerticalBox )

										+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SNew(SHorizontalBox)

												+ SHorizontalBox::Slot()
													.HAlign(HAlign_Left)
													[
														SNew(STextBlock)
															.Text( LOCTEXT("AutomationTest_GraphicalResults", "Automation Test Graphical Results:"))
													]

												+ SHorizontalBox::Slot()
													.HAlign(HAlign_Right)
													.AutoWidth()
													[
														SNew(STextBlock)
															.Text( LOCTEXT("AutomationTest_Display", "Display:"))
													]

												+ SHorizontalBox::Slot()
													.HAlign(HAlign_Right)
													.AutoWidth()
													[
														SNew(SCheckBox)
															.Style(FCoreStyle::Get(), "RadioButton")
															.IsChecked(this, &SAutomationWindow::HandleResultDisplayTypeIsChecked, EAutomationGrapicalDisplayType::DisplayName)
															.OnCheckStateChanged(this, &SAutomationWindow::HandleResultDisplayTypeStateChanged, EAutomationGrapicalDisplayType::DisplayName)
															[
																SNew(STextBlock)
																	.Text( LOCTEXT("AutomationTest_GraphicalResultsDisplayName", "Name"))
															]
													]

												+ SHorizontalBox::Slot()
													.HAlign(HAlign_Right)
													.AutoWidth()
													[
														SNew(SCheckBox)
															.Style(FCoreStyle::Get(), "RadioButton")
															.IsChecked(this, &SAutomationWindow::HandleResultDisplayTypeIsChecked, EAutomationGrapicalDisplayType::DisplayTime)
															.OnCheckStateChanged(this, &SAutomationWindow::HandleResultDisplayTypeStateChanged, EAutomationGrapicalDisplayType::DisplayTime)
															[
																SNew(STextBlock)
																	.Text( LOCTEXT("AutomationTest_GraphicalResultsDisplayTime", "Time"))
															]
													]
											]

										+ SVerticalBox::Slot()
											.FillHeight(1.0f)
											[
												SNew(SBorder)
												[
													SNew(SScrollBox)

													+ SScrollBox::Slot()
														[
															SAssignNew(GraphicalResultBox, SAutomationGraphicalResultBox, InAutomationController)
														]
												]
											]
									]
							]

						+ SOverlay::Slot()
						[
							SNew(SBox)
							.Visibility(this, &SAutomationWindow::GetTestLogVisibility)
							[
								//results panel
								SNew( SVerticalBox )
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew( STextBlock )
									.Text( LOCTEXT("AutomationTest_Results", "Automation Test Results:") )
								]

								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								.Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									//list of results for the selected test
									SNew(SBorder)
									.BorderImage(FEditorStyle::GetBrush("MessageLog.ListBorder"))
									[
										SAssignNew(LogListView, SListView<TSharedPtr<FAutomationOutputMessage> >)
										.ItemHeight(18)
										.ListItemsSource(&LogMessages)
										.SelectionMode(ESelectionMode::Multi)
										.OnGenerateRow(this, &SAutomationWindow::OnGenerateWidgetForLog)
										.OnSelectionChanged(this, &SAutomationWindow::HandleLogListSelectionChanged)
									]
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(SBorder)
									.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
									.Padding(FMargin(8.0f, 6.0f))
									[
										// Add the command bar
										SAssignNew(CommandBar, SAutomationWindowCommandBar, NotificationList)
										.OnCopyLogClicked(this, &SAutomationWindow::HandleCommandBarCopyLogClicked)
									]
								]
							]
						]
					]
			]

		+ SOverlay::Slot()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Padding( 15.0f )
			[
				NotificationList
			]

		+ SOverlay::Slot()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Padding( 15.0f )
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
					.Padding(8.0f)
					.Visibility(this, &SAutomationWindow::HandleSelectSessionOverlayVisibility)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SelectSessionOverlayText", "Please select at least one instance from the Session Browser"))
					]
			]
	];

	SessionManager->OnCanSelectSession().AddSP( this, &SAutomationWindow::HandleSessionManagerCanSelectSession );
	SessionManager->OnSelectedSessionChanged().AddSP( this, &SAutomationWindow::HandleSessionManagerSelectionChanged );
	SessionManager->OnSessionInstanceUpdated().AddSP( this, &SAutomationWindow::HandleSessionManagerInstanceChanged );

	FindWorkers();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void SAutomationWindow::HandleResultDisplayTypeStateChanged( ECheckBoxState NewRadioState, EAutomationGrapicalDisplayType::Type NewDisplayType)
{
	if (NewRadioState == ECheckBoxState::Checked)
	{
		GraphicalResultBox->SetDisplayType(NewDisplayType);
	}
}

ECheckBoxState SAutomationWindow::HandleResultDisplayTypeIsChecked( EAutomationGrapicalDisplayType::Type InDisplayType ) const
{
	return (GraphicalResultBox->GetDisplayType() == InDisplayType)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

const FSlateBrush* SAutomationWindow::GetTestBackgroundBorderImage() const
{
	switch(TestBackgroundType)
	{
	case EAutomationTestBackgroundStyle::Game:
		return FEditorStyle::GetBrush("AutomationWindow.GameGroupBorder");

	case EAutomationTestBackgroundStyle::Editor:
		return FEditorStyle::GetBrush("AutomationWindow.EditorGroupBorder");

	case EAutomationTestBackgroundStyle::Unknown:
	default:
		return FEditorStyle::GetBrush("ToolPanel.GroupBorder");
	}
}

void SAutomationWindow::CreateCommands()
{
	check(!AutomationWindowActions.IsValid());
	AutomationWindowActions = MakeShareable(new FUICommandList);

	const FAutomationWindowCommands& Commands = FAutomationWindowCommands::Get();
	FUICommandList& ActionList = *AutomationWindowActions;

	ActionList.MapAction( Commands.RefreshTests,
		FExecuteAction::CreateRaw( this, &SAutomationWindow::ListTests ),
		FCanExecuteAction::CreateRaw( this, &SAutomationWindow::IsAutomationControllerIdle )
		);

	ActionList.MapAction( Commands.FindWorkers,
		FExecuteAction::CreateRaw( this, &SAutomationWindow::FindWorkers ),
		FCanExecuteAction::CreateRaw( this, &SAutomationWindow::IsAutomationControllerIdle )
		);

	ActionList.MapAction( Commands.ErrorFilter,
		FExecuteAction::CreateRaw( this, &SAutomationWindow::OnToggleErrorFilter ),
		FCanExecuteAction::CreateRaw( this, &SAutomationWindow::IsAutomationControllerIdle ),
		FIsActionChecked::CreateRaw( this, &SAutomationWindow::IsErrorFilterOn )
		);

	ActionList.MapAction( Commands.WarningFilter,
		FExecuteAction::CreateRaw( this, &SAutomationWindow::OnToggleWarningFilter ),
		FCanExecuteAction::CreateRaw( this, &SAutomationWindow::IsAutomationControllerIdle ),
		FIsActionChecked::CreateRaw( this, &SAutomationWindow::IsWarningFilterOn )
		);

	ActionList.MapAction( Commands.DeveloperDirectoryContent,
		FExecuteAction::CreateRaw( this, &SAutomationWindow::OnToggleDeveloperDirectoryIncluded ),
		FCanExecuteAction::CreateRaw( this, &SAutomationWindow::IsAutomationControllerIdle ),
		FIsActionChecked::CreateRaw( this, &SAutomationWindow::IsDeveloperDirectoryIncluded )
		);

	// Added button for running the currently open level test.
#if WITH_EDITOR
	ActionList.MapAction(Commands.RunLevelTest,
		FExecuteAction::CreateRaw(this, &SAutomationWindow::OnRunLevelTest),
		FCanExecuteAction::CreateRaw(this, &SAutomationWindow::CanExecuteRunLevelTest)
		);
#endif // WITH_EDITOR
}

TSharedRef< SWidget > SAutomationWindow::MakeAutomationWindowToolBar( const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<class SAutomationWindow> InAutomationWindow )
{
	return InAutomationWindow->MakeAutomationWindowToolBar(InCommandList);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef< SWidget > SAutomationWindow::MakeAutomationWindowToolBar( const TSharedRef<FUICommandList>& InCommandList )
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> RunTests, TSharedRef<SWidget> PresetBox, TSharedRef<SWidget> HistoryBox, TWeakPtr<class SAutomationWindow> InAutomationWindow)
		{
			ToolbarBuilder.BeginSection("Automation");
			{
				ToolbarBuilder.AddWidget( RunTests );
				FUIAction DefaultAction;
				ToolbarBuilder.AddComboButton(
					DefaultAction,
					FOnGetContent::CreateStatic( &SAutomationWindow::GenerateTestsOptionsMenuContent, InAutomationWindow ),
					LOCTEXT( "TestOptions_Label", "Test Options" ),
					LOCTEXT( "TestOptionsToolTip", "Test Options" ),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationWindow.TestOptions"),
					true);

				// Added button for running the currently open level test.
#if WITH_EDITOR
				ToolbarBuilder.AddToolBarButton(
					FAutomationWindowCommands::Get().RunLevelTest,
					NAME_None,
					TAttribute<FText>(),
					LOCTEXT("RunLevelTest_ToolTip", "If the currently loaded editor level is a test map, click this to select the test and run it immediately."),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationWindow.RunTests"));
#endif

				ToolbarBuilder.AddToolBarButton( FAutomationWindowCommands::Get().RefreshTests );
				ToolbarBuilder.AddToolBarButton( FAutomationWindowCommands::Get().FindWorkers );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Filters");
			{
				ToolbarBuilder.AddToolBarButton( FAutomationWindowCommands::Get().ErrorFilter );
				ToolbarBuilder.AddToolBarButton( FAutomationWindowCommands::Get().WarningFilter );
				ToolbarBuilder.AddToolBarButton( FAutomationWindowCommands::Get().DeveloperDirectoryContent );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("GroupFlags");
			{
				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateStatic( &SAutomationWindow::GenerateGroupOptionsMenuContent, InAutomationWindow ),
					LOCTEXT( "GroupOptions_Label", "Device Groups" ),
					LOCTEXT( "GroupOptionsToolTip", "Device Group Options" ),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationWindow.GroupSettings"));
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Presets");
			{
				ToolbarBuilder.AddWidget( PresetBox );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("History");
			{
				ToolbarBuilder.AddWidget(HistoryBox);

				FUIAction DefaultAction;
				ToolbarBuilder.AddComboButton(
					DefaultAction,
					FOnGetContent::CreateStatic(&SAutomationWindow::GenerateTestHistoryMenuContent, InAutomationWindow),
					LOCTEXT("TestHistoryOptions_Label", "Test History Options"),
					LOCTEXT("TestHistoryOptionsToolTip", "Test History Options"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationWindow.TestHistoryOptions"),
					true);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedRef<SWidget> RunTests = 
		SNew( SButton )
		.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
		.ToolTipText( LOCTEXT( "StartStop Tests", "Start / Stop tests" ) )
		.OnClicked( this, &SAutomationWindow::RunTests )
		.IsEnabled( this, &SAutomationWindow::IsAutomationRunButtonEnabled )
		.ContentPadding(0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew( SVerticalBox )
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign( HAlign_Center )
				[
					SNew( SOverlay )
					+SOverlay::Slot()
					[
						SNew( SImage ) 
						.Image( this, &SAutomationWindow::GetRunAutomationIcon )
					]
					+SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					[
						SNew( STextBlock )
						.Text( this, &SAutomationWindow::OnGetNumEnabledTestsString )
						.ColorAndOpacity( FLinearColor::White )
						.ShadowOffset( FVector2D::UnitVector )
						.Font( FEditorStyle::GetFontStyle( FName( "ToggleButton.LabelFont" ) ) )
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign( HAlign_Center )
				[
					SNew( STextBlock )
					.Visibility( this, &SAutomationWindow::GetLargeToolBarVisibility )
					.Text( this, &SAutomationWindow::GetRunAutomationLabel )
					.Font( FEditorStyle::GetFontStyle( FName( "ToggleButton.LabelFont" ) ) )
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset( FVector2D::UnitVector )
				]
			]
		];

	const float HistoryWidth = 200.0f;
	TSharedRef<SWidget> History =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.MaxWidth(HistoryWidth)
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &SAutomationWindow::IsTrackingHistory)
			.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
			.OnCheckStateChanged(this, &SAutomationWindow::OnToggleTrackHistory)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.FillHeight(1.f)
				.Padding(2.0f)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("AutomationWindow.TrackHistory"))
				]
				+ SVerticalBox::Slot()
				.Padding(2.0f)
				.VAlign(VAlign_Bottom)
				.AutoHeight()
				[
					SNew(STextBlock)
					.ShadowOffset(FVector2D::UnitVector)
					.Text(LOCTEXT("AutomationHistoryLabel", "Track History"))
					.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
				]
			]
		];

	TSharedRef<SWidget> TestPresets = 
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.FillHeight(0.75f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			.VAlign(VAlign_Bottom)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("AutomationPresetLabel", "Preset:") )
				.IsEnabled( this, &SAutomationWindow::IsAutomationControllerIdle )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Bottom)
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				//Preset Combo / Text
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					.Visibility(this,&SAutomationWindow::HandlePresetComboVisibility)
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					[
						SAssignNew( PresetComboBox, SComboBox< TSharedPtr<FAutomationTestPreset> > )
						.OptionsSource( &TestPresetManager->GetAllPresets() )
						.OnGenerateWidget( this, &SAutomationWindow::GeneratePresetComboItem )
						.OnSelectionChanged( this, &SAutomationWindow::HandlePresetChanged )
						.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
						[
							SNew( STextBlock )
							.Text( this, &SAutomationWindow::GetPresetComboText )
						]
					]
				]
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					.Visibility(this,&SAutomationWindow::HandlePresetTextVisibility)
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					[
						SAssignNew(PresetTextBox, SEditableTextBox)
						.OnTextCommitted(this, &SAutomationWindow::HandlePresetTextCommited)
						.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
					]
				]
			]

			//New button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SButton )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.OnClicked( this, &SAutomationWindow::HandleNewPresetClicked )
				.ToolTipText( LOCTEXT("AutomationPresetNewButtonTooltip", "Create a new preset") )
				.IsEnabled(this, &SAutomationWindow::IsAddButtonEnabled)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("AutomationWindow.PresetNew"))
				]
			]

			//Save button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SButton )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.OnClicked( this, &SAutomationWindow::HandleSavePresetClicked )
				.ToolTipText( LOCTEXT("AutomationPresetSaveButtonTooltip", "Save the current test list") )
				.IsEnabled(this, &SAutomationWindow::IsSaveButtonEnabled)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("AutomationWindow.PresetSave"))
				]
			]

			//Remove button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// remove button
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.OnClicked( this, &SAutomationWindow::HandleRemovePresetClicked )
				.ToolTipText(LOCTEXT("AutomationPresetRemoveButtonTooltip", "Remove the selected preset"))
				.IsEnabled(this, &SAutomationWindow::IsRemoveButtonEnabled)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::Get().GetBrush("AutomationWindow.PresetRemove"))
				]
			]
		];

	RequestedFilterComboList.Empty();
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("All Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Smoke Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Engine Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Product Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Performance Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Stress Tests"))));
	RequestedFilterComboList.Add(MakeShareable(new FString(TEXT("Standard Tests"))));

	FToolBarBuilder ToolbarBuilder( InCommandList, FMultiBoxCustomization::None );
	TWeakPtr<SAutomationWindow> AutomationWindow = SharedThis(this);
	Local::FillToolbar(ToolbarBuilder, RunTests, TestPresets, History, AutomationWindow);

	// Create the tool bar!
	return
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew( SBorder )
			.Padding(0)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			[
				ToolbarBuilder.MakeWidget()
			]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility SAutomationWindow::HandlePresetComboVisibility( ) const
{
	return bAddingTestPreset ? EVisibility::Hidden : EVisibility::Visible;
}

EVisibility SAutomationWindow::HandlePresetTextVisibility( ) const
{
	return bAddingTestPreset ? EVisibility::Visible : EVisibility::Hidden;
}

bool SAutomationWindow::IsAddButtonEnabled() const
{
	return !bAddingTestPreset && IsAutomationControllerIdle();
}

bool SAutomationWindow::IsSaveButtonEnabled() const
{
	return (!bAddingTestPreset && SelectedPreset.IsValid() && IsAutomationControllerIdle());
}

bool SAutomationWindow::IsRemoveButtonEnabled() const
{
	return (!bAddingTestPreset && SelectedPreset.IsValid() && IsAutomationControllerIdle());
}

void SAutomationWindow::HandlePresetTextCommited( const FText& CommittedText, ETextCommit::Type CommitType )
{
	if( CommitType == ETextCommit::OnEnter )
	{
		bAddingTestPreset = false;
		if(CommittedText.IsEmpty())
		{
			return;
		}

		TArray<FString> EnabledTests;
		AutomationController->GetEnabledTestNames(EnabledTests);
		AutomationPresetPtr NewPreset = TestPresetManager->AddNewPreset(CommittedText,EnabledTests);
		PresetComboBox->SetSelectedItem(NewPreset);
		SelectedPreset = NewPreset;

		PresetTextBox->SetText(FText());
	}
	else if( CommitType == ETextCommit::OnCleared || CommitType == ETextCommit::OnUserMovedFocus )
	{
		if( bAddingTestPreset )
		{
			bAddingTestPreset = false;
			SelectedPreset = nullptr;
			PresetComboBox->ClearSelection();
			PresetTextBox->SetText(FText());
		}
	}
}

void SAutomationWindow::HandlePresetChanged( TSharedPtr<FAutomationTestPreset> Item, ESelectInfo::Type SelectInfo )
{
	if( Item.IsValid() )
	{
		SelectedPreset = Item;
		AutomationController->SetEnabledTests(Item->GetEnabledTests());
		TestTable->RequestTreeRefresh();
		
		//Expand selected items
		TestTable->ClearExpandedItems();
		TArray< TSharedPtr< IAutomationReport > >& TestReports = AutomationController->GetReports();
		for( int32 Index = 0; Index < TestReports.Num(); Index++ )
		{
			ExpandEnabledTests(TestReports[Index]);
		}
	}
	else
	{
		SelectedPreset.Reset();

		TArray<FString> EnabledTests;
		AutomationController->SetEnabledTests(EnabledTests);
		TestTable->ClearExpandedItems();
		TestTable->RequestTreeRefresh();
	}
}

void SAutomationWindow::HandleRequesteFilterChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	int32 EntryIndex = RequestedFilterComboList.Find(Item);
	uint32 NewRequestedFlags = EAutomationTestFlags::SmokeFilter;

	switch (EntryIndex)
	{
		case 0:	//	"All Tests"
			NewRequestedFlags = EAutomationTestFlags::FilterMask;
			break;
		case 1:	//	"Smoke Tests"
			NewRequestedFlags = EAutomationTestFlags::SmokeFilter;
			break;
		case 2:	//	"Engine Tests"
			NewRequestedFlags = EAutomationTestFlags::EngineFilter;
			break;
		case 3:	//	"Product Tests"
			NewRequestedFlags = EAutomationTestFlags::ProductFilter;
			break;
		case 4:	//	"Performance Tests"
			NewRequestedFlags = EAutomationTestFlags::PerfFilter;
			break;
		case 5:	//	"Stress Tests"
			NewRequestedFlags = EAutomationTestFlags::StressFilter;
			break;
		case 6:	//	"Standard Tests"
			NewRequestedFlags = EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::PerfFilter;
			break;
	}
	AutomationController->SetRequestedTestFlags(NewRequestedFlags);
}


void SAutomationWindow::ExpandEnabledTests( TSharedPtr< IAutomationReport > InReport )
{
	// Expand node if the report is enabled or contains an enabled test
	TestTable->SetItemExpansion( InReport, InReport->IsEnabled() || InReport->GetEnabledTestsNum() > 0 );

	// Iterate through the child nodes to see if they should be expanded
	TArray<TSharedPtr< IAutomationReport > > Reports = InReport->GetFilteredChildren();

	for ( int32 ChildItem = 0; ChildItem < Reports.Num(); ChildItem++ )
	{
		ExpandEnabledTests( Reports[ ChildItem ] );
	}
}

FReply SAutomationWindow::HandleNewPresetClicked()
{
	bAddingTestPreset = true;
	return FReply::Handled().SetUserFocus(PresetTextBox.ToSharedRef(), EFocusCause::SetDirectly);
}

FReply SAutomationWindow::HandleSavePresetClicked()
{
	if(SelectedPreset.IsValid())
	{
		TArray<FString> EnabledTests;
		AutomationController->GetEnabledTestNames(EnabledTests);
		SelectedPreset->SetEnabledTests(EnabledTests);
		TestPresetManager->SavePreset(SelectedPreset.ToSharedRef());
	}
	return FReply::Handled();
}

FReply SAutomationWindow::HandleRemovePresetClicked()
{
	if(SelectedPreset.IsValid())
	{
		TestPresetManager->RemovePreset(SelectedPreset.ToSharedRef());
		SelectedPreset = nullptr;
		PresetComboBox->ClearSelection();
	}
	return FReply::Handled();
}

FText SAutomationWindow::GetPresetComboText() const
{
	if( SelectedPreset.IsValid() )
	{
		return SelectedPreset->GetPresetName();
	}
	else
	{
		return LOCTEXT("AutomationPresetComboLabel", "None");
	}
}

FText SAutomationWindow::GetRequestedFilterComboText() const
{
	if (RequestedFilterComboBox->GetSelectedItem().IsValid())
	{
		return FText::FromString(*RequestedFilterComboBox->GetSelectedItem());
	}
	else
	{
		return LOCTEXT("AutomationRequestedFilterComboLabel", "All Tests");
	}
}


TSharedRef<SWidget> SAutomationWindow::GeneratePresetComboItem(TSharedPtr<FAutomationTestPreset> InItem)
{
	if ( InItem.IsValid() )
	{
		return SNew(STextBlock)
			.Text(InItem->GetPresetName());
	}
	else
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("AutomationPreset_None", "None"));
	}
}

TSharedRef<SWidget> SAutomationWindow::GenerateRequestedFilterComboItem(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem));
}

TSharedRef< SWidget > SAutomationWindow::GenerateGroupOptionsMenuContent( TWeakPtr<class SAutomationWindow> InAutomationWindow )
{
	TSharedPtr<SAutomationWindow> AutomationWindow(InAutomationWindow.Pin());
	if( AutomationWindow.IsValid() )
	{
		return AutomationWindow->GenerateGroupOptionsMenuContent();
	}

	//Return empty menu
	FMenuBuilder MenuBuilder( true, nullptr );
	MenuBuilder.BeginSection("AutomationWindowGroupOptions", LOCTEXT("DeviceGroupOptions", "Device Group Options"));
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > SAutomationWindow::GenerateGroupOptionsMenuContent( )
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, AutomationWindowActions );
	const uint32 NumFlags = EAutomationDeviceGroupTypes::Max;
	TSharedPtr<SWidget> FlagWidgets[NumFlags];
	for( int32 i=0; i<NumFlags; i++ )
	{
		FlagWidgets[i] = 
			SNew(SCheckBox)
			.IsChecked(this, &SAutomationWindow::IsDeviceGroupCheckBoxIsChecked, i)
			.OnCheckStateChanged(this, &SAutomationWindow::HandleDeviceGroupCheckStateChanged, i)
			.Padding(FMargin(4.0f, 0.0f))
			.ToolTipText(EAutomationDeviceGroupTypes::ToDescription((EAutomationDeviceGroupTypes::Type)i))
			.IsEnabled( this, &SAutomationWindow::IsAutomationControllerIdle )
			.Content()
			[
				SNew(STextBlock)
				.Text(EAutomationDeviceGroupTypes::ToName((EAutomationDeviceGroupTypes::Type)i))
			];
	}

	MenuBuilder.BeginSection("AutomationWindowGroupDevices", LOCTEXT("GroupTypeOptions", "Group Types"));
	{
		for( int32 i=0; i<NumFlags;i++ )
		{
			MenuBuilder.AddWidget(FlagWidgets[i].ToSharedRef(),FText::GetEmpty());
		}
	}

	return MenuBuilder.MakeWidget();
}

/** Returns if full size screen shots are enabled */
ECheckBoxState SAutomationWindow::IsDeviceGroupCheckBoxIsChecked(const int32 DeviceGroupFlag) const
{
	return AutomationController->IsDeviceGroupFlagSet((EAutomationDeviceGroupTypes::Type)DeviceGroupFlag) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
/** Toggles if we are collecting full size screenshots */
void SAutomationWindow::HandleDeviceGroupCheckStateChanged(ECheckBoxState CheckBoxState, const int32 DeviceGroupFlag)
{
	//Update the device groups
	AutomationController->ToggleDeviceGroupFlag((EAutomationDeviceGroupTypes::Type)DeviceGroupFlag);
	AutomationController->UpdateDeviceGroups();
	
	//Update header
	RebuildPlatformIcons();

	//Need to force the tree to do a full refresh here because the reports have changed but the tree will keep using cached data.
	TestTable->ReCreateTreeView();
}

TSharedRef< SWidget > SAutomationWindow::GenerateTestsOptionsMenuContent( TWeakPtr<class SAutomationWindow> InAutomationWindow )
{
	TSharedPtr<SAutomationWindow> AutomationWindow(InAutomationWindow.Pin());
	if( AutomationWindow.IsValid() )
	{
		return AutomationWindow->GenerateTestsOptionsMenuContent();
	}

	//Return empty menu
	FMenuBuilder MenuBuilder( true, nullptr );
	MenuBuilder.BeginSection("AutomationWindowRunTest", LOCTEXT("RunTestOptions", "Advanced Settings"));
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > SAutomationWindow::GenerateTestsOptionsMenuContent( )
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, AutomationWindowActions );
	TSharedRef<SWidget> NumTests = 
		SNew(SBox)
		.WidthOverride( 200.0f )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding(0.0f,0.0f,4.0f, 0.0f)
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NumTestsToolTip", "Number of runs:") )
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(1)
				.MaxValue(1000)
				.MinSliderValue(1)
				.MaxSliderValue(1000)
				.Value(this,&SAutomationWindow::GetRepeatCount)
				.OnValueChanged(this,&SAutomationWindow::OnChangeRepeatCount)
				.IsEnabled( this, &SAutomationWindow::IsAutomationControllerIdle )
			]

		];

	
	TSharedRef<SWidget> SendAnalyticsWidget =
		SNew(SCheckBox)
		.IsChecked(this, &SAutomationWindow::IsSendAnalyticsCheckBoxChecked)
		.OnCheckStateChanged(this, &SAutomationWindow::HandleSendAnalyticsBoxCheckStateChanged)
		.Padding(FMargin(4.0f, 0.0f))
		.ToolTipText(LOCTEXT("AutomationSendAnalyticsTip", "If checked, tests send analytics results to the backend"))
		.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutomationSendAnalyticsText", "Enable analytics"))
		];

	TSharedRef<SWidget> EnableScreenshotsWidget =
		SNew(SCheckBox)
		.IsChecked(this, &SAutomationWindow::IsEnableScreenshotsCheckBoxChecked)
		.OnCheckStateChanged(this, &SAutomationWindow::HandleEnableScreenshotsBoxCheckStateChanged)
		.Padding(FMargin(4.0f, 0.0f))
		.ToolTipText(LOCTEXT("AutomationEnableScreenshotsTip", "If checked, tests that support it will save screenshots"))
		.IsEnabled( this, &SAutomationWindow::IsAutomationControllerIdle )
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutomationNoScreenshotText", "Enable screenshots"))
		];

	TSharedRef<SWidget> FullSizeScreenshotsWidget =
		SNew(SCheckBox)
		.IsChecked(this, &SAutomationWindow::IsFullSizeScreenshotsCheckBoxChecked)
		.OnCheckStateChanged(this, &SAutomationWindow::HandleFullSizeScreenshotsBoxCheckStateChanged)
		.Padding(FMargin(4.0f, 0.0f))
		.ToolTipText(LOCTEXT("AutomationFullSizeScreenshotsTip", "If checked, test that take screenshots will send back full size images instead of thumbnails."))
		.IsEnabled( this, &SAutomationWindow::IsFullSizeScreenshotsOptionEnabled )
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutomationFullSizeScreenshotText", "Full size screenshots"))
		];



	MenuBuilder.BeginSection("AutomationWindowRunTest", LOCTEXT("RunTestOptions", "Advanced Settings"));
	{
		MenuBuilder.AddWidget(NumTests, FText::GetEmpty());
		MenuBuilder.AddWidget(SendAnalyticsWidget, FText::GetEmpty());
	}
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("AutomationWindowScreenshots", LOCTEXT("ScreenshotOptions", "Screenshot Settings"));
	{
		MenuBuilder.AddWidget(EnableScreenshotsWidget, FText::GetEmpty());
		MenuBuilder.AddWidget(FullSizeScreenshotsWidget, FText::GetEmpty());
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > SAutomationWindow::GenerateTestHistoryMenuContent(TWeakPtr<class SAutomationWindow> InAutomationWindow)
{
	TSharedPtr<SAutomationWindow> AutomationWindow(InAutomationWindow.Pin());
	if (AutomationWindow.IsValid())
	{
		return AutomationWindow->GenerateTestHistoryMenuContent();
	}

	//Return empty menu
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("AutomationWindowTestHistory", LOCTEXT("AutomationWindowTestHistory", "Settings"));
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > SAutomationWindow::GenerateTestHistoryMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, AutomationWindowActions);

	TSharedRef<SWidget> HistoryCountWidget =
	SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Bottom)
	.Padding(FMargin( 0.f, 0.f, 4.f, 0.f ))
	[
		SNew(STextBlock)
		.Text(LOCTEXT("NumberItemsToTrack", "Number Items To Track:"))
	]
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Bottom)
	[
		SNew(SBox)
		.WidthOverride(50.0f)
		[
			SNew(SSpinBox<int32>)
			.MinValue(1)
			.MaxValue(AutomationReportConstants::MaximumLogsToKeep)
			.MinSliderValue(1)
			.MaxSliderValue(AutomationReportConstants::MaximumLogsToKeep)
			.Value(this, &SAutomationWindow::GetTestHistoryCount)
			.OnValueChanged(this, &SAutomationWindow::OnChangeTestHistoryCount)
			.IsEnabled(this, &SAutomationWindow::IsAutomationControllerIdle)
		]

	];

	MenuBuilder.BeginSection("AutomationWindowTestHistory", LOCTEXT("AutomationWindowTestHistory", "Settings"));
	{
		MenuBuilder.AddWidget(HistoryCountWidget, FText::GetEmpty());
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

ECheckBoxState SAutomationWindow::IsFullSizeScreenshotsCheckBoxChecked() const
{
	return AutomationController->IsUsingFullSizeScreenshots() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAutomationWindow::HandleFullSizeScreenshotsBoxCheckStateChanged(ECheckBoxState CheckBoxState)
{
	AutomationController->SetUsingFullSizeScreenshots(CheckBoxState == ECheckBoxState::Checked);
}

ECheckBoxState SAutomationWindow::IsSendAnalyticsCheckBoxChecked() const
{
	return AutomationController->IsSendAnalytics() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAutomationWindow::HandleSendAnalyticsBoxCheckStateChanged(ECheckBoxState CheckBoxState)
{
	AutomationController->SetSendAnalytics(CheckBoxState == ECheckBoxState::Checked);
}


ECheckBoxState SAutomationWindow::IsEnableScreenshotsCheckBoxChecked() const
{
	return AutomationController->IsScreenshotAllowed() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAutomationWindow::HandleEnableScreenshotsBoxCheckStateChanged(ECheckBoxState CheckBoxState)
{
	AutomationController->SetScreenshotsEnabled(CheckBoxState == ECheckBoxState::Checked);
}

bool SAutomationWindow::IsFullSizeScreenshotsOptionEnabled() const
{
	return AutomationController->IsScreenshotAllowed() && IsAutomationControllerIdle();
}

TArray<FString> SAutomationWindow::SaveExpandedTestNames(TSet<TSharedPtr<IAutomationReport>> ExpandedItems)
{
	TArray<FString> ExpandedItemsNames;
	for ( TSharedPtr<IAutomationReport> ExpandedItem : ExpandedItems )
	{
		ExpandedItemsNames.Add(ExpandedItem->GetDisplayNameWithDecoration());
	}
	return ExpandedItemsNames;
}

// Expanded the given item if its name is in the array of strings given.
void SAutomationWindow::ExpandItemsInList(TSharedPtr<SAutomationTestTreeView<TSharedPtr<IAutomationReport>>> InTestTable, TSharedPtr<IAutomationReport> InReport, TArray<FString> ItemsToExpand)
{
	InTestTable->SetItemExpansion(InReport, ItemsToExpand.Contains(InReport->GetDisplayNameWithDecoration()));

	TArray<TSharedPtr<IAutomationReport>> ChildReports = InReport->GetFilteredChildren();

	for ( int32 Index = 0; Index < ChildReports.Num(); Index++ )
	{
		ExpandItemsInList(InTestTable, ChildReports[Index], ItemsToExpand);
	}
}

// Only valid in the editor
#if WITH_EDITOR
TSharedPtr<SWidget> SAutomationWindow::HandleAutomationListContextMenuOpening()
{
	TArray< TSharedPtr<IAutomationReport> >SelectedReport = TestTable->GetSelectedItems();

	TArray<FString> AssetNames;
	for (int32 ReportIndex = 0; ReportIndex < SelectedReport.Num(); ++ReportIndex)
	{
		// TODO This is super sketch, we were interpreting the parameter always as the asset, this is no good.
		if (SelectedReport[ReportIndex].IsValid() && (SelectedReport[ReportIndex]->GetTestParameter().Len() > 0))
		{
			AssetNames.Add(SelectedReport[ReportIndex]->GetTestParameter());
		}
	}		
	if (AssetNames.Num())
	{
		return SNew(SAutomationTestItemContextMenu, AssetNames);
	}

	return nullptr;
}

void SAutomationWindow::RunSelectedTests()
{
	AutomationController->SetVisibleTestsEnabled(false);
	SetAllSelectedTestsChecked(true);
	RunTests();
}

namespace
{
	bool MakeMapPathUrl(FString& InPath)
	{
		if ( FPaths::MakePathRelativeTo(InPath, *FPaths::GameContentDir()) )
		{
			InPath.InsertAt(0, TEXT("/Game/"));
			InPath.RemoveFromEnd(TEXT(".umap"));
			return true;
		}
		return false;
	}

	/**
	 * Kind of a hack - this requires that we know we group all the map tests coming from blueprints under "Functional Tests"
	 */
	TSharedPtr<IAutomationReport> GetFunctionalTestsReport(const TArray< TSharedPtr< IAutomationReport > >& TestReports)
	{
		for ( auto& Report : TestReports )
		{
			if ( Report->GetDisplayName() == TEXT("Functional Tests") )
			{
				return Report;
			}

			auto FoundInChild = GetFunctionalTestsReport(Report->GetChildReports());
			if ( FoundInChild.IsValid() )
			{
				return FoundInChild;
			}
		}
		return TSharedPtr<IAutomationReport>();
	}

	void FindReportByGameRelativeAssetPath(const TSharedPtr<IAutomationReport>& RootReport, const FString& AssetRelativePath, TArray<TSharedPtr<IAutomationReport>>& OutLevelReports)
	{
		FString TestAssetRelativePath(RootReport->GetTestParameter());

		if ( TestAssetRelativePath.StartsWith(AssetRelativePath) )
		{
			OutLevelReports.Add(RootReport);
		}
		else
		{
			// Branch node
			for ( auto ChildReport : RootReport->GetChildReports() )
			{
				FindReportByGameRelativeAssetPath(ChildReport, AssetRelativePath, OutLevelReports);
			}
		}
	}
} // namespace

void SAutomationWindow::FindTestReportsForCurrentEditorLevel(TArray<TSharedPtr<IAutomationReport>>& OutLevelReports)
{
	// Find the current map path
	if ( GWorld && GWorld->GetCurrentLevel() )
	{
		FString MapUrl(FEditorFileUtils::GetFilename(GWorld->GetCurrentLevel()));
		if ( MakeMapPathUrl(MapUrl) )
		{
			auto FunctionTestsReport = GetFunctionalTestsReport(AutomationController->GetReports());
			if ( FunctionTestsReport.IsValid() )
			{
				FindReportByGameRelativeAssetPath(FunctionTestsReport, MapUrl, OutLevelReports);
			}
		}
	}
}

bool SAutomationWindow::CanExecuteRunLevelTest()
{
	return IsAutomationControllerIdle();
}

void SAutomationWindow::OnRunLevelTest()
{
	TArray<TSharedPtr<IAutomationReport>> LevelReports;
	FindTestReportsForCurrentEditorLevel(LevelReports);

	if ( LevelReports.Num() > 0 )
	{
		TestTable->ClearSelection();
		for ( auto& LevelReport : LevelReports )
		{
			TestTable->SetItemSelection(LevelReport, true);
		}

		ScrollToTest(LevelReports[0]);
		RunSelectedTests();
	}
}

void SAutomationWindow::ScrollToTest(TSharedPtr<IAutomationReport> InReport)
{
	auto& RootReports = AutomationController->GetReports();
	for ( auto ChildReport : RootReports )
	{
		auto ShouldExpand = ExpandToTest(ChildReport, InReport);
		TestTable->SetItemExpansion(ChildReport, ShouldExpand);
	}

	TestTable->RequestScrollIntoView(InReport);
}

bool SAutomationWindow::ExpandToTest(TSharedPtr<IAutomationReport> InRoot, TSharedPtr<IAutomationReport> InReport)
{
	if ( InRoot == InReport )
		return true;

	bool WasExpanded = false;

	for ( auto ChildReport : InRoot->GetChildReports() )
	{
		auto ShouldExpand = ExpandToTest(ChildReport, InReport);
		TestTable->SetItemExpansion(ChildReport, ShouldExpand);

		if ( ShouldExpand )
		{
			// Here we could just return true, but we want to collapse all the other reports 
			// so we keep going and just remember that we found the test.
			WasExpanded = true;
		}
	}

	return WasExpanded;
}

#endif


void SAutomationWindow::PopulateReportSearchStrings( const TSharedPtr< IAutomationReport >& Report, OUT TArray< FString >& OutSearchStrings ) const
{
	if( !Report.IsValid() )
	{
		return;
	}

	OutSearchStrings.Add( Report->GetDisplayName() );
}


void SAutomationWindow::OnGetChildren(TSharedPtr<IAutomationReport> InItem, TArray<TSharedPtr<IAutomationReport> >& OutItems)
{
	OutItems = InItem->GetFilteredChildren();
}

void SAutomationWindow::OnTestExpansionRecursive(TSharedPtr<IAutomationReport> InAutomationReport, bool bInIsItemExpanded)
{
	if ( InAutomationReport.IsValid() )
	{
		TArray<TSharedPtr<IAutomationReport> >& FilteredChildren = InAutomationReport->GetFilteredChildren();

		TestTable->SetItemExpansion(InAutomationReport, bInIsItemExpanded);

		for ( TSharedPtr<IAutomationReport>& Child : FilteredChildren )
		{
			OnTestExpansionRecursive(Child, bInIsItemExpanded);
		}
	}
}

void SAutomationWindow::OnTestSelectionChanged(TSharedPtr<IAutomationReport> Selection, ESelectInfo::Type /*SelectInfo*/)
{
	TSharedPtr<IAutomationReport> PreviousSelectionLock = PreviousSelection.Pin();
	if ( PreviousSelectionLock.IsValid() )
	{
		PreviousSelectionLock->OnSetResults.Unbind();
	}

	bHasChildTestSelected = false;

	UpdateTestLog(Selection);

	if ( Selection.IsValid() )
	{
		Selection->OnSetResults.BindRaw(this, &SAutomationWindow::UpdateTestLog);
		PreviousSelection = Selection;

		if ( Selection->GetTotalNumChildren() == 0 )
		{
			bHasChildTestSelected = true;
		}
	}
}

void SAutomationWindow::UpdateTestLog(TSharedPtr<IAutomationReport> Selection)
{
	//empty the previous log
	LogMessages.Empty();

	if (Selection.IsValid())
	{
		//accumulate results for each device cluster that supports the test
		int32 NumClusters = AutomationController->GetNumDeviceClusters();
		for (int32 ClusterIndex = 0; ClusterIndex < NumClusters; ++ClusterIndex)
		{
			//no sense displaying device name if only one is available
			if (NumClusters > 1)
			{
				FString DeviceTypeName = AutomationController->GetClusterGroupName(ClusterIndex) + TEXT("  -  ") + Selection->GetGameInstanceName(ClusterIndex);
				LogMessages.Add(MakeShareable(new FAutomationOutputMessage(DeviceTypeName, TEXT("Automation.Header"))));
			}

			const int32 NumOfPasses = Selection->GetNumResults(ClusterIndex);
			for( int32 PassIndex = 0; PassIndex < NumOfPasses; ++PassIndex )
			{
				//get strings out of the report and populate the Log Messages
				FAutomationTestResults TestResults = Selection->GetResults(ClusterIndex,PassIndex);

				//no sense displaying device name if only one is available
				if (NumOfPasses > 1)
				{
					FString PassHeader = LOCTEXT("TestPassHeader", "Pass:").ToString();
					PassHeader += FString::Printf(TEXT("%i"),PassIndex+1);
					LogMessages.Add(MakeShareable(new FAutomationOutputMessage(PassHeader, TEXT("Automation.Header"))));
				}

				for (int32 ErrorIndex = 0; ErrorIndex < TestResults.Errors.Num(); ++ErrorIndex)
				{
					LogMessages.Add(MakeShareable(new FAutomationOutputMessage(TestResults.Errors[ErrorIndex].ToString(), TEXT("Automation.Error"))));
				}
				for (int32 WarningIndex = 0; WarningIndex < TestResults.Warnings.Num(); ++WarningIndex)
				{
					LogMessages.Add(MakeShareable(new FAutomationOutputMessage(TestResults.Warnings[WarningIndex], TEXT("Automation.Warning"))));
				}
				for (int32 LogIndex = 0; LogIndex < TestResults.Logs.Num(); ++LogIndex)
				{
					LogMessages.Add(MakeShareable(new FAutomationOutputMessage(TestResults.Logs[LogIndex], TEXT("Automation.Normal"))));
				}
				if ((TestResults.Warnings.Num() == 0) &&(TestResults.Errors.Num() == 0) && (Selection->GetState(ClusterIndex,PassIndex)==EAutomationState::Success))
				{
					LogMessages.Add(MakeShareable(new FAutomationOutputMessage(LOCTEXT( "AutomationTest_SuccessMessage", "Success" ).ToString(), TEXT("Automation.Normal"))));
				}

				LogMessages.Add(MakeShareable(new FAutomationOutputMessage(TEXT(""), TEXT("Log.Normal"))));
			}
		}
	}

	//rebuild UI
	LogListView->RequestListRefresh();
}


EVisibility SAutomationWindow::GetTestLogVisibility( ) const
{
	return (GetTestGraphVisibility() == EVisibility::Visible) ? EVisibility::Hidden : EVisibility::Visible;
}


EVisibility SAutomationWindow::GetTestGraphVisibility( ) const
{
	//Show the graphical window if we don't have a child test selected and we have results to view
	return (!bHasChildTestSelected && GraphicalResultBox->HasResults()) ? EVisibility::Visible : EVisibility::Hidden;
}


void SAutomationWindow::HeaderCheckboxStateChange(ECheckBoxState InCheckboxState)
{
	const bool bState = (InCheckboxState == ECheckBoxState::Checked)? true : false;

	AutomationController->SetVisibleTestsEnabled(bState);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAutomationWindow::RebuildPlatformIcons()
{
	//empty header UI
	PlatformsHBox->ClearChildren();

	//for each device type
	int32 NumClusters = AutomationController->GetNumDeviceClusters();
	for (int32 ClusterIndex = 0; ClusterIndex < NumClusters; ++ClusterIndex)
	{
		//find the right platform icon
		FString DeviceImageName = TEXT("Launcher.Platform_");
		FString DeviceTypeName = AutomationController->GetDeviceTypeName(ClusterIndex);
		DeviceImageName += DeviceTypeName;
		const FSlateBrush* ImageToUse = FEditorStyle::GetBrush(*DeviceImageName);

		PlatformsHBox->AddSlot()
		.AutoWidth()
		.MaxWidth(ColumnWidth)
		[
			SNew( SOverlay )
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ErrorReporting.Box") )
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding( FMargin(3,0) )
				.BorderBackgroundColor( FSlateColor( FLinearColor( 1.0f, 0.0f, 1.0f, 0.0f ) ) )
				.ToolTipText( CreateDeviceTooltip( ClusterIndex ) )
				[
					SNew(SImage)
					.Image(ImageToUse)
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				//Overlay how many devices are in the cluster
				SNew( STextBlock )
				.Text( this, &SAutomationWindow::OnGetNumDevicesInClusterString, ClusterIndex )
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SAutomationWindow::CreateDeviceTooltip(int32 ClusterIndex)
{
	FTextBuilder ReportBuilder;

	const int32 NumClusters = AutomationController->GetNumDeviceClusters();
	if( NumClusters > 1 )
	{
		ReportBuilder.AppendLine(LOCTEXT("ToolTipClusterName", "Cluster Name:"));
		ReportBuilder.AppendLine(AutomationController->GetClusterGroupName(ClusterIndex));
	}

	ReportBuilder.AppendLine(LOCTEXT("ToolTipGameInstances", "Game Instances:"));

	int32 NumDevices = AutomationController->GetNumDevicesInCluster( ClusterIndex );
	for ( int32 DeviceIndex = 0; DeviceIndex < NumDevices; ++DeviceIndex )
	{
		ReportBuilder.AppendLine(AutomationController->GetGameInstanceName(ClusterIndex, DeviceIndex).LeftPad(2));
	}

	return ReportBuilder.ToText();
}


void SAutomationWindow::ClearAutomationUI ()
{
	// Clear results from the automation controller
	AutomationController->ClearAutomationReports();
	TestTable->RequestTreeRefresh();

	// Clear the platform icons
	if (PlatformsHBox.IsValid())
	{
		PlatformsHBox->ClearChildren();
	}
	
	// Clear the log
	LogMessages.Empty();
	LogListView->RequestListRefresh();
}


TSharedRef<ITableRow> SAutomationWindow::OnGenerateWidgetForTest( TSharedPtr<IAutomationReport> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	bIsRequestingTests = false;
	return SNew( SAutomationTestItem, OwnerTable )
		.TestStatus( InItem )
		.ColumnWidth( ColumnWidth )
		.HighlightText(this, &SAutomationWindow::HandleAutomationHighlightText)
		.OnCheckedStateChanged(this, &SAutomationWindow::HandleItemCheckBoxCheckedStateChanged);
}


TSharedRef<ITableRow> SAutomationWindow::OnGenerateWidgetForLog(TSharedPtr<FAutomationOutputMessage> Message, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Message.IsValid());

	// ^((?:[\w]\:|\\)(?:(?:\\[a-z_\-\s0-9\.]+)+)\.(?:cpp|h))\((\d+)\)
	// https://regex101.com/r/vV4cV7/1
	FRegexPattern FileAndLinePattern(TEXT("^((?:[\\w]\\:|\\\\)(?:(?:\\\\[a-z_\\-\\s0-9\\.]+)+)\\.(?:cpp|h))\\((\\d+)\\)"));
	FRegexMatcher FileAndLineRegexMatcher(FileAndLinePattern, Message->Text);

	TSharedRef<SWidget> SourceLink = SNullWidget::NullWidget;

	FString MessageString = Message->Text;

	if ( FileAndLineRegexMatcher.FindNext() )
	{
		FString FileName = FileAndLineRegexMatcher.GetCaptureGroup(1);
		int32 LineNumber = FCString::Atoi(*FileAndLineRegexMatcher.GetCaptureGroup(2));

		// Remove the hyperlink from the message, since we're splitting it into its own string.
		MessageString = MessageString.RightChop(FileAndLineRegexMatcher.GetMatchEnding());

		SourceLink = SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "Common.GotoNativeCodeHyperlink")
			.TextStyle(FEditorStyle::Get(), Message->Style)
			.OnNavigate_Lambda([=] { FSlateApplication::Get().GotoLineInSource(FileName, LineNumber); })
			.Text(FText::FromString(FileAndLineRegexMatcher.GetCaptureGroup(0)));
	}

	return SNew(STableRow<TSharedPtr<FAutomationOutputMessage> >, OwnerTable)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0)
			[
				SourceLink
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0)
			[
				SNew(STextBlock)
				.TextStyle( FEditorStyle::Get(), Message->Style )
				.Text(FText::FromString(MessageString))
			]
		];
}


FText SAutomationWindow::OnGetNumEnabledTestsString() const
{
	int32 NumPasses = AutomationController->GetNumPasses();
	if( NumPasses > 1 )
	{
		return FText::Format(LOCTEXT("NumEnabledTestsFmt", "{0} x{1}"), FText::AsNumber(AutomationController->GetEnabledTestsNum()), FText::AsNumber(NumPasses));
	}
	else
	{
		return FText::AsNumber(AutomationController->GetEnabledTestsNum());
	}
}


FText SAutomationWindow::OnGetNumDevicesInClusterString(const int32 ClusterIndex) const
{
	return FText::AsNumber(AutomationController->GetNumDevicesInCluster(ClusterIndex));
}

void SAutomationWindow::OnRefreshTestCallback()
{
	//if the window hasn't been created yet
	if (!PlatformsHBox.IsValid())
	{
		return;
	}

	//rebuild the platform header
	RebuildPlatformIcons();

	//filter the tests that are shown
	AutomationController->SetFilter( AutomationFilters );

	// Only expand the child nodes if we have a text filter
	bool ExpandChildren = !AutomationTextFilter->GetRawFilterText().IsEmpty();

	TArray< TSharedPtr< IAutomationReport > >& TestReports = AutomationController->GetReports();

	for( int32 Index = 0; Index < TestReports.Num(); Index++ )
	{
		ExpandTreeView( TestReports[ Index ], ExpandChildren );

		// Expand any items that where expanded before refresh tests was pressed.
		if( !ExpandChildren )
		{
			ExpandItemsInList( TestTable, TestReports[Index], SavedExpandedItems );
		}
	}

	// Check Tests that where checked before refresh tests was pressed.
	AutomationController->SetEnabledTests(SavedEnabledTests);
	SavedEnabledTests.Empty();
	SavedExpandedItems.Empty();

	//rebuild the UI
	TestTable->RequestTreeRefresh();

	//update the background style
	UpdateTestListBackgroundStyle();
}


void SAutomationWindow::OnTestAvailableCallback( EAutomationControllerModuleState::Type InAutomationControllerState )
{
	AutomationControllerState = InAutomationControllerState;

#if WITH_EDITOR
	// Only list tests on opening the Window if the asset registry isn't in the middle of loading tests.
	if ( InAutomationControllerState == EAutomationControllerModuleState::Ready && AutomationController->GetReports().Num() == 0 )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if ( !AssetRegistryModule.Get().IsLoadingAssets() )
		{
			ListTests();
		}
	}
#else
	ListTests();
#endif
}

void SAutomationWindow::OnTestsCompleteCallback()
{
	// Simulate selection again after testing finishes.
	if ( TestTable->GetNumItemsSelected() > 0 )
	{
		OnTestSelectionChanged(TestTable->GetSelectedItems()[0], ESelectInfo::Direct);
	}
}


void SAutomationWindow::ExpandTreeView( TSharedPtr< IAutomationReport > InReport, const bool ShouldExpand )
{
	// Expand node if the report is flagged
	TestTable->SetItemExpansion( InReport, ShouldExpand && InReport->ExpandInUI() );

	// Iterate through the child nodes to see if they should be expanded
	TArray<TSharedPtr< IAutomationReport > > Reports = InReport->GetFilteredChildren();

	for ( int32 ChildItem = 0; ChildItem < Reports.Num(); ChildItem++ )
	{
		ExpandTreeView( Reports[ ChildItem ], ShouldExpand );
	}
}

//TODO AUTOMATION - remove
/** Updates list of all the tests */
void SAutomationWindow::ListTests( )
{
	// Save Expanded and Enabled Test Names
	AutomationController->GetEnabledTestNames(SavedEnabledTests);

	TSet<TSharedPtr<IAutomationReport>> ExpandedItems;
	TestTable->GetExpandedItems(ExpandedItems);
	SavedExpandedItems = SaveExpandedTestNames(ExpandedItems);

	AutomationController->RequestTests();
}


//TODO AUTOMATION - remove
/** Finds available workers */
void SAutomationWindow::FindWorkers()
{
	ActiveSession = SessionManager->GetSelectedSession();

	bool SessionIsValid = ActiveSession.IsValid() && (ActiveSession->GetSessionOwner() == FPlatformProcess::UserName(false));

	if (SessionIsValid)
	{
		bIsRequestingTests = true;

		AutomationController->RequestAvailableWorkers(ActiveSession->GetSessionId());

		RebuildPlatformIcons();
	}
	else
	{
		bIsRequestingTests = false;
		// Clear UI if the session is invalid
		ClearAutomationUI();
	}

	MenuBar->SetEnabled( SessionIsValid );
}

void SAutomationWindow::HandleSessionManagerInstanceChanged()
{
	UpdateTestListBackgroundStyle();
}

void SAutomationWindow::UpdateTestListBackgroundStyle()
{
	TArray<ISessionInstanceInfoPtr> OutInstances;

	if( ActiveSession.IsValid() )
	{
		ActiveSession->GetInstances(OutInstances);
	}

	TestBackgroundType = EAutomationTestBackgroundStyle::Unknown;

	if( OutInstances.Num() > 0 )
	{
		FString FirstInstanceType = OutInstances[0]->GetInstanceType();

		if( FirstInstanceType.Contains(TEXT("Editor")) )
		{
			TestBackgroundType = EAutomationTestBackgroundStyle::Editor;
		}
		else if( FirstInstanceType.Contains(TEXT("Game")) )
		{
			TestBackgroundType = EAutomationTestBackgroundStyle::Game;
		}
	}
}


FReply SAutomationWindow::RunTests()
{
	if( AutomationControllerState == EAutomationControllerModuleState::Running )
	{
		AutomationController->StopTests();
	}
	else
	{
		// Prompt to save current map when running a test. 
#if WITH_EDITOR
		if ( !GIsDemoMode )
		{
			// If there are any unsaved changes to the current level, see if the user wants to save those first.
			const bool bPromptUserToSave = true;
			const bool bSaveMapPackages = true;
			const bool bSaveContentPackages = true;
			if ( FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages) == false )
			{
				// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
				return FReply::Handled();
			}
		}
#endif

		AutomationController->RunTests( ActiveSession->IsStandalone() );
	}

	LogMessages.Empty();
	LogListView->RequestListRefresh();

	//Clear old results
	GraphicalResultBox->ClearResults();

	return FReply::Handled();
}


/** Filtering */
void SAutomationWindow::OnFilterTextChanged( const FText& InFilterText )
{
	AutomationTextFilter->SetRawFilterText( InFilterText );
	AutomationSearchBox->SetError( AutomationTextFilter->GetFilterErrorText() );

	//update the widget
	OnRefreshTestCallback();
}


bool SAutomationWindow::IsDeveloperDirectoryIncluded() const
{
	return AutomationController->IsDeveloperDirectoryIncluded();
}


void SAutomationWindow::OnToggleDeveloperDirectoryIncluded()
{
	//Change controller filter
	AutomationController->SetDeveloperDirectoryIncluded(!IsDeveloperDirectoryIncluded());
	// need to call this to request update
	ListTests();
}


bool SAutomationWindow::IsSmokeTestFilterOn() const
{
	return AutomationGeneralFilter->OnlyShowSmokeTests();
}


void SAutomationWindow::OnToggleSmokeTestFilter()
{
	AutomationGeneralFilter->SetOnlyShowSmokeTests( !IsSmokeTestFilterOn() );
	OnRefreshTestCallback();
}


bool SAutomationWindow::IsWarningFilterOn() const
{
	return AutomationGeneralFilter->ShouldShowWarnings();
}


void SAutomationWindow::OnToggleWarningFilter()
{
	AutomationGeneralFilter->SetShowWarnings( !IsWarningFilterOn() );
	OnRefreshTestCallback();
}


bool SAutomationWindow::IsErrorFilterOn() const
{
	return AutomationGeneralFilter->ShouldShowErrors();
}


void SAutomationWindow::OnToggleErrorFilter()
{
	AutomationGeneralFilter->SetShowErrors( !IsErrorFilterOn() );
	OnRefreshTestCallback();
}


ECheckBoxState SAutomationWindow::IsTrackingHistory() const
{
	return bIsTrackingHistory ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAutomationWindow::OnToggleTrackHistory(ECheckBoxState InState)
{
	bIsTrackingHistory = InState == ECheckBoxState::Checked;

	AutomationController->TrackReportHistory(bIsTrackingHistory, NumHistoryElementsToTrack);

	if (bIsTrackingHistory)
	{
		TestTableHeaderRow->AddColumn(
			SHeaderRow::Column(AutomationTestWindowConstants::History)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			.FixedWidth(100.0f)
			.DefaultLabel(LOCTEXT("TestHistory", "Test History"))
		);
	}
	else
	{
		TestTableHeaderRow->RemoveColumn(AutomationTestWindowConstants::History);
	}


	OnRefreshTestCallback();
}

void SAutomationWindow::OnChangeTestHistoryCount(int32 InNewValue)
{
	NumHistoryElementsToTrack = InNewValue;
	AutomationController->TrackReportHistory(bIsTrackingHistory, InNewValue);
}

int32 SAutomationWindow::GetTestHistoryCount() const
{
	return NumHistoryElementsToTrack;
}


void SAutomationWindow::OnChangeRepeatCount(int32 InNewValue)
{
	AutomationController->SetNumPasses(InNewValue);
}

int32 SAutomationWindow::GetRepeatCount() const
{
	return AutomationController->GetNumPasses();
}


FString SAutomationWindow::GetSmallIconExtension() const
{
	FString Brush;
	if (FMultiBoxSettings::UseSmallToolBarIcons.Get())
	{
		Brush += TEXT( ".Small" );
	}
	return Brush;
}


EVisibility SAutomationWindow::GetLargeToolBarVisibility() const
{
	return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
}


const FSlateBrush* SAutomationWindow::GetRunAutomationIcon() const
{
	FString Brush = TEXT( "AutomationWindow" );
	if( AutomationControllerState == EAutomationControllerModuleState::Running )
	{
		Brush += TEXT( ".StopTests" );	// Temporary brush type for stop tests
	}
	else
	{
		Brush += TEXT( ".RunTests" );
	}
	Brush += GetSmallIconExtension();
	return FEditorStyle::GetBrush( *Brush );
}


FText SAutomationWindow::GetRunAutomationLabel() const
{
	if( AutomationControllerState == EAutomationControllerModuleState::Running )
	{
		return LOCTEXT( "RunStopTestsLabel", "Stop Tests" );
	}
	else
	{
		return LOCTEXT( "RunStartTestsLabel", "Start Tests" );
	}
}


FText SAutomationWindow::HandleAutomationHighlightText( ) const
{
	if ( AutomationSearchBox.IsValid() )
	{
		return AutomationSearchBox->GetText();
	}
	return FText();
}


EVisibility SAutomationWindow::HandleSelectSessionOverlayVisibility( ) const
{
	if (SessionManager->GetSelectedInstances().Num() > 0)
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}


void SAutomationWindow::HandleSessionManagerCanSelectSession( const ISessionInfoPtr& Session, bool& CanSelect )
{
	if (ActiveSession.IsValid() && AutomationController->CheckTestResultsAvailable())
	{
		EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ChangeSessionDialog", "Are you sure you want to change sessions?\nAll automation results data will be lost"));
		CanSelect = Result == EAppReturnType::Yes ? true : false;
	}
}


void SAutomationWindow::HandleSessionManagerSelectionChanged( const ISessionInfoPtr& SelectedSession )
{
	FindWorkers();
}


bool SAutomationWindow::IsAutomationControllerIdle() const
{
	return AutomationControllerState != EAutomationControllerModuleState::Running;
}


bool SAutomationWindow::IsAutomationRunButtonEnabled() const
{
	return AutomationControllerState != EAutomationControllerModuleState::Disabled;
}


void SAutomationWindow::CopyLog( )
{
	TArray<TSharedPtr<FAutomationOutputMessage> > SelectedItems = LogListView->GetSelectedItems();

	if (SelectedItems.Num() > 0)
	{
		FString SelectedText;

		for( int32 Index = 0; Index < SelectedItems.Num(); ++Index )
		{
			SelectedText += SelectedItems[Index]->Text;
			SelectedText += LINE_TERMINATOR;
		}

		FPlatformMisc::ClipboardCopy( *SelectedText );
	}
}


FReply SAutomationWindow::HandleCommandBarCopyLogClicked( )
{
	CopyLog();

	return FReply::Handled();
}


void SAutomationWindow::HandleLogListSelectionChanged( TSharedPtr<FAutomationOutputMessage> InItem, ESelectInfo::Type SelectInfo )
{
	CommandBar->SetNumLogMessages(LogListView->GetNumItemsSelected());
}


void SAutomationWindow::ChangeTheSelectionToThisRow(TSharedPtr< IAutomationReport >  ThisRow) 
{
	TestTable->SetSelection(ThisRow, ESelectInfo::Direct);
}


bool SAutomationWindow::IsRowSelected(TSharedPtr< IAutomationReport >  ThisRow)
{
	TArray< TSharedPtr<IAutomationReport> >SelectedReport = TestTable->GetSelectedItems();

	bool ThisRowIsInTheSelectedSet = false;

	for (int i = 0; i<SelectedReport.Num();++i)
	{
		if (SelectedReport[i] == ThisRow)
		{
			ThisRowIsInTheSelectedSet = true;
		}
	}
	return ThisRowIsInTheSelectedSet;
}


void SAutomationWindow::SetAllSelectedTestsChecked( bool InChecked )
{
	TArray< TSharedPtr<IAutomationReport> >SelectedReport = TestTable->GetSelectedItems();

	for (int i = 0; i<SelectedReport.Num();++i)
	{
		if (SelectedReport[i].IsValid())
		{
			SelectedReport[i]->SetEnabled(InChecked);
		}
	}
}


bool SAutomationWindow::IsAnySelectedRowEnabled()
{
	TArray< TSharedPtr<IAutomationReport> >SelectedReport = TestTable->GetSelectedItems();

	//Do check or uncheck selected rows based on current settings
	bool bFoundCheckedRow = false;
	bool bFoundNotCheckedRow = false;
	bool bRowCheckedValue = true;

	//Check all the rows if there is a mixture of checked and unchecked then we set all checked, otherwise set to opposite of current values

	for (int i = 0; i<SelectedReport.Num();++i)
	{
		if (SelectedReport[i].IsValid())
		{
			if (SelectedReport[i]->IsEnabled())
			{
				bFoundCheckedRow = true;
			}
			else
			{
				bFoundNotCheckedRow = true;
			}
		}
		//break when all rows checked or different values found
		if (bFoundCheckedRow && bFoundNotCheckedRow)
		{
			break;
		}
	}

	//if rows were all checked set to unchecked otherwise we can set to checked
	if (bFoundCheckedRow && !bFoundNotCheckedRow)
	{
		bRowCheckedValue = false;
	}

	return bRowCheckedValue;
}


/* SWidget implementation
 *****************************************************************************/

FReply SAutomationWindow::OnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	if (InKeyEvent.GetKey() == EKeys::SpaceBar)
	{
		SetAllSelectedTestsChecked(IsAnySelectedRowEnabled());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply SAutomationWindow::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (InKeyEvent.IsControlDown())
	{
		if (InKeyEvent.GetKey() == EKeys::C)
		{
			CopyLog();

			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}


/* SAutomationWindow callbacks
 *****************************************************************************/

void SAutomationWindow::HandleItemCheckBoxCheckedStateChanged( TSharedPtr< IAutomationReport > TestStatus )
{
	//If multiple rows selected then handle all the rows
	if (AreMultipleRowsSelected())
	{
		//if current row is not in the selected list select that row
		if(IsRowSelected(TestStatus))
		{
			//Just set them all to the opposite of the row just clicked.
			SetAllSelectedTestsChecked(!TestStatus->IsEnabled());
		}
		else
		{
			//Change the selection to this row rather than keep other rows selected unrelated to the ticked/unticked item
			ChangeTheSelectionToThisRow(TestStatus);
			TestStatus->SetEnabled( !TestStatus->IsEnabled() );
		}
	}
	else
	{
		TestStatus->SetEnabled( !TestStatus->IsEnabled() );
	}
}


bool SAutomationWindow::HandleItemCheckBoxIsEnabled( ) const
{
	return IsAutomationControllerIdle();
}


bool SAutomationWindow::HandleMainContentIsEnabled() const
{
	return (SessionManager->GetSelectedInstances().Num() > 0);
}

#if WITH_EDITOR
// React to asset registry finishing updating.
// We only want to do this if there are no tests already listed, otherwise this fires every time you save a map for example.
void SAutomationWindow::OnAssetRegistryFileLoadProgress(const IAssetRegistry::FFileLoadProgressUpdateData& ProgressUpdateData)
{
	if ( ProgressUpdateData.NumAssetsProcessedByAssetRegistry == ProgressUpdateData.NumTotalAssets && IsAutomationControllerIdle() && AutomationController->GetReports().Num() == 0 )
	{
		ListTests();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
