// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginBrowserPrivatePCH.h"
#include "PluginStyle.h"
#include "SPluginBrowser.h"
#include "SPluginTileList.h"
#include "SPluginCategoryTree.h"
#include "SSearchBox.h"
#include "PluginBrowserModule.h"
#include "DirectoryWatcherModule.h"

#define LOCTEXT_NAMESPACE "PluginsEditor"

SPluginBrowser::~SPluginBrowser()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	for(auto& Pair: WatchDirectories)
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
	}
}

void SPluginBrowser::Construct( const FArguments& Args )
{
	// Get the root directories which contain plugins
	TArray<FString> WatchDirectoryNames;
	WatchDirectoryNames.Add(FPaths::EnginePluginsDir());
	if(FApp::HasGameName())
	{
		WatchDirectoryNames.Add(FPaths::GamePluginsDir());
	}

	// Add watchers for any change events on those directories
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	for(const FString& WatchDirectoryName: WatchDirectoryNames)
	{
		FDelegateHandle Handle;
		if(DirectoryWatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(WatchDirectoryName, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &SPluginBrowser::OnPluginDirectoryChanged), Handle, true))
		{
			WatchDirectories.Add(WatchDirectoryName, Handle);
		}
	}

	RegisterActiveTimer (0.f, FWidgetActiveTimerDelegate::CreateSP (this, &SPluginBrowser::TriggerBreadcrumbRefresh));

	struct Local
	{
		static void PluginToStringArray( const IPlugin* Plugin, OUT TArray< FString >& StringArray )
		{
			// NOTE: Only the friendly name is searchable for now.  We don't display the actual plugin name in the UI.
			StringArray.Add( Plugin->GetDescriptor().FriendlyName );
		}
	};

	// Setup text filtering
	PluginTextFilter = MakeShareable( new FPluginTextFilter( FPluginTextFilter::FItemToStringArray::CreateStatic( &Local::PluginToStringArray ) ) );

	const float PaddingAmount = 2.0f;


	PluginCategories = SNew( SPluginCategoryTree, SharedThis( this ) );


	ChildSlot
	[ 
		SNew( SHorizontalBox )

		+SHorizontalBox::Slot()
		.Padding( PaddingAmount )
		.AutoWidth()	// @todo plugedit: Probably want a splitter here (w/ saved layout)
		[
			PluginCategories.ToSharedRef()
		]

		+SHorizontalBox::Slot()
		.Padding( PaddingAmount )
		[
			SNew( SVerticalBox )

			+SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, PaddingAmount))
			.AutoHeight()
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.Padding( PaddingAmount )
				[
					SAssignNew( BreadcrumbTrail, SBreadcrumbTrail<TSharedPtr<FPluginCategory>> )
					.DelimiterImage( FPluginStyle::Get()->GetBrush( "Plugins.BreadcrumbArrow" ) ) 
					.ShowLeadingDelimiter( true )
					.OnCrumbClicked( this, &SPluginBrowser::BreadcrumbTrail_OnCrumbClicked )
				]
	
				+SHorizontalBox::Slot()
				.Padding( PaddingAmount )
				[
					SNew( SSearchBox )
					.OnTextChanged( this, &SPluginBrowser::SearchBox_OnPluginSearchTextChanged )
				]
			]

			+SVerticalBox::Slot()
			[
				SAssignNew( PluginList, SPluginTileList, SharedThis( this ) )
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(PaddingAmount, PaddingAmount, PaddingAmount, 0.0f))
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor::Yellow)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				.Visibility(this, &SPluginBrowser::HandleRestartEditorNoticeVisibility)
				[
					SNew( SHorizontalBox )

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
					[
						SNew(SImage)
						.Image(FPluginStyle::Get()->GetBrush("Plugins.Warning"))
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PluginSettingsRestartNotice", "Unreal Editor must be restarted for the plugin changes to take effect."))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					[
						SNew(SButton)
						.Text(LOCTEXT("PluginSettingsRestartEditor", "Restart Now"))
						.OnClicked(this, &SPluginBrowser::HandleRestartEditorButtonClicked)
					]
				]
			]
		]
	];
}


