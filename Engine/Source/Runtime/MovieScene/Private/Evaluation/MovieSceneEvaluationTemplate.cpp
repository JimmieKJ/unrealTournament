// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "MovieSceneSequence.h"

FMovieSceneSequenceCachedSignature::FMovieSceneSequenceCachedSignature(UMovieSceneSequence& InSequence)
	: Sequence(&InSequence)
	, CachedSignature(InSequence.GetSignature())
{
}

#if WITH_EDITORONLY_DATA

void FCachedMovieSceneEvaluationTemplate::Initialize(UMovieSceneSequence& InSequence, FMovieSceneSequenceTemplateStore* InOrigin)
{
	SourceSequence = &InSequence;
	Origin = InOrigin;
}

void FCachedMovieSceneEvaluationTemplate::Regenerate()
{
	Regenerate(CachedCompilationParams);
}

void FCachedMovieSceneEvaluationTemplate::Regenerate(const FMovieSceneTrackCompilationParams& Params)
{
	if (IsOutOfDate(Params))
	{
		RegenerateImpl(Params);
	}
}

void FCachedMovieSceneEvaluationTemplate::ForceRegenerate(const FMovieSceneTrackCompilationParams& Params)
{
	// Reassign the template back to default
	static_cast<FMovieSceneEvaluationTemplate&>(*this) = FMovieSceneEvaluationTemplate();
	RegenerateImpl(Params);
}

void FCachedMovieSceneEvaluationTemplate::RegenerateImpl(const FMovieSceneTrackCompilationParams& Params)
{
	CachedSignatures.Reset();
	CachedCompilationParams = Params;

	if (UMovieSceneSequence* Sequence = SourceSequence.Get())
	{
		FMovieSceneSequenceTemplateStore DefaultStore;
		Sequence->GenerateEvaluationTemplate(*this, Params, Origin ? *Origin : DefaultStore);

		CachedSignatures.Add(FMovieSceneSequenceCachedSignature(*Sequence));
		for (auto& Pair : Hierarchy.AllSubSequenceData())
		{
			if (UMovieSceneSequence* SubSequence = Pair.Value.Sequence)
			{
				CachedSignatures.Add(FMovieSceneSequenceCachedSignature(*SubSequence));
			}
		}
	}
}

bool FCachedMovieSceneEvaluationTemplate::IsOutOfDate(const FMovieSceneTrackCompilationParams& Params) const
{
	if (Params != CachedCompilationParams || CachedSignatures.Num() == 0)
	{
		return true;
	}

	// out of date if the cached signautres contains any out of date sigs
	return CachedSignatures.ContainsByPredicate(
		[](const FMovieSceneSequenceCachedSignature& Sig){
			UMovieSceneSequence* Sequence = Sig.Sequence.Get();
			return !Sequence || Sequence->GetSignature() != Sig.CachedSignature;
		}
	);
}

#endif // WITH_EDITORONLY_DATA

bool FMovieSceneGenerationLedger::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		int32 NumReferenceCounts = 0;

		Ar << NumReferenceCounts;
		for (int32 Index = 0; Index < NumReferenceCounts; ++Index)
		{
			FMovieSceneTrackIdentifier Identifier;
			FMovieSceneTrackIdentifier::StaticStruct()->SerializeItem(Ar, &Identifier, nullptr);

			int32 Count = 0;
			Ar << Count;

			TrackReferenceCounts.Add(Identifier, Count);
		}

		int32 SignatureToTrackIDs = 0;
		Ar << SignatureToTrackIDs;

		for (int32 Index = 0; Index < SignatureToTrackIDs; ++Index)
		{
			FGuid Signature;
			Ar << Signature;

			TArray<FMovieSceneTrackIdentifier, TInlineAllocator<1>>& Identifiers = TrackSignatureToTrackIdentifier.Add(Signature);

			int32 NumIdentifiers = 0;
			Ar << NumIdentifiers;
			for (int32 IdentifierIndex = 0; IdentifierIndex < NumIdentifiers; ++IdentifierIndex)
			{
				FMovieSceneTrackIdentifier Identifier;
				FMovieSceneTrackIdentifier::StaticStruct()->SerializeItem(Ar, &Identifier, nullptr);

				Identifiers.Add(Identifier);
			}
		}

		return true;
	}
	else if (Ar.IsSaving())
	{
		int32 NumReferenceCounts = TrackReferenceCounts.Num();

		Ar << NumReferenceCounts;
		for (auto& Pair : TrackReferenceCounts)
		{
			FMovieSceneTrackIdentifier Identifier = Pair.Key;
			FMovieSceneTrackIdentifier::StaticStruct()->SerializeItem(Ar, &Identifier, nullptr);

			int32 Count = Pair.Value;
			Ar << Count;
		}

		int32 SignatureToTrackIDs = TrackSignatureToTrackIdentifier.Num();
		Ar << SignatureToTrackIDs;

		for (auto& Pair : TrackSignatureToTrackIdentifier)
		{
			FGuid Signature = Pair.Key;
			Ar << Signature;

			int32 NumIdentifiers = Pair.Value.Num();
			Ar << NumIdentifiers;

			for (FMovieSceneTrackIdentifier Identifier : Pair.Value)
			{
				FMovieSceneTrackIdentifier::StaticStruct()->SerializeItem(Ar, &Identifier, nullptr);
			}
		}

		return true;
	}

	return false;
}

TArrayView<FMovieSceneTrackIdentifier> FMovieSceneGenerationLedger::FindTracks(const FGuid& InSignature)
{
	if (auto* Tracks = TrackSignatureToTrackIdentifier.Find(InSignature))
	{
		return *Tracks;
	}
	return TArrayView<FMovieSceneTrackIdentifier>();
}

void FMovieSceneGenerationLedger::AddTrack(const FGuid& InSignature, FMovieSceneTrackIdentifier Identifier)
{
	TrackSignatureToTrackIdentifier.FindOrAdd(InSignature).Add(Identifier);
	++TrackReferenceCounts.FindOrAdd(Identifier);
}

FMovieSceneTrackIdentifier FMovieSceneEvaluationTemplate::AddTrack(const FGuid& InSignature, FMovieSceneEvaluationTrack&& InTrack)
{
	FMovieSceneTrackIdentifier NewIdentifier = ++Ledger.LastTrackIdentifier;
	
	InTrack.SetupOverrides();
	Tracks.Add(NewIdentifier.Value, MoveTemp(InTrack));
	Ledger.AddTrack(InSignature, NewIdentifier);

	return NewIdentifier;
}

void FMovieSceneEvaluationTemplate::RemoveTrack(const FGuid& InSignature)
{
	for (FMovieSceneTrackIdentifier TrackIdentifier : Ledger.FindTracks(InSignature))
	{
		int32* RefCount = Ledger.TrackReferenceCounts.Find(TrackIdentifier);
		if (ensure(RefCount) && --(*RefCount) == 0)
		{
			if (bKeepStaleTracks)
			{
				if (FMovieSceneEvaluationTrack* Track = Tracks.Find(TrackIdentifier.Value))
				{
					StaleTracks.Add(TrackIdentifier.Value, MoveTemp(*Track));
				}
			}
			
			Tracks.Remove(TrackIdentifier.Value);
			Ledger.TrackReferenceCounts.Remove(TrackIdentifier);
		}
	}
	Ledger.TrackSignatureToTrackIdentifier.Remove(InSignature);
}

const TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& FMovieSceneEvaluationTemplate::GetTracks() const
{
	// Reinterpret the uint32 as FMovieSceneTrackIdentifier
	static_assert(sizeof(FMovieSceneTrackIdentifier) == sizeof(uint32), "FMovieSceneTrackIdentifier is not convertible directly to/from uint32.");
	return *reinterpret_cast<const TMap<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>*>(&Tracks);
}

TArrayView<FMovieSceneTrackIdentifier> FMovieSceneEvaluationTemplate::FindTracks(const FGuid& InSignature)
{
	return Ledger.FindTracks(InSignature);
}
