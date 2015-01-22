// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.

#ifndef FRACTURE_TOOLS_H
#define FRACTURE_TOOLS_H

#include "foundation/Px.h"
#include "ExplicitHierarchicalMesh.h"

PX_PUSH_PACK_DEFAULT

namespace physx
{
	namespace apex
	{
		struct IntersectMesh;
		class NxDestructibleAsset;
	}
}

namespace FractureTools
{

/**
	These parameters are passed into the fracturing functions to guide mesh processing.
*/
struct MeshProcessingParameters
{
	/**
		If this is true, separate mesh islands will be turned into separate chunks.
		Default = false.
	*/
	bool mIslandGeneration;

	/**
		If this is true, all T-junctions will be removed from the mesh.
		Default = false.
	*/
	bool mRemoveTJunctions;

	/**
		The mesh is initially scaled to fit in a unit cube, then (if gridSize is not
		zero), the vertices of the scaled mesh are snapped to a grid of size 1/gridSize.
		A power of two is recommended.
		Default = 65536.
	*/
	unsigned mMicrogridSize;

	/**
		Debug output verbosity level.  The higher the number, the more messages are output.
		Default = 0.
	*/
	int mVerbosity;

	/** Constructor sets defaults */
	MeshProcessingParameters()
	{
		setToDefault();
	}

	/** Set default values */
	void	setToDefault()
	{
		mIslandGeneration = false;
		mRemoveTJunctions = false;
		mMicrogridSize = 65536;
		mVerbosity = 0;
	}
};


/**
	Interface to a "cutout set," used with chippable fracturing.  A cutout set is created from a bitmap.  The
	result is turned into cutouts which are applied to the mesh.  For example, a bitmap which looks like a brick
	pattern will generate a cutout for each "brick," forming the cutout set.

	Each cutout is a 2D entity, meant to be projected onto various faces of a mesh.  They are represented
	by a set of 2D vertices, which form closed loops.  More than one loop may represent a single cutout, if
	the loops are forced to be convex.  Otherwise, a cutout is represented by a single loop.
*/
class ICutoutSet
{
public:
	/** Returns the number of cutouts in the set. */
	virtual physx::PxU32			getCutoutCount() const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the number of vertices in the cutout.
	*/
	virtual physx::PxU32			getCutoutVertexCount(physx::PxU32 cutoutIndex) const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the number of loops in this cutout.
	*/
	virtual physx::PxU32			getCutoutLoopCount(physx::PxU32 cutoutIndex) const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the vertex indexed by vertexIndex.  (Only the X and Y coordinates are used.)
	*/
	virtual const physx::PxVec3&	getCutoutVertex(physx::PxU32 cutoutIndex, physx::PxU32 vertexIndex) const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the number of vertices in the loop indexed by loopIndex.
	*/
	virtual physx::PxU32			getCutoutLoopSize(physx::PxU32 coutoutIndex, physx::PxU32 loopIndex) const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the vertex index of the vertex indexed by vertexNum, in the loop
		indexed by loopIndex.
	*/
	virtual physx::PxU32			getCutoutLoopVertexIndex(physx::PxU32 cutoutIndex, physx::PxU32 loopIndex, physx::PxU32 vertexNum) const = 0;

	/**
		Applies to the cutout indexed by cutoutIndex:
		Returns the flags of the vertex indexed by vertexNum, in the loop
		indexed by loopIndex.
	*/
	virtual physx::PxU32			getCutoutLoopVertexFlags(physx::PxU32 cutoutIndex, physx::PxU32 loopIndex, physx::PxU32 vertexNum) const = 0;

	/**
		Whether or not this cutout set is to be tiled.
	*/
	virtual bool					isPeriodic() const = 0;

	/**
		The dimensions of the fracture map used to create the cutout set.
	*/
	virtual const physx::PxVec2&	getDimensions() const = 0;

	/** Serialization */
	//virtual void			serialize(physx::general_PxIOStream2::PxFileBuf& stream) const = 0;
	//virtual void			deserialize(physx::general_PxIOStream2::PxFileBuf& stream) = 0;

	/** Releases all memory and deletes itself. */
	virtual void					release() = 0;

protected:
	/** Protected destructor.  Use the release() method. */
	virtual							~ICutoutSet() {}
};


/**
	NoiseParameters
	These parameters are used to build a splitting surface.
*/
struct NoiseParameters
{
	/**
		Size of the fluctuations, relative to mesh size
	*/
	float	amplitude;

	/**
		Noise frequencey relative to 1/(grid spacing).  On scales much smaller than this, the function is smooth.
		On scales much large, the function looks uncorrelated
	*/
	float	frequency;

	/**
		Suggested number of grid elements across the mesh.  The actual number may vary depending
		on the mesh's proportions.
	*/
	int		gridSize;

	/**
		Noise function to use.  This parameter is currently unused.
		Noise is generated by superposition of many fourier modes in random directions,
		with frequencies randomly chosen in a band around the input frequency,
	*/
	int		type;

	/** Constructor sets defaults */
	NoiseParameters()
	{
		setToDefault();
	}

