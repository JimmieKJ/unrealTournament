// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MatineeImportTools.h"

#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackAnimControl.h"

#include "MovieSceneFloatTrack.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieSceneParticleTrack.h"
#include "MovieSceneSkeletalAnimationTrack.h"

#include "MovieSceneFloatSection.h"
#include "MovieScene3DTransformSection.h"
#include "MovieSceneSkeletalAnimationSection.h"


ERichCurveInterpMode FMatineeImportTools::MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode )
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


ERichCurveTangentMode FMatineeImportTools::MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_CurveBreak:
		return ERichCurveTangentMode::RCTM_Break;
	case CIM_CurveUser:
		return ERichCurveTangentMode::RCTM_User;
	default:
		return ERichCurveTangentMode::RCTM_Auto;
	}
}


bool FMatineeImportTools::TryConvertMatineeToggleToOutParticleKey( ETrackToggleAction ToggleAction, EParticleKey::Type& OutParticleKey )
{
	switch ( ToggleAction )
	{
	case ETrackToggleAction::ETTA_On:
		OutParticleKey = EParticleKey::Activate;
		return true;
	case ETrackToggleAction::ETTA_Off:
		OutParticleKey = EParticleKey::Deactivate;
		return true;
	case ETrackToggleAction::ETTA_Trigger:
		OutParticleKey = EParticleKey::Trigger;
		return true;
	}
	return false;
}


void FMatineeImportTools::SetOrAddKey( FRichCurve& Curve, float Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode )
{
	FKeyHandle KeyHandle = Curve.AddKey( Time, Value, false, Curve.FindKey( Time ) );
	FRichCurveKey& Key = Curve.GetKey( KeyHandle );
	Key.ArriveTangent = ArriveTangent;
	Key.LeaveTangent = LeaveTangent;
	Key.InterpMode = MatineeInterpolationToRichCurveInterpolation( MatineeInterpMode );
	Key.TangentMode = MatineeInterpolationToRichCurveTangent( MatineeInterpMode );
}


void FMatineeImportTools::CopyInterpFloatTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFloatTrack", "Paste Matinee float track" ) );
	bool bSectionCreated = false;

	FloatTrack->Modify();

	float KeyTime = MatineeFloatTrack->GetKeyframeTime( 0 );
	UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>( MovieSceneHelpers::FindSectionAtTime( FloatTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneFloatSection>( FloatTrack->CreateNewSection() );
		FloatTrack->AddSection( *Section );
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& FloatCurve = Section->GetFloatCurve();
		for ( const auto& Point : MatineeFloatTrack->FloatTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( FloatCurve, Point.InVal, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );

		if ( bSectionCreated )
		{
			Sequencer->NotifyMovieSceneDataChanged();
		}
	}
}


void FMatineeImportTools::CopyInterpMoveTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrack", "Paste Matinee move track" ) );
	bool bSectionCreated = false;

	TransformTrack->Modify();

	float KeyTime = MoveTrack->GetKeyframeTime( 0 );
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>( MovieSceneHelpers::FindSectionAtTime( TransformTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieScene3DTransformSection>( TransformTrack->CreateNewSection() );
		Section->GetScaleCurve(EAxis::X).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Y).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Z).SetDefaultValue(1);
		TransformTrack->AddSection( *Section );
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& TranslationXCurve = Section->GetTranslationCurve( EAxis::X );
		FRichCurve& TranslationYCurve = Section->GetTranslationCurve( EAxis::Y );
		FRichCurve& TranslationZCurve = Section->GetTranslationCurve( EAxis::Z );

		for ( const auto& Point : MoveTrack->PosTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( TranslationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( TranslationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( TranslationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		FRichCurve& RotationXCurve = Section->GetRotationCurve( EAxis::X );
		FRichCurve& RotationYCurve = Section->GetRotationCurve( EAxis::Y );
		FRichCurve& RotationZCurve = Section->GetRotationCurve( EAxis::Z );

		for ( const auto& Point : MoveTrack->EulerTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( RotationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( RotationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( RotationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );

		if ( bSectionCreated )
		{
			Sequencer->NotifyMovieSceneDataChanged();
		}
	}
}


void FMatineeImportTools::CopyInterpParticleTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeParticleTrack", "Paste Matinee particle track" ) );
	bool bSectionCreated = false;

	ParticleTrack->Modify();

	float KeyTime = MatineeToggleTrack->GetKeyframeTime( 0 );
	UMovieSceneParticleSection* Section = Cast<UMovieSceneParticleSection>( MovieSceneHelpers::FindSectionAtTime( ParticleTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneParticleSection>( ParticleTrack->CreateNewSection() );
		ParticleTrack->AddSection( *Section );
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FIntegralCurve& ParticleCurve = Section->GetParticleCurve();
		for ( const auto& Key : MatineeToggleTrack->ToggleTrack )
		{
			EParticleKey::Type ParticleKey;
			if ( TryConvertMatineeToggleToOutParticleKey( Key.ToggleAction, ParticleKey ) )
			{
				ParticleCurve.AddKey( Key.Time, (int32)ParticleKey, ParticleCurve.FindKey( Key.Time ) );
			}
			SectionMin = FMath::Min( SectionMin, Key.Time );
			SectionMax = FMath::Max( SectionMax, Key.Time );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );

		if ( bSectionCreated )
		{
			Sequencer->NotifyMovieSceneDataChanged();
		}
	}
}


void FMatineeImportTools::CopyInterpAnimControlTrack( TSharedRef<ISequencer> Sequencer, UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack )
{
	// @todo - Sequencer - Add support for slot names once they are implemented.
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeAnimTrack", "Paste Matinee anim track" ) );

	SkeletalAnimationTrack->Modify();
	SkeletalAnimationTrack->RemoveAllAnimationData();

	for (int32 i = 0; i < MatineeAnimControlTrack->AnimSeqs.Num(); i++)
	{
		const auto& AnimSeq = MatineeAnimControlTrack->AnimSeqs[i];

		float EndTime;
		if( AnimSeq.bLooping )
		{
			if( i < MatineeAnimControlTrack->AnimSeqs.Num() - 1 )
			{
				EndTime = MatineeAnimControlTrack->AnimSeqs[i + 1].StartTime;
			}
			else
			{
				EndTime = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue();
			}
		}
		else
		{
			EndTime = AnimSeq.StartTime + ( ( ( AnimSeq.AnimSeq->SequenceLength - AnimSeq.AnimEndOffset ) - AnimSeq.AnimStartOffset ) / AnimSeq.AnimPlayRate );
		}

		UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>( SkeletalAnimationTrack->CreateNewSection() );
		NewSection->SetStartTime( AnimSeq.StartTime );
		NewSection->SetEndTime( EndTime );
		NewSection->SetStartOffset( AnimSeq.AnimStartOffset );
		NewSection->SetEndOffset( AnimSeq.AnimEndOffset );
		NewSection->SetPlayRate( AnimSeq.AnimPlayRate );
		NewSection->SetAnimSequence( AnimSeq.AnimSeq );

		SkeletalAnimationTrack->AddSection( *NewSection );
	}

	Sequencer->NotifyMovieSceneDataChanged();
}
