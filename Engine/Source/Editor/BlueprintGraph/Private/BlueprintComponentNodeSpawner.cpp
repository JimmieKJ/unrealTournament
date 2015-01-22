// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "BlueprintComponentNodeSpawner.h"
#include "K2Node_AddComponent.h"
#include "ClassIconFinder.h" // for FindIconNameForClass()
#include "BlueprintNodeTemplateCache.h" // for IsTemplateOuter()
#include "ComponentAssetBroker.h" // for GetComponentsForAsset()/AssignAssetToComponent()
#include "BlueprintActionFilter.h"	// for FBlueprintActionContext

#define LOCTEXT_NAMESPACE "BlueprintComponenetNodeSpawner"

/*******************************************************************************
 * Static UBlueprintComponentNodeSpawner Helpers
 ******************************************************************************/

namespace BlueprintComponentNodeSpawnerImpl
{
	static FText GetDefaultMenuCategory(TSubclassOf<UActorComponent> const ComponentClass);
}

//------------------------------------------------------------------------------
static FText BlueprintComponentNodeSpawnerImpl::GetDefaultMenuCategory(TSubclassOf<UActorComponent> const ComponentClass)
{
	FText ClassGroup;
	TArray<FString> ClassGroupNames;
	ComponentClass->GetClassGroupNames(ClassGroupNames);

	static FText const DefaultClassGroup(LOCTEXT("DefaultClassGroup", "Common"));
	// 'Common' takes priority over other class groups
	if (ClassGroupNames.Contains(DefaultClassGroup.ToString()) || (ClassGroupNames.Num() == 0))
	{
		ClassGroup = DefaultClassGroup;
	}
	else
	{
		ClassGroup = FText::FromString(ClassGroupNames[0]);
	}
	return FText::Format(LOCTEXT("ComponentCategory", "Add Component|{0}"), ClassGroup);
}

/*******************************************************************************
 * UBlueprintComponentNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintComponentNodeSpawner* UBlueprintComponentNodeSpawner::Create(TSubclassOf<UActorComponent> const ComponentClass, UObject* Outer/* = nullptr*/)
{
	check(ComponentClass != nullptr);

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintComponentNodeSpawner* NodeSpawner = NewObject<UBlueprintComponentNodeSpawner>(Outer);
	NodeSpawner->ComponentClass = ComponentClass;
	NodeSpawner->NodeClass      = UK2Node_AddComponent::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	FText const ComponentTypeName = FText::FromName(ComponentClass->GetFName());
	MenuSignature.MenuName = FText::Format(LOCTEXT("AddComponentMenuName", "Add {0}"), ComponentTypeName);
	MenuSignature.Category = BlueprintComponentNodeSpawnerImpl::GetDefaultMenuCategory(ComponentClass);
	MenuSignature.Tooltip  = FText::Format(LOCTEXT("AddComponentTooltip", "Spawn a {0}"), ComponentTypeName);
	MenuSignature.Keywords = ComponentClass->GetMetaData(FBlueprintMetadata::MD_FunctionKeywords);
	// add at least one character, so that PrimeDefaultMenuSignature() doesn't 
	// attempt to query the template node
	MenuSignature.Keywords.AppendChar(TEXT(' '));
	MenuSignature.IconName = FClassIconFinder::FindIconNameForClass(ComponentClass);

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintComponentNodeSpawner::UBlueprintComponentNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintComponentNodeSpawner::GetSpawnerSignature() const
{
	FBlueprintNodeSignature SpawnerSignature(NodeClass);
	SpawnerSignature.AddSubObject(ComponentClass);
	return SpawnerSignature;
}

//------------------------------------------------------------------------------
// Evolved from a combination of FK2ActionMenuBuilder::CreateAddComponentAction()
// and FEdGraphSchemaAction_K2AddComponent::PerformAction().
UEdGraphNode* UBlueprintComponentNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	check(ComponentClass != nullptr);
	
	auto PostSpawnLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FCustomizeNodeDelegate UserDelegate)
	{		
		UK2Node_AddComponent* AddCompNode = CastChecked<UK2Node_AddComponent>(NewNode);
		UBlueprint* Blueprint = AddCompNode->GetBlueprint();
		
		UFunction* AddComponentFunc = FindFieldChecked<UFunction>(AActor::StaticClass(), UK2Node_AddComponent::GetAddComponentFunctionName());
		AddCompNode->FunctionReference.SetFromField<UFunction>(AddComponentFunc, FBlueprintEditorUtils::IsActorBased(Blueprint));

		UserDelegate.ExecuteIfBound(NewNode, bIsTemplateNode);
	};

	FCustomizeNodeDelegate PostSpawnDelegate = FCustomizeNodeDelegate::CreateStatic(PostSpawnLambda, CustomizeNodeDelegate);
	// let SpawnNode() allocate default pins (so we can modify them)
	UK2Node_AddComponent* NewNode = Super::SpawnNode<UK2Node_AddComponent>(NodeClass, ParentGraph, FBindingSet(), Location, PostSpawnDelegate);

	// set the return type to be the type of the template
	UEdGraphPin* ReturnPin = NewNode->GetReturnValuePin();
	if (ReturnPin != nullptr)
	{
		ReturnPin->PinType.PinSubCategoryObject = *ComponentClass;
	}

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	if (!bIsTemplateNode)
	{
		UBlueprint* Blueprint = NewNode->GetBlueprint();
		UActorComponent* ComponentTemplate = ConstructObject<UActorComponent>(ComponentClass, Blueprint->GeneratedClass);
		ComponentTemplate->SetFlags(RF_ArchetypeObject);

		Blueprint->ComponentTemplates.Add(ComponentTemplate);

		// set the name of the template as the default for the TemplateName param
		UEdGraphPin* TemplateNamePin = NewNode->GetTemplateNamePinChecked();
		if (TemplateNamePin != nullptr)
		{
			TemplateNamePin->DefaultValue = ComponentTemplate->GetName();
		}
	}

	// apply bindings, after we've setup the template pin
	ApplyBindings(NewNode, Bindings);

	return NewNode;
}

