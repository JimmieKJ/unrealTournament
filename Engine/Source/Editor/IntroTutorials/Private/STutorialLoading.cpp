// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IntroTutorialsPrivatePCH.h"
#include "STutorialLoading.h"
#include "SThrobber.h"

#define LOCTEXT_NAMESPACE "Tutorials"

void STutorialLoading::Construct(const FArguments& InArgs)
{
	ContextWindow = InArgs._ContextWindow;

	ChildSlot
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SCircularThrobber)
				]
				+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ShadowColorAndOpacity(FLinearColor::Black)
						.ColorAndOpacity(FLinearColor::White)
						.ShadowOffset(FIntPoint(-1, 1))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
						.Text(LOCTEXT("LoadingContentTut", "Loading Tutorial Content"))
					]
			]
		];
}

#undef LOCTEXT_NAMESPACE
