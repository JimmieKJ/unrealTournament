// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetEditorModeManager.h"
#include "PreviewScene.h"
#include "Engine/Selection.h"

//////////////////////////////////////////////////////////////////////////
// FAssetEditorModeManager

FAssetEditorModeManager::FAssetEditorModeManager()
	: PreviewScene(nullptr)
{
	ActorSet = NewObject<USelection>();
	ActorSet->SetFlags(RF_Transactional);
	ActorSet->AddToRoot();

	ObjectSet = NewObject<USelection>();
	ObjectSet->SetFlags(RF_Transactional);
	ObjectSet->AddToRoot();

	ComponentSet = NewObject<USelection>();
	ComponentSet->SetFlags(RF_Transactional);
	ComponentSet->AddToRoot();
}

FAssetEditorModeManager::~FAssetEditorModeManager()
{
	ActorSet->RemoveFromRoot();
	ActorSet = nullptr;
	ObjectSet->RemoveFromRoot();
	ObjectSet = nullptr;
	ComponentSet->RemoveFromRoot();
	ComponentSet = nullptr;
}

USelection* FAssetEditorModeManager::GetSelectedActors() const
{
	return ActorSet;
}

USelection* FAssetEditorModeManager::GetSelectedObjects() const
{
	return ObjectSet;
}

USelection* FAssetEditorModeManager::GetSelectedComponents() const
{
	return ComponentSet;
}

UWorld* FAssetEditorModeManager::GetWorld() const
{
	return (PreviewScene != nullptr) ? PreviewScene->GetWorld() : GEditor->GetEditorWorldContext().World();
}

void FAssetEditorModeManager::SetPreviewScene(class FPreviewScene* NewPreviewScene)
{
	PreviewScene = NewPreviewScene;
}

