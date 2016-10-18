// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeQualityLevel.h"
#include "Sound/SoundNodeSoundClass.h"
#include "Sound/SoundWave.h"
#include "GameFramework/GameUserSettings.h"
#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "UnrealEd.h"
#endif

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

USoundCue::USoundCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeMultiplier = 0.75f;
	PitchMultiplier = 1.0f;
}

#if WITH_EDITOR
void USoundCue::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		CreateGraph();
	}
}


void USoundCue::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USoundCue* This = CastChecked<USoundCue>(InThis);

	Collector.AddReferencedObject(This->SoundCueGraph, This);

	Super::AddReferencedObjects(InThis, Collector);
}
#endif // WITH_EDITOR


void USoundCue::Serialize(FArchive& Ar)
{
	// Always force the duration to be updated when we are saving or cooking
	if (Ar.IsSaving() || Ar.IsCooking())
	{
		Duration = (FirstNode ? FirstNode->GetDuration() : 0.f);
	}

	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		FStripDataFlags StripFlags(Ar);
#if WITH_EDITORONLY_DATA
		if (!StripFlags.IsEditorDataStripped())
		{
			Ar << SoundCueGraph;
		}
#endif
	}
#if WITH_EDITOR
	else
	{
		Ar << SoundCueGraph;
	}
#endif
}

void USoundCue::PostLoad()
{
	Super::PostLoad();

	// Game doesn't care if there are NULL graph nodes
#if WITH_EDITOR
	if (GIsEditor )
	{
		if (SoundCueGraph)
		{
			// Deal with SoundNode types being removed - iterate in reverse as nodes may be removed
			for (int32 idx = SoundCueGraph->Nodes.Num() - 1; idx >= 0; --idx)
			{
				USoundCueGraphNode* Node = Cast<USoundCueGraphNode>(SoundCueGraph->Nodes[idx]);

				if (Node && Node->SoundNode == NULL)
				{
					FBlueprintEditorUtils::RemoveNode(NULL, Node, true);
				}
			}
		}
		else
		{
			// we should have a soundcuegraph unless we are contained in a package which is missing editor only data
			check( GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly) );
		}

		// Always load all sound waves in the editor
		for (USoundNode* SoundNode : AllNodes)
		{
			if (USoundNodeAssetReferencer* AssetReferencerNode = Cast<USoundNodeAssetReferencer>(SoundNode))
			{
				AssetReferencerNode->LoadAsset();
			}
		}
	}
	else
#endif
	if (GEngine)
	{
		EvaluateNodes(false);
	}
	else
	{
		OnPostEngineInitHandle = UEngine::OnPostEngineInit.AddUObject(this, &USoundCue::OnPostEngineInit);
	}
}

void USoundCue::OnPostEngineInit()
{
	UEngine::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	OnPostEngineInitHandle.Reset();

	EvaluateNodes(true);
}

void USoundCue::EvaluateNodes(bool bAddToRoot)
{
	TArray<USoundNode*> NodesToEvaluate;
	NodesToEvaluate.Push(FirstNode);

	while (NodesToEvaluate.Num() > 0)
	{
		if (USoundNode* SoundNode = NodesToEvaluate.Pop(false))
		{
			if (USoundNodeAssetReferencer* AssetReferencerNode = Cast<USoundNodeAssetReferencer>(SoundNode))
			{
				AssetReferencerNode->LoadAsset(bAddToRoot);
			}
			else if (USoundNodeQualityLevel* QualityLevelNode = Cast<USoundNodeQualityLevel>(SoundNode))
			{
				// Only pick the node connected for current quality, currently don't support changing audio quality on the fly
				static const int32 CachedQualityLevel = GEngine->GetGameUserSettings()->GetAudioQualityLevel();
				if (CachedQualityLevel < QualityLevelNode->ChildNodes.Num())
				{
					NodesToEvaluate.Add(QualityLevelNode->ChildNodes[CachedQualityLevel]);
				}
			}
			else
			{
				NodesToEvaluate.Append(SoundNode->ChildNodes);
			}
		}
	}
}