	/**
		Set default values:

		amplitude = 0.0f;
		frequency = 0.25f;
		gridSize = 10;
		type = 0;
	*/
	void	setToDefault()
	{
		amplitude = 0.0f;
		frequency = 0.25f;
		gridSize = 10;
		type = 0;
	}
};


/**
	SliceParameters

	The slicing parameters for X, Y, and Z slicing of a mesh.
*/
struct SliceParameters
{
	/**
		Which axis order to slice the mesh.
		This only matters if there is randomness in the slice surface.
	*/
	enum Order
	{
		XYZ,
		YZX,
		ZXY,
		ZYX,
		YXZ,
		XZY,
		Through
	};

	/** The slicing order (see the Order enum) */
	unsigned order;

	/** How many times to slice along each axis */
	unsigned splitsPerPass[3];

	/**
		Variation in slice position along each axis.  This is a relative quantity.
		linearVariation[axis] = 0 means the slicing offsets are evenly spaced across the mesh along the axis.
		linearVariation[axis] = 1 means the slicing offsets are randomly chosen in a range of width 1/(splitsPerPass[axis]+1)
								  times the width of the mesh along the axis.
	*/
	float linearVariation[3];

	/**
		Variation in the slice surface angle along each axis.
		0 variation means the slice surfaces are axis-aligned.  Otherwise, the surface normal will be varied randomly,
		with angle to the axis somewhere within the given variation (in radians).
	*/
	float angularVariation[3];

	/** The noise for each slicing direction */
	NoiseParameters	noise[3];

	/** Constructor sets defaults */
	SliceParameters()
	{
		setToDefault();
	}

	/** Sets all NoiseParameters to their defaults:
		order = XYZ;
		splitsPerPass[0] = splitsPerPass[1] = splitsPerPass[2] = 1;
		linearVariation[0] = linearVariation[1] = linearVariation[2] = 0.1f;
		angularVariation[0] = angularVariation[1] = angularVariation[2] = 20.0f*3.1415927f/180.0f;
		noise[0].setToDefault();
		noise[1].setToDefault();
		noise[2].setToDefault();
	*/
	void	setToDefault()
	{
		order = XYZ;
		splitsPerPass[0] = splitsPerPass[1] = splitsPerPass[2] = 1;
		linearVariation[0] = linearVariation[1] = linearVariation[2] = 0.1f;
		angularVariation[0] = angularVariation[1] = angularVariation[2] = 20.0f * 3.1415927f / 180.0f;
		noise[0].setToDefault();
		noise[1].setToDefault();
		noise[2].setToDefault();
	}
};


/**
	FractureSliceDesc

	Descriptor for slice-mode fracturing.
*/
struct FractureSliceDesc
{
	/** How many times to recurse the slicing process */
	unsigned						maxDepth;

	/** Array of slice parameters; must be of length maxDepth */
	SliceParameters*				sliceParameters;

	/**
		If this is true, the targetProportions (see below) will be used.
	*/
	bool							useTargetProportions;

	/**
		If useTargetProportions is true, the splitsPerPass values will not necessarily be used.
		Instead, the closest values will be chosen at each recursion of slicing, in order to make
		the pieces match the target proportions as closely as possible.

		Note: the more noise there is in the slicing surfaces, the less accurate these proportions will be.
	*/
	float							targetProportions[3];

	/**
		Material application descriptor used for each slice axis.
	*/
	physx::NxFractureMaterialDesc	materialDesc[3];

	/**
		If instanceChunks is true, corresponding chunks in different destructible actors will be instanced.
	*/
	bool							instanceChunks;

	/**
		If true, slice geometry and noise will be stored in a separate displacement offset buffer.
	*/
	bool							useDisplacementMaps;

	/** Enum describing creation of displacement maps. */
	enum NoiseMode
	{
		NoiseWavePlane = 0,
		NoisePerlin2D,
		NoisePerlin3D,

		NoiseModeCount
	};

	/**
		The noise mode.  If displacement maps are enabled, NoisePerlin3D will be used.
	*/
	unsigned			noiseMode;

	/** Constructor sets defaults */
	FractureSliceDesc()
	{
		setToDefault();
	}

	/**
		Sets the default values:

		maxDepth = 0;
		sliceParameters = NULL;
		useTargetProportions = false;
		targetProportions[0..2] = 1.0f;
		materialDesc[0..2].setToDefault();
		instanceChunks = false;
		useDisplacementMaps = false;
		noiseMode = NoiseWavePlane;
	*/
	void	setToDefault()
	{
		maxDepth             = 0;
		sliceParameters      = NULL;
		useTargetProportions = false;
		for (int i = 0; i < 3; ++i)
		{
			targetProportions[i] = 1.0f;
			materialDesc[i].setToDefault();
		}
		instanceChunks       = false;
		useDisplacementMaps  = false;
		noiseMode            = NoiseWavePlane;
	}
};


/**
	CutoutParameters

	Parameters for a single cutout direction.
*/
struct CutoutParameters
{
	/**
		The depth to apply cutout fracturing.
		0 has a special value; it means cut all the way through
	*/
	physx::PxF32 depth;

	/**
		Material application descriptor used for the cutout direction.
		Note: The backface slice will use the U-direction and UV offset specified in each descriptor, however the cutout slices (surrounding
		each cutout chunk) will ignore these fields (only using the UV scale).
	*/
	physx::NxFractureMaterialDesc	materialDesc;

	/**
		Describes the characteristics of the backface cutting surface (along the various cutout directions).
		If the noise is 0, the cutting surface will be a plane.  Otherwise, there will be some variation,
		or roughness, to the surface.
	*/
	NoiseParameters	backfaceNoise;

