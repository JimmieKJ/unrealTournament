// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "SoundCueEditorModule.h"
#include "SoundCueEditorUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "GraphEditor.h"
#include "Sound/SoundCue.h"

bool FSoundCueEditorUtilities::CanPasteNodes(const class UEdGraph* Graph)
{
	bool bCanPaste = false;
	TSharedPtr<class ISoundCueEditor> SoundCueEditor = GetISoundCueEditorForObject(Graph);
	if(SoundCueEditor.IsValid())
	{
		bCanPaste = SoundCueEditor->CanPasteNodes();
	}
	return bCanPaste;
}

void FSoundCueEditorUtilities::PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location)
{
	TSharedPtr<class ISoundCueEditor> SoundCueEditor = GetISoundCueEditorForObject(Graph);
	if(SoundCueEditor.IsValid())
	{
		SoundCueEditor->PasteNodesHere(Location);
	}
}

void FSoundCueEditorUtilities::CreateWaveContainers(TArray<USoundWave*>& SelectedWaves, USoundCue* SoundCue, TArray<USoundNode*>& OutPlayers, FVector2D Location)
{
	const int32 NodeSpacing = 70;
	
	Location.Y -= static_cast<float>((SelectedWaves.Num() - 1) * NodeSpacing) / 2.f;

	for(int32 WaveIndex = 0; WaveIndex < SelectedWaves.Num(); WaveIndex++)
	{
		USoundWave* NewWave = SelectedWaves[WaveIndex];
		if(NewWave)
		{
			USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();

			WavePlayer->SetSoundWave(NewWave);

			WavePlayer->GraphNode->NodePosX = Location.X - WavePlayer->GetGraphNode()->EstimateNodeWidth();
			WavePlayer->GraphNode->NodePosY = Location.Y + (NodeSpacing * WaveIndex);

			OutPlayers.Add(WavePlayer);
		}
	}
}

bool FSoundCueEditorUtilities::GetBoundsForSelectedNodes(const UEdGraph* Graph, class FSlateRect& Rect, float Padding)
{
	TSharedPtr<class ISoundCueEditor> SoundCueEditor = GetISoundCueEditorForObject(Graph);
	if(SoundCueEditor.IsValid())
	{
		return SoundCueEditor->GetBoundsForSelectedNodes(Rect, Padding);
	}
	return false;
}

int32 FSoundCueEditorUtilities::GetNumberOfSelectedNodes(const UEdGraph* Graph)
{
	TSharedPtr<class ISoundCueEditor> SoundCueEditor = GetISoundCueEditorForObject(Graph);
	if(SoundCueEditor.IsValid())
	{
		return SoundCueEditor->GetNumberOfSelectedNodes();
	}
	return 0;
}

TSet<UObject*> FSoundCueEditorUtilities::GetSelectedNodes(const UEdGraph* Graph)
{
	TSharedPtr<class ISoundCueEditor> SoundCueEditor = GetISoundCueEditorForObject(Graph);
	if(SoundCueEditor.IsValid())
	{
		return SoundCueEditor->GetSelectedNodes();
	}
	return TSet<UObject*>();
}

TSharedPtr<class ISoundCueEditor> FSoundCueEditorUtilities::GetISoundCueEditorForObject(const UObject* ObjectToFocusOn)
{
	check(ObjectToFocusOn);

	// Find the associated SoundCue
	USoundCue* SoundCue = Cast<const USoundCueGraph>(ObjectToFocusOn)->GetSoundCue();
	
	TSharedPtr<ISoundCueEditor> SoundCueEditor;
	if (SoundCue != NULL)
	{
		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(SoundCue);
		if (FoundAssetEditor.IsValid())
		{
			SoundCueEditor = StaticCastSharedPtr<ISoundCueEditor>(FoundAssetEditor);
		}
	}
	return SoundCueEditor;
}
