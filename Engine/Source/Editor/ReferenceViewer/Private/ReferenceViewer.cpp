// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "ReferenceViewer.h"

#include "EdGraphUtilities.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"
//DEFINE_LOG_CATEGORY(LogReferenceViewer);

class FGraphPanelNodeFactory_ReferenceViewer : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if ( UEdGraphNode_Reference* DependencyNode = Cast<UEdGraphNode_Reference>(Node) )
		{
			return SNew(SReferenceNode, DependencyNode);
		}

		return NULL;
	}
};

class FReferenceViewerModule : public IReferenceViewerModule
{
public:
	FReferenceViewerModule()
		: ReferenceViewerTabId("ReferenceViewer")
	{
		
	}

	virtual void StartupModule() override
	{
		GraphPanelNodeFactory = MakeShareable( new FGraphPanelNodeFactory_ReferenceViewer() );
		FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ReferenceViewerTabId, FOnSpawnTab::CreateRaw( this, &FReferenceViewerModule::SpawnReferenceViewerTab ) )
			.SetDisplayName( LOCTEXT("ReferenceViewerTitle", "Reference Viewer") )
			.SetMenuType( ETabSpawnerMenuType::Hidden );
	}

	virtual void ShutdownModule() override
	{
		if ( GraphPanelNodeFactory.IsValid() )
		{
			FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory);
			GraphPanelNodeFactory.Reset();
		}

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ReferenceViewerTabId);
	}

	virtual void InvokeReferenceViewerTab(const TArray<FName>& GraphRootPackageNames) override
	{
		TSharedRef<SDockTab> NewTab = FGlobalTabmanager::Get()->InvokeTab( ReferenceViewerTabId );
		TSharedRef<SReferenceViewer> ReferenceViewer = StaticCastSharedRef<SReferenceViewer>( NewTab->GetContent() );
		ReferenceViewer->SetGraphRootPackageNames(GraphRootPackageNames);
	}

private:
	TSharedRef<SDockTab> SpawnReferenceViewerTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		NewTab->SetContent( SNew(SReferenceViewer) );

		return NewTab;
	}

private:
	TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory;
	FName ReferenceViewerTabId;
};

IMPLEMENT_MODULE( FReferenceViewerModule, ReferenceViewer )

#undef LOCTEXT_NAMESPACE