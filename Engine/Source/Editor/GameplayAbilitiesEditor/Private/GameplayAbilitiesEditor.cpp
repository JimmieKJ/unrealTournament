// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemEditorPrivatePCH.h"
#include "GameplayAbilitiesEditor.h"

#include "GameplayAbilityBlueprint.h"
#include "GameplayAbilityGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "FGameplayAbilitiesEditor"


/////////////////////////////////////////////////////
// FGameplayAbilitiesEditor

FGameplayAbilitiesEditor::FGameplayAbilitiesEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FGameplayAbilitiesEditor::~FGameplayAbilitiesEditor()
{
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FGameplayAbilitiesEditor::InitGameplayAbilitiesEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for (auto Blueprint : InBlueprints)
	{
		EnsureGameplayAbilityBlueprintIsUpToDate(Blueprint);
	}
}

void FGameplayAbilitiesEditor::EnsureGameplayAbilityBlueprintIsUpToDate(UBlueprint* Blueprint)
{
#if WITH_EDITORONLY_DATA
	int32 Count = Blueprint->UbergraphPages.Num();
	for (auto Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing Gameplay Ability Blueprints
		if (Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf(UGameplayAbilityGraphSchema::StaticClass()));
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
#endif
}

// FRED_TODO: don't merge this back
// FName FGameplayAbilitiesEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FGameplayAbilitiesEditor::GetToolkitFName() const
{
	return FName("GameplayAbilitiesEditor");
}

FText FGameplayAbilitiesEditor::GetBaseToolkitName() const
{
	return LOCTEXT("GameplayAbilitiesEditorAppLabel", "Gameplay Abilities Editor");
}

FText FGameplayAbilitiesEditor::GetToolkitName() const
{
	const auto EditingObjects = GetEditingObjects();

	check(EditingObjects.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjects[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("GameplayAbilitiesEditorAppLabel", "{ObjectName}{DirtyState}"), Args);
}

FText FGameplayAbilitiesEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check (EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FGameplayAbilitiesEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("GameplayAbilitiesEditor");
}

FLinearColor FGameplayAbilitiesEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

UBlueprint* FGameplayAbilitiesEditor::GetBlueprintObj() const
{
	auto EditingObjects = GetEditingObjects();
	for (int32 i = 0; i < EditingObjects.Num(); ++i)
	{
		if (EditingObjects[i]->IsA<UGameplayAbilityBlueprint>()) { return (UBlueprint*)EditingObjects[i]; }
	}
	return NULL;
}

FString FGameplayAbilitiesEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE

