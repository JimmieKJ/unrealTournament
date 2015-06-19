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
		UI_COMMAND(DeleteKey, "Delete Spline Point", "Delete the currently selected spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
		UI_COMMAND(DuplicateKey, "Duplicate Spline Point", "Duplicate the currently selected spline point.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddKey, "Add Spline Point Here", "Add a new spline point at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ResetToUnclampedTangent, "Unclamped Tangent", "Reset the tangent for this spline point to its default unclamped value.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ResetToClampedTangent, "Clamped Tangent", "Reset the tangent for this spline point to its default clamped value.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SetKeyToCurve, "Curve", "Set spline point to Curve type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetKeyToLinear, "Linear", "Set spline point to Linear type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetKeyToConstant, "Constant", "Set spline point to Constant type", EUserInterfaceActionType::RadioButton, FInputChord());
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
	, LastKeyIndexSelected(INDEX_NONE)
	, SelectedSegmentIndex(INDEX_NONE)
	, SelectedTangentHandle(INDEX_NONE)
	, SelectedTangentHandleType(ESelectedTangentHandle::None)
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
		if (SplineComp == EditedSplineComp)
		{
			for (int32 SelectedKey : SelectedKeys)
			{
				if (SplineInfo.Points[SelectedKey].IsCurveKey())
				{
					const FVector KeyPos = SplineComp->ComponentToWorld.TransformPosition(SplineInfo.Points[SelectedKey].OutVal);
					const FVector TangentWorldDirection = SplineComp->ComponentToWorld.TransformVector(SplineInfo.Points[SelectedKey].LeaveTangent);

					PDI->SetHitProxy(NULL);
					DrawDashedLine(PDI, KeyPos, KeyPos + TangentWorldDirection, SelectedColor, 5, SDPG_Foreground);
					DrawDashedLine(PDI, KeyPos, KeyPos - TangentWorldDirection, SelectedColor, 5, SDPG_Foreground);
					PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKey, false));
					PDI->DrawPoint(KeyPos + TangentWorldDirection, SelectedColor, TangentHandleSize, SDPG_Foreground);
					PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKey, true));
					PDI->DrawPoint(KeyPos - TangentWorldDirection, SelectedColor, TangentHandleSize, SDPG_Foreground);
					PDI->SetHitProxy(NULL);
				}
			}
		}

		FVector OldKeyPos(0);
		float OldKeyTime = 0.f;
		for (int32 KeyIdx = 0; KeyIdx < SplineInfo.Points.Num(); KeyIdx++)
		{
			const float NewKeyTime = SplineInfo.Points[KeyIdx].InVal;
			const FVector NewKeyPos = SplineComp->ComponentToWorld.TransformPosition(SplineInfo.Points[KeyIdx].OutVal);

			const FColor KeyColor = (SplineComp == EditedSplineComp && SelectedKeys.Contains(KeyIdx)) ? SelectedColor : NormalColor;

			// Draw the keypoint
			if (!SplineComp->IsClosedLoop() || KeyIdx < SplineInfo.Points.Num() - 1)
			{
				PDI->SetHitProxy(new HSplineKeyProxy(Component, KeyIdx));
				PDI->DrawPoint(NewKeyPos, KeyColor, GrabHandleSize, SDPG_Foreground);
				PDI->SetHitProxy(NULL);
			}

			// If not the first keypoint, draw a line to the previous keypoint.
			if (KeyIdx > 0)
			{
				const FColor LineColor = (SplineComp == EditedSplineComp && SelectedKeys.Contains(KeyIdx - 1)) ? SelectedColor : NormalColor;
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

void FSplineComponentVisualizer::ChangeSelectionState(int32 Index, bool bIsCtrlHeld)
{
	if (Index == INDEX_NONE)
	{
		SelectedKeys.Empty();
		LastKeyIndexSelected = INDEX_NONE;
	}
	else if (!bIsCtrlHeld)
	{
		SelectedKeys.Empty();
		SelectedKeys.Add(Index);
		LastKeyIndexSelected = Index;
	}
	else
	{
		// Add or remove from selection if Ctrl is held
		if (SelectedKeys.Contains(Index))
		{
			// If already in selection, toggle it off
			SelectedKeys.Remove(Index);

			if (LastKeyIndexSelected == Index)
			{
				if (SelectedKeys.Num() == 0)
				{
					// Last key selected: clear last key index selected
					LastKeyIndexSelected = INDEX_NONE;
				}
				else
				{
					// Arbitarily set last key index selected to first member of the set (so that it is valid)
					LastKeyIndexSelected = *SelectedKeys.CreateConstIterator();
				}
			}
		}
		else
		{
			// Add to selection
			SelectedKeys.Add(Index);
			LastKeyIndexSelected = Index;
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
				// Control point clicked

				HSplineKeyProxy* KeyProxy = (HSplineKeyProxy*)VisProxy;

				// Modify the selection state, unless right-clicking on an already selected key
				if (Click.GetKey() != EKeys::RightMouseButton || !SelectedKeys.Contains(KeyProxy->KeyIndex))
				{
					ChangeSelectionState(KeyProxy->KeyIndex, InViewportClient->IsCtrlPressed());
				}
				SelectedSegmentIndex = INDEX_NONE;
				SelectedTangentHandle = INDEX_NONE;
				SelectedTangentHandleType = ESelectedTangentHandle::None;

				bEditing = true;
			}
			else if (VisProxy->IsA(HSplineSegmentProxy::StaticGetType()))
			{
				// Spline segment clicked

				// Divide segment into subsegments and test each subsegment against ray representing click position and camera direction.
				// Closest encounter with the spline determines the spline position.
				const int32 NumSubdivisions = 16;

				HSplineSegmentProxy* SegmentProxy = (HSplineSegmentProxy*)VisProxy;
				ChangeSelectionState(SegmentProxy->SegmentIndex, InViewportClient->IsCtrlPressed());
				SelectedSegmentIndex = SegmentProxy->SegmentIndex;
				SelectedTangentHandle = INDEX_NONE;
				SelectedTangentHandleType = ESelectedTangentHandle::None;

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
				// Tangent handle clicked

				HSplineTangentHandleProxy* KeyProxy = (HSplineTangentHandleProxy*)VisProxy;

				// Note: don't change key selection when a tangent handle is clicked
				SelectedSegmentIndex = INDEX_NONE;
				SelectedTangentHandle = KeyProxy->KeyIndex;
				SelectedTangentHandleType = KeyProxy->bArriveTangent ? ESelectedTangentHandle::Arrive : ESelectedTangentHandle::Leave;

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
	if (SplineComp != nullptr)
	{
		if (SelectedTangentHandle != INDEX_NONE)
		{
			// If tangent handle index is set, use that
			check(SelectedTangentHandle < SplineComp->SplineInfo.Points.Num());
			const auto& Point = SplineComp->SplineInfo.Points[SelectedTangentHandle];

			check(SelectedTangentHandleType != ESelectedTangentHandle::None);
			if (SelectedTangentHandleType == ESelectedTangentHandle::Leave)
			{
				OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal + Point.LeaveTangent);
			}
			else if (SelectedTangentHandleType == ESelectedTangentHandle::Arrive)
			{
				OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal - Point.ArriveTangent);
			}

			return true;
		}
		else if (LastKeyIndexSelected != INDEX_NONE)
		{
			// Otherwise use the last key index set
			check(LastKeyIndexSelected < SplineComp->SplineInfo.Points.Num());
			check(SelectedKeys.Contains(LastKeyIndexSelected));
			const auto& Point = SplineComp->SplineInfo.Points[LastKeyIndexSelected];
			OutLocation = SplineComp->ComponentToWorld.TransformPosition(Point.OutVal);
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
		if (SplineComp != nullptr)
		{
			// First look at selected tangent handle for coordinate system
			int32 Index = SelectedTangentHandle;
			if (Index == INDEX_NONE)
			{
				// If not set, fall back to last key index selected
				Index = LastKeyIndexSelected;
			}

			if (Index != INDEX_NONE)
			{
				check(Index < SplineComp->SplineInfo.Points.Num());
				check(SelectedTangentHandle != INDEX_NONE || SelectedKeys.Contains(Index));

				const auto& Point = SplineComp->SplineInfo.Points[Index];
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
	if (SplineComp != nullptr)
	{
		const int32 NumPoints = SplineComp->SplineInfo.Points.Num();

		bool bChangedFirstPoint = false;

		if (SelectedTangentHandle != INDEX_NONE)
		{
			// When tangent handles are manipulated...

			check(SelectedTangentHandle < NumPoints);

			FInterpCurvePoint<FVector>& EditedPoint = SplineComp->SplineInfo.Points[SelectedTangentHandle];

			if (!DeltaTranslate.IsZero())
			{
				check(SelectedTangentHandleType != ESelectedTangentHandle::None);

				const FVector Delta = (SelectedTangentHandleType == ESelectedTangentHandle::Leave) ? DeltaTranslate : -DeltaTranslate;
				const FVector NewTangent = EditedPoint.LeaveTangent + SplineComp->ComponentToWorld.InverseTransformVector(Delta);
				EditedPoint.LeaveTangent = NewTangent;
				EditedPoint.ArriveTangent = NewTangent;
				EditedPoint.InterpMode = CIM_CurveUser;

				if (SelectedTangentHandle == 0)
				{
					bChangedFirstPoint = true;
				}
			}
		}
		else
		{
			// When spline keys are manipulated...

			check(LastKeyIndexSelected != INDEX_NONE);
			check(LastKeyIndexSelected < NumPoints);
			check(SelectedKeys.Num() > 0);

			SplineComp->Modify();

			if (ViewportClient->IsAltPressed() && bAllowDuplication)
			{
				OnDuplicateKey();
				// Don't duplicate again until we release LMB
				bAllowDuplication = false;
			}

			for (int32 SelectedKeyIndex : SelectedKeys)
			{
				FInterpCurvePoint<FVector>& EditedPoint = SplineComp->SplineInfo.Points[SelectedKeyIndex];

				if (!DeltaTranslate.IsZero())
				{
					// Find key position in world space
					const FVector CurrentWorldPos = SplineComp->ComponentToWorld.TransformPosition(EditedPoint.OutVal);
					// Move in world space
					const FVector NewWorldPos = CurrentWorldPos + DeltaTranslate;
					// Convert back to local space
					EditedPoint.OutVal = SplineComp->ComponentToWorld.InverseTransformPosition(NewWorldPos);
				}

				if (!DeltaRotate.IsZero())
				{
					// Set point tangent as user controlled
					EditedPoint.InterpMode = CIM_CurveUser;

					// Rotate tangent according to delta rotation
					const FVector NewTangent = DeltaRotate.RotateVector(EditedPoint.LeaveTangent);
					EditedPoint.LeaveTangent = NewTangent;
					EditedPoint.ArriveTangent = NewTangent;
				}

				if (!DeltaScale.IsZero())
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

				if (SelectedKeyIndex == 0)
				{
					bChangedFirstPoint = true;
				}
			}
		}

		// Duplicate first point to last point if it is a closed loop
		if (SplineComp->IsClosedLoop() && bChangedFirstPoint && NumPoints > 0)
		{
			const FInterpCurvePoint<FVector>& EditedPoint = SplineComp->SplineInfo.Points[0];
			FInterpCurvePoint<FVector>& DuplicatedPoint = SplineComp->SplineInfo.Points[NumPoints - 1];
			DuplicatedPoint.OutVal = EditedPoint.OutVal;
			DuplicatedPoint.InterpMode = EditedPoint.InterpMode;
			DuplicatedPoint.LeaveTangent = EditedPoint.LeaveTangent;
			DuplicatedPoint.ArriveTangent = EditedPoint.ArriveTangent;
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
	ChangeSelectionState(INDEX_NONE, false);
	SelectedSegmentIndex = INDEX_NONE;
	SelectedTangentHandle = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
}


void FSplineComponentVisualizer::OnDuplicateKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicateSplinePoint", "Duplicate Spline Point"));
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(LastKeyIndexSelected != INDEX_NONE);
	check(SelectedKeys.Num() > 0);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Remember whether it's a closed loop for later, and turn off the loop while we manipulate the points
	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	// Get a sorted list of all the selected indices, highest to lowest
	TArray<int32> SelectedKeysSorted;
	for (int32 SelectedKeyIndex : SelectedKeys)
	{
		SelectedKeysSorted.Add(SelectedKeyIndex);
	}
	SelectedKeysSorted.Sort([](int32 A, int32 B) { return A > B; });

	// Insert duplicates into the list, highest index first, so that the lower indices remain the same
	TArray<FInterpCurvePoint<FVector>>& Points = SplineComp->SplineInfo.Points;
	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		FInterpCurvePoint<FVector> KeyToDupe = Points[SelectedKeyIndex];
		KeyToDupe.InterpMode = CIM_CurveAuto;
		Points.Insert(KeyToDupe, SelectedKeyIndex);
	}

	// Repopulate the selected keys
	SelectedKeys.Empty();
	int32 Offset = SelectedKeysSorted.Num();
	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		SelectedKeys.Add(SelectedKeyIndex + Offset);

		if (LastKeyIndexSelected == SelectedKeyIndex)
		{
			LastKeyIndexSelected += Offset;
		}

		Offset--;
	}

	// Unset tangent handle selection
	SelectedTangentHandle = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	// Update Input value for all keys
	SplineComp->RefreshSplineInputs();

	// Restore old closed loop state
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
	check(SplineComp != nullptr);
	check(LastKeyIndexSelected != INDEX_NONE);
	check(SelectedKeys.Num() > 0);
	check(SelectedKeys.Contains(LastKeyIndexSelected));
	check(SelectedTangentHandle == INDEX_NONE);
	check(SelectedTangentHandleType == ESelectedTangentHandle::None);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Remember whether it's a closed loop for later, and turn off the loop while we manipulate the points
	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	FInterpCurvePoint<FVector> NewKey;
	NewKey.InVal = SelectedSegmentIndex;
	NewKey.OutVal = SplineComp->ComponentToWorld.InverseTransformPosition(SelectedSplinePosition);
	NewKey.InterpMode = CIM_CurveAuto;
	int32 NewKeyIndex = SplineComp->SplineInfo.Points.Insert(NewKey, SelectedSegmentIndex + 1);

	// Set selection to 'next' key
	ChangeSelectionState(SelectedSegmentIndex + 1, false);
	SelectedSegmentIndex = INDEX_NONE;

	// Update Input value for all keys
	SplineComp->RefreshSplineInputs();

	// Restore old closed loop state
	SplineComp->SetClosedLoop(bWasLoop);

	NotifyComponentModified();
}


void FSplineComponentVisualizer::OnDeleteKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSplinePoint", "Delete Spline Point"));
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(LastKeyIndexSelected != INDEX_NONE);
	check(SelectedKeys.Num() > 0);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Remember whether it's a closed loop for later, and turn off the loop while we manipulate the points
	const bool bWasLoop = SplineComp->IsClosedLoop();
	SplineComp->SetClosedLoop(false);

	// Get a sorted list of all the selected indices, highest to lowest
	TArray<int32> SelectedKeysSorted;
	for (int32 SelectedKeyIndex : SelectedKeys)
	{
		SelectedKeysSorted.Add(SelectedKeyIndex);
	}
	SelectedKeysSorted.Sort([](int32 A, int32 B) { return A > B; });

	// Delete selected keys from list, highest index first
	TArray<FInterpCurvePoint<FVector>>& Points = SplineComp->SplineInfo.Points;
	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		Points.RemoveAt(SelectedKeyIndex);
	}

	// Unselect everything
	ChangeSelectionState(INDEX_NONE, false);
	SelectedSegmentIndex = INDEX_NONE;
	SelectedTangentHandle = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	SplineComp->RefreshSplineInputs(); // update input value for each key

	// Restore old closed loop state
	SplineComp->SetClosedLoop(bWasLoop);

	NotifyComponentModified();
}


bool FSplineComponentVisualizer::CanDeleteKey() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp != nullptr &&
			SelectedKeys.Num() > 0 &&
			SelectedKeys.Num() != SplineComp->SplineInfo.Points.Num() &&
			LastKeyIndexSelected != INDEX_NONE);
}


bool FSplineComponentVisualizer::IsKeySelectionValid() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp != nullptr &&
			SelectedKeys.Num() > 0 &&
			LastKeyIndexSelected != INDEX_NONE);
}


