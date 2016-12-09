// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintProfilerConnectionDrawingPolicy.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Profiler/TracePath.h"
#include "Profiler/BlueprintProfilerSettings.h"
#include "EdGraphSchema_K2.h"
#endif // WITH_EDITOR

#include "BlueprintProfilerModule.h"
#include "ScriptInstrumentationPlayback.h"



/////////////////////////////////////////////////////
// FBlueprintProfilerPinConnectionFactory

FConnectionDrawingPolicy* FBlueprintProfilerPinConnectionFactory::CreateConnectionPolicy(const class UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	FConnectionDrawingPolicy* NewPolicy = nullptr;
	if (InGraphObj)
	{
		EBlueprintProfilerHeatMapDisplayMode WireHeatMode = GetDefault<UBlueprintProfilerSettings>()->WireHeatMapDisplayMode;
		if (WireHeatMode != EBlueprintProfilerHeatMapDisplayMode::None)
		{
			UBlueprint* Blueprint = InGraphObj->GetTypedOuter<UBlueprint>();
			UBlueprintGeneratedClass* BPGC = Blueprint ? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass) : nullptr;
			if (BPGC)
			{
				IBlueprintProfilerInterface& ProfilerModule = FModuleManager::LoadModuleChecked<IBlueprintProfilerInterface>("BlueprintProfiler");
				TSharedPtr<FBlueprintExecutionContext> BlueprintContext = ProfilerModule.FindBlueprintContext(BPGC->GetPathName());
				if (BlueprintContext.IsValid())
				{
					NewPolicy = new FBlueprintProfilerConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, WireHeatMode, InGraphObj, BlueprintContext);
					// When we have an active connection drawing policy, associate utility contexts with the active blueprint execution context.
					if (Blueprint->BlueprintType == BPTYPE_Normal)
					{
						ProfilerModule.AssociateUtilityContexts(BlueprintContext);
					}
				}
			}
		}
	}
	return NewPolicy;
}

/////////////////////////////////////////////////////
// FBlueprintProfilerConnectionDrawingPolicy

FBlueprintProfilerConnectionDrawingPolicy::FBlueprintProfilerConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, EBlueprintProfilerHeatMapDisplayMode InHeatmapType, UEdGraph* InGraphObj, TSharedPtr<FBlueprintExecutionContext> InBlueprintContext)
	: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	, WireHeatMode(InHeatmapType)
	, GraphReference(InGraphObj)
	, BlueprintContext(InBlueprintContext)
{
	AttackWireThickness = 8.f;
	SustainWireThickness = 6.f;
	ReleaseWireThickness = 4.f;
}

