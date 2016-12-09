// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine/UserInterfaceSettings.h"

#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

#include "Engine/DPICustomScalingRule.h"

#define LOCTEXT_NAMESPACE "Engine"

UUserInterfaceSettings::UUserInterfaceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RenderFocusRule(ERenderFocusRule::NavigationOnly)
	, ApplicationScale(1)
	, bLoadWidgetsOnDedicatedServer(true)
{
	SectionName = TEXT("UI");
}

void UUserInterfaceSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Allow the assets to be replaced in the editor, but make sure they're part of the root set in cooked games
#if WITH_EDITOR
	if ( HasAnyFlags(RF_ClassDefaultObject) == false )
	{
		ForceLoadResources();
	}
#else
	ForceLoadResources();
#endif
}

float UUserInterfaceSettings::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	float Scale = 1;

	if ( UIScaleRule == EUIScalingRule::Custom )
	{
		if ( CustomScalingRuleClassInstance == nullptr )
		{
			CustomScalingRuleClassInstance = CustomScalingRuleClass.TryLoadClass<UDPICustomScalingRule>();

			if ( CustomScalingRuleClassInstance == nullptr )
			{
				FMessageLog("MapCheck").Error()
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("CustomScalingRule_NotFound", "Project Settings - User Interface Custom Scaling Rule '{0}' could not be found."), FText::FromString(CustomScalingRuleClass.ToString()))));
				return 1;
			}
		}
		
		if ( CustomScalingRule == nullptr )
		{
			CustomScalingRule = CustomScalingRuleClassInstance->GetDefaultObject<UDPICustomScalingRule>();
		}

		Scale = CustomScalingRule->GetDPIScaleBasedOnSize(Size);
	}
	else
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

	return FMath::Max(Scale * ApplicationScale, 0.01f);
}

void UUserInterfaceSettings::ForceLoadResources()
{
	TArray<UObject*> LoadedClasses;
	LoadedClasses.Add(DefaultCursor.TryLoad());
	LoadedClasses.Add(TextEditBeamCursor.TryLoad());
	LoadedClasses.Add(CrosshairsCursor.TryLoad());
	LoadedClasses.Add(GrabHandCursor.TryLoad());
	LoadedClasses.Add(HandCursor.TryLoad());
	LoadedClasses.Add(GrabHandClosedCursor.TryLoad());
	LoadedClasses.Add(SlashedCircleCursor.TryLoad());

	for ( UObject* Cursor : LoadedClasses )
	{
		if ( Cursor )
		{
			CursorClasses.Add(Cursor);
		}
	}

	CustomScalingRuleClassInstance = CustomScalingRuleClass.TryLoadClass<UDPICustomScalingRule>();
}

#undef LOCTEXT_NAMESPACE
