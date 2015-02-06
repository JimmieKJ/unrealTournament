// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "Profiler"

SProfilerToolbar::SProfilerToolbar()
{
}

SProfilerToolbar::~SProfilerToolbar()
{
	// Remove ourselves from the profiler manager.
	if( FProfilerManager::Get().IsValid() )
	{
		FProfilerManager::Get()->OnSessionInstancesUpdated().RemoveAll( this );
	}
}

void SProfilerToolbar::Construct( const FArguments& InArgs )
{
	CreateCommands();

	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("File");
			{
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().ProfilerManager_Load );
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().ProfilerManager_Save );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Capture");
			{
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().ToggleDataPreview );
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().ProfilerManager_ToggleLivePreview );
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().ToggleDataCapture );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Profilers");
			{
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().StatsProfiler );
				//ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().MemoryProfiler );
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().FPSChart );
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Options");
			{
				ToolbarBuilder.AddToolBarButton( FProfilerCommands::Get().OpenSettings );
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	FToolBarBuilder ToolbarBuilder( ProfilerCommandList.ToSharedRef(), FMultiBoxCustomization::None );
	Local::FillToolbar( ToolbarBuilder );

	// Create the tool bar!
	ChildSlot
	[
		SNew( SHorizontalBox )

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.FillWidth(1.0)
		.Padding(0.0f)
		[
			SNew( SBorder )
			.Padding(0)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			[
				ToolbarBuilder.MakeWidget()
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(0.0f)
		[
			SAssignNew(Border,SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding( 2.0f )
			.Visibility( this, &SProfilerToolbar::IsInstancesOwnerVisible )
		]
	];

	FProfilerManager::Get()->OnSessionInstancesUpdated().AddSP( this, &SProfilerToolbar::ProfilerManager_SessionsUpdated );
}

void SProfilerToolbar::ShowStats()
{

}

void SProfilerToolbar::ShowMemory()
{

}

void DisplayFPSChart( const TSharedRef<FFPSAnalyzer> InFPSAnalyzer )
{
	static bool HasRegisteredFPSChart = false;
	if ( !HasRegisteredFPSChart )
	{
		TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout( "FPSChart_Layout" )
			->AddArea
			(
			FTabManager::NewArea(720, 360)
			->Split
			(
			FTabManager::NewStack()
			->AddTab("FPSChart", ETabState::ClosedTab)
			)
			);

		FGlobalTabmanager::Get()->RestoreFrom( Layout, TSharedPtr<SWindow>() );
		HasRegisteredFPSChart = true;
	}

	FGlobalTabmanager::Get()->InsertNewDocumentTab
	(
		"FPSChart", FTabManager::ESearchPreference::RequireClosedTab,
		SNew( SDockTab )
		.Label( LOCTEXT("Label_FPSHistogram", "FPS Histogram") )
		.TabRole( ETabRole::DocumentTab )
		[
			SNew( SProfilerFPSChartPanel )
			.FPSAnalyzer( InFPSAnalyzer )
		]
	);
}

void SProfilerToolbar::ShowFPSChart()
{
	for( auto It = FProfilerManager::Get()->GetProfilerInstancesIterator(); It; ++It )
	{
		const FProfilerSessionRef& ProfilerSession = It.Value();
		const FGuid SessionInstanceID = ProfilerSession->GetInstanceID();

		DisplayFPSChart( ProfilerSession->FPSAnalyzer );
	}
}

void SProfilerToolbar::CreateCommands()
{
	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& Commands = FProfilerCommands::Get();

	// Save command
	ProfilerCommandList->MapAction( Commands.ProfilerManager_Save,
		FExecuteAction(),
		FCanExecuteAction::CreateRaw( this, &SProfilerToolbar::IsImplemented )
	);

	// Stats command
	ProfilerCommandList->MapAction( Commands.StatsProfiler,
		FExecuteAction::CreateRaw( this, &SProfilerToolbar::ShowStats ),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw( this, &SProfilerToolbar::IsShowingStats )
	);

	// Memory command
	ProfilerCommandList->MapAction( Commands.MemoryProfiler,
		FExecuteAction::CreateRaw( this, &SProfilerToolbar::ShowMemory ),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw( this, &SProfilerToolbar::IsShowingMemory )
		);

	// FPSChart command
	ProfilerCommandList->MapAction( Commands.FPSChart,
		FExecuteAction::CreateRaw( this, &SProfilerToolbar::ShowFPSChart ),
		FCanExecuteAction()
		);
}

EVisibility SProfilerToolbar::IsInstancesOwnerVisible() const
{
	return FProfilerManager::GetSettings().bSingleInstanceMode ? EVisibility::Collapsed : EVisibility::Visible;
}

void SProfilerToolbar::ProfilerManager_SessionsUpdated()
{
	TSharedRef<SHorizontalBox> SessionsHBox = SNew(SHorizontalBox);

	if( FProfilerManager::Get()->GetProfilerInstancesNum() > 1 )
	{
		AddSessionInstanceItem( SessionsHBox, LOCTEXT("Global", "Global").ToString(), FGuid() );
	}

	for( auto It = FProfilerManager::Get()->GetProfilerInstancesIterator(); It; ++It )
	{
		const FProfilerSessionRef& ProfilerSession = It.Value();
		const FGuid SessionInstanceID = ProfilerSession->GetInstanceID();

		AddSessionInstanceItem( SessionsHBox, ProfilerSession->GetShortName(), SessionInstanceID );
	}

	Border->SetContent( SessionsHBox );
}

void SProfilerToolbar::AddSessionInstanceItem( TSharedRef<SHorizontalBox>& SessionsHBox, const FString& ProfilerSessionName, const FGuid& SessionInstanceID )
{
	SessionsHBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.AutoWidth()
	.Padding(2.0f)
	[
		SNew(SComboButton)	
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)

		.ButtonContent()
		[
			SNew(STextBlock)
			.Text( FText::FromString(ProfilerSessionName) )
			.ShadowOffset( FVector2D(1.0f,1.0f) )
			.ShadowColorAndOpacity( FColorList::SkyBlue )
		]

		.MenuContent()
		[
			BuildProfilerSessionContextMenu( SessionInstanceID )
		]
	];
}

TSharedRef<SWidget> SProfilerToolbar::BuildProfilerSessionContextMenu( const FGuid SessionInstanceID )
{
	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& ProfilerCommands = FProfilerManager::GetCommands();
	const FProfilerActionManager& ProfilerActionManager = FProfilerManager::GetActionManager();

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, ProfilerCommandList );

	MenuBuilder.BeginSection( "Menu_Instance", LOCTEXT("SessionInstance", "Session Instance") );
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection( "Menu_General", LOCTEXT("General", "General") );
	{
		FProfilerMenuBuilder::AddMenuEntry
		( 
			MenuBuilder, 
			ProfilerCommands.ToggleDataPreview, 
			ProfilerActionManager.ToggleDataPreview_Custom(SessionInstanceID) 
		);

		FProfilerMenuBuilder::AddMenuEntry
		( 
			MenuBuilder, 
			ProfilerCommands.ToggleDataCapture, 
			ProfilerActionManager.ToggleDataCapture_Custom(SessionInstanceID) 
		);

		if( SessionInstanceID.IsValid() )
		{
			FProfilerMenuBuilder::AddMenuEntry
			( 
				MenuBuilder, 
				ProfilerCommands.ToggleShowDataGraph, 
				ProfilerActionManager.ToggleShowDataGraph_Custom(SessionInstanceID) 
			);
		}
	}
	MenuBuilder.EndSection();

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

#undef LOCTEXT_NAMESPACE