void FSplineComponentVisualizer::OnResetToAutomaticTangent(EInterpCurveMode Mode)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"));

		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const bool bWasLoop = SplineComp->IsClosedLoop();
		SplineComp->SetClosedLoop(false);

		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			auto& Point = SplineComp->SplineInfo.Points[SelectedKeyIndex];
			if (Point.IsCurveKey())
			{
				Point.InterpMode = Mode;
			}
		}

		SplineComp->SetClosedLoop(bWasLoop);
		NotifyComponentModified();
	}
}


bool FSplineComponentVisualizer::CanResetToAutomaticTangent(EInterpCurveMode Mode) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr && LastKeyIndexSelected != INDEX_NONE)
	{
		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			const auto& Point = SplineComp->SplineInfo.Points[SelectedKeyIndex];
			if (Point.IsCurveKey() && Point.InterpMode != Mode)
			{
				return true;
			}
		}
	}

	return false;
}


void FSplineComponentVisualizer::OnSetKeyType(EInterpCurveMode Mode)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set Spline Point Type"));

		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const bool bWasLoop = SplineComp->IsClosedLoop();
		SplineComp->SetClosedLoop(false);

		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			SplineComp->SplineInfo.Points[SelectedKeyIndex].InterpMode = Mode;
		}

		SplineComp->SetClosedLoop(bWasLoop);
		NotifyComponentModified();
	}
}


