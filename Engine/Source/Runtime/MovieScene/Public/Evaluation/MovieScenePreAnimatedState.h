// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "MovieSceneExecutionToken.h"
#include "MovieSceneSection.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "UObject/ObjectKey.h"

class IMovieScenePlayer;

enum class ECapturePreAnimatedState : uint8
{
	None,
	Global,
	Entity,
};

struct IMovieScenePreAnimatedState
{
	virtual void EntityHasAnimatedObject(FMovieSceneEvaluationKey EntityKey, FObjectKey Object) = 0;
	virtual void EntityHasAnimatedMaster(FMovieSceneEvaluationKey EntityKey) = 0;
};

struct FMovieSceneEntityAndAnimTypeID
{
	FMovieSceneEvaluationKey EntityKey;
	FMovieSceneAnimTypeID AnimTypeID;

	friend bool operator==(const FMovieSceneEntityAndAnimTypeID& A, const FMovieSceneEntityAndAnimTypeID& B)
	{
		return A.AnimTypeID == B.AnimTypeID && A.EntityKey == B.EntityKey;
	}
};

/** Saved state for animation bound to particular animated objects */
struct FMovieSceneSavedObjectTokens
{
	FMovieSceneSavedObjectTokens(UObject& InObject)
		: Object(&InObject)
	{}
	
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMovieSceneSavedObjectTokens(FMovieSceneSavedObjectTokens&&) = default;
	FMovieSceneSavedObjectTokens& operator=(FMovieSceneSavedObjectTokens&&) = default;
#else
	FMovieSceneSavedObjectTokens(FMovieSceneSavedObjectTokens&& RHS)
	{
		*this = MoveTemp(RHS);
	}
	FMovieSceneSavedObjectTokens& operator=(FMovieSceneSavedObjectTokens&& RHS)
	{
		AnimatedGlobalTypeIDs = MoveTemp(RHS.AnimatedGlobalTypeIDs);
		AnimatedEntityTypeIDs = MoveTemp(RHS.AnimatedEntityTypeIDs);
		GlobalStateTokens = MoveTemp(RHS.GlobalStateTokens);
		EntityTokens = MoveTemp(RHS.EntityTokens);
		Object = MoveTemp(RHS.Object);
		return *this;
	}
#endif

	MOVIESCENE_API void Add(ECapturePreAnimatedState CaptureMode, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey AssociatedKey, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject, IMovieScenePreAnimatedState& Parent);

	MOVIESCENE_API void Restore(IMovieScenePlayer& Player);

	MOVIESCENE_API void Restore(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter);

	void RestoreEntity(IMovieScenePlayer& Player, FMovieSceneEvaluationKey EntityKey)
	{
		TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AnimTypesToRestore;

		for (int32 LUTIndex = AnimatedEntityTypeIDs.Num() - 1; LUTIndex >= 0; --LUTIndex)
		{
			FMovieSceneEntityAndAnimTypeID EntityAndAnimType = AnimatedEntityTypeIDs[LUTIndex];
			if (EntityAndAnimType.EntityKey == EntityKey)
			{
				AnimTypesToRestore.Add(EntityAndAnimType.AnimTypeID);
				AnimatedEntityTypeIDs.RemoveAtSwap(LUTIndex);
			}
		}

		UObject* ObjectPtr = Object.Get();
		for (int32 TokenIndex = EntityTokens.Num() - 1; TokenIndex >= 0; --TokenIndex)
		{
			FRefCountedPreAnimatedObjectToken& Token = EntityTokens[TokenIndex];
			if (AnimTypesToRestore.Contains(Token.AnimTypeID) && --Token.RefCount == 0)
			{
				if (ObjectPtr)
				{
					Token.Token->RestoreState(*ObjectPtr, Player);
				}
				EntityTokens.RemoveAtSwap(TokenIndex);
			}
		}
	}