	/**
		Describes the characteristics of the perimeter cutting surfaces (for the various cutout directions).
		If the noise is 0, the cutting surface will smooth.  Otherwise, there will be some variation,
		or roughness, to the surface.

		Note: this noise is applied only to the graphics of the cutout chunks.  The chunks' collision volumes AND the chunks'
		children (if fractured further) will NOT be affected by this noise.
	*/
	NoiseParameters edgeNoise;

	/** Constructor sets defaults */
	CutoutParameters()
	{
		setToDefault();
	}

	/**
		Set default values:

		depth = 1.0f;
		backfaceNoise.setToDefault();
		edgeNoise.setToDefault();
		materialDesc.setToDefault();
	*/
	void	setToDefault()
	{
		depth = 1.0f;
		backfaceNoise.setToDefault();
		materialDesc.setToDefault();
		edgeNoise.setToDefault();
	}
};


/**
	FractureCutoutDesc

	Descriptor for cutout-mode (chippable) fracturing.
*/
struct FractureCutoutDesc
{
	/** Enum describing the directions to apply cutout fracturing. */
	enum Directions
	{
		UserDefined = 0,	// If no flags are set, the cutout direction is taken from userDefinedDirection

		NegativeX =	1 << 0,
		PositiveX =	1 << 1,
		NegativeY =	1 << 2,
		PositiveY =	1 << 3,
		NegativeZ =	1 << 4,
		PositiveZ =	1 << 5,

		DirectionCount = 6
	};

	/** The directions to apply cutout fracturing.  (See the Directions enum.) */
	unsigned directions;

	/**
		The order in which to apply each cutout direction.
		The direction in directionOrder[0] is applied first, in directionOrder[1], second, and so on.
	*/
	unsigned directionOrder[DirectionCount];

	/** Cutout parameters used for the various pre-defined cutout directions. */
	CutoutParameters cutoutParameters[DirectionCount];

	/**
		The cutout direction if directions = 0.  When this is used, it must be have non-zero length (it will be
		normalized internally), and userUVMapping must be set (this may be done with NxApexAssetAuthoring::calculateCutoutUVMapping).
	*/
	physx::PxVec3 userDefinedDirection;

	/** The UV mapping used if directons = 0 (along with userDefinedDirection). */
	physx::PxMat33 userUVMapping;

	/** Cutout parameters used if user-defined (UV-based) cutout fracturing is selected by setting directions = 0 */
	CutoutParameters userDefinedCutoutParameters;

	/** Enum describing the instancing mode. */
	enum InstancingMode
	{
		DoNotInstance,
		InstanceCongruentChunks,
		InstanceAllChunks,

		InstanceModeCount
	};

	/**
		The instancing mode.
	*/
	unsigned instancingMode;

	/**
		If tileFractureMap is true, the map will be tiled across the destructible.
	*/
	bool tileFractureMap;

	/**
		The U,V width of the fracture map when instancing chunks
	*/
	physx::PxVec2 uvTileSize;

	/**
		If true, non-convex cutouts will be split into convex ones.
	*/
	bool splitNonconvexRegions;

	/**
		Fracturing to apply to cutout chunks (if any), to break them down further.
		Current options include none, slice fracturing, and voronoi fracturing.
	*/
	enum CutoutChunkFracturingMethod
	{
		DoNotFractureCutoutChunks,
		SliceFractureCutoutChunks,
		VoronoiFractureCutoutChunks,

		CutoutChunkFracturingMethodCount
	};

	/**
		If true, slice-mode fracturing will be applied to each cutout piece.
		The cutout function must be provided with a FractureSliceDesc as well to describe
		the slice parameters.  These parameters, however, must be interpreted from the
		point of view of the cutout direction.  That is, X and Y slice parameters will be
		used to slice along the cutout faces.  The Z slice parameters will be used to slice
		into the cutout faces.
	*/
	unsigned chunkFracturingMethod;

	/**
		If true, the backface and cutouts will be trimmed if (a) backface noise is non-zero or
		(b) the collision hull is something other than the mesh convex hull ("Wrap Graphics Mesh").
		Trimming is done by intersecting the face slice plane (without added noise) with the backface
		and cutouts.
		Default is true.
	*/
	bool trimFaceCollisionHulls;

	/** Scale to apply to the X coordinates of the cutout set (along the various cutout directions). */
	float cutoutWidthScale[DirectionCount];

	/** Scale to apply to the Y coordinates of the cutout set (along the various cutout directions). */
	float cutoutHeightScale[DirectionCount];

	/** Offset to apply to the X coordinates of the cutout set (along the various cutout directions). */
	float cutoutWidthOffset[DirectionCount];

	/** Offset to apply to the Y coordinates of the cutout set (along the various cutout directions). */
	float cutoutHeightOffset[DirectionCount];

	/** If true, the cutout map will be flipped in the X direction (along the various cutout directions). */
	bool cutoutWidthInvert[DirectionCount];

	/** If true, the cutout map will be flipped in the Y direction (along the various cutout directions). */
	bool cutoutHeightInvert[DirectionCount];

	/** The interpreted size of the cutout map in the X direction */
	physx::PxF32 cutoutSizeX;

	/** The interpreted size of the cutout map in the Y direction */
	physx::PxF32 cutoutSizeY;

