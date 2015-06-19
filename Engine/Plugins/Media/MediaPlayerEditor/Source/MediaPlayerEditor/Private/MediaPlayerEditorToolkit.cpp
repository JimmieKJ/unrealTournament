// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"
#include "Factories.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "FMediaPlayerEditorToolkit"

DEFINE_LOG_CATEGORY_STATIC(LogMediaPlayerEditor, Log, All);


/* Local constants
 *****************************************************************************/

static const FName DetailsTabId("Details");
static const FName MediaPlayerEditorAppIdentifier("MediaPlayerEditorApp");
static const FName ViewerTabId("Viewer");


/* FMediaPlayerEditorToolkit structors
 *****************************************************************************/

FMediaPlayerEditorToolkit::FMediaPlayerEditorToolkit( const TSharedRef<ISlateStyle>& InStyle )
	: MediaPlayer(nullptr)
	, Style(InStyle)
{ }


FMediaPlayerEditorToolkit::~FMediaPlayerEditorToolkit()
{
	FReimportManager::Instance()->OnPreReimport().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo(this);
}


/* FMediaPlayerEditorToolkit interface
 *****************************************************************************/

void FMediaPlayerEditorToolkit::Initialize( UMediaPlayer* InMediaPlayer, const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost )
{
	MediaPlayer = InMediaPlayer;

	// Support undo/redo
	MediaPlayer->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	BindCommands();

	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_MediaPlayerEditor_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.66f)
						->Split
						(
							FTabManager::NewStack()
								->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.1f)
								
						)
						->Split
						(
							FTabManager::NewStack()
								->AddTab(ViewerTabId, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.9f)
						)
				)
				->Split
				(
					FTabManager::NewStack()
						->AddTab(DetailsTabId, ETabState::OpenedTab)
						->SetSizeCoefficient(0.33f)
				)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	FAssetEditorToolkit::InitAssetEditor(InMode, InToolkitHost, MediaPlayerEditorAppIdentifier, Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InMediaPlayer);
	
//	IMediaPlayerEditorModule* MediaPlayerEditorModule = &FModuleManager::LoadModuleChecked<IMediaPlayerEditorModule>("MediaPlayerEditor");
//	AddMenuExtender(MediaPlayerEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolBar();
	RegenerateMenusAndToolbars();
}


/* FAssetEditorToolkit interface
 *****************************************************************************/

FString FMediaPlayerEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT("Engine/Content/Types/MediaAssets/Properties/Interface"));
}


void FMediaPlayerEditorToolkit::RegisterTabSpawners( const TSharedRef<class FTabManager>& TabManager )
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MediaPlayerEditor", "Media Player Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	TabManager->RegisterTabSpawner( ViewerTabId, FOnSpawnTab::CreateSP( this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, ViewerTabId ) )
		.SetDisplayName( LOCTEXT( "PlayerTabName", "Player" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon( FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports" ) );

	TabManager->RegisterTabSpawner( DetailsTabId, FOnSpawnTab::CreateSP( this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, DetailsTabId ) )
		.SetDisplayName( LOCTEXT( "DetailsTabName", "Details" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon( FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details" ) );
}


void FMediaPlayerEditorToolkit::UnregisterTabSpawners( const TSharedRef<class FTabManager>& TabManager )
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner(ViewerTabId);
	TabManager->UnregisterTabSpawner(DetailsTabId);
}


/* IToolkit interface
 *****************************************************************************/

FText FMediaPlayerEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Media Asset Editor");
}


FName FMediaPlayerEditorToolkit::GetToolkitFName() const
{
	return FName("MediaPlayerEditor");
}


FLinearColor FMediaPlayerEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}


FString FMediaPlayerEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MediaPlayer ").ToString();
}


/* FGCObject interface
 *****************************************************************************/

void FMediaPlayerEditorToolkit::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(MediaPlayer);
}


/* FEditorUndoClient interface
*****************************************************************************/

void FMediaPlayerEditorToolkit::PostUndo( bool bSuccess )
{ }


void FMediaPlayerEditorToolkit::PostRedo( bool bSuccess )
{
	PostUndo(bSuccess);
}


