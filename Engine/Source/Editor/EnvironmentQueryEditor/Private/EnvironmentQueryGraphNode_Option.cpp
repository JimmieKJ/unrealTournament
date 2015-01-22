// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu_EnvironmentQuery.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/EnvQueryOption.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeGraphNode"

UEnvironmentQueryGraphNode_Option::UEnvironmentQueryGraphNode_Option(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEnvironmentQueryGraphNode_Option::AllocateDefaultPins()
{
	UEdGraphPin* Inputs = CreatePin(EGPD_Input, TEXT("Transition"), TEXT(""), NULL, false, false, TEXT("Out"));
}

void UEnvironmentQueryGraphNode_Option::PostPlacedNewNode()
{
	if (EnvQueryNodeClass != NULL)
	{
		UEnvQuery* Query = Cast<UEnvQuery>(GetEnvironmentQueryGraph()->GetOuter());
		UEnvQueryOption* QueryOption = ConstructObject<UEnvQueryOption>(UEnvQueryOption::StaticClass(), Query);
		QueryOption->Generator = ConstructObject<UEnvQueryGenerator>(EnvQueryNodeClass, Query);
		QueryOption->Generator->UpdateGeneratorVersion();
		
		QueryOption->SetFlags(RF_Transactional);
		QueryOption->Generator->SetFlags(RF_Transactional);

		NodeInstance = QueryOption;
	}
}

void UEnvironmentQueryGraphNode_Option::ResetNodeOwner()
{
	Super::ResetNodeOwner();

	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(NodeInstance);
	if (OptionInstance && OptionInstance->Generator)
	{
		UEnvQuery* Query = Cast<UEnvQuery>(GetEnvironmentQueryGraph()->GetOuter());
		OptionInstance->Generator->Rename(NULL, Query, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UEnvironmentQueryGraphNode_Option::PrepareForCopying()
{
	Super::PrepareForCopying();

	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(NodeInstance);
	if (OptionInstance && OptionInstance->Generator)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		OptionInstance->Generator->Rename(NULL, this, REN_DontCreateRedirectors | REN_DoNotDirty );
	}
}

FText UEnvironmentQueryGraphNode_Option::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(NodeInstance);
	return OptionInstance ? OptionInstance->GetDescriptionTitle() : FText::GetEmpty();
}

FText UEnvironmentQueryGraphNode_Option::GetDescription() const
{
	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(NodeInstance);
	return OptionInstance ? OptionInstance->GetDescriptionDetails() : FText::GetEmpty();
}

void UEnvironmentQueryGraphNode_Option::AddSubNode(UEnvironmentQueryGraphNode_Test* NodeTemplate, class UEdGraph* ParentGraph)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
	ParentGraph->Modify();
	Modify();

	NodeTemplate->SetFlags(RF_Transactional);

	// set outer to be the graph so it doesn't go away
	NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
	NodeTemplate->ParentNode = this;
	Tests.Add(NodeTemplate);

	NodeTemplate->CreateNewGuid();
	NodeTemplate->PostPlacedNewNode();
	NodeTemplate->AllocateDefaultPins();
	NodeTemplate->AutowireNewNode(NULL);

	NodeTemplate->NodePosX = 0;
	NodeTemplate->NodePosY = 0;

	ParentGraph->NotifyGraphChanged();

	UEnvironmentQueryGraph* MyGraph = Cast<UEnvironmentQueryGraph>(ParentGraph);
	if (MyGraph)
	{
		MyGraph->UpdateAsset();
	}
}

void UEnvironmentQueryGraphNode_Option::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Context.MenuBuilder->AddSubMenu(
		LOCTEXT("AddTest", "Add Test..." ),
		LOCTEXT("AddTestTooltip", "Adds new test to generator" ),
		FNewMenuDelegate::CreateUObject( this, &UEnvironmentQueryGraphNode_Option::CreateAddTestSubMenu,(UEdGraph*)Context.Graph) 
		);
}

void UEnvironmentQueryGraphNode_Option::CreateAddTestSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const
{
	TSharedRef<SGraphEditorActionMenu_EnvironmentQuery> Menu =	
		SNew(SGraphEditorActionMenu_EnvironmentQuery)
		.GraphObj( Graph )
		.GraphNode((UEnvironmentQueryGraphNode_Option*)this)
		.AutoExpandActionMenu(true);

	MenuBuilder.AddWidget(Menu,FText(),true);
}

void UEnvironmentQueryGraphNode_Option::CalculateWeights()
{
	float MaxWeight = -1.0f;
	for (int32 i = 0; i < Tests.Num(); i++)
	{
		if (Tests[i] == NULL || !Tests[i]->bTestEnabled)
		{
			continue;
		}

		UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(Tests[i]->NodeInstance);
		if (TestInstance && !TestInstance->Weight.IsNamedParam())
		{
			MaxWeight = FMath::Max(MaxWeight, FMath::Abs(TestInstance->Weight.Value));
		}
	}

	if (MaxWeight <= 0.0f)
	{
		MaxWeight = 1.0f;
	}

	for (int32 i = 0; i < Tests.Num(); i++)
	{
		if (Tests[i] == NULL)
		{
			continue;
		}
		
		UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(Tests[i]->NodeInstance);
		const bool bHasNamed = TestInstance && Tests[i]->bTestEnabled && TestInstance->Weight.IsNamedParam();
		const float NewWeight = (TestInstance && Tests[i]->bTestEnabled) ?
			(TestInstance->Weight.IsNamedParam() ? 1.0f : FMath::Clamp(FMath::Abs(TestInstance->Weight.Value) / MaxWeight, 0.0f, 1.0f)) : 
			-1.0f;

		Tests[i]->SetDisplayedWeight(NewWeight, bHasNamed);
	}
}

#undef LOCTEXT_NAMESPACE
