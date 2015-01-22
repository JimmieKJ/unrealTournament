// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditorPrivatePCH.h"

#include "CurveTableEditor.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"
#include "Engine/CurveTable.h"
 
#define LOCTEXT_NAMESPACE "CurveTableEditor"

const FName FCurveTableEditor::CurveTableTabId( TEXT( "CurveTableEditor_CurveTable" ) );

void FCurveTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_CurveTableEditor", "Curve Table Editor"));

	TabManager->RegisterTabSpawner( CurveTableTabId, FOnSpawnTab::CreateSP(this, &FCurveTableEditor::SpawnTab_CurveTable) )
		.SetDisplayName( LOCTEXT("CurveTableTab", "Curve Table") )
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );
}

void FCurveTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( CurveTableTabId );
}

FCurveTableEditor::~FCurveTableEditor()
{

}


void FCurveTableEditor::InitCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCurveTable* Table )
{
	const TSharedRef< FTabManager::FLayout > StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_CurveTableEditor_Layout")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab( CurveTableTabId, ETabState::OpenedTab )
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FCurveTableEditorModule::CurveTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );
	
	FCurveTableEditorModule& CurveTableEditorModule = FModuleManager::LoadModuleChecked<FCurveTableEditorModule>( "CurveTableEditor" );
	AddMenuExtender(CurveTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( CurveTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

FName FCurveTableEditor::GetToolkitFName() const
{
	return FName("CurveTableEditor");
}

FText FCurveTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "CurveTable Editor" );
}


FString FCurveTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "CurveTable ").ToString();
}


FLinearColor FCurveTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


TSharedRef<SUniformGridPanel> FCurveTableEditor::CreateGridPanel() const
{
	TSharedRef<SUniformGridPanel> GridPanel = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f));

	UCurveTable* Table = Cast<UCurveTable>(GetEditingObject());
	if (Table != NULL)
	{
		int32 RowIdx = 0;
		for ( auto RowIt = Table->RowMap.CreateConstIterator(); RowIt; ++RowIt )
		{
			const FName RowName = RowIt.Key();
			const FRichCurve* RowCurve = RowIt.Value();

			// Make sure this row is valid
			if ( RowCurve )
			{
				FLinearColor RowColor = (RowIdx % 2 == 0) ? FLinearColor::Gray : FLinearColor::Black;
				int32 ColumnIdx = 0;

				// Row name
				GridPanel->AddSlot(ColumnIdx++, RowIdx)
				[
					SNew(SBorder)
					.Padding(1)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.BorderBackgroundColor(RowColor)
					[
						SNew(STextBlock) .Text(FText::FromName(RowName))
					]
				];

				// Row Values
				for (auto ValueIt(RowCurve->GetKeyIterator()); ValueIt; ++ValueIt)
				{
					GridPanel->AddSlot(ColumnIdx++, RowIdx)
					[
						SNew(SBorder)
						.Padding(1)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(RowColor)
						[
							SNew(STextBlock).Text(FText::AsNumber(ValueIt->Value))
						]
					];
				}

				RowIdx++;
			}
		}
	}

	return GridPanel;
}


TSharedRef<SDockTab> FCurveTableEditor::SpawnTab_CurveTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == CurveTableTabId );

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("CurveTableEditor.Tabs.Properties") )
		.Label( LOCTEXT("CurveTableTitle", "Curve Table") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SBox)
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					[
						CreateGridPanel()
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
