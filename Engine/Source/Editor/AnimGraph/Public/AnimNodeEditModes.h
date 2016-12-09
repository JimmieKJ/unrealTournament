// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor.h"

struct ANIMGRAPH_API AnimNodeEditModes
{
	const static FEditorModeID AnimNode;
	const static FEditorModeID TwoBoneIK;
	const static FEditorModeID ObserveBone;
	const static FEditorModeID ModifyBone;
	const static FEditorModeID Fabrik;
	const static FEditorModeID PoseDriver;
};