	/**
		Threshold angle to merge (smoothe) vertex normals around cutout, in degrees.
		If the exterior angle between two facets of a cutout region no more than this, the vertex normals and tangents will be
		averaged at the facet interface.  A value of 0 effectively disables smoothing.
		Default value = 60 degrees.
	*/
	physx::PxF32 facetNormalMergeThresholdAngle;

	/** Constructor sets defaults */
	FractureCutoutDesc()
	{
		setToDefault();
	}

	/**
		Set default values:

		directions = 0;
		directionOrder[0..5] = {NegativeX, .., PositiveZ};
		cutoutParameters[0..5].setToDefault();
		userDefinedDirection= physx::PxVec3(0.0f);
		userUVMapping		= physx::PxMat33::createIdentity();
		userDefinedCutoutParameters.setToDefault();
		instancingMode = DoNotInstance;
		tileFractureMap = false;
		uvTileSize = (0.0f,0.0f)
		cutoutParameters[0..5].setToDefault();
		cutoutWidthScale[0..5] = 1.0f;
		cutoutHeightScale[0..5] = 1.0f;
		cutoutWidthOffset[0..5] = 0.0f;
		cutoutHeightOffset[0..5] = 0.0f;
		cutoutWidthInvert[0..5] = false;
		cutoutHeightInvert[0..5] = false;
		cutoutSizeX = 1.0f;
		cutoutSizeY = 1.0f;
		facetNormalMergeThresholdAngle = 60.0f;
		splitNonconvexRegions = false;
		chunkFracturingMethod = DoNotFractureCutoutChunks;
		trimFaceCollisionHulls = true;
	*/
	void	setToDefault()
	{
		directions          = 0;
		userDefinedDirection= physx::PxVec3(0.0f);
		userUVMapping		= physx::PxMat33::createIdentity();
		userDefinedCutoutParameters.setToDefault();
		instancingMode      = DoNotInstance;
		tileFractureMap     = false;
		uvTileSize          = physx::PxVec2(0.0f);
		for (physx::PxU32 i = 0; i < DirectionCount; ++i)
		{
			directionOrder[i]     = 1 << i;
			cutoutParameters[i].setToDefault();
			cutoutWidthScale[i]   = 1.0f;
			cutoutHeightScale[i]  = 1.0f;
			cutoutWidthOffset[i]  = 0.0f;
			cutoutHeightOffset[i] = 0.0f;
			cutoutWidthInvert[i]  = false;
			cutoutHeightInvert[i] = false;
		}
		cutoutSizeX                    = 1.0f;
		cutoutSizeY                    = 1.0f;
		facetNormalMergeThresholdAngle = 60.0f;
		splitNonconvexRegions          = false;
		chunkFracturingMethod          = DoNotFractureCutoutChunks;
		trimFaceCollisionHulls         = true;
	}
};


/**
	FractureVoronoiDesc

	Descriptor for Voronoi decomposition fracturing.
*/
struct FractureVoronoiDesc
{
	/**
		Number of cell sites in the sites array.  Must be positive.
	*/
	unsigned				siteCount;

	/**
		Array of cell sites.  The length of this array is given by siteCount.
	*/
	const physx::PxVec3*	sites;

	/**
		Describes the characteristics of the interior surfaces.
		If the noise is 0, the cutting surface will smooth.  Otherwise, there will be some variation, or roughness, to the surface.

		Note: this noise is applied only to the graphics of the chunks.  The chunks' collision volumes AND the chunks'
		children (if fractured further) will NOT be affected by this noise.
	*/
	NoiseParameters			faceNoise;

	/**
		If instanceChunks is true, corresponding chunks in different destructible actors will be instanced.
	*/
	bool					instanceChunks;

	/**
		If true, slice geometry and noise will be stored in a separate displacement offset buffer.
	*/
	bool					useDisplacementMaps;

	/** Enum describing creation of displacement maps. */
	enum NoiseMode
	{
		NoiseWavePlane = 0,
		NoisePerlin2D,
		NoisePerlin3D,

		NoiseModeCount
	};

	/**
		The noise mode.  If displacement maps are enabled, NoisePerlin3D will be used.
	*/
	unsigned				noiseMode;

	/**
		Material application descriptor used for each slice.
		Note: the U-direction and UV offset in the descriptor will be ignored - UV mapping is done in arbitrary orientation and translation on each chunk face.
	*/
	physx::NxFractureMaterialDesc	materialDesc;


	/** Constructor sets defaults */
	FractureVoronoiDesc()
	{
		setToDefault();
	}

