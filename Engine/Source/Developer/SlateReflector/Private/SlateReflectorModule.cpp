// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateReflectorPrivatePCH.h"
#include "ISlateReflectorModule.h"
#include "SDockTab.h"
#include "ModuleManager.h"
#include "SAtlasVisualizer.h"


#define LOCTEXT_NAMESPACE "FSlateReflectorModule"


/**
 * Implements the SlateReflector module.
 */
class FSlateReflectorModule
	: public ISlateReflectorModule
{
public:

	// ISlateReflectorModule interface

	virtual TSharedRef<SWidget> GetWidgetReflector() override
	{
		TSharedPtr<SWidgetReflector> WidgetReflector = WidgetReflectorPtr.Pin();

		if (!WidgetReflector.IsValid())
		{
			WidgetReflector = SNew(SWidgetReflector);
			FSlateApplication::Get().SetWidgetReflector(WidgetReflector.ToSharedRef());
		}

		return WidgetReflector.ToSharedRef();
	}

	virtual TSharedRef<SWidget> GetAtlasVisualizer( ISlateAtlasProvider* InAtlasProvider ) override
	{
		return SNew(SAtlasVisualizer)
			.AtlasProvider(InAtlasProvider);
	}

	virtual TSharedRef<SWidget> GetTextureAtlasVisualizer() override
	{
		ISlateAtlasProvider* const AtlasProvider = FSlateApplication::Get().GetRenderer()->GetTextureAtlasProvider();

		if( AtlasProvider )
		{
			return GetAtlasVisualizer( AtlasProvider );
		}
		else
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoTextureAtlasProvider", "There is no texture atlas provider available for the current renderer."))
				];
		}
	}

	virtual TSharedRef<SWidget> GetFontAtlasVisualizer() override
	{
		ISlateAtlasProvider* const AtlasProvider = FSlateApplication::Get().GetRenderer()->GetFontAtlasProvider();

		if( AtlasProvider )
		{
			return GetAtlasVisualizer( AtlasProvider );
		}
		else
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoFontAtlasProvider", "There is no font atlas provider available for the current renderer."))
				];
		}
	}

	virtual void DisplayWidgetReflector() override
	{
		if( bHasRegisteredTabSpawners )
		{
			FGlobalTabmanager::Get()->InvokeTab(FTabId("WidgetReflector"));
		}
		else if( !WidgetReflectorWindowPtr.IsValid() )
		{
			const TSharedRef<SWindow> ReflectorWindow = SNew(SWindow)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.ClientSize(FVector2D(600,400))
				.Title(LOCTEXT("WidgetReflectorTitle", "Widget Reflector"))
				[
					GetWidgetReflector()
				];
		
			WidgetReflectorWindowPtr = ReflectorWindow;
		
			FSlateApplication::Get().AddWindow( ReflectorWindow );
		}
	}

	virtual void DisplayTextureAtlasVisualizer() override
	{
		if( bHasRegisteredTabSpawners )
		{
			FGlobalTabmanager::Get()->InvokeTab(FTabId("TextureAtlasVisualizer"));
		}
		else if( !TextureAtlasVisualizerWindowPtr.IsValid() )
		{
			const TSharedRef<SWindow> AtlasVisWindow = SNew(SWindow)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.ClientSize(FVector2D(1040, 900))
				.Title(LOCTEXT("TextureAtlasVisualizerTitle", "Texture Atlas Visualizer"))
				[
					GetTextureAtlasVisualizer()
				];
		
			TextureAtlasVisualizerWindowPtr = AtlasVisWindow;
		
			FSlateApplication::Get().AddWindow( AtlasVisWindow );
		}
	}

	virtual void DisplayFontAtlasVisualizer() override
	{
		if( bHasRegisteredTabSpawners )
		{
			FGlobalTabmanager::Get()->InvokeTab(FTabId("FontAtlasVisualizer"));
		}
		else if( !FontAtlasVisualizerWindowPtr.IsValid() )
		{
			const TSharedRef<SWindow> AtlasVisWindow = SNew(SWindow)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.ClientSize(FVector2D(1040, 900))
				.Title(LOCTEXT("FontAtlasVisualizerTitle", "Font Atlas Visualizer"))
				[
					GetFontAtlasVisualizer()
				];
		
			FontAtlasVisualizerWindowPtr = AtlasVisWindow;
		
			FSlateApplication::Get().AddWindow( AtlasVisWindow );
		}
	}

	virtual void RegisterTabSpawner( const TSharedRef<FWorkspaceItem>& WorkspaceGroup ) override
	{
		bHasRegisteredTabSpawners = true;

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("WidgetReflector", FOnSpawnTab::CreateRaw(this, &FSlateReflectorModule::MakeWidgetReflectorTab) )
			.SetDisplayName(LOCTEXT("WidgetReflectorTitle", "Widget Reflector"))
			.SetTooltipText(LOCTEXT("WidgetReflectorTooltipText", "Open the Widget Reflector tab."))
			.SetGroup(WorkspaceGroup)
			.SetIcon(FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "WidgetReflector.TabIcon"));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("TextureAtlasVisualizer", FOnSpawnTab::CreateRaw(this, &FSlateReflectorModule::MakeTextureAtlasVisualizerTab) )
			.SetDisplayName(LOCTEXT("TextureAtlasVisualizerTitle", "Texture Atlas Visualizer"))
			.SetTooltipText(LOCTEXT("TextureAtlasVisualizerTooltipText", "Open the Texture Atlas Visualizer tab."))
			.SetGroup(WorkspaceGroup)
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("FontAtlasVisualizer", FOnSpawnTab::CreateRaw(this, &FSlateReflectorModule::MakeFontAtlasVisualizerTab) )
			.SetDisplayName(LOCTEXT("FontAtlasVisualizerTitle", "Font Atlas Visualizer"))
			.SetTooltipText(LOCTEXT("FontAtlasVisualizerTooltipText", "Open the Font Atlas Visualizer tab."))
			.SetGroup(WorkspaceGroup)
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	}

	virtual void UnregisterTabSpawner() override
	{
		bHasRegisteredTabSpawners = false;

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("WidgetReflector");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("TextureAtlasVisualizer");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("FontAtlasVisualizer");
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		bHasRegisteredTabSpawners = false;
	}

	virtual void ShutdownModule() override
	{
		UnregisterTabSpawner();
	}

private:

	TSharedRef<SDockTab> MakeWidgetReflectorTab( const FSpawnTabArgs& )
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				GetWidgetReflector()
			];
	}

	TSharedRef<SDockTab> MakeTextureAtlasVisualizerTab( const FSpawnTabArgs& )
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				GetTextureAtlasVisualizer()
			];
	}

	TSharedRef<SDockTab> MakeFontAtlasVisualizerTab( const FSpawnTabArgs& )
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				GetFontAtlasVisualizer()
			];
	}

private:

	/** True if the tab spawners have been registered for this module */
	bool bHasRegisteredTabSpawners;

	/** Holds the widget reflector singleton. */
	TWeakPtr<SWidgetReflector> WidgetReflectorPtr;

	/** Holds the widget reflector singleton. */
	TWeakPtr<SWindow> WidgetReflectorWindowPtr;

	/** Holds the texture atlas viewer singleton. */
	TWeakPtr<SWindow> TextureAtlasVisualizerWindowPtr;

	/** Holds the font atlas viewer singleton. */
	TWeakPtr<SWindow> FontAtlasVisualizerWindowPtr;
};


IMPLEMENT_MODULE(FSlateReflectorModule, SlateReflector);


#undef LOCTEXT_NAMESPACE