	void Reset()
	{
		AnimatedGlobalTypeIDs.Reset();
		AnimatedEntityTypeIDs.Reset();
		GlobalStateTokens.Reset();
		EntityTokens.Reset();
	}

private:

	/** Array of type IDs for cache efficient searching. Must have 1-1 index mapping into GlobalStateTokens. */
	TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AnimatedGlobalTypeIDs;
	TArray<FMovieSceneEntityAndAnimTypeID, TInlineAllocator<8>> AnimatedEntityTypeIDs;

	/** Tokens mapped to specific anim type IDs. Can only ever be one at a time. */
	TArray<IMovieScenePreAnimatedTokenPtr> GlobalStateTokens;

	/** Tokens mapped to specific anim type IDs. Can only ever be one at a time. */
	struct FRefCountedPreAnimatedObjectToken
	{
		FRefCountedPreAnimatedObjectToken(IMovieScenePreAnimatedTokenPtr&& InToken, FMovieSceneAnimTypeID InAnimTypeID) : AnimTypeID(InAnimTypeID), RefCount(1), Token(MoveTemp(InToken)) {}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
		FRefCountedPreAnimatedObjectToken(FRefCountedPreAnimatedObjectToken&&) = default;
		FRefCountedPreAnimatedObjectToken& operator=(FRefCountedPreAnimatedObjectToken&&) = default;
#else
		FRefCountedPreAnimatedObjectToken(FRefCountedPreAnimatedObjectToken&& RHS)
			: AnimTypeID(RHS.AnimTypeID)
			, RefCount(RHS.RefCount)
			, Token(MoveTemp(RHS.Token))
		{
		}

		FRefCountedPreAnimatedObjectToken& operator=(FRefCountedPreAnimatedObjectToken&& RHS)
		{
			AnimTypeID = MoveTemp(RHS.AnimTypeID);
			RefCount = RHS.RefCount;
			Token = MoveTemp(RHS.Token);
			return *this;
		}
#endif

		/** The type of the animation */
		FMovieSceneAnimTypeID AnimTypeID;

		/** The number of entities that are referencing this data */
		uint32 RefCount;

		/** The token that defines how to restore this object's state */
		IMovieScenePreAnimatedTokenPtr Token;
	};
	TArray<FRefCountedPreAnimatedObjectToken> EntityTokens;

	/** The object that was animated */
	TWeakObjectPtr<> Object;
};


/** Saved state for animation bound to particular animated */
struct FMovieSceneSavedMasterTokens
{
	FMovieSceneSavedMasterTokens(){}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMovieSceneSavedMasterTokens(FMovieSceneSavedMasterTokens&&) = default;
	FMovieSceneSavedMasterTokens& operator=(FMovieSceneSavedMasterTokens&&) = default;
#else
	FMovieSceneSavedMasterTokens(FMovieSceneSavedMasterTokens&& RHS)
	{
		*this = MoveTemp(RHS);
	}
	FMovieSceneSavedMasterTokens& operator=(FMovieSceneSavedMasterTokens&& RHS)
	{
		AnimatedGlobalTypeIDs = MoveTemp(RHS.AnimatedGlobalTypeIDs);
		AnimatedEntityTypeIDs = MoveTemp(RHS.AnimatedEntityTypeIDs);
		GlobalStateTokens = MoveTemp(RHS.GlobalStateTokens);
		EntityTokens = MoveTemp(RHS.EntityTokens);
		return *this;
	}
#endif

	MOVIESCENE_API void Add(ECapturePreAnimatedState CaptureMode, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey AssociatedKey, const IMovieScenePreAnimatedGlobalTokenProducer& Producer, IMovieScenePreAnimatedState& Parent);

	MOVIESCENE_API void Restore(IMovieScenePlayer& Player);