void FBlueprintProfilerConnectionDrawingPolicy::BuildExecutionRoadmap()
{
	LatestTimeDiscovered = 0.0;

	// Only do highlighting in PIE or SIE
	if (!CanBuildRoadmap())
	{
		return;
	}

	// Grab relevant profiler interfaces
	const FName InstanceName = BlueprintContext->GetActiveInstanceName();
	TSharedPtr<FBlueprintFunctionContext> FunctionContext = BlueprintContext->GetFunctionContextFromGraph(GraphObj);

	if (!FunctionContext.IsValid())
	{
		// No point in proceeding without a valid function context.
		return;
	}

	TArray<double> SequentialNodeTimes;
	TArray<UEdGraphPin*> SequentialExecPinsInGraph;
	TArray<FTracePath> SequentialTracePathsInGraph;
	TArray<TWeakPtr<FScriptExecutionNode>> SequentialProfilerNodesInGraph;
	const FName ScopedFunctionName = FunctionContext->GetFunctionName();
	const FName GraphName = FunctionContext->GetGraphName();

	const TSimpleRingBuffer<FBlueprintExecutionTrace>& TraceHistory = BlueprintContext->GetTraceHistory();
	for (int32 i = 0; i < TraceHistory.Num(); ++i)
	{
		const FBlueprintExecutionTrace& Sample = TraceHistory(i);
		const bool bInstanceMatch = InstanceName == SPDN_Blueprint ? true : (InstanceName == Sample.InstanceName);
		if (bInstanceMatch && GraphName == Sample.GraphName)
		{
			if (Sample.ProfilerNode.IsValid())
			{
				const bool bSamplePinValid = Sample.PinReference.Get() != nullptr;
				// Patch in any missing pins using the script offset.
				UEdGraphPin* Pin = bSamplePinValid ? Sample.PinReference.Get() : const_cast<UEdGraphPin*>(FunctionContext->GetPinFromCodeLocation(Sample.Offset));

				if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					SequentialProfilerNodesInGraph.Add(Sample.ProfilerNode);
					SequentialNodeTimes.Add(Sample.ObservationTime);
					SequentialExecPinsInGraph.Add(Pin);
					SequentialTracePathsInGraph.Add(Sample.TracePath);
				}
			}
		}
	}
	// Run thru and apply bonus time
	const float InvNumNodes = 1.0f / (float)SequentialNodeTimes.Num();
	for (int32 i = 0; i < SequentialProfilerNodesInGraph.Num(); ++i)
	{
		double& ObservationTime = SequentialNodeTimes[i];

		const float PositionRatio = (SequentialNodeTimes.Num() - i) * InvNumNodes;
		const float PositionBonus = FMath::Pow(PositionRatio, TracePositionExponent) * TracePositionBonusPeriod;
		ObservationTime += PositionBonus;

		LatestTimeDiscovered = FMath::Max<double>(LatestTimeDiscovered, ObservationTime);
	}

	UEdGraphPin* OutputPin = nullptr;
	int32 OutputPinIndex = INDEX_NONE;
	for (int32 i = SequentialExecPinsInGraph.Num() - 1; i > 0; --i)
	{
		UEdGraphPin* InputPin = SequentialExecPinsInGraph[i];
		if (InputPin->Direction == EGPD_Output)
		{
			OutputPin = InputPin;
			OutputPinIndex = i;
		}
		else if (OutputPin)
		{
			bool bLinked = false;
			for (auto LinkedPin : OutputPin->LinkedTo)
			{
				if (LinkedPin == InputPin)
				{
					bLinked = true;
					break;
				}
			}
			if (bLinked)
			{
				TSharedPtr<FScriptExecutionNode> CurExecNode = SequentialProfilerNodesInGraph[OutputPinIndex].Pin();
				TSharedPtr<FScriptExecutionNode> NextExecNode = SequentialProfilerNodesInGraph[i].Pin();

				if (CurExecNode.IsValid() && NextExecNode.IsValid() && CurExecNode != NextExecNode)
				{
					double NextNodeTime = SequentialNodeTimes[i];
					UEdGraphNode* NextNode = const_cast<UEdGraphNode*>(NextExecNode->GetTypedObservedObject<UEdGraphNode>());
					FExecPairingMap& ExecPaths			= PredecessorPins.FindOrAdd(NextNode);
					FTimePair& ExecTiming				= ExecPaths.FindOrAdd(OutputPin);
					FProfilerPairingMap& ProfilerPaths	= ProfilerPins.FindOrAdd(NextNode);
					FProfilerPair& ProfilerData			= ProfilerPaths.FindOrAdd(OutputPin);
					// make sure that if we've already visited this exec-pin (like 
					// in a for-loop or something), that we're replacing it with a 
					// more recent execution time
					//
					// @TODO I don't see when this wouldn't be the case
					if (ExecTiming.ThisExecTime < NextNodeTime)
					{
						double CurNodeTime = SequentialNodeTimes[OutputPinIndex];
						ExecTiming.ThisExecTime = NextNodeTime;
						ExecTiming.PredExecTime = CurNodeTime;
						const bool bExecPin = CurExecNode->HasFlags(EScriptExecutionNodeFlags::ExecPin);
						const FTracePath& CurTracePath = bExecPin ? SequentialTracePathsInGraph[i] : SequentialTracePathsInGraph[OutputPinIndex];
						const FTracePath& NextTracePath = SequentialTracePathsInGraph[i];
						TSharedPtr<FScriptPerfData> CurPerfData = CurExecNode->GetPerfDataByInstanceAndTracePath(InstanceName, CurTracePath);
						TSharedPtr<FScriptPerfData> NextPerfData = NextExecNode->GetPerfDataByInstanceAndTracePath(InstanceName, NextTracePath);
						ProfilerData.bPureNode = CurExecNode->IsPureNode();
						switch(WireHeatMode)
						{
							case EBlueprintProfilerHeatMapDisplayMode::Average:
							case EBlueprintProfilerHeatMapDisplayMode::PinToPin:
							{
								ProfilerData.PredPerfData = CurPerfData->GetAverageHeatLevel();
								ProfilerData.ThisPerfData = NextPerfData->GetAverageHeatLevel();
								break;
							}
							case EBlueprintProfilerHeatMapDisplayMode::Inclusive:
							{
								ProfilerData.PredPerfData = CurPerfData->GetInclusiveHeatLevel();
								ProfilerData.ThisPerfData = NextPerfData->GetInclusiveHeatLevel();
								break;
							}
							case EBlueprintProfilerHeatMapDisplayMode::MaxTiming:
							{
								ProfilerData.PredPerfData = CurPerfData->GetMaxTimeHeatLevel();
								ProfilerData.ThisPerfData = NextPerfData->GetMaxTimeHeatLevel();
								break;
							}
							case EBlueprintProfilerHeatMapDisplayMode::Total:
							{
								ProfilerData.PredPerfData = CurPerfData->GetTotalHeatLevel();
								ProfilerData.ThisPerfData = NextPerfData->GetTotalHeatLevel();
								break;
							}
							case EBlueprintProfilerHeatMapDisplayMode::HottestPath:
							{
								ProfilerData.PredPerfData = CurPerfData->GetHottestPathHeatLevel();
								ProfilerData.ThisPerfData = NextPerfData->GetHottestPathHeatLevel();
								break;
							}
						}
					}
				}
			}
		}
	}
	// Fade only when free-running (since we're using FApp::GetCurrentTime(), instead of FPlatformTime::Seconds)
	const double MaxTimeAhead = FMath::Min(FApp::GetCurrentTime() + 2*TracePositionBonusPeriod, LatestTimeDiscovered); //@TODO: Rough clamping; should be exposed as a parameter
	CurrentTime = FMath::Max(FApp::GetCurrentTime(), MaxTimeAhead);
}

