// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class UEdGraphPin;

/////////////////////////////////////////////////////
// FGraphPinHandle

/** A handle to a pin, defined by its owning node's GUID, and the pin's name. Used to reference a pin without referring to its widget */
struct GRAPHEDITOR_API FGraphPinHandle
{
	/** The GUID of the node to which this pin belongs */
	FGuid NodeGuid;

	/** The GUID of the pin we are referencing */
	FGuid PinId;

	/** Constructor */
	FGraphPinHandle(UEdGraphPin* InPin);

	/** Find a pin widget in the specified panel from this handle */
	TSharedPtr<class SGraphPin> FindInGraphPanel(const class SGraphPanel& InPanel) const;
};

/////////////////////////////////////////////////////
// FGraphSplineOverlapResult

struct GRAPHEDITOR_API FGraphSplineOverlapResult
{
public:
	FGraphSplineOverlapResult()
		: Pin1Handle(nullptr)
		, Pin2Handle(nullptr)
		, BestPinHandle(nullptr)
		, Pin1(nullptr)
		, Pin2(nullptr)
		, DistanceSquared(FLT_MAX)
		, DistanceSquaredToPin1(FLT_MAX)
		, DistanceSquaredToPin2(FLT_MAX)
	{
	}

	FGraphSplineOverlapResult(UEdGraphPin* InPin1, UEdGraphPin* InPin2, float InDistanceSquared, float InDistanceSquaredToPin1, float InDistanceSquaredToPin2)
		: Pin1Handle(InPin1)
		, Pin2Handle(InPin2)
		, BestPinHandle(nullptr)
		, Pin1(InPin1)
		, Pin2(InPin2)
		, DistanceSquared(InDistanceSquared)
		, DistanceSquaredToPin1(InDistanceSquaredToPin1)
		, DistanceSquaredToPin2(InDistanceSquaredToPin2)
	{
	}

	bool IsValid() const
	{
		return DistanceSquared < FLT_MAX;
	}

	void ComputeBestPin();

	float GetDistanceSquared() const
	{
		return DistanceSquared;
	}

	TSharedPtr<class SGraphPin> GetBestPinWidget(const class SGraphPanel& InGraphPanel) const
	{
		TSharedPtr<class SGraphPin> Result;
		if (IsValid())
		{
			Result = BestPinHandle.FindInGraphPanel(InGraphPanel);
		}
		return Result;
	}

	bool GetPins(const class SGraphPanel& InGraphPanel, UEdGraphPin*& OutPin1, UEdGraphPin*& OutPin2) const;

protected:
	FGraphPinHandle Pin1Handle;
	FGraphPinHandle Pin2Handle;
	FGraphPinHandle BestPinHandle;
	UEdGraphPin* Pin1;
	UEdGraphPin* Pin2;
	float DistanceSquared;
	float DistanceSquaredToPin1;
	float DistanceSquaredToPin2;
};
