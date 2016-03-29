// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

class UTexture2D;
class UPrimitiveComponent;

/**
	* Checks whether a UTexture2D is supposed to be streaming.
	* @param Texture	Texture to check
	* @return			true if the UTexture2D is supposed to be streaming
	*/
static bool IsStreamingTexture( const UTexture2D* Texture2D )
{
	return Texture2D && Texture2D->bIsStreamable && Texture2D->NeverStream == false && Texture2D->GetNumMips() > UTexture2D::GetMinTextureResidentMipCount();
}

// Main Thread Job Requirement : find all instance of a component and update it's bound.
// Threaded Job Requirement : get the list of instance texture easily from the list of 

// Can be used either for static primitives or dynamic primitives
class FTextureInstanceManager
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

		/** X size of the bounds box extent of 4 texture instances */
		FVector4 ExtentX;
		/** Y size of the bounds box extent of 4 texture instances */
		FVector4 ExtentY;
		/** Z size of the bounds box extent of 4 texture instances */
		FVector4 ExtentZ;

		/** Sphere radii for the bounding sphere of 4 texture instances */
		FVector4 Radius;
		/** Minimal distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MinDistanceSq;
		/** Maximal distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MaxDistanceSq;

		/** Last visibility time for this bound, used for priority */
		FVector4 LastRenderTime;

		FORCEINLINE_DEBUGGABLE void Set(int32 Index, const FBoxSphereBounds& Bounds, float LastRenderTime, float MinDistance = 0, float MaxDistance = MAX_FLT);

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

		// Components are always updated as a whole, so individual elements can not be removed.
		int32 NextComponentLink;	// The next element that uses the same component as this Element. The first one is referred by ComponentMap and the last one will have INDEX_NONE.
	};

public:

	FTextureInstanceManager(bool InAllowNullComponents = false) : bAllowNullComponents(InAllowNullComponents), FirstDirtyComponentIndex(0) {}

	int32 AddBounds(const FBoxSphereBounds& Bounds, const UPrimitiveComponent* Component, float LastRenderTime, float MinDistance = 0 , float MaxDistance = MAX_FLT);

	void AddElement(const UPrimitiveComponent* Component, const UTexture2D* Texture, int BoundsIndex, float TexelFactor, int32*& ComponentLink);

	// Returns the next elements using the same component.
	void RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture);

	// Will also remove bounds
	void AddComponent(const UPrimitiveComponent* Component);
	void RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures);

	// Update the component bounds, assumed that this would only be used with component using component bounds
	void UpdateBounds(const UPrimitiveComponent* Component);

	bool Contains(const UPrimitiveComponent* Component) const { return ComponentMap.Contains(Component); }

	void IncrementalUpdateBounds(float Percentage);

private:

	void RemoveBounds(int32 Index);

private:

	TArray<FBounds4> Bounds4;
	TArray<FElement> Elements;

	TArray<int32> FreeBoundIndices;
	TArray<int32> FreeElementIndices;

	TMap<const UTexture2D*, int32> TextureMap;
	TMap<const UPrimitiveComponent*, int32> ComponentMap;

	bool bAllowNullComponents;

	// Used to implement iterative bounds update and visibility update.
	int32 FirstDirtyComponentIndex; // Ranges from 0 to Components.Num()
	TArray<const UPrimitiveComponent*> Bounds4Components; // The components mapped to Bounds4

	friend class FTextureBoundsVisibility;
};

// Data used to compute visibility
class FTextureBoundsVisibility
{
public:

	void Set(const FTextureInstanceManager& Instances);
	void QuickSet(FTextureInstanceManager& Instances);

	void ComputeBoundsViewInfos(const TArray<FStreamingViewInfo>& ViewInfos, bool bUseAABB, float MaxEffectiveScreenSize);

	// MaxSize_InRange : Biggest instance size for in range views
	// MaxSize_AnyRange : Biggest instance size not considering range.
	bool GetTexelSize(const UTexture2D* InTexture, float& MaxSize_InRange, float& MinDistanceSq_InRange, float& MaxSize_AnyRange, float& MinDistanceSq_AnyRange) const;

private:

	typedef FTextureInstanceManager::FBounds4 FBounds4;
	typedef FTextureInstanceManager::FElement FElement;

	TArray<FBounds4> Bounds4;
	TArray<FElement> Elements;
	TMap<const UTexture2D*, int32> TextureMap;

	struct FBoundsViewInfo4
	{
		FVector4 ScreenSizeOverDistance;
		FVector4 DistanceSq;
		FVector4 InRangeMask;
	};

	// Normalized Texel Factors for each bounds and view.
	// @TODO : store data for different views continuously to improve reads.
	TArray< TArray<FBoundsViewInfo4> > BoundsViewInfo4;
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

	void IncrementalTick(FRemovedTextureArray& RemovedTextures, float Percentage);

	const FTextureInstanceManager& GetTextureInstances() const { return TextureInstances; }
	FTextureInstanceManager& GetTextureInstances() { return TextureInstances; }

	bool IsDeleted(const UPrimitiveComponent* Component) const 
	{ 	
		const FComponentState* State = ComponentStates.Find(Component);
		return !State || State->bToDelete;
	}

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
	FTextureInstanceManager TextureInstances;
};
