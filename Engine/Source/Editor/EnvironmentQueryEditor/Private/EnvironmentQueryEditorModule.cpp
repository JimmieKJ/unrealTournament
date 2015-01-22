// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"

#include "DetailCustomizations/EnvDirectionCustomization.h"
#include "DetailCustomizations/EnvTraceDataCustomization.h"
#include "DetailCustomizations/EnvQueryParamSetupCustomization.h"
#include "DetailCustomizations/EnvQueryTestDetails.h"

#include "ModuleManager.h"
#include "Toolkits/ToolkitManager.h"
#include "SGraphNode_EnvironmentQuery.h"
#include "EdGraphUtilities.h"


IMPLEMENT_MODULE( FEnvironmentQueryEditorModule, EnvironmentQueryEditor );
DEFINE_LOG_CATEGORY(LogEnvironmentQueryEditor);

const FName FEnvironmentQueryEditorModule::EnvironmentQueryEditorAppIdentifier( TEXT( "EnvironmentQueryEditorApp" ) );

class FGraphPanelNodeFactory_EnvironmentQuery : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UEnvironmentQueryGraphNode* EnvQueryNode = Cast<UEnvironmentQueryGraphNode>(Node))
		{
			return SNew(SGraphNode_EnvironmentQuery, EnvQueryNode);
		}

		return NULL;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_EnvironmentQuery;

void FEnvironmentQueryEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	GraphPanelNodeFactory_EnvironmentQuery = MakeShareable( new FGraphPanelNodeFactory_EnvironmentQuery() );
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_EnvironmentQuery);

	ItemDataAssetTypeActions = MakeShareable(new FAssetTypeActions_EnvironmentQuery);
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(ItemDataAssetTypeActions.ToSharedRef());

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout( "EnvFloatParam", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEnvQueryParamSetupCustomization::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "EnvIntParam", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEnvQueryParamSetupCustomization::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "EnvBoolParam", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEnvQueryParamSetupCustomization::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "EnvDirection", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEnvDirectionCustomization::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "EnvTraceData", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEnvTraceDataCustomization::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( "EnvQueryTest", FOnGetDetailCustomizationInstance::CreateStatic( &FEnvQueryTestDetails::MakeInstance ) );
	PropertyModule.NotifyCustomizationModuleChanged();
}


void FEnvironmentQueryEditorModule::ShutdownModule()
{
	if (!UObjectInitialized())
	{
		return;
	}

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	if ( GraphPanelNodeFactory_EnvironmentQuery.IsValid() )
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory_EnvironmentQuery);
		GraphPanelNodeFactory_EnvironmentQuery.Reset();
	}

	// Unregister the EnvironmentQuery item data asset type actions
	if (ItemDataAssetTypeActions.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(ItemDataAssetTypeActions.ToSharedRef());
		}
		ItemDataAssetTypeActions.Reset();
	}

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout( "EnvFloatParam" );
		PropertyModule.UnregisterCustomPropertyTypeLayout( "EnvIntParam" );
		PropertyModule.UnregisterCustomPropertyTypeLayout( "EnvBoolParam" );
		PropertyModule.UnregisterCustomPropertyTypeLayout( "EnvDirection" );
		PropertyModule.UnregisterCustomPropertyTypeLayout( "EnvTraceData" );

		PropertyModule.UnregisterCustomClassLayout( "EnvQueryTest" );
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}


TSharedRef<IEnvironmentQueryEditor> FEnvironmentQueryEditorModule::CreateEnvironmentQueryEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UEnvQuery* Script )
{
	TSharedRef< FEnvironmentQueryEditor > NewEnvironmentQueryEditor( new FEnvironmentQueryEditor() );
	NewEnvironmentQueryEditor->InitEnvironmentQueryEditor( Mode, InitToolkitHost, Script );
	return NewEnvironmentQueryEditor;
}