void FBlueprintProfilerConnectionDrawingPolicy::DeterminePerfWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FScriptPerfConnectionParams& Params)
{
	// Initiate using kismet connection drawing policy
	FKismetConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
	// Override the colors using the profiler Data
	static const FLinearColor BaseColor(1.f, 1.f, 1.f, 0.2f);
	Params.WireColor = Params.WireColor2 = BaseColor;
	if (FProfilerPairingMap* ExecMap = ProfilerPins.Find(InputPin->GetOwningNode()))
	{
		if (FProfilerPair* PerfData = ExecMap->Find(OutputPin))
		{
			static const FLinearColor ImpureHeatColor(1.f, 0.f, 0.f, 1.f);
			static const FLinearColor PureHeatColor(0.f, 1.f, 0.f, 1.f);
			Params.PerformanceData1 = FMath::Clamp(PerfData->PredPerfData, 0.f, 1.f);
			Params.PerformanceData2 = FMath::Clamp(PerfData->ThisPerfData, 0.f, 1.f);
			Params.WireColor = FLinearColor::LerpUsingHSV(BaseColor, PerfData->bPureNode ? PureHeatColor : ImpureHeatColor, Params.PerformanceData1);
			Params.WireColor2 = FLinearColor::LerpUsingHSV(BaseColor, PerfData->bPureNode ? PureHeatColor : ImpureHeatColor, Params.PerformanceData2);
		}
	}
}

void FBlueprintProfilerConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes)
{
	// Build the execution roadmap (also populates execution times)
	BuildExecutionRoadmap();

	PinGeometries = &InPinGeometries;

	PinToPinWidgetMap.Empty();
	for (TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator ConnectorIt(InPinGeometries); ConnectorIt; ++ConnectorIt)
	{
		TSharedRef<SWidget> SomePinWidget = ConnectorIt.Key();
		SGraphPin& PinWidget = static_cast<SGraphPin&>(SomePinWidget.Get());

		PinToPinWidgetMap.Add(PinWidget.GetPinObj(), StaticCastSharedRef<SGraphPin>(SomePinWidget));
	}

	for (TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator ConnectorIt(InPinGeometries); ConnectorIt; ++ConnectorIt)
	{
		TSharedRef<SWidget> SomePinWidget = ConnectorIt.Key();
		SGraphPin& PinWidget = static_cast<SGraphPin&>(SomePinWidget.Get());
		UEdGraphPin* ThePin = PinWidget.GetPinObj();

		if (ThePin->Direction == EGPD_Output)
		{
			for (int32 LinkIndex=0; LinkIndex < ThePin->LinkedTo.Num(); ++LinkIndex)
			{
				FArrangedWidget* LinkStartWidgetGeometry = nullptr;
				FArrangedWidget* LinkEndWidgetGeometry = nullptr;

				UEdGraphPin* TargetPin = ThePin->LinkedTo[LinkIndex];

				DetermineLinkGeometry(ArrangedNodes, SomePinWidget, ThePin, TargetPin, /*out*/ LinkStartWidgetGeometry, /*out*/ LinkEndWidgetGeometry);

				if (( LinkEndWidgetGeometry && LinkStartWidgetGeometry ) && !IsConnectionCulled( *LinkStartWidgetGeometry, *LinkEndWidgetGeometry ))
				{
					FScriptPerfConnectionParams Params;
					DeterminePerfWiringStyle(ThePin, TargetPin, Params);
					DrawInterpColorSpline(LinkStartWidgetGeometry->Geometry, LinkEndWidgetGeometry->Geometry, Params);
				}
			}
		}
	}
}

bool FBlueprintProfilerConnectionDrawingPolicy::CanBuildRoadmap() const
{
	return BlueprintContext.IsValid() && BlueprintContext->IsContextValid();
}

