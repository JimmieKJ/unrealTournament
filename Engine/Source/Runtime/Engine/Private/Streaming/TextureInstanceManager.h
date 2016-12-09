// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "ContentStreaming.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Components/PrimitiveComponent.h"

#define MAX_TEXTURE_SIZE (float(1 << (MAX_TEXTURE_MIP_COUNT - 1)))


class FStreamingTextureLevelContext;
class ULevel;
class UPrimitiveComponent;
class UTexture2D;

class FTextureInstanceState;

class FUpdateLastRenderTimeTask : public FNonAbandonableTask
{
public:
	/** Task arguments. */
	struct FArguments
	{
		FArguments(const TRefCountPtr<FTextureInstanceState>& InLevelState, int32 InFirstIndex, int32 InLastIndex) : LevelState(InLevelState), FirstIndex(InFirstIndex), LastIndex(InLastIndex) {}

		// The state to update.
		TRefCountPtr<FTextureInstanceState> LevelState;
		// First bound index to udpate.
		int32 FirstIndex;
		// Last bound index to update.
		int32 LastIndex;
	};

	/** Initialization constructor. */

	void Reserve(int32 MaxNumLevels) { LevelArgs.Empty(MaxNumLevels); }
	bool HasLevelData() const { return LevelArgs.Num() != 0; }

	void Push(const FArguments& InArg) { LevelArgs.Push(InArg); }

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FUpdateLastRenderTimeTask, STATGROUP_ThreadPoolAsyncTasks); }

private:

	/** Task arguments per level update. */
	TArray<FArguments> LevelArgs;
};

class FAsyncUpdateLastRenderTimeTask : public FAsyncTask<FUpdateLastRenderTimeTask>, public FRefCountedObject
{};

// Main Thread Job Requirement : find all instance of a component and update it's bound.
// Threaded Job Requirement : get the list of instance texture easily from the list of 

// Can be used either for static primitives or dynamic primitives
class FTextureInstanceState : public FRefCountedObject
{
public:

	// The bounds are into their own arrays to allow SIMD-friendly processing
	struct FBounds4
	{
		FORCEINLINE FBounds4();

		/** X coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginX;
		/** Y coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginY;
		/** Z coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginZ;

		/** X coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginX;
		/** Y coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginY;
		/** Z coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginZ;

		/** X size of the bounds box extent of 4 texture instances */
		FVector4 ExtentX;
		/** Y size of the bounds box extent of 4 texture instances */
		FVector4 ExtentY;
		/** Z size of the bounds box extent of 4 texture instances */
		FVector4 ExtentZ;

		/** Sphere radii for the bounding sphere of 4 texture instances */
		FVector4 Radius;

		/** The relative box the bound was computed with */
		FUintVector4 PackedRelativeBox;

		/** Minimal distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MinDistanceSq;
		/** Minimal range distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MinRangeSq;
		/** Maximal range distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MaxRangeSq;

		/** Last visibility time for this bound, used for priority */
		FVector4 LastRenderTime; //(FApp::GetCurrentTime() - Component->LastRenderTime);


		FORCEINLINE_DEBUGGABLE void Set(int32 Index, const FBoxSphereBounds& Bounds, uint32 InPackedRelativeBox, float LastRenderTime, const FVector& RangeOrigin, float MinDistance, float MinRange, float MaxRange);
		FORCEINLINE_DEBUGGABLE void UnpackBounds(int32 Index, const FBoxSphereBounds& Bounds);
		FORCEINLINE_DEBUGGABLE void FullUpdate(int32 Index, const FBoxSphereBounds& Bounds, float LastRenderTime);
		FORCEINLINE_DEBUGGABLE void Update(int32 Index, float LastRenderTime);

		// Clears entry between 0 and 4
		FORCEINLINE void Clear(int32 Index);
	};

	struct FElement
	{
		FORCEINLINE FElement();

		const UPrimitiveComponent* Component; // Which component this relates too
		const UTexture2D* Texture;	// Texture, never dereferenced.

		int32 BoundsIndex;		// The Index associated to this component (static component can have several bounds).
		float TexelFactor;		// The texture scale to be applied to this instance.

		int32 PrevTextureLink;	// The previous element which uses the same texture as this Element. The first element referred by TextureMap will have INDEX_NONE.
		int32 NextTextureLink;	// The next element which uses the same texture as this Element. Last element will have INDEX_NONE

		// Components are always updated as a whole, so individual elements can not be removed. Removing the need for PrevComponentLink.
		int32 NextComponentLink;	// The next element that uses the same component as this Element. The first one is referred by ComponentMap and the last one will have INDEX_NONE.
	};


	/**
 	 * FCompiledElement is a stripped down version of element and is stored in an array instead of using a linked list.
     * It is only used when the data is not expected to change and reduce that cache cost of iterating on all elements.
     **/
	struct FCompiledElement
	{
		FCompiledElement() {}
		FCompiledElement(int32 InBoundsIndex, float InTexelFactor) : BoundsIndex(InBoundsIndex), TexelFactor(InTexelFactor) {}

		int32 BoundsIndex;
		float TexelFactor;

		FORCEINLINE bool operator==(const FCompiledElement& Rhs) const { return BoundsIndex == Rhs.BoundsIndex && TexelFactor == Rhs.TexelFactor; }
	};


	// Iterator processing all elements refering to a texture.
	class FTextureLinkConstIterator
	{
	public:
		FTextureLinkConstIterator(const FTextureInstanceState& InState, const UTexture2D* InTexture);

		FORCEINLINE operator bool() const { return CurrElementIndex != INDEX_NONE; }
		FORCEINLINE void operator++() { CurrElementIndex = State.Elements[CurrElementIndex].NextTextureLink; }

		void OutputToLog(float MaxNormalizedSize, float MaxNormalizedSize_VisibleOnly, const TCHAR* Prefix) const;

		FORCEINLINE int32 GetBoundsIndex() const { return State.Elements[CurrElementIndex].BoundsIndex; }
		FORCEINLINE float GetTexelFactor() const { return State.Elements[CurrElementIndex].TexelFactor; }

	protected:

		FORCEINLINE const UPrimitiveComponent* GetComponent() const { return State.Elements[CurrElementIndex].Component; }
		FBoxSphereBounds GetBounds() const;

		const FTextureInstanceState& State;
		int32 CurrElementIndex;
	};

	class FTextureLinkIterator : public FTextureLinkConstIterator
	{
	public:
		FTextureLinkIterator(FTextureInstanceState& InState, const UTexture2D* InTexture) : FTextureLinkConstIterator(InState, InTexture) {}

		FORCEINLINE void ClampTexelFactor(float CMin, float CMax)
		{ 
			float& TexelFactor = const_cast<FTextureInstanceState&>(State).Elements[CurrElementIndex].TexelFactor;
			TexelFactor = FMath::Clamp<float>(TexelFactor, CMin, CMax);
		}
	};

	class FTextureIterator
	{
	public:
		FTextureIterator(const FTextureInstanceState& InState) : MapIt(InState.TextureMap) {}

		operator bool() const { return (bool)MapIt; }
		void operator++() { ++MapIt; }

		const UTexture2D* operator*() const { return MapIt.Key(); }

	private:

		TMap<const UTexture2D*, int32>::TConstIterator MapIt;
	};

public:

	~FTextureInstanceState()
	{
		WaitForAsyncUpdateLastRenderTimeTask();
	}

	// Will also remove bounds
	bool AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext);
	bool AddComponent(const UPrimitiveComponent* Component);

	void RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures);
	bool RemoveComponentReferences(const UPrimitiveComponent* Component);

	bool Contains(const UPrimitiveComponent* Component) const { return ComponentMap.Contains(Component); }

	void UpdateBounds(const UPrimitiveComponent* Component);

	bool UpdateBounds(int32 BoundIndex);
	void UpdateLastRenderTime(int32 BoundIndex);
	void UpdateAsyncUpdateLastRenderTimeTask(const TRefCountPtr<FAsyncUpdateLastRenderTimeTask>& InAsyncUpdateLastRenderTimeTask);

	FORCEINLINE int32 NumBounds() const { return Bounds4Components.Num(); }
	FORCEINLINE int32 NumBounds4() const { return Bounds4.Num(); }

	FORCEINLINE const FBounds4& GetBounds4(int32 Bounds4Index ) const {  return Bounds4[Bounds4Index]; }

	FORCEINLINE FTextureLinkIterator GetElementIterator(const UTexture2D* InTexture ) {  return FTextureLinkIterator(*this, InTexture); }
	FORCEINLINE FTextureLinkConstIterator GetElementIterator(const UTexture2D* InTexture ) const {  return FTextureLinkConstIterator(*this, InTexture); }
	FORCEINLINE FTextureIterator GetTextureIterator( ) const {  return FTextureIterator(*this); }

	uint32 GetAllocatedSize() const;

	// Generate the compiled elements.
	int32 CompileElements();

	// Whether or not this state has compiled elements.
	bool HasCompiledElements() const { return CompiledTextureMap.Num() != 0; }

	// If this has compiled elements, return the array relate to a given texture.
	const TArray<FCompiledElement>* GetCompiledElements(const UTexture2D* Texture) const { return CompiledTextureMap.Find(Texture); }

	int32 CheckRegistrationAndUnpackBounds();

	/** Move around one bound to free the last bound indices. This allows to keep the number of dynamic bounds low. */
	void MoveBound(int32 OldBoundIndex, int32 NewBoundIndex);
	void TrimBounds();

	FORCEINLINE bool HasComponent(int32 BoundIndex) const { return Bounds4Components[BoundIndex] != nullptr; }

	void WaitForAsyncUpdateLastRenderTimeTask();

private:

	void AddElement(const UPrimitiveComponent* Component, const UTexture2D* Texture, int BoundsIndex, float TexelFactor, int32*& ComponentLink);

	// Returns the next elements using the same component.
	void RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture);

	int32 AddBounds(const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, const UPrimitiveComponent* Component, float LastRenderTime, const FVector4& RangeOrigin, float MinDistance, float MinRange, float MaxRange);

	FORCEINLINE int32 AddBounds(const UPrimitiveComponent* Component)
	{
		return AddBounds(Component->Bounds, PackedRelativeBox_Identity, Component, Component->LastRenderTimeOnScreen, Component->Bounds.Origin, 0, 0, FLT_MAX);
	}

	void RemoveBounds(int32 Index);

private:

	/** Whether inserting / removing and resizing is forbidden. Happens when the state is shared between the different threads. */
	TRefCountPtr<FAsyncUpdateLastRenderTimeTask> AsyncUpdateLastRenderTimeTask;

	TArray<FBounds4> Bounds4;

	/** 
	 * Components related to each of the Bounds4 elements. This is stored in another array to allow 
	 * passing Bounds4 to the threaded task without loosing the bound components, allowing incremental update.
	 */
	TArray<const UPrimitiveComponent*> Bounds4Components;

	TArray<FElement> Elements;

	TArray<int32> FreeBoundIndices;
	TArray<int32> FreeElementIndices;

	TMap<const UTexture2D*, int32> TextureMap;
	
	// CompiledTextureMap is used to iterate more quickly on each elements by avoiding the linked list indirections.
	TMap<const UTexture2D*, TArray<FCompiledElement> > CompiledTextureMap;

	TMap<const UPrimitiveComponent*, int32> ComponentMap;

	friend class FTextureLinkIterator;
	friend class FTextureIterator;
	friend class FDynamicTextureInstanceManager;
};

class FStaticTextureInstanceManager
{
public:

	FStaticTextureInstanceManager() : State(new FTextureInstanceState()), DirtyIndex(0) {}

	// Update the component bounds, assumed that this would only be used with component using component bounds
	void IncrementalUpdate(const TRefCountPtr<FAsyncUpdateLastRenderTimeTask>& InAsyncUpdateLastRenderTimeTask, float Percentage);

	FORCEINLINE int32 CheckRegistrationAndUnpackBounds() { return State->CheckRegistrationAndUnpackBounds(); }
	FORCEINLINE bool AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext) { return State->AddComponent(Component, LevelContext); }
	FORCEINLINE void RemoveComponentReferences(const UPrimitiveComponent* Component) { State->RemoveComponentReferences(Component); }

	// Because static data does not change, there is no need to duplicate the states.
	TRefCountPtr<const FTextureInstanceState> GetAsyncState() 
	{ 
		WaitForAsyncTasks();
		DirtyIndex = 0; // Triggers a refresh.
		return TRefCountPtr<const FTextureInstanceState>(State.GetReference()); 
	}

	FORCEINLINE FTextureInstanceState::FTextureIterator GetTextureIterator( ) const {  return State->GetTextureIterator(); }

	int32 NormalizeLightmapTexelFactor();
	FORCEINLINE int32 CompileElements() { return State->CompileElements(); }

	uint32 GetAllocatedSize() const
	{
		return State.IsValid() ? (sizeof(FTextureInstanceState) + State->GetAllocatedSize()) : 0;
	}

	void WaitForAsyncTasks()
	{
		if (State.IsValid())
		{
			State->WaitForAsyncUpdateLastRenderTimeTask();
		}
	}