	/**
		Sets the default values:

		siteCount = 0;
		sites = NULL;
		faceNoise.setToDefault();
		instanceChunks = false;
		useDisplacementMaps = false;
		noiseMode = NoiseWavePlane;
		materialDesc.setToDefault();
	*/
	void	setToDefault()
	{
		siteCount = 0;
		sites = NULL;
		faceNoise.setToDefault();
		instanceChunks = false;
		useDisplacementMaps = false;
		noiseMode = NoiseWavePlane;
		materialDesc.setToDefault();
	}
};


/**
	Tools for fracturing meshes.
*/


/** Instantiates a blank ICutoutSet */
ICutoutSet* createCutoutSet();

/**
	Builds a cutout set (which must have been initially created by createCutoutSet()).
	Uses a bitmap described by pixelBuffer, bufferWidth, and bufferHeight.  Each pixel is represented
	by one byte in the buffer.

	cutoutSet: the ICutoutSet to build
	pixelBuffer: pointer to be beginning of the pixel buffer
	bufferWidth: the width of the buffer in pixels
	bufferHeight: the height of the buffer in pixels
	snapThreshold: the pixel distance at which neighboring cutout vertices and segments may be fudged into alignment.
	periodic: whether or not to use periodic boundary conditions when creating cutouts from the map
*/
void buildCutoutSet(ICutoutSet& cutoutSet, const physx::PxU8* pixelBuffer, physx::PxU32 bufferWidth, physx::PxU32 bufferHeight, physx::PxF32 snapThreshold, bool periodic);

/**
	Calculate the mapping between a cutout fracture map and a given triangle.
	The result is a 3 by 3 matrix M composed by an affine transformation and a rotation, we can get the 3-D projection for a texture coordinate pair (u,v) with such a formula:
	(x,y,z) = M*PxVec3(u,v,1)

	triangle: the target face's normal
	theMapping: resulted mapping, composed by an affine transformation and a rotation
*/
bool calculateCutoutUVMapping(const physx::NxExplicitRenderTriangle& triangle, physx::PxMat33& theMapping);

/**
	Uses the passed-in target direction to find the best triangle in the root mesh with normal near the given targetDirection.  If triangles exist
	with normals within one degree of the given target direction, then one with the greatest area of such triangles is used.  Otherwise, the triangle
	with normal closest to the given target direction is used.  The resulting triangle is used to calculate a UV mapping as in the function
	calculateCutoutUVMapping (above).

	The assumption is that there exists a single mapping for all triangles on a specified face, for this feature to be useful. 

	hMesh: the explicit mesh with well rectangle-shaped faces
	targetDirection: the target face's normal
	theMapping: resulted mapping, composed by an affine transformation and a rotation
*/
bool calculateCutoutUVMapping(physx::IExplicitHierarchicalMesh& hMesh, const physx::PxVec3& targetDirection, physx::PxMat33& theMapping);

/**
	Splits the mesh in chunk[0], forming fractured pieces chunks[1...] using
	Voronoi decomposition fracturing.

	hMesh: the mesh to split
	iHMeshCore: if this mesh is not empty, chunk 0 will be used as an indestructible "core" of the fractured
	mesh.  That is, it will be subtracted from hMesh, and placed at level 1 of the hierarchy.  The remainder
	of hMesh will be split as usual, creating chunks at level 1 (and possibly deeper).
	exportCoreMesh: if true, a core mesh chunk will be created from iHMeshCore
	coreMeshImprintSubmeshIndex: if this is < 0, use the core mesh materials (was applyCoreMeshMaterialToNeighborChunks).  Otherwise, use the given submesh
	meshProcessingParams: describes generic mesh processing directives
	desc: describes the voronoi splitting parameters surfaces (see FractureVoronoiDesc)
	collisionDesc: convex hulls will be generated for each chunk using the method.  See NxCollisionDesc.
	randomSeed: seed for the random number generator, to ensure reproducibility.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	cancel: if not NULL and *cancel is set to true, the root mesh will be restored to its original state, and the function will return at its earliest opportunity.  Meant to be set from another thread.

	returns true if successful.
*/
bool	createVoronoiSplitMesh
(
	physx::IExplicitHierarchicalMesh& hMesh,
	physx::IExplicitHierarchicalMesh& iHMeshCore,
	bool exportCoreMesh,
	physx::PxI32 coreMeshImprintSubmeshIndex,
	const MeshProcessingParameters& meshProcessingParams,
	const FractureVoronoiDesc& desc,
	const physx::NxCollisionDesc& collisionDesc,
	physx::PxU32 randomSeed,
	physx::IProgressListener& progressListener,
	volatile bool* cancel = NULL
);

/**
	Generates a set of uniformly distributed points in the interior of the root mesh.

	hMesh: the mesh in which to distribute sites
	siteBuffer: an array of PxVec3, at least the size of siteCount
	siteCount: the number of points to write into siteBuffer
	randomSeed: pointer to a seed for the random number generator, to ensure reproducibility.  If NULL, the random number generator will not be re-seeded.
	microgridSize: pointer to a grid size used for BSP creation.  If NULL, the default settings will be used.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	chunkIndex: If this is a valid index, the voronoi sites will only be created within the volume of the indexed chunk.  Otherwise,
		the sites will be created within each of the root-level chunks.  Default value is an invalid index.
*/
void	createVoronoiSitesInsideMesh
(
	physx::IExplicitHierarchicalMesh& hMesh,
	physx::PxVec3* siteBuffer,
	physx::PxU32 siteCount,
	physx::PxU32* randomSeed,
	physx::PxU32* microgridSize,
	physx::IProgressListener& progressListener,
	physx::PxU32 chunkIndex = 0xFFFFFFFF
);

/**
	Creates scatter mesh sites randomly distributed on the mesh.

	meshIndices: user-allocated array of size scatterMeshInstancesBufferSize which will be filled in by this function, giving the scatter mesh index used
	relativeTransforms: user-allocated array of size scatterMeshInstancesBufferSize which will be filled in by this function, giving the chunk-relative transform for each chunk instance
	chunkMeshStarts: user-allocated array which will be filled in with offsets into the meshIndices and relativeTransforms array.
		For a chunk indexed by i, the corresponding range [chunkMeshStart[i], chunkMeshStart[i+1]-1] in meshIndices and relativeTransforms is used.
		*NOTE*: chunkMeshStart array must be of at least size N+1, where N is the number of chunks in the base explicit hierarchical mesh.
	scatterMeshInstancesBufferSize: the size of meshIndices and relativeTransforms array.
	scatterMeshInstancesBufferSize: the size of meshIndices and relativeTransforms array.
	hMesh: the mesh in which to distribute sites
	targetChunkCount: how many chunks are in the array targetChunkIndices
	targetChunkIndices: an array of chunk indices which are candidates for scatter meshes.  The elements in the array chunkIndices will come from this array
	randomSeed: pointer to a seed for the random number generator, to ensure reproducibility.  If NULL, the random number generator will not be re-seeded.
	scatterMeshAssetCount: the number of different scatter meshes (not instances).  Should not exceed 255.  If scatterMeshAssetCount > 255, only the first 255 will be used.
	scatterMeshAssets: an array of size scatterMeshAssetCount, of the render mesh assets which will be used for the scatter meshes
	minCount: an array of size scatterMeshAssetCount, giving the minimum number of instances to place for each mesh
	maxCount: an array of size scatterMeshAssetCount, giving the maximum number of instances to place for each mesh
	minScales: an array of size scatterMeshAssetCount, giving the minimum scale to apply to each scatter mesh
	maxScales: an array of size scatterMeshAssetCount, giving the maximum scale to apply to each scatter mesh
	maxAngles: an array of size scatterMeshAssetCount, giving a maximum deviation angle (in degrees) from the surface normal to apply to each scatter mesh

	return value: the number of instances placed in indices and relativeTransforms (will not exceed scatterMeshInstancesBufferSize)
*/
physx::PxU32	createScatterMeshSites
(
	physx::PxU8*						meshIndices,
	physx::PxMat44*						relativeTransforms,
	physx::PxU32*						chunkMeshStarts,
	physx::PxU32						scatterMeshInstancesBufferSize,
	physx::IExplicitHierarchicalMesh&	hMesh,
	physx::PxU32						targetChunkCount,
	const physx::PxU16*					targetChunkIndices,
	physx::PxU32*						randomSeed,
	physx::PxU32						scatterMeshAssetCount,
	physx::NxRenderMeshAsset**			scatterMeshAssets,
	const physx::PxU32*					minCount,
	const physx::PxU32*					maxCount,
	const physx::PxF32*					minScales,
	const physx::PxF32*					maxScales,
	const physx::PxF32*					maxAngles
);

/**
	Utility to visualize Voronoi cells for a given set of sites.

	debugRender: rendering object which will receive the drawing primitives associated with this cell visualization
	sites: an array of Voronoi cell sites, of length siteCount
	siteCount: the number of Voronoi cell sites (length of sites array)
	cellColors: an optional array of colors (see NxApexRenderDebug for format) for the cells.  If NULL, the white (0xFFFFFFFF) color will be used.
		If not NULL, this (of length cellColorCount) is used to color the cell graphics.  The number cellColorCount need not match siteCount.  If
		cellColorCount is less than siteCount, the cell colors will cycle.  That is, site N gets cellColor[N%cellColorCount].
	cellColorCount: the number of cell colors (the length of cellColors array)
	bounds: defines an axis-aligned bounding box which clips the visualization, since some cells extend to infinity
	cellIndex: if this is a valid index (cellIndex < siteCount), then only the cell corresponding to sites[cellIndex] will be drawn.  Otherwise, all cells will be drawn.
*/
void	visualizeVoronoiCells
(
	physx::NxApexRenderDebug& debugRender,
	const physx::PxVec3* sites,
	physx::PxU32 siteCount,
	const physx::PxU32* cellColors,
	physx::PxU32 cellColorCount,
	const physx::PxBounds3& bounds,
	physx::PxU32 cellIndex = 0xFFFFFFFF
);

/**
	Builds a new IExplicitHierarchicalMesh from an array of triangles.

	iHMesh: the IExplicitHierarchicalMesh to build
	meshTriangles: pointer to array of NxExplicitRenderTriangles which make up the mesh
	meshTriangleCount the size of the meshTriangles array
	submeshData: pointer to array of NxExplicitSubmeshData, describing the submeshes
	submeshCount: the size of the submeshData array
	meshPartition: if not NULL, an array of size meshPartitionCount, giving the end elements of contiguous subsets of meshTriangles.
		If meshPartition is NULL, one partition is assumed.
		When there is one partition, these triangles become the level 0 part.
		When there is more than one partition, these triangles become level 1 parts, the behavior is determined by firstPartitionIsDepthZero (see below).
	meshPartitionCount: if meshPartition is not NULL, this is the size of the meshPartition array.
	firstPartitionIsDepthZero: if a meshPartition is given and there is more than one part, then if firstPartitionIsDepthZero = true, the first partition is the depth 0 chunk.
		Otherwise, all parts created from the input partition will be depth 1, and the depth 0 chunk will be created from the union of the depth 1 chunks.
*/
bool buildExplicitHierarchicalMesh
(
    physx::IExplicitHierarchicalMesh& iHMesh,
    const physx::NxExplicitRenderTriangle* meshTriangles,
    physx::PxU32 meshTriangleCount,
    const physx::NxExplicitSubmeshData* submeshData,
    physx::PxU32 submeshCount,
    physx::PxU32* meshPartition = NULL,
    physx::PxU32 meshPartitionCount = 0,
	bool firstPartitionIsDepthZero = false
);

/**
	Set the tolerances used in CSG calculations with BSPs.

	linearTolerance: relative (to mesh size) tolerance used with angularTolerance to determine coplanarity.  Default = 1.0e-4.
	angularTolerance: used with linearTolerance to determine coplanarity.  Default = 1.0e-3
	baseTolerance: relative (to mesh size) tolerance used for spatial partitioning
	clipTolerance: relative (to mesh size) tolerance used when clipping triangles for CSG mesh building operations.  Default = 1.0e-4.
	cleaningTolerance: relative (to mesh size) tolerance used when cleaning the out put mesh generated from the toMesh() function.  Default = 1.0e-7.
*/
void setBSPTolerances
(
	physx::PxF32 linearTolerance,
	physx::PxF32 angularTolerance,
	physx::PxF32 baseTolerance,
	physx::PxF32 clipTolerance,
	physx::PxF32 cleaningTolerance
);

/**
	Set the parameters used in BSP building operations.

	logAreaSigmaThreshold:	At each step in the tree building process, the surface with maximum triangle area is compared
		to the other surface triangle areas.  If the maximum area surface is far from the "typical" set of
		surface areas, then that surface is chosen as the next splitting plane.  Otherwise, a random
		test set is chosen and a winner determined based upon the weightings below.
		The value logAreaSigmaThreshold determines how "atypical" the maximum area surface must be to
		be chosen in this manner.
		Default value = 2.0.
	testSetSize: Larger values of testSetSize may find better BSP trees, but will take more time to create.
		testSetSize = 0 is treated as infinity (all surfaces will be tested for each branch).
		Default value = 10.
	splitWeight: How much to weigh the relative number of triangle splits when searching for a BSP surface.
		Default value = 0.5.
	imbalanceWeight: How much to weigh the relative triangle imbalance when searching for a BSP surface.
		Default value = 0.0.
*/
void setBSPBuildParameters
(
	physx::PxF32 logAreaSigmaThreshold,
	physx::PxU32 testSetSize,
	physx::PxF32 splitWeight,
	physx::PxF32 imbalanceWeight
);


/** 
	Builds the root IExplicitHierarchicalMesh from an NxRenderMeshAsset.
	Since an NxDestructibleAsset contains no hierarchy information, the input mesh must have only one part.

	iHMesh: the IExplicitHierarchicalMesh to build
	renderMeshAsset: Input RenderMesh asset
	maxRootDepth: cap the root depth at this value.  Re-fracturing of the mesh will occur at this depth.  Default = PX_MAX_U32
*/
bool buildExplicitHierarchicalMeshFromRenderMeshAsset(physx::IExplicitHierarchicalMesh& iHMesh, const physx::NxRenderMeshAsset& renderMeshAsset, physx::PxU32 maxRootDepth = PX_MAX_U32);

/** 
	Builds the root IExplicitHierarchicalMesh from an NxDestructibleAsset.
	Since an NxDestructibleAsset contains hierarchy information, the explicit mesh formed
	will have this hierarchy structure.

	iHMesh: the IExplicitHierarchicalMesh to build
	destructibleAsset: Input Destructible asset
	maxRootDepth: cap the root depth at this value.  Re-fracturing of the mesh will occur at this depth.  Default = PX_MAX_U32
*/
bool buildExplicitHierarchicalMeshFromDestructibleAsset(physx::IExplicitHierarchicalMesh& iHMesh, const physx::NxDestructibleAsset& destructibleAsset, physx::PxU32 maxRootDepth = PX_MAX_U32);

/**
Partitions (and possibly re-orders) the mesh array if the triangles form disjoint islands.
mesh: pointer to array of NxExplicitRenderTriangles which make up the mesh
meshTriangleCount: the size of the meshTriangles array
meshPartition: user-allocated array for mesh partition, will be filled with the end elements of contiguous subsets of meshTriangles.
meshPartitionMaxCount: size of user-allocated meshPartitionArray
padding: distance (as a fraction of the mesh size) to consider vertices touching

Returns the number of partitions.  The value may be larger than meshPartitionMaxCount.  In that case, the partitions beyond meshPartitionMaxCount are not recorded.
*/
physx::PxU32 partitionMeshByIslands
(
	physx::NxExplicitRenderTriangle* mesh,
	physx::PxU32 meshTriangleCount,
	physx::PxU32* meshPartition,
	physx::PxU32 meshPartitionMaxCount,
	physx::PxF32 padding = 0.0001f
);

/**
	Splits the mesh in chunk[0], forming a hierarchy of fractured meshes in chunks[1...]

	hMesh: the mesh to split
	iHMeshCore: if this mesh is not empty, chunk 0 will be used as an indestructible "core" of the fractured
		mesh.  That is, it will be subtracted from hMesh, and placed at level 1 of the hierarchy.  The remainder
		of hMesh will be split as usual, creating chunks at level 1 (and possibly deeper).
	exportCoreMesh: if true, a core mesh chunk will be created from iHMeshCore
	coreMeshImprintSubmeshIndex: if this is < 0, use the core mesh materials (was applyCoreMeshMaterialToNeighborChunks).  Otherwise, use the given submesh
	meshProcessingParams: describes generic mesh processing directives
	desc: describes the slicing surfaces (see FractureSliceDesc)
	collisionDesc: convex hulls will be generated for each chunk using the method.  See NxCollisionDesc.
	randomSeed: seed for the random number generator, to ensure reproducibility.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	cancel: if not NULL and *cancel is set to true, the root mesh will be restored to its original state, and the function will return at its earliest opportunity.  Meant to be set from another thread.

	returns true if successful.
*/
bool createHierarchicallySplitMesh
(
    physx::IExplicitHierarchicalMesh& hMesh,
    physx::IExplicitHierarchicalMesh& iHMeshCore,
    bool exportCoreMesh,
	physx::PxI32 coreMeshImprintSubmeshIndex,
    const MeshProcessingParameters& meshProcessingParams,
    const FractureSliceDesc& desc,
	const physx::NxCollisionDesc& collisionDesc,
    physx::PxU32 randomSeed,
    physx::IProgressListener& progressListener,
    volatile bool* cancel = NULL
);

/**
	Chips the mesh in chunk[0], forming a hierarchy of fractured meshes in chunks[1...]

	hMesh: the mesh to split
	meshProcessingParams: describes generic mesh processing directives
	desc: describes the slicing surfaces (see FractureCutoutDesc)
	iCutoutSet: the cutout set to use for fracturing (see ICutoutSet)
	sliceDesc: used if desc.chunkFracturingMethod = SliceFractureCutoutChunks
	voronoiDesc: used if desc.chunkFracturingMethod = VoronoiFractureCutoutChunks
	collisionDesc: convex hulls will be generated for each chunk using the method.  See NxCollisionDesc.
	randomSeed: seed for the random number generator, to ensure reproducibility.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	cancel: if not NULL and *cancel is set to true, the root mesh will be restored to its original state, and the function will return at its earliest opportunity.  Meant to be set from another thread.

	returns true if successful.
*/
bool createChippedMesh
(
    physx::IExplicitHierarchicalMesh& hMesh,
    const MeshProcessingParameters& meshProcessingParams,
    const FractureCutoutDesc& desc,
    const ICutoutSet& iCutoutSet,
    const FractureSliceDesc& sliceDesc,
	const FractureVoronoiDesc& voronoiDesc,
	const physx::NxCollisionDesc& collisionDesc,
    physx::PxU32 randomSeed,
    physx::IProgressListener& progressListener,
    volatile bool* cancel = NULL
);

/**
	Splits the chunk in chunk[chunkIndex], forming a hierarchy of fractured chunks using
	slice-mode fracturing.  The chunks will be rearranged so that they are in breadth-first order.

	hMesh: the ExplicitHierarchicalMesh to act upon
	chunkIndex: index of chunk to be split
	meshProcessingParams: used to create a BSP for this chunk
	desc: describes the slicing surfaces (see FractureSliceDesc)
	collisionDesc: convex hulls will be generated for each chunk using the method.  See NxCollisionDesc.
	randomSeed: pointer to a seed for the random number generator, to ensure reproducibility.  If NULL, the random number generator will not be re-seeded.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	cancel: if not NULL and *cancel is set to true, the root mesh will be restored to its original state, and the function will return at its earliest opportunity.  Meant to be set from another thread.

	returns true if successful.
*/
bool hierarchicallySplitChunk
(
	physx::IExplicitHierarchicalMesh& hMesh,
	physx::PxU32 chunkIndex,
	const MeshProcessingParameters& meshProcessingParams,
	const FractureSliceDesc& desc,
	const physx::NxCollisionDesc& collisionDesc,
	physx::PxU32* randomSeed,
	physx::IProgressListener& progressListener,
	volatile bool* cancel = NULL
);

/**
	Splits the chunk in chunk[chunkIndex], forming fractured chunks using
	Voronoi decomposition fracturing.  The chunks will be rearranged so that they are in breadth-first order.

	hMesh: the ExplicitHierarchicalMesh to act upon
	chunkIndex: index of chunk to be split
	meshProcessingParams: describes generic mesh processing directives
	desc: describes the voronoi splitting parameters surfaces (see FractureVoronoiDesc)
	collisionDesc: convex hulls will be generated for each chunk using the method.  See NxCollisionDesc.
	randomSeed: pointer to a seed for the random number generator, to ensure reproducibility.  If NULL, the random number generator will not be re-seeded.
	progressListener: The user must instantiate an IProgressListener, so that this function may report progress of this operation
	cancel: if not NULL and *cancel is set to true, the root mesh will be restored to its original state, and the function will return at its earliest opportunity.  Meant to be set from another thread.

	returns true if successful.
*/
bool voronoiSplitChunk
(
	physx::IExplicitHierarchicalMesh& hMesh,
	physx::PxU32 chunkIndex,
    const MeshProcessingParameters& meshProcessingParams,
	const FractureVoronoiDesc& desc,
	const physx::NxCollisionDesc& collisionDesc,
    physx::PxU32* randomSeed,
    physx::IProgressListener& progressListener,
    volatile bool* cancel = NULL
);

/**
	Builds a mesh used for slice fracturing, given the noise parameters and random seed.  This function is mostly intended
	for visualization - to give the user a "typical" slice surface used for fracturing.
*/
bool buildSliceMesh
(
	physx::IntersectMesh& intersectMesh,
	physx::IExplicitHierarchicalMesh& referenceMesh,
	const physx::PxPlane& slicePlane,
	const FractureTools::NoiseParameters& noiseParameters,
	physx::PxU32 randomSeed
);

PX_POP_PACK

} // namespace FractureTools

#endif // FRACTURE_TOOLS_H