void FBlueprintProfilerConnectionDrawingPolicy::DrawInterpColorSpline(const FGeometry& StartGeom, const FGeometry& EndGeom, const FScriptPerfConnectionParams& Params)
{
	//@TODO: These values should be pushed into the Slate style, they are compensating for a bit of
	// empty space inside of the pin brush images.
	const float StartFudgeX = 4.0f;
	const float EndFudgeX = 4.0f;
	const FVector2D StartPoint = FGeometryHelper::VerticalMiddleRightOf(StartGeom) - FVector2D(StartFudgeX, 0.0f);
	const FVector2D EndPoint = FGeometryHelper::VerticalMiddleLeftOf(EndGeom) - FVector2D(ArrowRadius.X - EndFudgeX, 0);

	// Draw the spline
	DrawPerfConnection(WireLayerID, StartPoint, EndPoint, Params);

	// Draw the arrow
	if (ArrowImage)
	{
		FVector2D ArrowPoint = EndPoint - ArrowRadius;

		FSlateDrawElement::MakeBox(
			DrawElementsList,
			ArrowLayerID,
			FPaintGeometry(ArrowPoint, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
			ArrowImage,
			ClippingRect,
			ESlateDrawEffect::None,
			Params.WireColor
			);
	}
}

void FBlueprintProfilerConnectionDrawingPolicy::DrawPerfConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FScriptPerfConnectionParams& Params)
{
	const FVector2D& P0 = Start;
	const FVector2D& P1 = End;

	const FVector2D SplineTangent = ComputeSplineTangent(P0, P1);
	const FVector2D P0Tangent = (Params.StartDirection == EGPD_Output) ? SplineTangent : -SplineTangent;
	const FVector2D P1Tangent = (Params.EndDirection == EGPD_Input) ? SplineTangent : -SplineTangent;

	if (Settings->bTreatSplinesLikePins)
	{
		// Distance to consider as an overlap
		const float QueryDistanceTriggerThresholdSquared = FMath::Square(Settings->SplineHoverTolerance + Params.WireThickness * 0.5f);

		// Distance to pass the bounding box cull test (may want to expand this later on if we want to do 'closest pin' actions that don't require an exact hit)
		const float QueryDistanceToBoundingBoxSquared = QueryDistanceTriggerThresholdSquared;

		bool bCloseToSpline = false;
		{
			// The curve will include the endpoints but can extend out of a tight bounds because of the tangents
			// P0Tangent coefficient maximizes to 4/27 at a=1/3, and P1Tangent minimizes to -4/27 at a=2/3.
			const float MaximumTangentContribution = 4.0f / 27.0f;
			FBox2D Bounds(ForceInit);

			Bounds += FVector2D(P0);
			Bounds += FVector2D(P0 + MaximumTangentContribution * P0Tangent);
			Bounds += FVector2D(P1);
			Bounds += FVector2D(P1 - MaximumTangentContribution * P1Tangent);

			bCloseToSpline = Bounds.ComputeSquaredDistanceToPoint(LocalMousePosition) < QueryDistanceToBoundingBoxSquared;
		}

		if (bCloseToSpline)
		{
			// Find the closest approach to the spline
			FVector2D ClosestPoint(ForceInit);
			float ClosestDistanceSquared = FLT_MAX;

			const int32 NumStepsToTest = 16;
			const float StepInterval = 1.0f / (float)NumStepsToTest;
			FVector2D Point1 = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, 0.0f);
			for (float TestAlpha = 0.0f; TestAlpha < 1.0f; TestAlpha += StepInterval)
			{
				const FVector2D Point2 = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, TestAlpha + StepInterval);

				const FVector2D ClosestPointToSegment = FMath::ClosestPointOnSegment2D(LocalMousePosition, Point1, Point2);
				const float DistanceSquared = (LocalMousePosition - ClosestPointToSegment).SizeSquared();

				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestPoint = ClosestPointToSegment;
				}

				Point1 = Point2;
			}

			// Record the overlap
			if (ClosestDistanceSquared < QueryDistanceTriggerThresholdSquared)
			{
				if (ClosestDistanceSquared < SplineOverlapResult.GetDistanceSquared())
				{
					const float SquaredDistToPin1 = (Params.AssociatedPin1 != nullptr) ? (P0 - ClosestPoint).SizeSquared() : FLT_MAX;
					const float SquaredDistToPin2 = (Params.AssociatedPin2 != nullptr) ? (P1 - ClosestPoint).SizeSquared() : FLT_MAX;

					SplineOverlapResult = FGraphSplineOverlapResult(Params.AssociatedPin1, Params.AssociatedPin2, ClosestDistanceSquared, SquaredDistToPin1, SquaredDistToPin2);
				}
			}
		}
	}

	// Draw the spline itself
	const float WireThickness = Params.WireThickness * ZoomFactor;
	TArray<FSlateGradientStop> Gradients;
	Gradients.Add(FSlateGradientStop(FVector2D::ZeroVector, Params.WireColor));
	Gradients.Add(FSlateGradientStop(FVector2D::ZeroVector, Params.WireColor2));
	
	FSlateDrawElement::MakeDrawSpaceGradientSpline(
		DrawElementsList,
		LayerId,
		P0, P0Tangent,
		P1, P1Tangent,
		ClippingRect,
		Gradients,
		WireThickness,
		ESlateDrawEffect::None);

	if (Params.bDrawBubbles || (MidpointImage != nullptr))
	{
		// This table maps distance along curve to alpha
		FInterpCurve<float> SplineReparamTable;
		const float SplineLength = MakeSplineReparamTable(P0, P0Tangent, P1, P1Tangent, SplineReparamTable);

		// Draw bubbles on the spline
		if (Params.bDrawBubbles)
		{
			const float BubbleSpacing = 64.f * ZoomFactor;
			const float BubbleSpeed = 192.f * ZoomFactor;

			float Time = (FPlatformTime::Seconds() - GStartTime);
			const float BubbleOffset = FMath::Fmod(Time * BubbleSpeed, BubbleSpacing);
			const int32 NumBubbles = FMath::CeilToInt(SplineLength/BubbleSpacing);
			const float SizeMin = WireThickness * 0.05f;
			const float SizeScale = WireThickness * 0.10f;
			const float SizeA = SizeMin + (Params.PerformanceData1 * SizeScale);
			const float SizeB = SizeMin + (Params.PerformanceData2 * SizeScale);

			for (int32 i = 0; i < NumBubbles; ++i)
			{
				const float Distance = ((float)i * BubbleSpacing) + BubbleOffset;
				if (Distance < SplineLength)
				{
					const float Alpha = SplineReparamTable.Eval(Distance, 0.f);
					FVector2D BubblePos = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, Alpha);
					const FVector2D BubbleSize = BubbleImage->ImageSize * FMath::Lerp(SizeA, SizeB, Alpha);
					const FLinearColor ElementColor = FLinearColor::LerpUsingHSV(Params.WireColor, Params.WireColor2, Alpha);
					BubblePos -= (BubbleSize * 0.5f);
					
					FSlateDrawElement::MakeBox(
						DrawElementsList,
						LayerId,
						FPaintGeometry( BubblePos, BubbleSize, ZoomFactor  ),
						BubbleImage,
						ClippingRect,
						ESlateDrawEffect::None,
						ElementColor
						);
				}
			}
		}

		// Draw the midpoint image
		if (MidpointImage != nullptr)
		{
			// Determine the spline position for the midpoint
			const float MidpointAlpha = SplineReparamTable.Eval(SplineLength * 0.5f, 0.f);
			const FVector2D Midpoint = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha);

			// Approximate the slope at the midpoint (to orient the midpoint image to the spline)
			const FVector2D MidpointPlusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha + KINDA_SMALL_NUMBER);
			const FVector2D MidpointMinusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha - KINDA_SMALL_NUMBER);
			const FVector2D SlopeUnnormalized = MidpointPlusE - MidpointMinusE;

			// Draw the arrow
			const FVector2D MidpointDrawPos = Midpoint - MidpointRadius;
			const float AngleInRadians = SlopeUnnormalized.IsNearlyZero() ? 0.0f : FMath::Atan2(SlopeUnnormalized.Y, SlopeUnnormalized.X);

			FLinearColor ElementColor = FLinearColor::LerpUsingHSV(Params.WireColor, Params.WireColor2, 0.5f);
			FSlateDrawElement::MakeRotatedBox(
				DrawElementsList,
				LayerId,
				FPaintGeometry(MidpointDrawPos, MidpointImage->ImageSize * ZoomFactor, ZoomFactor),
				MidpointImage,
				ClippingRect,
				ESlateDrawEffect::None,
				AngleInRadians,
				TOptional<FVector2D>(),
				FSlateDrawElement::RelativeToElement,
				ElementColor
				);
		}
	}
}
void FBlueprintProfilerConnectionDrawingPolicy::ApplyHoverDeemphasis(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ float& Thickness, /*inout*/ FLinearColor& WireColor)
{
	const float FadeInBias = 0.75f; // Time in seconds before the fading starts to occur
	const float FadeInPeriod = 0.6f; // Time in seconds after the bias before the fade is fully complete
	const float TimeFraction = FMath::SmoothStep(0.0f, FadeInPeriod, (float)(FSlateApplication::Get().GetCurrentTime() - LastHoverTimeEvent - FadeInBias));

	const bool bContainsBoth = HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin);
	const bool bContainsOutput = HoveredPins.Contains(OutputPin);
	const bool bEmphasize = bContainsBoth || (bContainsOutput && (InputPin == nullptr));
	if (!bEmphasize)
	{
		Thickness = FMath::Lerp(Thickness, Thickness * 0.3f, TimeFraction);
	}
}
