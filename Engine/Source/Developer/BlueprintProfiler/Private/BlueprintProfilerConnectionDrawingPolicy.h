// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/GraphEditor/Public/BlueprintConnectionDrawingPolicy.h"
#include "Editor/Kismet/Public/Profiler/BlueprintProfilerSettings.h"

/////////////////////////////////////////////////////
// FBlueprintProfilerPinConnectionFactory

struct FBlueprintProfilerPinConnectionFactory : public FGraphPanelPinConnectionFactory
{
public:
    virtual ~FBlueprintProfilerPinConnectionFactory() {}

	// FGraphPanelPinConnectionFactory
    virtual class FConnectionDrawingPolicy* CreateConnectionPolicy(const class UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	// ~FGraphPanelPinConnectionFactory

};

/////////////////////////////////////////////////////
// FScriptPerfConnectionParams

struct FScriptPerfConnectionParams : public FConnectionParams
{
	float PerformanceData1;
	float PerformanceData2;
	FLinearColor WireColor2;

	FScriptPerfConnectionParams()
		: PerformanceData1(0.f)
		, PerformanceData2(0.f)
		, WireColor2(FLinearColor::White)
	{
	}
};

/////////////////////////////////////////////////////
// FBlueprintProfilerConnectionDrawingPolicy

class FBlueprintProfilerConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
protected:

	// Profiler data for one execution pair within the current graph
	struct FProfilerPair
	{
		bool bPureNode;
		float PredPerfData;
		float ThisPerfData;

		FProfilerPair()
			: PredPerfData(0.f)
			, ThisPerfData(0.f)
		{
		}
	};

	typedef TMap<UEdGraphPin*, FProfilerPair> FProfilerPairingMap;

	// Map of executed nodes, and the execution pins that lead to them being ran
	TMap<UEdGraphNode*, FProfilerPairingMap> ProfilerPins;

public:

	FBlueprintProfilerConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, EBlueprintProfilerHeatMapDisplayMode InHeatmapType, UEdGraph* InGraphObj);

	// FKismetConnectionDrawingPolicy
	virtual void BuildExecutionRoadmap() override;
	// ~FKismetConnectionDrawingPolicy

	// FConnectionDrawingPolicy interface
	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& PinGeometries, FArrangedChildren& ArrangedNodes) override;
	// End of FConnectionDrawingPolicy interface

private:

	/** Determine wiring style for interpolated performance wires */
	void DeterminePerfWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FScriptPerfConnectionParams& Params);

	/** Draw interpolated color spline */
	void DrawInterpColorSpline(const FGeometry& StartGeom, const FGeometry& EndGeom, const FScriptPerfConnectionParams& Params);

	/** Draw performance connection */
	void DrawPerfConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FScriptPerfConnectionParams& Params);

private:

	/** The current wire heat map mode */
	EBlueprintProfilerHeatMapDisplayMode WireHeatMode;
	/** The current graph */
	TWeakObjectPtr<UEdGraph> GraphReference;

};
