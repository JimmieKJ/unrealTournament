// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "CommonMovieSceneTools.h"

/** Converts a matinee interpolation mode to its equivalent rich curve interpolation mode. */
ERichCurveInterpMode MatineeImportTools::MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_Constant:
		return ERichCurveInterpMode::RCIM_Constant;
	case CIM_CurveAuto:
	case CIM_CurveAutoClamped:
	case CIM_CurveBreak:
	case CIM_CurveUser:
		return ERichCurveInterpMode::RCIM_Cubic;
	case CIM_Linear:
		return ERichCurveInterpMode::RCIM_Linear;
	default:
		return ERichCurveInterpMode::RCIM_None;
	}
}


/** Converts a matinee interpolation mode to its equivalent rich curve tangent mode. */
ERichCurveTangentMode MatineeImportTools::MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_CurveAuto:
	case CIM_CurveAutoClamped:
		return ERichCurveTangentMode::RCTM_Auto;
	case CIM_CurveBreak:
		return ERichCurveTangentMode::RCTM_Break;
	case CIM_CurveUser:
		return ERichCurveTangentMode::RCTM_User;
	default:
		return ERichCurveTangentMode::RCTM_None;
	}
}


/** Adds a key to a rich curve based on matinee curve key data. */
void MatineeImportTools::SetOrAddKey( FRichCurve& Curve, float Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode )
{
	FKeyHandle KeyHandle = Curve.AddKey( Time, Value, false, Curve.FindKey( Time ) );
	FRichCurveKey& Key = Curve.GetKey( KeyHandle );
	Key.ArriveTangent = ArriveTangent;
	Key.LeaveTangent = LeaveTangent;
	Key.InterpMode = MatineeInterpolationToRichCurveInterpolation( MatineeInterpMode );
	Key.TangentMode = MatineeInterpolationToRichCurveTangent( MatineeInterpMode );
}
