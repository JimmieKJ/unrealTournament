// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeConcatenator.h"

/*-----------------------------------------------------------------------------
         USoundNodeConcatenator implementation.
-----------------------------------------------------------------------------*/
USoundNodeConcatenator::USoundNodeConcatenator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool USoundNodeConcatenator::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance )
{
	FActiveSound& ActiveSound = *WaveInstance->ActiveSound;
	const UPTRINT NodeWaveInstanceHash = WaveInstance->NotifyBufferFinishedHooks.GetHashForNode(this);
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( int32 ) );
	DECLARE_SOUNDNODE_ELEMENT( int32, NodeIndex );
	check( *RequiresInitialization == 0 );

	// Allow wave instance to be played again the next iteration.
	WaveInstance->bIsStarted = false;
	WaveInstance->bIsFinished = false;

	// Advance index.
	NodeIndex++;

	return (NodeIndex < ChildNodes.Num());
}


float USoundNodeConcatenator::GetDuration()
{
	// Sum up length of child nodes.
	float Duration = 0.0f;
	for( int32 ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ChildNodeIndex++ )
	{
		USoundNode* ChildNode = ChildNodes[ ChildNodeIndex ];
		if( ChildNode )
		{
			Duration += ChildNode->GetDuration();
		}
	}
	return Duration;
}

void USoundNodeConcatenator::CreateStartingConnectors()
{
	// Concatenators default to two two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}


void USoundNodeConcatenator::InsertChildNode( int32 Index )
{
	Super::InsertChildNode( Index );
	InputVolume.InsertUninitialized( Index );
	InputVolume[ Index ] = 1.0f;
}


void USoundNodeConcatenator::RemoveChildNode( int32 Index )
{
	Super::RemoveChildNode( Index );
	InputVolume.RemoveAt( Index );
}

void USoundNodeConcatenator::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( int32 ) );
	DECLARE_SOUNDNODE_ELEMENT( int32, NodeIndex );

	// Start from the beginning.
	if( *RequiresInitialization )
	{
		NodeIndex = 0;
		*RequiresInitialization = false;
	}

	// Play the current node.
	if( NodeIndex < ChildNodes.Num() )
	{
		FSoundParseParameters UpdatedParams = ParseParams;
		UpdatedParams.NotifyBufferFinishedHooks.AddNotify(this, NodeWaveInstanceHash);

		// Play currently active node.
		USoundNode* ChildNode = ChildNodes[ NodeIndex ];
		if( ChildNode )
		{
			UpdatedParams.VolumeMultiplier *= InputVolume[NodeIndex];
			ChildNode->ParseNodes( AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNode, NodeIndex), ActiveSound, UpdatedParams, WaveInstances);
		}
	}
}

#if WITH_EDITOR
void USoundNodeConcatenator::SetChildNodes(TArray<USoundNode*>& InChildNodes)
{
	Super::SetChildNodes(InChildNodes);

	if (InputVolume.Num() < ChildNodes.Num())
	{
		while (InputVolume.Num() < ChildNodes.Num())
		{
			int32 NewIndex = InputVolume.Num();
			InputVolume.InsertUninitialized(NewIndex);
			InputVolume[ NewIndex ] = 1.0f;
		}
	}
	else if (InputVolume.Num() > ChildNodes.Num())
	{
		const int32 NumToRemove = InputVolume.Num() - ChildNodes.Num();
		InputVolume.RemoveAt(InputVolume.Num() - NumToRemove, NumToRemove);
	}
}
#endif //WITH_EDITOR