// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeBranch.h"

#define LOCTEXT_NAMESPACE "SoundNodeBranch"

/*-----------------------------------------------------------------------------
    USoundNodeLocality implementation.
-----------------------------------------------------------------------------*/
USoundNodeBranch::USoundNodeBranch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeBranch::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	BranchPurpose BranchToUse = BranchPurpose::ParameterUnset;
	bool bParamValue = false;

	if (ActiveSound.GetBoolParameter( BoolParameterName, bParamValue ))
	{
		BranchToUse = (bParamValue ? BranchPurpose::ParameterTrue : BranchPurpose::ParameterFalse);
	}


	const int32 ChildNodeIndex = (int32)BranchToUse;
	if (ChildNodeIndex < ChildNodes.Num() && ChildNodes[ChildNodeIndex])
	{
		ChildNodes[ChildNodeIndex]->ParseNodes(AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[ChildNodeIndex], ChildNodeIndex), ActiveSound, ParseParams, WaveInstances);
	}
}

void USoundNodeBranch::CreateStartingConnectors()
{
	// Locality Nodes default with two connectors - Locally instigated and .
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

#if WITH_EDITOR
FString USoundNodeBranch::GetInputPinName(int32 PinIndex) const
{
	const BranchPurpose Branch = (BranchPurpose)PinIndex;
	switch (Branch)
	{
	case BranchPurpose::ParameterTrue:
		return LOCTEXT("True", "True").ToString();

	case BranchPurpose::ParameterFalse:
		return LOCTEXT("False", "False").ToString();

	case BranchPurpose::ParameterUnset:
		return LOCTEXT("ParamUnset", "Parameter Unset").ToString();
	}

	return Super::GetInputPinName(PinIndex);
}

FString USoundNodeBranch::GetTitle() const
{
	return FString::Printf(TEXT("%s (%s)"), *Super::GetTitle(), *BoolParameterName.ToString());
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE
