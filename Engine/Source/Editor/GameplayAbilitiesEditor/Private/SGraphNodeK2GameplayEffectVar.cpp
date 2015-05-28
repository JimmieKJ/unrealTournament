// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AbilitySystemEditorPrivatePCH.h"

#include "SNodePanel.h"
#include "SGraphNode.h"
#include "../../Engine/Source/Editor/GraphEditor/Private/KismetNodes/SGraphNodeK2Base.h"
#include "SGraphNodeK2GameplayEffectVar.h"
#include "K2Node_GameplayEffectVariable.h"
#include "GameplayEffectTypes.h"
#include "ClassIconFinder.h"
#include "IDocumentation.h"
#include "AssetEditorManager.h"
#include "GameplayEffect.h"

#define LOCTEXT_NAMESPACE "GraphNodeGameplayEffectVar"

void SGraphNodeK2GameplayEffectVar::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

FSlateColor SGraphNodeK2GameplayEffectVar::GetVariableColor() const
{
	return GraphNode->GetNodeTitleColor();
}

void SGraphNodeK2GameplayEffectVar::UpdateGraphNode()
{
	SGraphNodeK2Var::UpdateGraphNode();

	UK2Node_GameplayEffectVariable* GameplayEffectNode = Cast<UK2Node_GameplayEffectVariable>(GraphNode);
	UGameplayEffect* GameplayEffect = GameplayEffectNode ? GameplayEffectNode->GameplayEffect : NULL;

	FText DurationText = GameplayEffect ? GameplayEffect->DurationMagnitude.GetValueForEditorDisplay() : FText::GetEmpty();
	FText PeriodText = FText::Format(LOCTEXT("PeriodTimeInSeconds", "{0} s"), FText::AsNumber((GameplayEffect) ? GameplayEffect->Period.Value : 0.f));

	RightNodeBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DurationLabel", "Duration:"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(DurationText)
			]
		];

	RightNodeBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PeriodLabel", "Period:"))
			]
			+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(PeriodText)
				]
		];

	RightNodeBox->AddSlot()
		.FillHeight(0.3f)
		[
			SNew(SSpacer)
		];

	if (GameplayEffect && (GameplayEffect->ChanceToApplyToTarget.Value < 1.f))
	{
		FText ApplyToTargetText = FText::AsNumber(GameplayEffect->ChanceToApplyToTarget.Value);

		RightNodeBox->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ChanceToApplyToTargetLabel", "Chance to Apply to Target:"))
				]
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(ApplyToTargetText)
					]
			];

		RightNodeBox->AddSlot()
			.FillHeight(0.3f)
			[
				SNew(SSpacer)
			];
	}

	const int32 ModifierCount = GameplayEffect ? GameplayEffect->Modifiers.Num() : 0;
	for (int32 Idx = 0; Idx < ModifierCount; ++Idx)
	{
		RightNodeBox->AddSlot()
			.FillHeight(0.3f)
			[
				SNew(SSpacer)
			];

		RightNodeBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString(GameplayEffect->Modifiers[Idx].Attribute.GetName()))
			];

		FText ModString = FText::Format(LOCTEXT("ModOpAndValue", "{0}: {1}"), FText::FromString(EGameplayModOpToString(GameplayEffect->Modifiers[Idx].ModifierOp)), FText::AsNumber(GameplayEffect->Modifiers[Idx].Magnitude.Value));

		RightNodeBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(ModString)
			];
	}

	TSharedRef<SWidget> OpenGameplayEffectButton = OpenGameplayEffectButtonContent(
		LOCTEXT("FormatTextNodeOpenGameplayEffectButton", "Open Gameplay Effect"),
		LOCTEXT("FormatTextNodeOpenGameplayEffectButton_Tooltip", "Opens this gameplay effect so that it can be edited."));

	LeftNodeBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(10, 10, 10, 4)
		[
			OpenGameplayEffectButton
		];
}

TSharedRef<SWidget> SGraphNodeK2GameplayEffectVar::OpenGameplayEffectButtonContent(FText ButtonText, FText ButtonTooltipText, FString DocumentationExcerpt)
{
	TSharedPtr<SWidget> ButtonContent;

	SAssignNew(ButtonContent, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 7, 0)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Browse")))
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(ButtonText)
			.ColorAndOpacity(FLinearColor::White)
		];


	TSharedPtr<SToolTip> Tooltip;

	if (!DocumentationExcerpt.IsEmpty())
	{
		Tooltip = IDocumentation::Get()->CreateToolTip(ButtonTooltipText, NULL, GraphNode->GetDocumentationLink(), DocumentationExcerpt);
	}

	TSharedRef<SButton> OpenGameplayEffectButton = SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.OnClicked(this, &SGraphNodeK2GameplayEffectVar::OnOpenGameplayEffect)
		.ToolTipText(ButtonTooltipText)
		.ToolTip(Tooltip)
		.Visibility(this, &SGraphNodeK2GameplayEffectVar::IsOpenGameplayEffectButtonVisible)
		[
			ButtonContent.ToSharedRef()
		];

	OpenGameplayEffectButton->SetCursor(EMouseCursor::Hand);

	return OpenGameplayEffectButton;
}

FReply SGraphNodeK2GameplayEffectVar::OnOpenGameplayEffect()
{
	UK2Node_GameplayEffectVariable* GameplayEffectNode = Cast<UK2Node_GameplayEffectVariable>(GraphNode);

	if (GameplayEffectNode && GameplayEffectNode->GameplayEffect)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(GameplayEffectNode->GameplayEffect);
	}

	return FReply::Handled(); 
}

EVisibility SGraphNodeK2GameplayEffectVar::IsOpenGameplayEffectButtonVisible() const
{
	UK2Node_GameplayEffectVariable* GameplayEffectNode = Cast<UK2Node_GameplayEffectVariable>(GraphNode);
	if (GameplayEffectNode)
	{
		// set this to be hidden if there is no gameplay effect associated with this node
	}
	return EVisibility::Visible;
}

const FSlateBrush* SGraphNodeK2GameplayEffectVar::GetShadowBrush(bool bSelected) const
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bCompactMode = K2Node->ShouldDrawCompact();

	if (bCompactMode)
	{
		return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.VarNode.ShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.VarNode.Shadow"));
	}
	return SGraphNodeK2Base::GetShadowBrush(bSelected);
}

#undef LOCTEXT_NAMESPACE
