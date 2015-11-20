// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class USlateMeshData;


/**
 * A widget that draws vertexes provided by a 2.5D StaticMesh.
 * The Mesh's material is used.
 * Hardware instancing is supported.
 */
class UMG_API SMeshWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshWidget)
		: _MeshData(nullptr)
	{}
		/** The StaticMesh asset that should be drawn. */
		SLATE_ARGUMENT(USlateMeshData*, MeshData)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	/**
	 * Draw the InStaticMesh when this widget paints.
	 *
	 * @return the Index of the mesh data that was added; cache this value for use with @see FRenderRun.
	 */
	uint32 AddMesh(const USlateMeshData& InMeshData);

	/** Discard any previous runs and reserve space for new render runs if needed. */
	void ClearRuns(int32 NumRuns);

	/**
	 * Tell the widget to draw instances of a mesh a given number of times starting at
	 * a given offset.
	 *
	 * @param InMeshIndex        Which mesh to draw; returned by @see AddMesh
	 * @param InInstanceOffset   Start drawing with this instance
	 * @param InNumInstances     Draw this many instances
	 */
	FORCEINLINE void AddRenderRun(uint32 InMeshIndex, uint32 InInstanceOffset, uint32 InNumInstances)
	{
		RenderRuns.Add(FRenderRun(InMeshIndex, InInstanceOffset, InNumInstances));
	}

	/** Begin an update to the per instance buffer. Enables hardware instancing. */
	TSharedPtr<FSlateInstanceBufferUpdate> BeginPerInstanceBufferUpdate(uint32 MeshId, int32 InitialSize);

protected:
	// BEGIN SLeafWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	// END SLeafWidget interface

private:

	struct FRenderData
	{
		/** Holds a copy of the Static Mesh's data converted to a format that Slate understands. */
		TArray<FSlateVertex> VertexData;
		/** Connectivity data: Order in which the vertexes occur to make up a series of triangles. */
		TArray<SlateIndex> IndexData;
		/** Holds on to the material that is found on the StaticMesh. */
		TSharedPtr<FSlateBrush> Brush;
		/** A rendering handle used to quickly access the rendering data for the slate element*/
		FSlateResourceHandle RenderingResourceHandle;
		/** Per instance data that can be passed to */
		TSharedPtr<ISlateUpdatableInstanceBuffer> PerInstanceBuffer;
	};
	TArray<FRenderData, TInlineAllocator<3>> RenderData;

	/** Which mesh to draw, starting with which instance offset and how many instances to draw in this run/batch. */
	class FRenderRun
	{
	public:
		FRenderRun(uint32 InMeshIndex, uint32 InInstanceOffset, uint32 InNumInstances)
			: MeshIndex(InMeshIndex)
			, InstanceOffset(InInstanceOffset)
			, NumInstances(InNumInstances)
		{
		}

		uint32 GetMeshIndex() const { return MeshIndex; }
		uint32 GetInstanceOffset() const { return InstanceOffset; }
		uint32 GetNumInstances() const { return NumInstances; }

	private:
		uint32 MeshIndex;
		uint32 InstanceOffset;
		uint32 NumInstances;
	};
	TArray<FRenderRun> RenderRuns;
};