	void RestoreEntity(IMovieScenePlayer& Player, FMovieSceneEvaluationKey EntityKey)
	{
		TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AnimTypesToRestore;

		for (int32 LUTIndex = AnimatedEntityTypeIDs.Num() - 1; LUTIndex >= 0; --LUTIndex)
		{
			FMovieSceneEntityAndAnimTypeID EntityAndAnimType = AnimatedEntityTypeIDs[LUTIndex];
			if (EntityAndAnimType.EntityKey == EntityKey)
			{
				AnimTypesToRestore.Add(EntityAndAnimType.AnimTypeID);
				AnimatedEntityTypeIDs.RemoveAtSwap(LUTIndex);
			}
		}

		for (int32 TokenIndex = EntityTokens.Num() - 1; TokenIndex >= 0; --TokenIndex)
		{
			FRefCountedPreAnimatedGlobalToken& Token = EntityTokens[TokenIndex];
			if (AnimTypesToRestore.Contains(Token.AnimTypeID) && --Token.RefCount == 0)
			{
				Token.Token->RestoreState(Player);
				EntityTokens.RemoveAtSwap(TokenIndex);
			}
		}
	}

	void Reset()
	{
		AnimatedGlobalTypeIDs.Reset();
		AnimatedEntityTypeIDs.Reset();
		GlobalStateTokens.Reset();
		EntityTokens.Reset();
	}

private:

	TArray<FMovieSceneAnimTypeID, TInlineAllocator<8>> AnimatedGlobalTypeIDs;
	TArray<FMovieSceneEntityAndAnimTypeID, TInlineAllocator<8>> AnimatedEntityTypeIDs;

	/** Tokens mapped to specific anim type IDs. Can only ever be one at a time. */
	TArray<IMovieScenePreAnimatedGlobalTokenPtr> GlobalStateTokens;

	/** Tokens mapped to specific anim type IDs. Can only ever be one at a time. */
	struct FRefCountedPreAnimatedGlobalToken
	{
		FRefCountedPreAnimatedGlobalToken(IMovieScenePreAnimatedGlobalTokenPtr&& InToken, FMovieSceneAnimTypeID InAnimTypeID) : AnimTypeID(InAnimTypeID), RefCount(1), Token(MoveTemp(InToken)) {}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
		FRefCountedPreAnimatedGlobalToken(FRefCountedPreAnimatedGlobalToken&&) = default;
		FRefCountedPreAnimatedGlobalToken& operator=(FRefCountedPreAnimatedGlobalToken&&) = default;
#else
		FRefCountedPreAnimatedGlobalToken(FRefCountedPreAnimatedGlobalToken&& RHS)
			: AnimTypeID(RHS.AnimTypeID)
			, RefCount(RHS.RefCount)
			, Token(MoveTemp(RHS.Token))
		{
		}
		FRefCountedPreAnimatedGlobalToken& operator=(FRefCountedPreAnimatedGlobalToken&& RHS)
		{
			AnimTypeID = MoveTemp(RHS.AnimTypeID);
			RefCount = RHS.RefCount;
			Token = MoveTemp(RHS.Token);
			return *this;
		}
#endif

		/** The type of the animation */
		FMovieSceneAnimTypeID AnimTypeID;

		/** The number of entities that are referencing this data */
		uint32 RefCount;

		/** The token that defines how to restore the state */
		IMovieScenePreAnimatedGlobalTokenPtr Token;
	};
	TArray<FRefCountedPreAnimatedGlobalToken> EntityTokens;
};

/**
 * Class that caches pre-animated state for objects that were manipulated by sequencer
 */
class FMovieScenePreAnimatedState : IMovieScenePreAnimatedState
{
public:

	/**
	 * Default construction
	 */
	FMovieScenePreAnimatedState()
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::None;
	}

	FMovieScenePreAnimatedState(const FMovieScenePreAnimatedState&) = delete;
	FMovieScenePreAnimatedState& operator=(const FMovieScenePreAnimatedState&) = delete;

	/**
	 * Check whether we're currently caching pre-animated state at a global level
	 */
	bool IsGlobalCaptureEnabled() const
	{
		return DefaultGlobalCaptureMode == ECapturePreAnimatedState::Global;
	}

	/**
	 * Enable this cache and allow it to start caching state
	 */
	void EnableGlobalCapture()
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::Global;
	}
	
	/**
	 * Disable this cache, preventing it from caching state
	 */
	void DisableGlobalCapture()
	{
		DefaultGlobalCaptureMode = ECapturePreAnimatedState::None;
	}