/* FMediaPlayerEditorToolkit implementation
 *****************************************************************************/

void FMediaPlayerEditorToolkit::BindCommands()
{
	const FMediaPlayerEditorCommands& Commands = FMediaPlayerEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.ForwardMedia,
		FExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleForwardMediaActionExecute),
		FCanExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleForwardMediaActionCanExecute));

	ToolkitCommands->MapAction(
		Commands.PauseMedia,
		FExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandlePauseMediaActionExecute),
		FCanExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandlePauseMediaActionCanExecute));

	ToolkitCommands->MapAction(
		Commands.PlayMedia,
		FExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandlePlayMediaActionExecute),
		FCanExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandlePlayMediaActionCanExecute));

	ToolkitCommands->MapAction(
		Commands.ReverseMedia,
		FExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleReverseMediaActionExecute),
		FCanExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleReverseMediaActionCanExecute));

	ToolkitCommands->MapAction(
		Commands.RewindMedia,
		FExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleRewindMediaActionExecute),
		FCanExecuteAction::CreateSP(this, &FMediaPlayerEditorToolkit::HandleRewindMediaActionCanExecute));
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMediaPlayerEditorToolkit::ExtendToolBar()
{
	struct Local
	{
		static void FillToolbar( FToolBarBuilder& ToolbarBuilder, const TSharedRef<FUICommandList> ToolkitCommands )
		{
			ToolbarBuilder.BeginSection("PlaybackControls");
			{
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().RewindMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().ReverseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().PlayMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().PauseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().ForwardMedia);
			}
			ToolbarBuilder.EndSection();
		}
	};


	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, GetToolkitCommands())
	);

	AddToolbarExtender(ToolbarExtender);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


float FMediaPlayerEditorToolkit::GetForwardRate() const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate < 1.0f)
	{
		Rate = 1.0f;
	}

	return 2.0f * Rate;
}


float FMediaPlayerEditorToolkit::GetReverseRate() const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate > -1.0f)
	{
		return -1.0f;
	}

	return 2.0f * Rate;
}


/* FMediaPlayerEditorToolkit callbacks
 *****************************************************************************/

bool FMediaPlayerEditorToolkit::HandleForwardMediaActionCanExecute() const
{
	return MediaPlayer->CanPlay() && MediaPlayer->SupportsRate(GetForwardRate(), false);
}


void FMediaPlayerEditorToolkit::HandleForwardMediaActionExecute()
{
	MediaPlayer->SetRate(GetForwardRate());
}


bool FMediaPlayerEditorToolkit::HandlePauseMediaActionCanExecute() const
{
	return MediaPlayer->CanPause();
}


void FMediaPlayerEditorToolkit::HandlePauseMediaActionExecute()
{
	MediaPlayer->Pause();
}


bool FMediaPlayerEditorToolkit::HandlePlayMediaActionCanExecute() const
{
	return MediaPlayer->CanPlay() && (MediaPlayer->GetRate() != 1.0f);
}


void FMediaPlayerEditorToolkit::HandlePlayMediaActionExecute()
{
	MediaPlayer->Play();
}


bool FMediaPlayerEditorToolkit::HandleReverseMediaActionCanExecute() const
{
	return MediaPlayer->CanPlay() && MediaPlayer->SupportsRate(GetReverseRate(), false);
}


void FMediaPlayerEditorToolkit::HandleReverseMediaActionExecute()
{
	MediaPlayer->SetRate(GetReverseRate());
}


bool FMediaPlayerEditorToolkit::HandleRewindMediaActionCanExecute() const
{
	return MediaPlayer->CanPlay() && MediaPlayer->GetTime() > FTimespan::Zero();
}


void FMediaPlayerEditorToolkit::HandleRewindMediaActionExecute()
{
	MediaPlayer->Rewind();
}


TSharedRef<SDockTab> FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab( const FSpawnTabArgs& Args, FName TabIdentifier )
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (TabIdentifier == DetailsTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorDetails, MediaPlayer, Style);
	}
	else if (TabIdentifier == ViewerTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorViewer, MediaPlayer, Style);
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
