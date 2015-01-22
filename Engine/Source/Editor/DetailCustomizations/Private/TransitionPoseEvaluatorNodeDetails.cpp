// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"

#include "AnimGraphDefinitions.h"
#include "AnimGraphNode_TransitionPoseEvaluator.h"

#include "AnimTransitionNodeDetails.h"
#include "KismetEditorUtilities.h"

#include "TransitionPoseEvaluatorNodeDetails.h"

#define LOCTEXT_NAMESPACE "FTransitionPoseEvaluatorNodeDetails"

/////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FTransitionPoseEvaluatorNodeDetails::MakeInstance()
{
	return MakeShareable( new FTransitionPoseEvaluatorNodeDetails );
}

void FTransitionPoseEvaluatorNodeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{	
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();
	for (int32 ObjectIndex = 0; (EvaluatorNode == NULL) && (ObjectIndex < SelectedObjects.Num()); ++ObjectIndex)
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if (CurrentObject.IsValid())
		{
			EvaluatorNode = Cast<UAnimGraphNode_TransitionPoseEvaluator>(CurrentObject.Get());
		}
	}

	IDetailCategoryBuilder& PoseCategory = DetailBuilder.EditCategory("Pose", LOCTEXT("PoseCategoryName", "Pose") );
	TSharedPtr<IPropertyHandle> FramesToCachePosePropety = DetailBuilder.GetProperty(TEXT("Node.FramesToCachePose"));

	//@TODO: CONDUIT: try both
	DetailBuilder.HideProperty(FramesToCachePosePropety);
	PoseCategory.AddProperty( FramesToCachePosePropety ).Visibility( TAttribute<EVisibility>( this, &FTransitionPoseEvaluatorNodeDetails::GetFramesToCachePoseVisibility ) );
}


EVisibility FTransitionPoseEvaluatorNodeDetails::GetFramesToCachePoseVisibility() const
{
	if (UAnimGraphNode_TransitionPoseEvaluator* EvaluatorNodePtr = EvaluatorNode.Get())
	{
		// pose evaluators that are not using the delayed freeze mode should hide the cache frames entry
		if ((EvaluatorNodePtr != NULL) && (EvaluatorNodePtr->Node.EvaluatorMode != EEvaluatorMode::EM_DelayedFreeze))
		{
			return EVisibility::Hidden;
		}
	}

	return EVisibility::Visible;
}



#undef LOCTEXT_NAMESPACE
