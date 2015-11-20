// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeWavePlayer.h"

/*-----------------------------------------------------------------------------
	USoundNodeLooping implementation.
-----------------------------------------------------------------------------*/
USoundNodeLooping::USoundNodeLooping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LoopCount(1)
	, bLoopIndefinitely(true)
{
}

void USoundNodeLooping::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD(sizeof(int32));
	DECLARE_SOUNDNODE_ELEMENT(int32, CurrentLoopCount);

	if (*RequiresInitialization)
	{
		CurrentLoopCount = 0;

		*RequiresInitialization = false;
	}

	FSoundParseParameters UpdatedParams = ParseParams;
	UpdatedParams.NotifyBufferFinishedHooks.AddNotify(this, NodeWaveInstanceHash);

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances );
}

bool USoundNodeLooping::NotifyWaveInstanceFinished( FWaveInstance* InWaveInstance )
{
	FActiveSound& ActiveSound = *InWaveInstance->ActiveSound;
	const UPTRINT NodeWaveInstanceHash = InWaveInstance->NotifyBufferFinishedHooks.GetHashForNode(this);
	RETRIEVE_SOUNDNODE_PAYLOAD(sizeof(int32));
	DECLARE_SOUNDNODE_ELEMENT(int32, CurrentLoopCount);
	check(*RequiresInitialization == 0);

	if (bLoopIndefinitely || ++CurrentLoopCount < LoopCount)
	{
		struct FNodeHashPairs
		{
			USoundNode* Node;
			UPTRINT NodeWaveInstanceHash;

			FNodeHashPairs(USoundNode* InNode, const UPTRINT InHash)
				: Node(InNode)
				, NodeWaveInstanceHash(InHash)
			{
			}
		};

		TArray<FNodeHashPairs> NodesToReset;

		for (int32 ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ++ChildNodeIndex)
		{
			USoundNode* ChildNode = ChildNodes[ChildNodeIndex];
			if (ChildNode)
			{
				NodesToReset.Add(FNodeHashPairs(ChildNode, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNode, ChildNodeIndex)));
			}
		}

		// GetAllNodes includes current node so we have to start at Index 1.
		for (int32 ResetNodeIndex = 0; ResetNodeIndex < NodesToReset.Num(); ++ResetNodeIndex)
		{
			const FNodeHashPairs& NodeHashPair = NodesToReset[ResetNodeIndex];

			// Reset all child nodes so they are initialized again.
			uint32* Offset = ActiveSound.SoundNodeOffsetMap.Find(NodeHashPair.NodeWaveInstanceHash);
			if (Offset)
			{
				bool* bRequiresInitialization = (bool*)&ActiveSound.SoundNodeData[*Offset];
				*bRequiresInitialization = true;
			}

			USoundNode* ResetNode = NodeHashPair.Node;

			if (ResetNode->ChildNodes.Num())
			{
				for (int32 ResetChildIndex = 0; ResetChildIndex < ResetNode->ChildNodes.Num(); ++ResetChildIndex)
				{
					USoundNode* ResetChildNode = ResetNode->ChildNodes[ResetChildIndex];
					if (ResetChildNode)
					{
						NodesToReset.Add(FNodeHashPairs(ResetChildNode, GetNodeWaveInstanceHash(NodeHashPair.NodeWaveInstanceHash, ResetChildNode, ResetChildIndex)));
					}
				}
			}
			else if (ResetNode->IsA<USoundNodeWavePlayer>())
			{
				FWaveInstance* WaveInstance = ActiveSound.FindWaveInstance(NodeHashPair.NodeWaveInstanceHash);
				if (WaveInstance)
				{
					WaveInstance->bAlreadyNotifiedHook = true;
				}
			}
		}

		// Reset wave instances that notified us of completion.
		InWaveInstance->bIsStarted = false;
		InWaveInstance->bIsFinished = false;

		return true;
	}

	return false;
}

float USoundNodeLooping::GetDuration()
{
	return INDEFINITELY_LOOPING_DURATION;
}

