// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "LevelEditor.h"
#include "LevelEditorMenu.h"
#include "LevelEditorActions.h"
#include "SLevelEditor.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "MRUFavoritesList.h"
#include "GlobalEditorCommonCommands.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "GenericCommands.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "LevelEditorMenu"

class SFavouriteMenuEntry : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SFavouriteMenuEntry ){}
		SLATE_ARGUMENT( FText, LabelOverride )
		/** Called to when an entry is clicked */
		SLATE_EVENT( FExecuteAction, OnOpenClickedDelegate )
		/** Called to when the button remove an entry is clicked */
		SLATE_EVENT( FExecuteAction, OnRemoveClickedDelegate )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs )
	{
		const FText AssetDisplayName = InArgs._LabelOverride;
		OnOpenClickedDelegate = InArgs._OnOpenClickedDelegate;
		OnRemoveClickedDelegate = InArgs._OnRemoveClickedDelegate;

		const FText OpenDisplayName = FText::Format( LOCTEXT("OpenLevelToolTipFmt", "Open level: {0}"), AssetDisplayName );
		FSlateFontInfo MenuEntryFont = FEditorStyle::GetFontStyle( "Menu.Label.Font" );

		ChildSlot
		[
			SNew(SButton)
			.ButtonStyle( FEditorStyle::Get(), "Menu.Button" )
			.ForegroundColor( TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SFavouriteMenuEntry::InvertOnHover ) ) )
			.Text( AssetDisplayName )
			.ToolTipText( OpenDisplayName )
			.OnClicked(this, &SFavouriteMenuEntry::OnOpen)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.ContentPadding( FMargin(4.0, 0.0) )
			[
				SNew( SOverlay )

				+SOverlay::Slot()
				.Padding( FMargin( 12.0, 0.0 ) )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew( STextBlock )
					.Font( MenuEntryFont )
					.ColorAndOpacity( TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SFavouriteMenuEntry::InvertOnHover ) ) )
					.Text( AssetDisplayName )
				]

				+SOverlay::Slot()
				.Padding( FMargin( 0.0, 0.0 ) )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ContentPadding( FMargin(4.0, 0.0) )
					.ButtonStyle( FEditorStyle::Get(), "Docking.Tab.CloseButton" )
					.ToolTipText( FText::Format( LOCTEXT("RemoveFavoriteToolTipFmt", "Remove {0} from Favorites"), AssetDisplayName ) )
					.OnClicked(this, &SFavouriteMenuEntry::OnRemove)
				]
			]
		];
	}

	/**
	 * Calls the open-a-level delegate, dismisses the menu
	 */
	FReply OnOpen()
	{
		OnOpenClickedDelegate.ExecuteIfBound();
		// Dismiss the entire menu stack when a button is clicked to close all sub-menus
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	/**
	 * Calls the remove-a-favourite delegate, dismisses the menu
	 */
	FReply OnRemove()
	{
		OnRemoveClickedDelegate.ExecuteIfBound();
		// Dismiss the entire menu stack when a button is clicked to close all sub-menus
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

private:
	FSlateColor InvertOnHover() const
	{
		if ( this->IsHovered() )
		{
			return FLinearColor::Black;
		}
		else
		{
			return FSlateColor::UseForeground();
		}
	}

	FExecuteAction OnOpenClickedDelegate;
	FExecuteAction OnRemoveClickedDelegate;
};

TSharedRef< SWidget > FLevelEditorMenu::MakeLevelEditorMenu( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor )
{
	struct Local
	{
		static void FillFileLoadAndSaveItems( FMenuBuilder& MenuBuilder )
		{
			// New Level
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().NewLevel );

			// Open Level
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().OpenLevel );

			// Open Asset
			//@TODO: Doesn't work when summoned from here: MenuBuilder.AddMenuEntry( FGlobalEditorCommonCommands::Get().SummonOpenAssetDialog );

			// Save
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().Save );
	
			// Save As
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().SaveAs );

			// Save Levels
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().SaveAllLevels );
		}

		static void FillFileRecentAndFavoriteFileItems( FMenuBuilder& MenuBuilder )
		{
			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
			const FMainMRUFavoritesList& RecentsAndFavorites = *MainFrameModule.GetMRUFavoritesList();

			// Import/Export
			{
				MenuBuilder.BeginSection("FileActors", LOCTEXT("ImportExportHeading", "Actors") );
				{
					// Import Into Level
					MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ImportScene);

					// Export All
					MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ExportAll );

					// Export Selected
					MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ExportSelected );
				}
				MenuBuilder.EndSection();
			}

			// Favorite files
			{
				static const int32 FavoritesToDisplayInMainMenu = 5;
				const int32 NumFavorites = RecentsAndFavorites.GetNumFavorites();

				MenuBuilder.BeginSection("FileFavorites", LOCTEXT("FavoriteFilesHeading", "Favorites") );

				if( NumFavorites > 0 )
				{
					// Display the most recent favorites on the main menu
					const int32 MainMenuFavorites = FMath::Min( NumFavorites, FavoritesToDisplayInMainMenu );
					for (int32 CurFavoriteIndex = 0; CurFavoriteIndex < MainMenuFavorites; ++CurFavoriteIndex)
					{
						const FString CurFavorite = FPaths::GetBaseFilename(RecentsAndFavorites.GetFavoritesItem( CurFavoriteIndex ));
						const bool bNoIndent = false;

						MenuBuilder.AddWidget(
							SNew(SFavouriteMenuEntry)
								.LabelOverride( FText::FromString(FPaths::GetBaseFilename(CurFavorite)) )
								.OnOpenClickedDelegate(FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenFavoriteFile, CurFavoriteIndex))
								.OnRemoveClickedDelegate(FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::RemoveFavorite, CurFavoriteIndex)), 
								FText(), bNoIndent );
					}

					// Any remaining favorites go into a submenu
					if (NumFavorites > FavoritesToDisplayInMainMenu)
					{
						struct Local
						{
							static void MakeFavoriteLevelMenu(FMenuBuilder& InMenuBuilder)
							{
								const FMainMRUFavoritesList& MRUFavorites = *FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").GetMRUFavoritesList();
								const int32 NumMRUFavorites = MRUFavorites.GetNumFavorites();

								for (int32 CurFavoriteIndex = FavoritesToDisplayInMainMenu; CurFavoriteIndex < NumMRUFavorites; ++CurFavoriteIndex)
								{
									const FString CurFavorite = FPaths::GetBaseFilename(MRUFavorites.GetFavoritesItem(CurFavoriteIndex));
									const bool bNoIndent = true;

									InMenuBuilder.AddWidget(
										SNew(SFavouriteMenuEntry)
										.LabelOverride(FText::FromString(FPaths::GetBaseFilename(CurFavorite)))
										.OnOpenClickedDelegate(FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenFavoriteFile, CurFavoriteIndex))
										.OnRemoveClickedDelegate(FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::RemoveFavorite, CurFavoriteIndex)),
										FText(), bNoIndent);
								}
							}
						};

						MenuBuilder.AddSubMenu(
							LOCTEXT("MoreFavoriteFilesSubMenu", "More Favorites"),
							LOCTEXT("MoreFavoriteFilesTooltip", "Select other favorite levels"),
							FNewMenuDelegate::CreateStatic(&Local::MakeFavoriteLevelMenu),
							false,
							FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.FavoriteLevels")
							);
					}
				}

				MenuBuilder.EndSection();

				// Add a button to add/remove the currently loaded map as a favorite
				if( FLevelEditorActionCallbacks::ToggleFavorite_CanExecute() )
				{
					struct Local
					{
						static FText GetToggleFavoriteLabelText()
						{
							if( FLevelEditorActionCallbacks::ToggleFavorite_CanExecute() )
							{
								const FText LevelName = FText::FromString( FPackageName::GetShortName( GWorld->GetOutermost()->GetFName() ) );
								if( !FLevelEditorActionCallbacks::ToggleFavorite_IsChecked() )
								{
									return FText::Format( LOCTEXT("ToggleFavorite_Add", "Add {0} to Favorites"), LevelName );
								}
							}
							return LOCTEXT("ToggleFavorite", "Toggle Favorite");
						}
					};

					const FString LevelName = FPackageName::GetShortName( GWorld->GetOutermost()->GetFName() );
					if( !FLevelEditorActionCallbacks::ToggleFavorite_IsChecked() )
					{
						TAttribute<FText> ToggleFavoriteLabel;
						ToggleFavoriteLabel.BindStatic( &Local::GetToggleFavoriteLabelText );

						MenuBuilder.BeginSection("LevelEditorToggleFavorite");
						{
							MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ToggleFavorite, NAME_None, ToggleFavoriteLabel );
						}
						MenuBuilder.EndSection();
					}
				}
			}

			// Recent files
			{
				struct FRecentLevelMenu
				{
					static void MakeRecentLevelMenu( FMenuBuilder& InMenuBuilder )
					{
						const FMainMRUFavoritesList& MRUFavorites = *FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" ).GetMRUFavoritesList();
						const int32 NumRecents = MRUFavorites.GetNumItems();

						const int32 AllowedRecents = FMath::Min( NumRecents, FLevelEditorCommands::Get().OpenRecentFileCommands.Num() );
						for ( int32 CurRecentIndex = 0; CurRecentIndex < AllowedRecents; ++CurRecentIndex )
						{
							TSharedPtr< FUICommandInfo > OpenRecentFile = FLevelEditorCommands::Get().OpenRecentFileCommands[ CurRecentIndex ];

							const FString CurRecent = MRUFavorites.GetMRUItem( CurRecentIndex );

							const FText ToolTip = FText::Format( LOCTEXT( "RecentLevelToolTip", "Opens recent level: {0}" ), FText::FromString( CurRecent ) );
							const FText Label = FText::FromString( FPaths::GetBaseFilename( CurRecent ) );

							InMenuBuilder.AddMenuEntry( OpenRecentFile, NAME_None, Label, ToolTip );
						}
					}
				};

				const int32 NumRecents = RecentsAndFavorites.GetNumItems();

				MenuBuilder.BeginSection( "FileRecentLevels" );

				if ( NumRecents > 0 )
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("RecentLevelsSubMenu", "Recent Levels"),
						LOCTEXT("RecentLevelsSubMenu_ToolTip", "Select a level to load"),
						FNewMenuDelegate::CreateStatic( &FRecentLevelMenu::MakeRecentLevelMenu ), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.RecentLevels") );
				}

				MenuBuilder.EndSection();
			}
		}

		static void FillEditMenu( FMenuBuilder& MenuBuilder )
		{
			// Edit Actor
			{
				MenuBuilder.BeginSection("EditMain", LOCTEXT("MainHeading", "Edit") );
				{		
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Cut );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Copy );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Paste );

					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Duplicate );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Delete );
				}
				MenuBuilder.EndSection();
			}
		}

		static void ExtendHelpMenu( FMenuBuilder& MenuBuilder )
		{
			MenuBuilder.BeginSection("HelpBrowse", NSLOCTEXT("MainHelpMenu", "Browse", "Browse"));
			{
				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseDocumentation );

				if(IDocumentation::Get()->CanOpenAPIHome())
				{
					MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseAPIReference );
				}
				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseCVars );

				MenuBuilder.AddMenuSeparator();

				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseViewportControls );
			}
			MenuBuilder.EndSection();

			
		}
	};

	TSharedRef< FExtender > Extender( new FExtender() );
	
	// Add level loading and saving menu items
	Extender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::First,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillFileLoadAndSaveItems ) );

	// Add recent / favorites
	Extender->AddMenuExtension(
		"FileRecentFiles",
		EExtensionHook::Before,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillFileRecentAndFavoriteFileItems ) );

	// Extend the Edit menu
	Extender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillEditMenu ) );

	// Extend the Help menu
	Extender->AddMenuExtension(
		"HelpOnline",
		EExtensionHook::Before,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::ExtendHelpMenu ) );

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( Extender );
	TSharedPtr<FExtender> Extenders = LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	TSharedRef< SWidget > MenuBarWidget = MainFrameModule.MakeMainTabMenu( LevelEditor->GetTabManager(), Extenders.ToSharedRef() );

	return MenuBarWidget;
}

TSharedRef< SWidget > FLevelEditorMenu::MakeNotificationBar( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor )
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	const TSharedPtr<FExtender> NotificationBarExtenders = LevelEditorModule.GetNotificationBarExtensibilityManager()->GetAllExtenders();

	FToolBarBuilder NotificationBarBuilder( CommandList, FMultiBoxCustomization::None, NotificationBarExtenders, Orient_Horizontal );
	NotificationBarBuilder.SetStyle(&FEditorStyle::Get(), "NotificationBar");
	{
		NotificationBarBuilder.BeginSection("Start");
		NotificationBarBuilder.EndSection();
	}

	return NotificationBarBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
