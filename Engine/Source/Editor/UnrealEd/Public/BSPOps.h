// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __BSPOPS_H__
#define __BSPOPS_H__

class FBSPOps
{
public:
	/** Quality level for rebuilding Bsp. */
	enum EBspOptimization
	{
		BSP_Lame,
		BSP_Good,
		BSP_Optimal
	};

	/** Possible positions of a child Bsp node relative to its parent (for BspAddToNode) */
	enum ENodePlace 
	{
		NODE_Back		= 0, // Node is in back of parent              -> Bsp[iParent].iBack.
		NODE_Front		= 1, // Node is in front of parent             -> Bsp[iParent].iFront.
		NODE_Plane		= 2, // Node is coplanar with parent           -> Bsp[iParent].iPlane.
		NODE_Root		= 3, // Node is the Bsp root and has no parent -> Bsp[0].
	};

	UNREALED_API static void csgPrepMovingBrush( ABrush* Actor );
	UNREALED_API static void csgCopyBrush( ABrush* Dest, ABrush* Src, uint32 PolyFlags, EObjectFlags ResFlags, bool bNeedsPrep, bool bCopyPosRotScale, bool bAllowEmpty = false );
	UNREALED_API static ABrush*	csgAddOperation( ABrush* Actor, uint32 PolyFlags, EBrushType BrushType );

	static int32 bspAddVector( UModel* Model, FVector* V, bool Exact );
	static int32 bspAddPoint( UModel* Model, FVector* V, bool Exact );
	UNREALED_API static void bspBuild( UModel* Model, enum EBspOptimization Opt, int32 Balance, int32 PortalBias, int32 RebuildSimplePolys, int32 iNode );
	UNREALED_API static void bspRefresh( UModel* Model, bool NoRemapSurfs );

	UNREALED_API static void bspBuildBounds( UModel* Model );

	static void bspValidateBrush( UModel* Brush, bool ForceValidate, bool DoStatusUpdate );
	UNREALED_API static void bspUnlinkPolys( UModel* Brush );
	static int32 bspAddNode( UModel* Model, int32 iParent, enum ENodePlace ENodePlace, uint32 NodeFlags, FPoly* EdPoly );

	/**
	 * Rebuild some brush internals
	 */
	UNREALED_API static void RebuildBrush(UModel* Brush);

	static FPoly BuildInfiniteFPoly( UModel* Model, int32 iNode );

	/**
	 * Rotates the specified brush's vertices.
	 */
	static void RotateBrushVerts(ABrush* Brush, const FRotator& Rotation, bool bClearComponents);

	/** Called when an AVolume shape is changed*/
	static void HandleVolumeShapeChanged(AVolume& Volume);

	/** Errors encountered in Csg operation. */
	static int32 GErrors;
	static bool GFastRebuild;

protected:
	static void SplitPolyList
	(
		UModel				*Model,
		int32                 iParent,
		ENodePlace			NodePlace,
		int32                 NumPolys,
		FPoly				**PolyList,
		EBspOptimization	Opt,
		int32					Balance,
		int32					PortalBias,
		int32					RebuildSimplePolys
	);
};

/** @todo: remove when uses of ENodePlane have been replaced with FBSPOps::ENodePlace. */
typedef FBSPOps::ENodePlace ENodePlace;

#endif // __BSPOPS_H__
