// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"
#include "SplineComponentVisualizer.h"
#include "ScopedTransaction.h"

IMPLEMENT_HIT_PROXY(HSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HSplineKeyProxy, HSplineVisProxy);
IMPLEMENT_HIT_PROXY(HSplineSegmentProxy, HSplineVisProxy);
IMPLEMENT_HIT_PROXY(HSplineTangentHandleProxy, HSplineVisProxy);

#define LOCTEXT_NAMESPACE "SplineComponentVisualizer"


/** Define commands for the spline component visualizer */
class FSplineComponentVisualizerCommands : public TCommands<FSplineComponentVisualizerCommands>
{
public:
	FSplineComponentVisualizerCommands() : TCommands <FSplineComponentVisualizerCommands>
	(
		"SplineComponentVisualizer",	// Context name for fast lookup
		LOCTEXT("SplineComponentVisualizer", "Spline Component Visualizer"),	// Localized context name for displaying
		NAME_None,	// Parent
		FEditorStyle::GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(DeleteKey, "Delete Spline Point", "Delete the currently selected spline point.", EUserInterfaceActionType::Button, FInputGesture(EKeys::Delete));
		UI_COMMAND(DuplicateKey, "Duplicate Spline Point", "Duplicate the currently selected spline point.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(AddKey, "Add Spline Point Here", "Add a new spline point at the cursor location.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(ResetToUnclampedTangent, "Unclamped Tangent", "Reset the tangent for this spline point to its default unclamped value.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(ResetToClampedTangent, "Clamped Tangent", "Reset the tangent for this spline point to its default clamped value.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(SetKeyToCurve, "Curve", "Set spline point to Curve type", EUserInterfaceActionType::RadioButton, FInputGesture());
		UI_COMMAND(SetKeyToLinear, "Linear", "Set spline point to Linear type", EUserInterfaceActionType::RadioButton, FInputGesture());
		UI_COMMAND(SetKeyToConstant, "Constant", "Set spline point to Constant type", EUserInterfaceActionType::RadioButton, FInputGesture());
	}

public:
	/** Delete key */
	TSharedPtr<FUICommandInfo> DeleteKey;

	/** Duplicate key */
	TSharedPtr<FUICommandInfo> DuplicateKey;

	/** Add key */
	TSharedPtr<FUICommandInfo> AddKey;

	/** Reset to unclamped tangent */
	TSharedPtr<FUICommandInfo> ResetToUnclampedTangent;

	/** Reset to clamped tangent */
	TSharedPtr<FUICommandInfo> ResetToClampedTangent;

	/** Set spline key to Curve type */
	TSharedPtr<FUICommandInfo> SetKeyToCurve;

	/** Set spline key to Linear type */
	TSharedPtr<FUICommandInfo> SetKeyToLinear;

	/** Set spline key to Constant type */
	TSharedPtr<FUICommandInfo> SetKeyToConstant;
};



FSplineComponentVisualizer::FSplineComponentVisualizer()
	: FComponentVisualizer()
	, SelectedKeyIndex(INDEX_NONE)
	, SelectedSegmentIndex(INDEX_NONE)
	, SelectedTangentHandle(ESelectedTangentHandle::None)
	, bAllowDuplication(true)
{
	FSplineComponentVisualizerCommands::Register();

	SplineComponentVisualizerActions = MakeShareable(new FUICommandList);
}

void FSplineComponentVisualizer::OnRegister()
{
	const auto& Commands = FSplineComponentVisualizerCommands::Get();

	SplineComponentVisualizerActions->MapAction(
		Commands.DeleteKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnDeleteKey),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanDeleteKey));

	SplineComponentVisualizerActions->MapAction(
		Commands.DuplicateKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnDuplicateKey),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsKeySelectionValid));

	SplineComponentVisualizerActions->MapAction(
		Commands.AddKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnAddKey),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanAddKey));

	SplineComponentVisualizerActions->MapAction(
		Commands.ResetToUnclampedTangent,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnResetToAutomaticTangent, CIM_CurveAuto),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanResetToAutomaticTangent, CIM_CurveAuto));

	SplineComponentVisualizerActions->MapAction(
		Commands.ResetToClampedTangent,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnResetToAutomaticTangent, CIM_CurveAutoClamped),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanResetToAutomaticTangent, CIM_CurveAutoClamped));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToCurve,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_CurveAuto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_CurveAuto));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToLinear,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_Linear),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_Linear));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToConstant,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_Constant),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_Constant));
}

