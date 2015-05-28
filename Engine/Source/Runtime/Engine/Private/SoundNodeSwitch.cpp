// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeSwitch.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#define LOCTEXT_NAMESPACE "SoundNodeSwitch"

USoundNodeSwitch::USoundNodeSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeSwitch::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	int32 ChildNodeIndex = 0;

	if (ActiveSound.GetIntParameter( IntParameterName, ChildNodeIndex ))
	{
		ChildNodeIndex += 1;
	}
	
	if (ChildNodeIndex < 0 || ChildNodeIndex >= ChildNodes.Num())
	{
		ChildNodeIndex = 0;
	}

	if (ChildNodeIndex < ChildNodes.Num() && ChildNodes[ChildNodeIndex])
	{
		ChildNodes[ChildNodeIndex]->ParseNodes(AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[ChildNodeIndex], ChildNodeIndex), ActiveSound, ParseParams, WaveInstances);
	}
}

void USoundNodeSwitch::CreateStartingConnectors()
{
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

#if WITH_EDITOR
void USoundNodeSwitch::RenamePins()
{
	TArray<class UEdGraphPin*> InputPins;

#if WITH_EDITORONLY_DATA
	GetGraphNode()->GetInputPins(InputPins);
#endif

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		if (InputPins[i])
		{
			InputPins[i]->PinName = GetInputPinName(i);
		}
	}
}

FString USoundNodeSwitch::GetInputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return LOCTEXT("ParamUnset", "Parameter Unset").ToString();
	}
	else
	{
		return FString::Printf(TEXT("%d"), PinIndex - 1);
	}

	return Super::GetInputPinName(PinIndex);
}

FString USoundNodeSwitch::GetTitle() const
{
	return FString::Printf(TEXT("%s (%s)"), *Super::GetTitle(), *IntParameterName.ToString());
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE