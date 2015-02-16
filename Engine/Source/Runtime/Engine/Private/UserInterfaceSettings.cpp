// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlayerInput.cpp: Unreal input system.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/RendererSettings.h"
#include "Engine/UserInterfaceSettings.h"


UUserInterfaceSettings::UUserInterfaceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  RenderFocusRule(ERenderFocusRule::NavigationOnly)
{
}

void UUserInterfaceSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if ( HasAnyFlags(RF_ClassDefaultObject) == false )
	{
		LoadCursors();
	}
}

float UUserInterfaceSettings::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	float Scale = 1;

	//if ( UIScaleRule == EUIScalingRule::Custom )
	//{

	//}
	//else
	{
		int32 EvalPoint = 0;
		switch ( UIScaleRule )
		{
		case EUIScalingRule::ShortestSide:
			EvalPoint = FMath::Min(Size.X, Size.Y);
			break;
		case EUIScalingRule::LongestSide:
			EvalPoint = FMath::Max(Size.X, Size.Y);
			break;
		case EUIScalingRule::Horizontal:
			EvalPoint = Size.X;
			break;
		case EUIScalingRule::Vertical:
			EvalPoint = Size.Y;
			break;
		}

		const FRichCurve* DPICurve = UIScaleCurve.GetRichCurveConst();
		Scale = DPICurve->Eval((float)EvalPoint, 1.0f);
	}

	return FMath::Max(Scale, 0.01f);
}

void UUserInterfaceSettings::LoadCursors()
{
	TArray<UObject*> LoadedClasses;
	LoadedClasses.Add(DefaultCursor.TryLoad());
	LoadedClasses.Add(TextEditBeamCursor.TryLoad());
	LoadedClasses.Add(CrosshairsCursor.TryLoad());
	LoadedClasses.Add(GrabHandCursor.TryLoad());
	LoadedClasses.Add(GrabHandClosedCursor.TryLoad());
	LoadedClasses.Add(SlashedCircleCursor.TryLoad());

	for ( UObject* Cursor : LoadedClasses )
	{
		if ( Cursor )
		{
			CursorClasses.Add(Cursor);
		}
	}
}

#if WITH_EDITOR
void UUserInterfaceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( PropertyChangedEvent.Property )
	{
		URendererSettings* RendererSettings = GetMutableDefault<URendererSettings>(URendererSettings::StaticClass());
		RendererSettings->UpdateDefaultConfigFile();
	}
}
#endif // #if WITH_EDITOR