FSplineComponentVisualizer::~FSplineComponentVisualizer()
{
	FSplineComponentVisualizerCommands::Unregister();
}

void FSplineComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const USplineComponent* SplineComp = Cast<const USplineComponent>(Component))
	{
		if (!SplineComp->bAllowSplineEditingPerInstance)
		{
			return;
		}

		USplineComponent* EditedSplineComp = GetEditedSplineComponent();

		const FColor NormalColor = SplineComp->EditorUnselectedSplineSegmentColor;
		const FColor SelectedColor = SplineComp->EditorSelectedSplineSegmentColor;
		const float GrabHandleSize = 12.0f;
		const float TangentHandleSize = 10.0f;

		const FInterpCurveVector& SplineInfo = SplineComp->SplineInfo;

		// Draw the tangent handles before anything else so they will not overdraw the rest of the spline
		if (SplineComp == EditedSplineComp && SelectedKeyIndex != INDEX_NONE)
		{
			const FVector KeyPos = SplineComp->ComponentToWorld.TransformPosition(SplineInfo.Points[SelectedKeyIndex].OutVal);
			const FVector TangentWorldDirection = SplineComp->ComponentToWorld.TransformVector(SplineInfo.Points[SelectedKeyIndex].LeaveTangent);

			PDI->SetHitProxy(NULL);
			DrawDashedLine(PDI, KeyPos, KeyPos + TangentWorldDirection, SelectedColor, 5, SDPG_Foreground);
			DrawDashedLine(PDI, KeyPos, KeyPos - TangentWorldDirection, SelectedColor, 5, SDPG_Foreground);
			PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKeyIndex, false));
			PDI->DrawPoint(KeyPos + TangentWorldDirection, SelectedColor, TangentHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKeyIndex, true));
			PDI->DrawPoint(KeyPos - TangentWorldDirection, SelectedColor, TangentHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}

		FVector OldKeyPos(0);
		float OldKeyTime = 0.f;
		for (int32 KeyIdx = 0; KeyIdx < SplineInfo.Points.Num(); KeyIdx++)
		{
			const float NewKeyTime = SplineInfo.Points[KeyIdx].InVal;
			const FVector NewKeyPos = SplineComp->ComponentToWorld.TransformPosition( SplineInfo.Eval(NewKeyTime, FVector::ZeroVector) );

			const FColor KeyColor = (SplineComp == EditedSplineComp && KeyIdx == SelectedKeyIndex) ? SelectedColor : NormalColor;

			// Draw the keypoint
			if (!SplineComp->IsClosedLoop() || KeyIdx < SplineInfo.Points.Num() - 1)
			{
				PDI->SetHitProxy(new HSplineKeyProxy(Component, KeyIdx));
				PDI->DrawPoint(NewKeyPos, KeyColor, GrabHandleSize, SDPG_Foreground);
				PDI->SetHitProxy(NULL);
			}

			// If not the first keypoint, draw a line to the previous keypoint.
			if (KeyIdx>0)
			{
				const FColor LineColor = (SplineComp == EditedSplineComp && KeyIdx == SelectedKeyIndex + 1) ? SelectedColor : NormalColor;
				PDI->SetHitProxy(new HSplineSegmentProxy(Component, KeyIdx - 1));

				// For constant interpolation - don't draw ticks - just draw dotted line.
				if (SplineInfo.Points[KeyIdx - 1].InterpMode == CIM_Constant)
				{
					DrawDashedLine(PDI, OldKeyPos, NewKeyPos, LineColor, 20, SDPG_World);
				}
				else
				{
					const int32 NumSteps = FMath::CeilToInt((NewKeyTime - OldKeyTime) * 32.0f);
					const float DrawSubstep = (NewKeyTime - OldKeyTime) / NumSteps;

					// Find position on first keyframe.
					float OldTime = OldKeyTime;
					FVector OldPos = OldKeyPos;

					// Then draw a line for each substep.
					for (int32 StepIdx = 1; StepIdx<NumSteps + 1; StepIdx++)
					{
						const float NewTime = OldKeyTime + StepIdx*DrawSubstep;
						const FVector NewPos = SplineComp->ComponentToWorld.TransformPosition( SplineInfo.Eval(NewTime, FVector::ZeroVector) );

						PDI->DrawLine(OldPos, NewPos, LineColor, SDPG_Foreground);

						OldTime = NewTime;
						OldPos = NewPos;
					}
				}

				PDI->SetHitProxy(NULL);
			}

			OldKeyTime = NewKeyTime;
			OldKeyPos = NewKeyPos;
		}
	}
}

