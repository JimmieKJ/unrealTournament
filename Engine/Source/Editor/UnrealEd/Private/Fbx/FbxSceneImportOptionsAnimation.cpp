// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"


UFbxSceneImportOptionsAnimation::UFbxSceneImportOptionsAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bImportAnimations(true)
	, AnimationLength(EFbxSceneVertexColorImportOption::Replace)
	, StartFrame(0)
	, EndFrame(0)
	, bUseDefaultSampleRate(false)
	, bImportCustomAttribute(true)
	, bPreserveLocalTransform(false)
	, bDeleteExistingMorphTargetCurves(false)
{
	SourceAnimationName.Empty();
}

