// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"
#include "AnimGraphDefinitions.h"

//
// Forward declarations.
//
class UAnimGraphNode_TransitionPoseEvaluator;

class FTransitionPoseEvaluatorNodeDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	EVisibility GetFramesToCachePoseVisibility( ) const;

private:
	TWeakObjectPtr<UAnimGraphNode_TransitionPoseEvaluator> EvaluatorNode;
};