bool FSplineComponentVisualizer::VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bool bEditing = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		const USplineComponent* SplineComp = CastChecked<const USplineComponent>(VisProxy->Component.Get());

		SplineCompPropName = GetComponentPropertyName(SplineComp);
		if(SplineCompPropName != NAME_None)
		{
			SplineOwningActor = SplineComp->GetOwner();

			if (VisProxy->IsA(HSplineKeyProxy::StaticGetType()))
			{
				HSplineKeyProxy* KeyProxy = (HSplineKeyProxy*)VisProxy;
				SelectedKeyIndex = KeyProxy->KeyIndex;
				SelectedSegmentIndex = INDEX_NONE;
				SelectedTangentHandle = ESelectedTangentHandle::None;
				bEditing = true;
			}
			else if (VisProxy->IsA(HSplineSegmentProxy::StaticGetType()))
			{
				// Divide segment into subsegments and test each subsegment against ray representing click position and camera direction.
				// Closest encounter with the spline determines the spline position.
				const int32 NumSubdivisions = 16;

				HSplineSegmentProxy* SegmentProxy = (HSplineSegmentProxy*)VisProxy;
				SelectedKeyIndex = SegmentProxy->SegmentIndex;
				SelectedSegmentIndex = SegmentProxy->SegmentIndex;
				SelectedTangentHandle = ESelectedTangentHandle::None;

				float SubsegmentStartKey = static_cast<float>(SelectedSegmentIndex);
				FVector SubsegmentStart = SplineComp->ComponentToWorld.TransformPosition(SplineComp->SplineInfo.Eval(SubsegmentStartKey, FVector::ZeroVector));

				float ClosestDistance = TNumericLimits<float>::Max();
				FVector BestLocation = SubsegmentStart;

				for (int32 Step = 1; Step < NumSubdivisions; Step++)
				{
					const float SubsegmentEndKey = SelectedSegmentIndex + Step / static_cast<float>(NumSubdivisions);
					const FVector SubsegmentEnd = SplineComp->ComponentToWorld.TransformPosition(SplineComp->SplineInfo.Eval(SubsegmentEndKey, FVector::ZeroVector));

					FVector SplineClosest;
					FVector RayClosest;
					FMath::SegmentDistToSegmentSafe(SubsegmentStart, SubsegmentEnd, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f, SplineClosest, RayClosest);

					const float Distance = FVector::DistSquared(SplineClosest, RayClosest);
					if (Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						BestLocation = SplineClosest;
					}

					SubsegmentStartKey = SubsegmentEndKey;
					SubsegmentStart = SubsegmentEnd;
				}

				SelectedSplinePosition = BestLocation;
				bEditing = true;
			}
			else if (VisProxy->IsA(HSplineTangentHandleProxy::StaticGetType()))
			{
				HSplineTangentHandleProxy* KeyProxy = (HSplineTangentHandleProxy*)VisProxy;
				SelectedKeyIndex = KeyProxy->KeyIndex;
				SelectedSegmentIndex = INDEX_NONE;
				SelectedTangentHandle = KeyProxy->bArriveTangent ? ESelectedTangentHandle::Arrive : ESelectedTangentHandle::Leave;
				bEditing = true;
			}
		}
		else
		{
			SplineOwningActor = NULL;
		}
	}

	return bEditing;
}

USplineComponent* FSplineComponentVisualizer::GetEditedSplineComponent() const
{
	return Cast<USplineComponent>(GetComponentFromPropertyName(SplineOwningActor.Get(), SplineCompPropName));
}