#if WITH_EDITOR

void USoundCue::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (It->Sound == this && It->bIsActive)
			{
				It->Stop();
				It->Play();
			}
		}
	}
}
#endif

template<typename T> ENGINE_API void USoundCue::RecursiveFindNode( USoundNode* Node, TArray<T*>& OutNodes )
{
	if( Node )
	{
		// Record the node if it is the desired type
		if( Node->IsA( T::StaticClass() ) )
		{
			OutNodes.AddUnique( static_cast<T*>( Node ) );
		}

		// Recurse.
		const int32 MaxChildNodes = Node->GetMaxChildNodes();
		for( int32 ChildIndex = 0 ; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes ; ++ChildIndex )
		{
			RecursiveFindNode<T>( Node->ChildNodes[ ChildIndex ], OutNodes );
		}
	}
}
// Explicitly instantiate the USoundNodeMixer template so it can be found by ContentAuditCommandlet
template ENGINE_API void USoundCue::RecursiveFindNode<USoundNodeMixer>(USoundNode* Node, TArray<USoundNodeMixer*>& OutNodes);

void USoundCue::RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes )
{
	RecursiveFindNode<USoundNodeAttenuation>( Node, OutNodes );
}

void USoundCue::RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*> &OutNodes )
{
	if( Node )
	{
		OutNodes.AddUnique( Node );

		// Recurse.
		const int32 MaxChildNodes = Node->GetMaxChildNodes();
		for( int32 ChildIndex = 0 ; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes ; ++ChildIndex )
		{
			RecursiveFindAllNodes( Node->ChildNodes[ ChildIndex ], OutNodes );
		}
	}
}

