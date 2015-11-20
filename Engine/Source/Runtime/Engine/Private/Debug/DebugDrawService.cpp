// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawService.h"

TArray<TArray<FDebugDrawDelegate> > UDebugDrawService::Delegates;
FEngineShowFlags UDebugDrawService::ObservedFlags(ESFIM_Editor);

UDebugDrawService::UDebugDrawService(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Delegates.Reserve(sizeof(FEngineShowFlags)*8);
}

FDelegateHandle UDebugDrawService::Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate)
{
	check(IsInGameThread());

	int32 Index = FEngineShowFlags::FindIndexByName(Name);

	FDelegateHandle Result;
	if (Index != INDEX_NONE)
	{
		if (Index >= Delegates.Num())
		{
			Delegates.AddZeroed(Index - Delegates.Num() + 1);
		}
		Delegates[Index].Add(NewDelegate);
		Result = Delegates[Index].Last().GetHandle();
		ObservedFlags.SetSingleFlag(Index, true);
	}
	return Result;
}

void UDebugDrawService::Unregister(FDelegateHandle HandleToRemove)
{
	check(IsInGameThread());

	TArray<FDebugDrawDelegate>* DelegatesArray = Delegates.GetData();
	for (int32 Flag = 0; Flag < Delegates.Num(); ++Flag, ++DelegatesArray)
	{
		check(DelegatesArray); //it shouldn't happen, but to be sure
		const uint32 Index = DelegatesArray->IndexOfByPredicate([=](const FDebugDrawDelegate& Delegate){ return Delegate.GetHandle() == HandleToRemove; });
		if (Index != INDEX_NONE)
		{
			DelegatesArray->RemoveAtSwap(Index, 1, false);
			if (DelegatesArray->Num() == 0)
			{
				ObservedFlags.SetSingleFlag(Flag, false);
			}
		}
	}	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, FViewport* Viewport, FSceneView* View, FCanvas* Canvas)
{
	UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(),TEXT("DebugCanvasObject"));
	if (CanvasObject == NULL)
	{
		CanvasObject = NewObject<UCanvas>(GetTransientPackage(), TEXT("DebugCanvasObject"));
		CanvasObject->AddToRoot();
	}

	CanvasObject->Init(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height(), View);
	CanvasObject->Update();	
	CanvasObject->Canvas = Canvas;
	CanvasObject->SetView(View);

	// PreRender the player's view.
	Draw(Flags, CanvasObject);	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, UCanvas* Canvas)
{
	if (Canvas == NULL)
	{
		return;
	}
	
	for (int32 FlagIndex = 0; FlagIndex < Delegates.Num(); ++FlagIndex)
	{
		if (Flags.GetSingleFlag(FlagIndex) && ObservedFlags.GetSingleFlag(FlagIndex) && Delegates[FlagIndex].Num() > 0)
		{
			for (int32 i = Delegates[FlagIndex].Num() - 1; i >= 0; --i)
			{
				FDebugDrawDelegate& Delegate = Delegates[FlagIndex][i];

				if (Delegate.IsBound())
				{
					Delegate.Execute(Canvas, NULL);
				}
				else
				{
					Delegates[FlagIndex].RemoveAtSwap(i, 1, /*bAllowShrinking=*/false);
				}
			}
		}
	}
}

//----------------------------------------------------------------------//
// FDebugShapeElement
//----------------------------------------------------------------------//

FDrawDebugShapeElement::FDrawDebugShapeElement(EDrawDebugShapeElement InType)
	: TransformationMatrix(FMatrix::Identity)
	, Type(InType)
	, Color(0xff)
	, ThicknesOrRadius(0)
{
	// Empty
}

FDrawDebugShapeElement::FDrawDebugShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness)
	: TransformationMatrix(FMatrix::Identity)
	, Type(EDrawDebugShapeElement::Invalid)
	, ThicknesOrRadius(InThickness)
{
	if (InDescription.IsEmpty() == false)
	{
		Description = InDescription;
	}
	SetColor(InColor);
}

void FDrawDebugShapeElement::SetColor(const FColor& InColor)
{
	Color = ((InColor.DWColor() >> 30) << 6) | (((InColor.DWColor() & 0x00ff0000) >> 22) << 4) | (((InColor.DWColor() & 0x0000ff00) >> 14) << 2) | ((InColor.DWColor() & 0x000000ff) >> 6);
}

EDrawDebugShapeElement FDrawDebugShapeElement::GetType() const
{
	return Type;
}

void FDrawDebugShapeElement::SetType(EDrawDebugShapeElement InType)
{
	Type = InType;
}

FColor FDrawDebugShapeElement::GetFColor() const
{
	FColor RetColor(((Color & 0xc0) << 24) | ((Color & 0x30) << 18) | ((Color & 0x0c) << 12) | ((Color & 0x03) << 6));
	RetColor.A = (RetColor.A * 255) / 192; // convert alpha to 0-255 range
	return RetColor;
}

FDrawDebugShapeElement FDrawDebugShapeElement::MakeString(const FString& Description, const FVector& Location)
{
	FDrawDebugShapeElement StringElement(Description, FColor::White, 0);
	StringElement.SetType(EDrawDebugShapeElement::String);
	StringElement.Points.Add(Location);

	return StringElement;
}

FDrawDebugShapeElement FDrawDebugShapeElement::MakeLine(const FVector& Start, const FVector& End, FColor Color, float Thickness)
{
	FDrawDebugShapeElement LineElement(EDrawDebugShapeElement::Segment);
	LineElement.Points.Add(Start);
	LineElement.Points.Add(End);
	LineElement.SetColor(Color);
	LineElement.ThicknesOrRadius = Thickness;

	return LineElement;
}

FDrawDebugShapeElement FDrawDebugShapeElement::MakePoint(const FVector& Location, FColor Color, float Radius)
{
	FDrawDebugShapeElement PointElement(EDrawDebugShapeElement::SinglePoint);
	PointElement.Points.Add(Location);
	PointElement.ThicknesOrRadius = Radius;
	PointElement.SetColor(Color);

	return PointElement;
}

FDrawDebugShapeElement FDrawDebugShapeElement::MakeCylinder(FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color)
{
	FDrawDebugShapeElement ShapeElement(EDrawDebugShapeElement::Cylinder);
	ShapeElement.Points.Add(Start);
	ShapeElement.Points.Add(End);
	ShapeElement.ThicknesOrRadius = Radius;
	ShapeElement.SetColor(Color);

	return ShapeElement;
}