public:

	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject)
	{
		SavePreAnimatedState(InTokenType, Producer, InObject, CurrentCaptureState, CapturingStateFor);
	}

	FORCEINLINE void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& Producer)
	{
		SavePreAnimatedState(InTokenType, Producer, CurrentCaptureState, CapturingStateFor);
	}

	void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject, ECapturePreAnimatedState CaptureState, FMovieSceneEvaluationKey CaptureEntity)
	{
		if (CaptureState == ECapturePreAnimatedState::None)
		{
			return;
		}

		FObjectKey ObjectKey(&InObject);

		FMovieSceneSavedObjectTokens* Container = ObjectTokens.Find(ObjectKey);
		if (!Container)
		{
			Container = &ObjectTokens.Add(ObjectKey, FMovieSceneSavedObjectTokens(InObject));
		}

		Container->Add(CaptureState, InTokenType, CaptureEntity, Producer, InObject, *this);
	}

	void SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& Producer, ECapturePreAnimatedState CaptureState, FMovieSceneEvaluationKey CaptureEntity)
	{
		if (CaptureState == ECapturePreAnimatedState::None)
		{
			return;
		}

		MasterTokens.Add(CaptureState, InTokenType, CaptureEntity, Producer, *this);
	}

	void RestorePreAnimatedState(IMovieScenePlayer& Player, const FMovieSceneEvaluationKey& Key)
	{
		auto* AnimatedObjects = EntityToAnimatedObjects.Find(Key);
		if (!AnimatedObjects)
		{
			return;
		}

		for (FObjectKey ObjectKey : *AnimatedObjects)
		{
			if (ObjectKey == FObjectKey())
			{
				MasterTokens.RestoreEntity(Player, Key);
			}
			else if (FMovieSceneSavedObjectTokens* FoundState = ObjectTokens.Find(ObjectKey))
			{
				FoundState->RestoreEntity(Player, Key);
			}
		}

		EntityToAnimatedObjects.Remove(Key);
	}

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player);

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object);

	MOVIESCENE_API void RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter);

public:

	void SetCaptureEntity(FMovieSceneEvaluationKey InEntity, EMovieSceneCompletionMode InCompletionMode)
	{
		CapturingStateFor = InEntity;

		switch(InCompletionMode)
		{
		case EMovieSceneCompletionMode::RestoreState: 	CurrentCaptureState = ECapturePreAnimatedState::Entity;	break;
		case EMovieSceneCompletionMode::KeepState: 		CurrentCaptureState = DefaultGlobalCaptureMode;	break;
		}
	}

private:

	virtual void EntityHasAnimatedObject(FMovieSceneEvaluationKey EntityKey, FObjectKey ObjectKey) override
	{
		EntityToAnimatedObjects.FindOrAdd(EntityKey).Add(ObjectKey);
	}

	virtual void EntityHasAnimatedMaster(FMovieSceneEvaluationKey EntityKey) override
	{
		EntityToAnimatedObjects.FindOrAdd(EntityKey).Add(FObjectKey());
	}

	TMap<FObjectKey, FMovieSceneSavedObjectTokens> ObjectTokens;
	FMovieSceneSavedMasterTokens MasterTokens;

	TMap<FMovieSceneEvaluationKey, TArray<FObjectKey, TInlineAllocator<4>>> EntityToAnimatedObjects;

	FMovieSceneEvaluationKey CapturingStateFor;

	ECapturePreAnimatedState CurrentCaptureState;
	ECapturePreAnimatedState DefaultGlobalCaptureMode;
};