bool FSplineComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp && SelectedKeyIndex != INDEX_NONE)
	{
		if (SelectedKeyIndex < SplineComp->SplineInfo.Points.Num())
		{
			const auto& Point = SplineComp->SplineInfo.Points[SelectedKeyIndex];

			switch (SelectedTangentHandle)
			{
			case ESelectedTangentHandle::None:
				OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal);
				break;

			case ESelectedTangentHandle::Leave:
				OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal + Point.LeaveTangent);
				break;

			case ESelectedTangentHandle::Arrive:
				OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal - Point.ArriveTangent);
				break;
			}

			return true;
		}
	}

	return false;
}


bool FSplineComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local)
	{
		USplineComponent* SplineComp = GetEditedSplineComponent();
		if (SplineComp && SelectedKeyIndex != INDEX_NONE)
		{
			if (SelectedKeyIndex < SplineComp->SplineInfo.Points.Num())
			{
				const auto& Point = SplineComp->SplineInfo.Points[SelectedKeyIndex];
				const FVector Tangent = Point.ArriveTangent.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : Point.ArriveTangent.GetSafeNormal();
				const FVector Bitangent = (Tangent.Z == 1.0f) ? FVector(1.0f, 0.0f, 0.0f) : FVector(-Tangent.Y, Tangent.X, 0.0f).GetSafeNormal();
				const FVector Normal = FVector::CrossProduct(Tangent, Bitangent);

				OutMatrix = FMatrix(Tangent, Bitangent, Normal, FVector::ZeroVector) * FQuatRotationTranslationMatrix(SplineComp->ComponentToWorld.GetRotation(), FVector::ZeroVector);
				return true;
			}
		}
	}

	return false;
}


bool FSplineComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	const int32 NumPoints = SplineComp->SplineInfo.Points.Num();

	if (SplineComp && 
		SelectedKeyIndex != INDEX_NONE &&
		SelectedKeyIndex < NumPoints)
	{
		SplineComp->Modify();

		if (ViewportClient->IsAltPressed() && SelectedTangentHandle == ESelectedTangentHandle::None && bAllowDuplication)
		{
			OnDuplicateKey();
			// Don't duplicate again until we release LMB
			bAllowDuplication = false;
		}

		FInterpCurvePoint<FVector>& EditedPoint = SplineComp->SplineInfo.Points[SelectedKeyIndex];

		if (!DeltaTranslate.IsZero())
		{
			if (SelectedTangentHandle == ESelectedTangentHandle::None)
			{
				// Find key position in world space
				const FVector CurrentWorldPos = SplineComp->ComponentToWorld.TransformPosition(EditedPoint.OutVal);
				// Move in world space
				const FVector NewWorldPos = CurrentWorldPos + DeltaTranslate;
				// Convert back to local space
				EditedPoint.OutVal = SplineComp->ComponentToWorld.InverseTransformPosition(NewWorldPos);
			}
			else
			{
				const FVector Delta = (SelectedTangentHandle == ESelectedTangentHandle::Leave) ? DeltaTranslate : -DeltaTranslate;
				const FVector NewTangent = EditedPoint.LeaveTangent + SplineComp->ComponentToWorld.InverseTransformVector(Delta);
				EditedPoint.LeaveTangent = NewTangent;
				EditedPoint.ArriveTangent = NewTangent;
				EditedPoint.InterpMode = CIM_CurveUser;
			}
		}

		if (!DeltaRotate.IsZero() && SelectedTangentHandle == ESelectedTangentHandle::None)
		{
			// Set point tangent as user controlled
			EditedPoint.InterpMode = CIM_CurveUser;

			// Rotate tangent according to delta rotation
			const FVector NewTangent = DeltaRotate.RotateVector(EditedPoint.LeaveTangent);
			EditedPoint.LeaveTangent = NewTangent;
			EditedPoint.ArriveTangent = NewTangent;
		}

		if (!DeltaScale.IsZero() && SelectedTangentHandle == ESelectedTangentHandle::None)
		{
			// Set point tangent as user controlled
			EditedPoint.InterpMode = CIM_CurveUser;

			// Break tangent into direction and length so we can change its scale (the 'tension')
			// independently of its direction.
			FVector Direction;
			float Length;
			EditedPoint.LeaveTangent.ToDirectionAndLength(Direction, Length);

			// Figure out which component has changed, and use it
			float DeltaScaleValue = (DeltaScale.X != 0.0f) ? DeltaScale.X : ((DeltaScale.Y != 0.0f) ? DeltaScale.Y : DeltaScale.Z);

			// Change scale, avoiding singularity by never allowing a scale of 0, hence preserving direction.
			Length += DeltaScaleValue * 10.0f;
			if (Length == 0.0f)
			{
				Length = SMALL_NUMBER;
			}

			const FVector NewTangent = Direction * Length;
			EditedPoint.LeaveTangent = NewTangent;
			EditedPoint.ArriveTangent = NewTangent;
		}

		// Duplicate first point to last point if it is a closed loop
		if (SplineComp->IsClosedLoop() && SelectedKeyIndex == 0 && NumPoints > 0)
		{
			SplineComp->SplineInfo.Points[NumPoints - 1].OutVal = EditedPoint.OutVal;
			SplineComp->SplineInfo.Points[NumPoints - 1].LeaveTangent = EditedPoint.LeaveTangent;
			SplineComp->SplineInfo.Points[NumPoints - 1].ArriveTangent = EditedPoint.ArriveTangent;
		}

		NotifyComponentModified();

		return true;
	}

	return false;
}