//------------------------------------------------------------------------------
FBlueprintActionUiSpec UBlueprintComponentNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	if (Bindings.Num() > 0)
	{
		FText AssetName;
		if (UObject* AssetBinding = Bindings.CreateConstIterator()->Get())
		{
			AssetName = FText::FromName(AssetBinding->GetFName());
		}

		FText const ComponentTypeName = FText::FromName(ComponentClass->GetFName());
		MenuSignature.MenuName = FText::Format(LOCTEXT("AddBoundComponentMenuName", "Add {0} (as {1})"), AssetName, ComponentTypeName);
		MenuSignature.Tooltip  = FText::Format(LOCTEXT("AddBoundComponentTooltip", "Spawn {0} using {1}"), ComponentTypeName, AssetName);
	}
	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

//------------------------------------------------------------------------------
bool UBlueprintComponentNodeSpawner::IsBindingCompatible(UObject const* BindingCandidate) const
{
	bool bCanBindWith = false;
	if (BindingCandidate->IsAsset())
	{
		TArray< TSubclassOf<UActorComponent> > ComponentClasses = FComponentAssetBrokerage::GetComponentsForAsset(BindingCandidate);
		bCanBindWith = ComponentClasses.Contains(ComponentClass);
	}
	return bCanBindWith;
}

//------------------------------------------------------------------------------
bool UBlueprintComponentNodeSpawner::BindToNode(UEdGraphNode* Node, UObject* Binding) const
{
	bool bSuccessfulBinding = false;
	UK2Node_AddComponent* AddCompNode = CastChecked<UK2Node_AddComponent>(Node);

	UActorComponent* ComponentTemplate = AddCompNode->GetTemplateFromNode();
	if (ComponentTemplate != nullptr)
	{
		bSuccessfulBinding = FComponentAssetBrokerage::AssignAssetToComponent(ComponentTemplate, Binding);
		AddCompNode->ReconstructNode();
	}
	return bSuccessfulBinding;
}

//------------------------------------------------------------------------------
TSubclassOf<UActorComponent> UBlueprintComponentNodeSpawner::GetComponentClass() const
{
	return ComponentClass;
}

#undef LOCTEXT_NAMESPACE