bool FSplineComponentVisualizer::IsKeyTypeSet(EInterpCurveMode Mode) const
{
	if (IsKeySelectionValid())
	{
		USplineComponent* SplineComp = GetEditedSplineComponent();
		check(SplineComp != nullptr);

		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			const auto& SelectedPoint = SplineComp->SplineInfo.Points[SelectedKeyIndex];
			if ((Mode == CIM_CurveAuto && SelectedPoint.IsCurveKey()) || SelectedPoint.InterpMode == Mode)
			{
				return true;
			}
		}
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
			else if (LastKeyIndexSelected != INDEX_NONE)
			{
				MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DeleteKey);
				MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DuplicateKey);

				MenuBuilder.AddSubMenu(
					LOCTEXT("SplinePointType", "Spline Point Type"),
					LOCTEXT("KeyTypeTooltip", "Define the type of the spline point."),
					FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateSplinePointTypeSubMenu));

				// Only add the Automatic Tangents submenu if any of the keys is a curve type
				USplineComponent* SplineComp = GetEditedSplineComponent();
				if (SplineComp != nullptr)
				{
					for (int32 SelectedKeyIndex : SelectedKeys)
					{
						const auto& Point = SplineComp->SplineInfo.Points[SelectedKeyIndex];
						if (Point.IsCurveKey())
						{
							MenuBuilder.AddSubMenu(
								LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"),
								LOCTEXT("ResetToAutomaticTangentTooltip", "Reset the spline point tangent to an automatically generated value."),
								FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateTangentTypeSubMenu));
							break;
						}
					}
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