bool FSplineComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	if (Key == EKeys::LeftMouseButton && Event == IE_Released)
	{
		// Reset duplication flag on LMB release
		bAllowDuplication = true;
	}

	if (Event == IE_Pressed)
	{
		bHandled = SplineComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}


void FSplineComponentVisualizer::EndEditing()
{
	SplineOwningActor = NULL;
	SplineCompPropName = NAME_None;
	SelectedKeyIndex = INDEX_NONE;
}


void FSplineComponentVisualizer::OnDuplicateKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicateSplinePoint", "Duplicate Spline Point"));
	USplineComponent* SplineComp = GetEditedSplineComponent();

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	FInterpCurvePoint<FVector> KeyToDupe = SplineComp->SplineInfo.Points[SelectedKeyIndex];
	KeyToDupe.InterpMode = CIM_CurveAuto;
	int32 NewKeyIndex = SplineComp->SplineInfo.Points.Insert(KeyToDupe, SelectedKeyIndex);
	// move selection to 'next' key
	SelectedKeyIndex++;

	// Update Input value for all keys
	SplineComp->RefreshSplineInputs();

	SplineComp->SetClosedLoop(bWasLoop);

	NotifyComponentModified();
}


bool FSplineComponentVisualizer::CanAddKey() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp &&
		SelectedSegmentIndex != INDEX_NONE &&
		SelectedSegmentIndex < SplineComp->SplineInfo.Points.Num() - 1);
}


void FSplineComponentVisualizer::OnAddKey()
{
	const FScopedTransaction Transaction(LOCTEXT("AddSplinePoint", "Add Spline Point"));
	USplineComponent* SplineComp = GetEditedSplineComponent();

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	FInterpCurvePoint<FVector> NewKey;
	NewKey.InVal = SelectedSegmentIndex;
	NewKey.OutVal = SplineComp->ComponentToWorld.InverseTransformPosition(SelectedSplinePosition);
	NewKey.InterpMode = CIM_CurveAuto;
	int32 NewKeyIndex = SplineComp->SplineInfo.Points.Insert(NewKey, SelectedSegmentIndex + 1);
	// move selection to 'next' key
	SelectedKeyIndex = SelectedSegmentIndex + 1;
	SelectedSegmentIndex = INDEX_NONE;

	// Update Input value for all keys
	SplineComp->RefreshSplineInputs();

	SplineComp->SetClosedLoop(bWasLoop);

	NotifyComponentModified();
}


void FSplineComponentVisualizer::OnDeleteKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSplinePoint", "Delete Spline Point"));
	USplineComponent* SplineComp = GetEditedSplineComponent();

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	SplineComp->SplineInfo.Points.RemoveAt(SelectedKeyIndex);
	SplineComp->RefreshSplineInputs(); // update input value for each key

	SplineComp->SetClosedLoop(bWasLoop);

	SelectedKeyIndex = INDEX_NONE; // deselect any keys
	SelectedSegmentIndex = INDEX_NONE;

	NotifyComponentModified();
}


bool FSplineComponentVisualizer::CanDeleteKey() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp &&
		SplineComp->SplineInfo.Points.Num() >= 2 &&
		SelectedSegmentIndex == INDEX_NONE &&
		SelectedKeyIndex != INDEX_NONE &&
		SelectedKeyIndex < SplineComp->SplineInfo.Points.Num());
}