bool USoundCue::RecursiveFindPathToNode(USoundNode* CurrentNode, const UPTRINT CurrentHash, const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const
{
	OutPath.Push(CurrentNode);
	if (CurrentHash == NodeHashToFind)
	{
		return true;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentNode->ChildNodes.Num(); ++ChildIndex)
	{
		USoundNode* ChildNode = CurrentNode->ChildNodes[ChildIndex];
		if (ChildNode)
		{
			if (RecursiveFindPathToNode(ChildNode, USoundNode::GetNodeWaveInstanceHash(CurrentHash, ChildNode, ChildIndex), NodeHashToFind, OutPath))
			{
				return true;
			}
		}
	}

	OutPath.Pop();
	return false;
}

bool USoundCue::FindPathToNode(const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const
{
	return RecursiveFindPathToNode(FirstNode, (UPTRINT)FirstNode, NodeHashToFind, OutPath);
}


FString USoundCue::GetDesc()
{
	FString Description = TEXT( "" );

	// Display duration
	const float CueDuration = GetDuration();
	if( CueDuration < INDEFINITELY_LOOPING_DURATION )
	{
		Description = FString::Printf( TEXT( "%3.2fs" ), CueDuration );
	}
	else
	{
		Description = TEXT( "Forever" );
	}

	// Display group
	Description += TEXT( " [" );
	Description += *GetSoundClass()->GetName();
	Description += TEXT( "]" );

	return Description;
}

SIZE_T USoundCue::GetResourceSize(EResourceSizeMode::Type Mode)
{
	if( Mode == EResourceSizeMode::Exclusive )
	{
		return( 0 );
	}
	else
	{
		// Sum up the size of referenced waves
		int32 ResourceSize = 0;

		TArray<USoundNodeWavePlayer*> WavePlayers;
		RecursiveFindNode<USoundNodeWavePlayer>( FirstNode, WavePlayers );

		for( int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex )
		{
			USoundWave* SoundWave = WavePlayers[WaveIndex]->GetSoundWave();
			if (SoundWave)
			{
				ResourceSize += SoundWave->GetResourceSize(Mode);
			}
		}

		return( ResourceSize );
	}
}

int32 USoundCue::GetResourceSizeForFormat(FName Format)
{
	TArray<USoundNodeWavePlayer*> WavePlayers;
	RecursiveFindNode<USoundNodeWavePlayer>(FirstNode, WavePlayers);

	int32 ResourceSize = 0;
	for (int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex)
	{
		USoundWave* SoundWave = WavePlayers[WaveIndex]->GetSoundWave();
		if (SoundWave)
		{
			ResourceSize += SoundWave->GetResourceSizeForFormat(Format);
		}
	}

	return ResourceSize;
}

float USoundCue::GetMaxAudibleDistance()
{
	if (FirstNode)
	{
		// Always recalc the max audible distance when in the editor as it could change
		if ((GIsEditor && !FApp::IsGame()) || (MaxAudibleDistance < SMALL_NUMBER))
		{
			// initialize AudibleDistance
			TArray<USoundNode*> SoundNodes;

			FirstNode->GetAllNodes( SoundNodes );
			for( int32 i = 0; i < SoundNodes.Num(); ++i )
			{
				MaxAudibleDistance = SoundNodes[ i ]->MaxAudibleDistance( MaxAudibleDistance );
			}
			if( MaxAudibleDistance < SMALL_NUMBER )
			{
				MaxAudibleDistance = WORLD_MAX;
			}
		}
	}
	else
	{
		MaxAudibleDistance = 0.f;
	}

	return MaxAudibleDistance;
}

float USoundCue::GetDuration()
{
	// Always recalc the duration when in the editor as it could change
	if( GIsEditor || ( Duration < SMALL_NUMBER ) )
	{
		if( FirstNode )
		{
			Duration = FirstNode->GetDuration();
		}
	}

	return Duration;
}

bool USoundCue::ShouldApplyInteriorVolumes() const
{
	if (Super::ShouldApplyInteriorVolumes())
	{
		return true;
	}

	// TODO: Consider caching this so we only reevaluate in editor
	TArray<UObject*> Children;
	GetObjectsWithOuter(this, Children);

	for (UObject* Child : Children)
	{
		if (USoundNodeSoundClass* SoundClassNode = Cast<USoundNodeSoundClass>(Child))
		{
			if (SoundClassNode->SoundClassOverride && SoundClassNode->SoundClassOverride->Properties.bApplyAmbientVolumes)
			{
				return true;
			}
		}
	}

	return false;
}

bool USoundCue::IsPlayable() const
{
	return FirstNode != nullptr;
}

void USoundCue::Parse( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	if (FirstNode)
	{
		FirstNode->ParseNodes(AudioDevice,(UPTRINT)FirstNode,ActiveSound,ParseParams,WaveInstances);
	}
}

float USoundCue::GetVolumeMultiplier()
{
	return VolumeMultiplier;
}

float USoundCue::GetPitchMultiplier()
{
	return PitchMultiplier;
}

const FAttenuationSettings* USoundCue::GetAttenuationSettingsToApply() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}
	return Super::GetAttenuationSettingsToApply();
}

#if WITH_EDITOR
USoundCueGraph* USoundCue::GetGraph()
{ 
	return CastChecked<USoundCueGraph>(SoundCueGraph);
}

void USoundCue::CreateGraph()
{
	if (SoundCueGraph == nullptr)
	{
		SoundCueGraph = CastChecked<USoundCueGraph>(FBlueprintEditorUtils::CreateNewGraph(this, NAME_None, USoundCueGraph::StaticClass(), USoundCueGraphSchema::StaticClass()));
		SoundCueGraph->bAllowDeletion = false;

		// Give the schema a chance to fill out any required nodes (like the results node)
		const UEdGraphSchema* Schema = SoundCueGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*SoundCueGraph);
	}
}

void USoundCue::ClearGraph()
{
	if (SoundCueGraph)
	{
		SoundCueGraph->Nodes.Empty();
		// Give the schema a chance to fill out any required nodes (like the results node)
		const UEdGraphSchema* Schema = SoundCueGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*SoundCueGraph);
	}
}

