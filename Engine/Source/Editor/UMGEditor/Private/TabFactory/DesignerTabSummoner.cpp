// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TabFactory/DesignerTabSummoner.h"
#include "Designer/SDesignerView.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "UMGStyle.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName FDesignerTabSummoner::TabID(TEXT("SlatePreview"));

FDesignerTabSummoner::FDesignerTabSummoner(TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
{
	TabLabel = LOCTEXT("DesignerTabLabel", "Designer");
	TabIcon = FSlateIcon(FUMGStyle::GetStyleSetName(), "Designer.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SlatePreview_ViewMenu_Desc", "Designer");
	ViewMenuTooltip = LOCTEXT("SlatePreview_ViewMenu_ToolTip", "Show the Designer");
}

TSharedRef<SWidget> FDesignerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return 
		SNew(SScissorRectBox)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SDesignerView, BlueprintEditor.Pin())
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Designer")))
			]
		];
}

#undef LOCTEXT_NAMESPACE 
