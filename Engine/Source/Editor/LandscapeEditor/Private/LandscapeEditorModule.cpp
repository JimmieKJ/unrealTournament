// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEditorCommands.h"
#include "LandscapeEdMode.h"
#include "Classes/ActorFactoryLandscape.h"


#include "LandscapeEditorDetails.h"
#include "LandscapeEditorDetailCustomizations.h"
#include "LandscapeSplineDetails.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"

#include "Editor/LevelEditor/Public/LevelEditor.h"

#include "Landscape.h"
#include "LandscapeRender.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor"

class FLandscapeEditorModule : public ILandscapeEditorModule
{
public:
	
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override
	{
		FLandscapeEditorCommands::Register();

		// register the editor mode
		FEditorModeRegistry::Get().RegisterMode<FEdModeLandscape>(
			FBuiltinEditorModes::EM_Landscape,
			NSLOCTEXT("EditorModes", "LandscapeMode", "Landscape"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.LandscapeMode", "LevelEditor.LandscapeMode.Small"),
			true,
			300
			);

		// register customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("LandscapeEditorObject",       FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeEditorDetails::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("GizmoImportLayer",     FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLandscapeEditorStructCustomization_FGizmoImportLayer::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("LandscapeImportLayer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::MakeInstance));

		PropertyModule.RegisterCustomClassLayout("LandscapeSplineControlPoint", FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSplineDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("LandscapeSplineSegment",      FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSplineDetails::MakeInstance));

		// add menu extension
		TSharedRef<FUICommandList> CommandList = MakeShareable(new FUICommandList);
		const FLandscapeEditorCommands& LandscapeActions = FLandscapeEditorCommands::Get();
		CommandList->MapAction(LandscapeActions.ViewModeNormal,       FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::Normal),       FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::Normal));
		CommandList->MapAction(LandscapeActions.ViewModeLOD,          FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::LOD),          FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::LOD));
		CommandList->MapAction(LandscapeActions.ViewModeLayerDensity, FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::LayerDensity), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::LayerDensity));
		CommandList->MapAction(LandscapeActions.ViewModeLayerDebug,   FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::DebugLayer),   FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::DebugLayer));
		CommandList->MapAction(LandscapeActions.ViewModeWireframeOnTop,FExecuteAction::CreateStatic(&ChangeLandscapeViewMode, ELandscapeViewMode::WireframeOnTop), FCanExecuteAction(), FIsActionChecked::CreateStatic(&IsLandscapeViewModeSelected, ELandscapeViewMode::WireframeOnTop));

		ViewportMenuExtender = MakeShareable(new FExtender);
		ViewportMenuExtender->AddMenuExtension("LevelViewportLandscape", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateStatic(&ConstructLandscapeViewportMenu));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(ViewportMenuExtender);

		// add actor factories
		UActorFactoryLandscape* LandscapeActorFactory = NewObject<UActorFactoryLandscape>();
		LandscapeActorFactory->NewActorClass = ALandscape::StaticClass();
		GEditor->ActorFactories.Add(LandscapeActorFactory);

		UActorFactoryLandscape* LandscapeProxyActorFactory = NewObject<UActorFactoryLandscape>();
		LandscapeProxyActorFactory->NewActorClass = ALandscapeProxy::StaticClass();
		GEditor->ActorFactories.Add(LandscapeProxyActorFactory);
	}

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override
	{
		FLandscapeEditorCommands::Unregister();

		// unregister the editor mode
		FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_Landscape);

		// unregister customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("LandscapeEditorObject");
		PropertyModule.UnregisterCustomPropertyTypeLayout("GizmoImportLayer");
		PropertyModule.UnregisterCustomPropertyTypeLayout("LandscapeImportLayer");

		PropertyModule.UnregisterCustomClassLayout("LandscapeSplineControlPoint");
		PropertyModule.UnregisterCustomClassLayout("LandscapeSplineSegment");

		// remove menu extension
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(ViewportMenuExtender);
		ViewportMenuExtender = nullptr;

		// remove actor factories
		// TODO - this crashes on shutdown
		// GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryLandscape>(); });
	}

	static void ConstructLandscapeViewportMenu(FMenuBuilder& MenuBuilder)
	{
		struct Local
		{
			static void BuildLandscapeVisualizersMenu(FMenuBuilder& InMenuBuilder)
			{
				const FLandscapeEditorCommands& LandscapeActions = FLandscapeEditorCommands::Get();

				InMenuBuilder.BeginSection("LandscapeVisualizers", LOCTEXT("LandscapeHeader", "Landscape Visualizers"));
				{
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeNormal, NAME_None, LOCTEXT("LandscapeViewModeNormal", "Normal"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLOD, NAME_None, LOCTEXT("LandscapeViewModeLOD", "LOD"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLayerDensity, NAME_None, LOCTEXT("LandscapeViewModeLayerDensity", "Layer Density"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeLayerDebug, NAME_None, LOCTEXT("LandscapeViewModeLayerDebug", "Layer Debug"));
					InMenuBuilder.AddMenuEntry(LandscapeActions.ViewModeWireframeOnTop, NAME_None, LOCTEXT("LandscapeViewModeWireframeOnTop", "Wireframe on Top"));
				}
				InMenuBuilder.EndSection();
			}
		};
		MenuBuilder.AddSubMenu(LOCTEXT("LandscapeSubMenu", "Visualizers"), LOCTEXT("LandscapeSubMenu_ToolTip", "Select a Landscape visualiser"), FNewMenuDelegate::CreateStatic(&Local::BuildLandscapeVisualizersMenu));
	}

	static void ChangeLandscapeViewMode(ELandscapeViewMode::Type ViewMode)
	{
		GLandscapeViewMode = ViewMode;
	}

	static bool IsLandscapeViewModeSelected(ELandscapeViewMode::Type ViewMode)
	{
		return GLandscapeViewMode == ViewMode;
	}

protected:
	TSharedPtr<FExtender> ViewportMenuExtender;
};

IMPLEMENT_MODULE( FLandscapeEditorModule, LandscapeEditor );

#undef LOCTEXT_NAMESPACE