private:

	/** The texture instances. Shared with the async task. */
	TRefCountPtr<FTextureInstanceState> State;

	/** Ranges from 0 to Bounds4Components.Num(). Used in the incremental update to update visibility. */
	int32 DirtyIndex;
};

class FDynamicTextureInstanceManager
{
public:

	FDynamicTextureInstanceManager() : State(new FTextureInstanceState()), DirtyIndex(0) {}

	// Update the component bounds, assumed that this would only be used with component using component bounds
	FORCEINLINE void UpdateBounds(const UPrimitiveComponent* Component) { State->UpdateBounds(Component); }
	void IncrementalUpdate(float Percentage);

	// Will also remove bounds
	FORCEINLINE bool AddComponent(const UPrimitiveComponent* Component) { return State->AddComponent(Component); }
	FORCEINLINE void RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures) { State->RemoveComponent(Component, RemovedTextures); }

	TRefCountPtr<const FTextureInstanceState> GetAsyncState();

	uint32 GetAllocatedSize() const 
	{ 
		return State.IsValid() ? (sizeof(FTextureInstanceState) + State->GetAllocatedSize()) : 0;
	}

private:

	/** The texture instances. Shared with the async task. */
	TRefCountPtr<FTextureInstanceState> State;

	/** Ranges from 0 to Bounds4Components.Num(). Used in the incremental update to update bounds and visibility. */
	int32 DirtyIndex;
};


// Data used to compute visibility
class FTextureInstanceAsyncView
{
public:

	FTextureInstanceAsyncView() {}

	FTextureInstanceAsyncView(const TRefCountPtr<const FTextureInstanceState>& InState) : State(InState) {}

	void UpdateBoundSizes_Async(const TArray<FStreamingViewInfo>& ViewInfos, float LastUpdateTime, const FTextureStreamingSettings& Settings);

	// MaxSize : Biggest texture size for all instances.
	// MaxSize_VisibleOnly : Biggest texture size for visble instances only.
	void GetTexelSize(const UTexture2D* InTexture, float& MaxSize, float& MaxSize_VisibleOnly, const TCHAR* LogPrefix) const;

	bool HasTextureReferences(const UTexture2D* InTexture) const;

private:

	TRefCountPtr<const FTextureInstanceState> State;

	struct FBoundsViewInfo
	{
		/** The biggest normalized size (ScreenSize / Distance) accross all view.*/
		float MaxNormalizedSize;
		/*
		 * The biggest normalized size accross all view for visible instances only.
		 * Visible instances are the one that are in range and also that have been seen recently.
		 */
		float MaxNormalizedSize_VisibleOnly;
	};

	// Normalized Texel Factors for each bounds and view. This is the data built by ComputeBoundsViewInfos
	// @TODO : store data for different views continuously to improve reads.
	TArray<FBoundsViewInfo> BoundsViewInfo;

	FORCEINLINE_DEBUGGABLE void ProcessElement(const FBoundsViewInfo& BoundsVieWInfo, float TexelFactor, float& MaxSize, float& MaxSize_VisibleOnly) const;
};


// Needs to be able to remove to defragment every time there are 10% free elements
// Defragmentation is time sliced. So we only need update a small percentage each frame.
// To reduce the number of reallocations, low indices must be used first and high last.

// Tick is timesliced. This requires Component to be valid.
class FDynamicComponentTextureManager
{
public:

	// Add a component entry, without actually processing the component now.
	void Add(const UPrimitiveComponent* Component, EDynamicPrimitiveType DynamicType, FRemovedTextureArray& RemovedTextures);

	// Remove the component immediately, removing all links. Only keep the dynamic type as reference.
	void Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures);

	void Update(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures);

	void IncrementalUpdate(FRemovedTextureArray& RemovedTextures, float Percentage);

	const FDynamicTextureInstanceManager& GetTextureInstances() const { return DynamicInstances; }

	FORCEINLINE FTextureInstanceAsyncView GetAsyncView() { return FTextureInstanceAsyncView(DynamicInstances.GetAsyncState()); }

	bool IsDeleted(const UPrimitiveComponent* Component) const 
	{ 	
		const FComponentState* State = ComponentStates.Find(Component);
		return !State || State->bToDelete;
	}

	uint32 GetAllocatedSize() const;

