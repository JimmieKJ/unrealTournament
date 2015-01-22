// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"

class UMovieScene;

/**
 * Sequencer public interface
 */
class ISequencerAssetEditor : public FAssetEditorToolkit
{

public:
	virtual TSharedRef<ISequencer> GetSequencerInterface() const = 0;
};
