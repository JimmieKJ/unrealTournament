// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

/** Niagara Editor public interface */
class INiagaraEffectEditor : public FAssetEditorToolkit
{

public:
	virtual class UNiagaraEffect *GetEffect() const = 0;
	// ...

};