void USoundCue::SetupSoundNode(USoundNode* InSoundNode, bool bSelectNewNode/* = true*/)
{
	// Create the graph node
	check(InSoundNode->GraphNode == NULL);

	FGraphNodeCreator<USoundCueGraphNode> NodeCreator(*SoundCueGraph);
	USoundCueGraphNode* GraphNode = NodeCreator.CreateNode(bSelectNewNode);
	GraphNode->SetSoundNode(InSoundNode);
	NodeCreator.Finalize();
}

void USoundCue::LinkGraphNodesFromSoundNodes()
{
	// Use SoundNodes to make GraphNode Connections
	if (FirstNode != NULL)
	{
		// Find the root node
		TArray<USoundCueGraphNode_Root*> RootNodeList;
		SoundCueGraph->GetNodesOfClass<USoundCueGraphNode_Root>(/*out*/ RootNodeList);
		check(RootNodeList.Num() == 1);

		RootNodeList[0]->Pins[0]->BreakAllPinLinks();
		RootNodeList[0]->Pins[0]->MakeLinkTo(FirstNode->GetGraphNode()->GetOutputPin());
	}

	for(TArray<USoundNode*>::TConstIterator It(AllNodes); It; ++It)
	{
		USoundNode* SoundNode = *It;
		if (SoundNode)
		{
			TArray<UEdGraphPin*> InputPins;
			SoundNode->GetGraphNode()->GetInputPins(/*out*/ InputPins);
			check(InputPins.Num() == SoundNode->ChildNodes.Num());
			for (int32 ChildIndex = 0; ChildIndex < SoundNode->ChildNodes.Num(); ChildIndex++)
			{
				USoundNode* ChildNode = SoundNode->ChildNodes[ChildIndex];
				if (ChildNode)
				{
					InputPins[ChildIndex]->BreakAllPinLinks();
					InputPins[ChildIndex]->MakeLinkTo(ChildNode->GetGraphNode()->GetOutputPin());
				}
			}
		}
	}
}

void USoundCue::CompileSoundNodesFromGraphNodes()
{
	// Use GraphNodes to make SoundNode Connections
	TArray<USoundNode*> ChildNodes;
	TArray<UEdGraphPin*> InputPins;

	for (int32 NodeIndex = 0; NodeIndex < SoundCueGraph->Nodes.Num(); ++NodeIndex)
	{
		USoundCueGraphNode* GraphNode = Cast<USoundCueGraphNode>(SoundCueGraph->Nodes[NodeIndex]);
		if (GraphNode && GraphNode->SoundNode)
		{
			// Set ChildNodes of each SoundNode
			
			GraphNode->GetInputPins(InputPins);
			ChildNodes.Empty();
			for (int32 PinIndex = 0; PinIndex < InputPins.Num(); ++PinIndex)
			{
				UEdGraphPin* ChildPin = InputPins[PinIndex];

				if (ChildPin->LinkedTo.Num() > 0)
				{
					USoundCueGraphNode* GraphChildNode = CastChecked<USoundCueGraphNode>(ChildPin->LinkedTo[0]->GetOwningNode());
					ChildNodes.Add(GraphChildNode->SoundNode);
				}
				else
				{
					ChildNodes.AddZeroed();
				}
			}

			GraphNode->SoundNode->SetFlags(RF_Transactional);
			GraphNode->SoundNode->Modify();
			GraphNode->SoundNode->SetChildNodes(ChildNodes);
			GraphNode->SoundNode->PostEditChange();
		}
		else
		{
			// Set FirstNode based on RootNode connection
			USoundCueGraphNode_Root* RootNode = Cast<USoundCueGraphNode_Root>(SoundCueGraph->Nodes[NodeIndex]);
			if (RootNode)
			{
				Modify();
				if (RootNode->Pins[0]->LinkedTo.Num() > 0)
				{
					FirstNode = CastChecked<USoundCueGraphNode>(RootNode->Pins[0]->LinkedTo[0]->GetOwningNode())->SoundNode;
				}
				else
				{
					FirstNode = NULL;
				}
				PostEditChange();
			}
		}
	}
}
#endif