EVisibility SPluginBrowser::HandleRestartEditorNoticeVisibility() const
{
	return (FPluginBrowserModule::Get().PendingEnablePlugins.Num() > 0)? EVisibility::Visible : EVisibility::Collapsed;
}


FReply SPluginBrowser::HandleRestartEditorButtonClicked() const
{
	const bool bWarn = false;
	FUnrealEdMisc::Get().RestartEditor(bWarn);
	return FReply::Handled();
}


void SPluginBrowser::SearchBox_OnPluginSearchTextChanged( const FText& NewText )
{
	PluginTextFilter->SetRawFilterText( NewText );
}


TSharedPtr< FPluginCategory > SPluginBrowser::GetSelectedCategory() const
{
	return PluginCategories->GetSelectedCategory();
}


void SPluginBrowser::OnCategorySelectionChanged()
{
	if( PluginList.IsValid() )
	{
		PluginList->SetNeedsRefresh();
	}

	// Breadcrumbs will need to be refreshed
	RegisterActiveTimer (0.f, FWidgetActiveTimerDelegate::CreateSP (this, &SPluginBrowser::TriggerBreadcrumbRefresh));
}

void SPluginBrowser::SetNeedsRefresh()
{
	if( PluginList.IsValid() )
	{
		PluginList->SetNeedsRefresh();
	}

	if( PluginCategories.IsValid() )
	{
		PluginCategories->SetNeedsRefresh();
	}

	// Breadcrumbs will need to be refreshed
	RegisterActiveTimer (0.f, FWidgetActiveTimerDelegate::CreateSP (this, &SPluginBrowser::TriggerBreadcrumbRefresh));
}

void SPluginBrowser::OnPluginDirectoryChanged(const TArray<struct FFileChangeData>&)
{
	if(UpdatePluginsTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(UpdatePluginsTimerHandle.ToSharedRef());
	}
	UpdatePluginsTimerHandle = RegisterActiveTimer(2.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SPluginBrowser::UpdatePluginsTimerCallback));
}

EActiveTimerReturnType SPluginBrowser::UpdatePluginsTimerCallback(double InCurrentTime, float InDeltaTime)
{
	IPluginManager::Get().RefreshPluginsList();
	SetNeedsRefresh();
	return EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SPluginBrowser::TriggerBreadcrumbRefresh(double InCurrentTime, float InDeltaTime)
{
	RefreshBreadcrumbTrail();
	return EActiveTimerReturnType::Stop;
}

void SPluginBrowser::RefreshBreadcrumbTrail()
{
	// Update breadcrumb trail
	if( BreadcrumbTrail.IsValid() )
	{
		TSharedPtr<FPluginCategory> SelectedCategory = PluginCategories->GetSelectedCategory();

		// Build up the list of categories, starting at the selected node and working our way backwards.
		TArray<TSharedPtr<FPluginCategory>> CategoryPath;
		if(SelectedCategory.IsValid())
		{
			for(TSharedPtr<FPluginCategory> NextCategory = SelectedCategory; NextCategory.IsValid(); NextCategory = NextCategory->ParentCategory.Pin())
			{
				CategoryPath.Insert( NextCategory, 0 );
			}
		}

		// Fill in the crumbs
		BreadcrumbTrail->ClearCrumbs();
		for(TSharedPtr<FPluginCategory>& Category: CategoryPath)
		{
			BreadcrumbTrail->PushCrumb( Category->DisplayName, Category );
		}
	}
}


void SPluginBrowser::BreadcrumbTrail_OnCrumbClicked( const TSharedPtr<FPluginCategory>& Category )
{
	if( PluginCategories.IsValid() )
	{
		PluginCategories->SelectCategory( Category );
	}
}



#undef LOCTEXT_NAMESPACE