bool FSplineComponentVisualizer::IsKeySelectionValid() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp &&
		SelectedSegmentIndex == INDEX_NONE &&
		SelectedKeyIndex != INDEX_NONE &&
		SelectedKeyIndex < SplineComp->SplineInfo.Points.Num());
}


void FSplineComponentVisualizer::OnResetToAutomaticTangent(EInterpCurveMode Mode)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();

	if (SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode != Mode)
	{
		const FScopedTransaction Transaction(LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"));
		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const bool bWasLoop = SplineComp->IsClosedLoop();
		SplineComp->SetClosedLoop(false);
		SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode = Mode;
		SplineComp->SetClosedLoop(bWasLoop);
		NotifyComponentModified();
	}
}


bool FSplineComponentVisualizer::CanResetToAutomaticTangent(EInterpCurveMode Mode) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp &&
		SelectedKeyIndex != INDEX_NONE &&
		SelectedKeyIndex < SplineComp->SplineInfo.Points.Num() &&
		SplineComp->SplineInfo.Points[SelectedKeyIndex].IsCurveKey() &&
		SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode != Mode);
}


void FSplineComponentVisualizer::OnSetKeyType(EInterpCurveMode Mode)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();

	if (SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode != Mode)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set Spline Point Type"));
		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const bool bWasLoop = SplineComp->IsClosedLoop();
		SplineComp->SetClosedLoop(false);
		SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode = Mode;
		SplineComp->SetClosedLoop(bWasLoop);
		NotifyComponentModified();
	}
}


bool FSplineComponentVisualizer::IsKeyTypeSet(EInterpCurveMode Mode) const
{
	if (IsKeySelectionValid())
	{
		USplineComponent* SplineComp = GetEditedSplineComponent();

		const auto& SelectedPoint = SplineComp->SplineInfo.Points[SelectedKeyIndex];
		return ((Mode == CIM_CurveAuto && SelectedPoint.IsCurveKey()) || SelectedPoint.InterpMode == Mode);
	}

	return false;
}


TSharedPtr<SWidget> FSplineComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, SplineComponentVisualizerActions);
	{
		MenuBuilder.BeginSection("SplineKeyEdit");
		{
			if (SelectedSegmentIndex != INDEX_NONE)
			{
				MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AddKey);
			}
			else if (SelectedKeyIndex != INDEX_NONE)
			{
				MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DeleteKey);
				MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DuplicateKey);

				MenuBuilder.AddSubMenu(
					LOCTEXT("SplinePointType", "Spline Point Type"),
					LOCTEXT("KeyTypeTooltip", "Define the type of the spline point."),
					FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateSplinePointTypeSubMenu));

				// Only add the Automatic Tangents submenu if the key is a curve type
				USplineComponent* SplineComp = GetEditedSplineComponent();
				if (SplineComp &&
					SelectedKeyIndex < SplineComp->SplineInfo.Points.Num() &&
					SplineComp->SplineInfo.Points[SelectedKeyIndex].IsCurveKey())
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"),
						LOCTEXT("ResetToAutomaticTangentTooltip", "Reset the spline point tangent to an automatically generated value."),
						FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateTangentTypeSubMenu));
				}
			}
		}
		MenuBuilder.EndSection();
	}

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


void FSplineComponentVisualizer::GenerateSplinePointTypeSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToCurve);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToLinear);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToConstant);
}


void FSplineComponentVisualizer::GenerateTangentTypeSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ResetToUnclampedTangent);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ResetToClampedTangent);
}


void FSplineComponentVisualizer::NotifyComponentModified()
{
	USplineComponent* SplineComp = GetEditedSplineComponent();

	SplineComp->UpdateSpline();

	// Notify that the spline info has been modified
	UProperty* SplineInfoProperty = FindField<UProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineInfo));
	FPropertyChangedEvent PropertyChangedEvent(SplineInfoProperty);
	SplineComp->PostEditChangeProperty(PropertyChangedEvent);

	// Notify of change so any CS is re-run
	if (SplineOwningActor.IsValid())
	{
		SplineOwningActor.Get()->PostEditMove(true);
	}

	GEditor->RedrawLevelEditingViewports(true);
}

#undef LOCTEXT_NAMESPACE
