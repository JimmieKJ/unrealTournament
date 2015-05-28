// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "IDocumentation.h"
#include "WidgetBlueprintEditorToolbar.h"

#include "WidgetBlueprintEditor.h"
#include "SModeWidget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprintApplicationModes.h"

#define LOCTEXT_NAMESPACE "UMG"

//////////////////////////////////////////////////////////////////////////
// SBlueprintModeSeparator

class SBlueprintModeSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SBlueprintModeSeparator) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArg)
	{
		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("BlueprintEditor.PipelineSeparator"))
			.Padding(0.0f)
			);
	}

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Height = 20.0f;
		const float Thickness = 16.0f;
		return FVector2D(Thickness, Height);
	}
	// End of SWidget interface
};

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintEditorToolbar

FWidgetBlueprintEditorToolbar::FWidgetBlueprintEditorToolbar(TSharedPtr<FWidgetBlueprintEditor>& InWidgetEditor)
	: WidgetEditor(InWidgetEditor)
{
}

void FWidgetBlueprintEditorToolbar::AddWidgetBlueprintEditorModesToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPtr = WidgetEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FWidgetBlueprintEditorToolbar::FillWidgetBlueprintEditorModesToolbar));
}

void FWidgetBlueprintEditorToolbar::FillWidgetBlueprintEditorModesToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPtr = WidgetEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	if( !BlueprintObj ||
		(!FBlueprintEditorUtils::IsLevelScriptBlueprint(BlueprintObj) 
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(BlueprintObj)
		&& !BlueprintObj->bIsNewlyCreated)
		)
	{
		TAttribute<FName> GetActiveMode(BlueprintEditorPtr.ToSharedRef(), &FBlueprintEditor::GetCurrentMode);
		FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(BlueprintEditorPtr.ToSharedRef(), &FBlueprintEditor::SetCurrentMode);

		// Left side padding
		BlueprintEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));

		BlueprintEditorPtr->AddToolbarWidget(
			SNew(SModeWidget, FWidgetBlueprintApplicationModes::GetLocalizedMode(FWidgetBlueprintApplicationModes::DesignerMode), FWidgetBlueprintApplicationModes::DesignerMode)
			.OnGetActiveMode(GetActiveMode)
			.OnSetActiveMode(SetActiveMode)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("DesignerModeButtonTooltip", "Switch to Blueprint Designer Mode"),
				NULL,
				TEXT("Shared/Editors/BlueprintEditor"),
				TEXT("DesignerMode")))
			.IconImage(FEditorStyle::GetBrush("UMGEditor.SwitchToDesigner"))
			.SmallIconImage(FEditorStyle::GetBrush("UMGEditor.SwitchToDesigner.Small"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("DesignerMode")))
		);

		BlueprintEditorPtr->AddToolbarWidget(SNew(SBlueprintModeSeparator));

		BlueprintEditorPtr->AddToolbarWidget(
			SNew(SModeWidget, FWidgetBlueprintApplicationModes::GetLocalizedMode(FWidgetBlueprintApplicationModes::GraphMode), FWidgetBlueprintApplicationModes::GraphMode)
			.OnGetActiveMode(GetActiveMode)
			.OnSetActiveMode(SetActiveMode)
			.CanBeSelected(BlueprintEditorPtr.Get(), &FBlueprintEditor::IsEditingSingleBlueprint)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("GraphModeButtonTooltip", "Switch to Graph Editing Mode"),
				NULL,
				TEXT("Shared/Editors/BlueprintEditor"),
				TEXT("GraphMode")))
			.ToolTipText(LOCTEXT("GraphModeButtonTooltip", "Switch to Graph Editing Mode"))
			.IconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode"))
			.SmallIconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode.Small"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("GraphMode")))
		);
		
		// Right side padding
		BlueprintEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
	}
}

#undef LOCTEXT_NAMESPACE