#if !UE_BUILD_SHIPPING
	// Get all (non removed) components refered by the manager. Debug only.
	void GetAllComponents(TArray<const UPrimitiveComponent*>& Components) const;
#endif

private:

	struct FComponentState
	{
		FComponentState(EDynamicPrimitiveType InDynamicType) : Data(0) 
		{ 
			DynamicType = InDynamicType; 
		}

		union 
		{
			uint32 Data;
			struct 
			{
				uint32 DynamicType : 8;
				uint32 bHasTextures : 1; // True if this is a new entry for which to update the texture lists.
				uint32 bToDelete : 1;	// True if this entry needs to be deleted in the next update.
			};
		};
	};

	TMap<const UPrimitiveComponent*, FComponentState> ComponentStates;
	TSet<const UPrimitiveComponent*> DirtyComponents; // Components that have a dirty states (to accelerate the incremental update).
	FDynamicTextureInstanceManager DynamicInstances;
};

// The streaming data of a level.
class FLevelTextureManager
{
public:

	FLevelTextureManager(ULevel* InLevel) : Level(InLevel), bIsInitialized(false), BuildStep(EStaticBuildStep::BuildTextureLookUpMap) {}

	ULevel* GetLevel() const { return Level; }

	// Remove the whole level
	void Remove(FDynamicComponentTextureManager& DynamicManager, FRemovedTextureArray& RemovedTextures);

	// Invalidate a component reference.

	void RemoveActorReferences(const AActor* Actor)
	{
		StaticActorsWithNonStaticPrimitives.RemoveSingleSwap(Actor); 
		UnprocessedStaticActors.RemoveSingleSwap(Actor); 
	}

	void RemoveComponentReferences(const UPrimitiveComponent* Component) 
	{ 
		// Check everywhere as the mobility can change in game.
		StaticInstances.RemoveComponentReferences(Component); 
		DynamicComponents.RemoveSingleSwap(Component); 
		UnprocessedStaticComponents.RemoveSingleSwap(Component); 
		PendingInsertionStaticPrimitives.RemoveSingleSwap(Component); 
	}

	const FStaticTextureInstanceManager& GetStaticInstances() const { return StaticInstances; }

	float GetWorldTime() const;

	FORCEINLINE FTextureInstanceAsyncView GetAsyncView() { return FTextureInstanceAsyncView(StaticInstances.GetAsyncState()); }

	void IncrementalUpdate(const TRefCountPtr<FAsyncUpdateLastRenderTimeTask>& InAsyncUpdateLastRenderTimeTask, FDynamicComponentTextureManager& DynamicManager, FRemovedTextureArray& RemovedTextures, int64& NumStepsLeftForIncrementalBuild, float Percentage, bool bUseDynamicStreaming);

	uint32 GetAllocatedSize() const;

	bool IsInitialized() const { return bIsInitialized; }

private:

	ULevel* Level;

	bool bIsInitialized;

	FStaticTextureInstanceManager StaticInstances;

	/** The list of dynamic components contained in the level. Used to removed them from the FDynamicComponentTextureManager on ::Remove. */
	TArray<const UPrimitiveComponent*> DynamicComponents;

	/** The static actors that had not only dynamic static components. */
	TArray<const AActor*> StaticActorsWithNonStaticPrimitives;

	/** Incremental build implementation. */

	enum class EStaticBuildStep : uint8
	{
		BuildTextureLookUpMap,
		GetActors,
		GetComponents,
		ProcessComponents,
		NormalizeLightmapTexelFactors,
		CompileElements,
		WaitForRegistration,
		Done,
	};

	// The current step of the incremental build.
	EStaticBuildStep BuildStep;
	// The actors / components left to be processed in ProcessComponents
	TArray<const AActor*> UnprocessedStaticActors;
	TArray<const UPrimitiveComponent*> UnprocessedStaticComponents;
	// The components that could not be processed by the incremental build.
	TArray<const UPrimitiveComponent*> PendingInsertionStaticPrimitives;
	// Reversed lookup for ULevel::StreamingTextureGuids.
	TMap<FGuid, int32> TextureGuidToLevelIndex;


	bool NeedsIncrementalBuild(int32 NumStepsLeftForIncrementalBuild) const;
	void IncrementalBuild(FStreamingTextureLevelContext& LevelContext, bool bForceCompletion, int64& NumStepsLeft);
};
