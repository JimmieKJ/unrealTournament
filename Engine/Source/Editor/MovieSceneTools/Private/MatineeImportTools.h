// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Matinee/InterpTrackToggle.h"
#include "MovieSceneParticleSection.h"

class UInterpTrackFloatBase;
class UInterpTrackMove;
class UInterpTrackToggle;
class UInterpTrackAnimControl;

class UMovieSceneFloatTrack;
class UMovieScene3DTransformTrack;
class UMovieSceneParticleTrack;
class UMovieSceneSkeletalAnimationTrack;

class FMatineeImportTools
{
public:

	/** Converts a matinee interpolation mode to its equivalent rich curve interpolation mode. */
	static ERichCurveInterpMode MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode );

	/** Converts a matinee interpolation mode to its equivalent rich curve tangent mode. */
	static ERichCurveTangentMode MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode );

	/** Tries to convert a matinee toggle to a particle key. */
	static bool TryConvertMatineeToggleToOutParticleKey( ETrackToggleAction ToggleAction, EParticleKey::Type& OutParticleKey );

	/** Adds a key to a rich curve based on matinee curve key data. */
	static void SetOrAddKey( FRichCurve& Curve, float Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode );

	/** Copies keys from a matinee float track to a sequencer float track. */
	static void CopyInterpFloatTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack );

	/** Copies keys from a matinee move track to a sequencer transform track. */
	static void CopyInterpMoveTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack );

	/** Copies keys from a matinee toggle track to a sequencer particle track. */
	static void CopyInterpParticleTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack );

	/** Copies keys from a matinee anim control track to a sequencer skeletal animation track. */
	static void CopyInterpAnimControlTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack );
};
