// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorModeManager.h"

//////////////////////////////////////////////////////////////////////////
// FAssetEditorModeManager

class UNREALED_API FAssetEditorModeManager : public FEditorModeTools
{
public:
	FAssetEditorModeManager();
	virtual ~FAssetEditorModeManager();

	// FEditorModeTools interface
	virtual class USelection* GetSelectedActors() const override;
	virtual class USelection* GetSelectedObjects() const override;
	virtual class USelection* GetSelectedComponents() const override;
	virtual UWorld* GetWorld() const override;
	// End of FEditorModeTools interface

	void SetPreviewScene(class FPreviewScene* NewPreviewScene);
protected:
	class USelection* ActorSet;
	class USelection* ObjectSet;
	class USelection* ComponentSet;
	class FPreviewScene* PreviewScene;
};
