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

#ifndef NX_MODULE_CLOTHING_H
#define NX_MODULE_CLOTHING_H

#include "NxModule.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

class NxClothingAsset;
class NxClothingAssetAuthoring;
class NxClothingPhysicalMesh;

class IProgressListener;
class NxRenderMeshAssetAuthoring;
class NxClothingIsoMesh;

/**
\mainpage APEX Clothing API Documentation

\section overview Overview

This document contains a full API documentation for all public classes.
*/



/**
\brief APEX Clothing Module

Used to generate simulated clothing on (mostly humanoid) characters.
*/
class NxModuleClothing : public NxModule
{
public:
	/**
	\brief creates an empty physical mesh. A custom mesh can be assigned to it.
	*/
	virtual NxClothingPhysicalMesh* createEmptyPhysicalMesh() = 0;

	/**
	\brief creates a physical mesh based on a render mesh asset. This will be a 1 to 1 copy of the render mesh

	\param [in] asset			The render mesh that is used as source for the physical mesh
	\param [in] subdivision		Modify the physical mesh such that all edges that are longer than (bounding box diagonal / subdivision) are split up. Must be <= 200
	\param [in] mergeVertices	All vertices with the same position will be welded together.
	\param [in] closeHoles		Close any hole found in the mesh.
	\param [in] progress		An optional callback for progress display.
	*/
	virtual NxClothingPhysicalMesh* createSingleLayeredMesh(NxRenderMeshAssetAuthoring* asset, physx::PxU32 subdivision, bool mergeVertices, bool closeHoles, IProgressListener* progress) = 0;

	/**
	\brief create an iso mesh as intermediate step to generating a physical mesh

	\param [in] asset				The render mesh that is used as source for the physical mesh.
	\param [in] subdivision			The grid size (derived by bounding box diagonal / subdivision) for the iso surface. Must be <= 200.
	\param [in] keepNBiggestMeshes	When having multiple disjointed iso meshes, only keep the N largest of them. 0 means keep all.
	\param [in] discardInnerMeshes	When having multiple disjointed iso meshes, only keep the outer meshes and discard the inner ones.
	\param [in] progress			An optional callback for progress display.
	*/
	virtual NxClothingIsoMesh* createMultiLayeredMesh(NxRenderMeshAssetAuthoring* asset, physx::PxU32 subdivision, physx::PxU32 keepNBiggestMeshes, bool discardInnerMeshes, IProgressListener* progress) = 0;


protected:
	virtual ~NxModuleClothing() {}
};

#if !defined(_USRDLL)
void instantiateModuleClothing();
#endif


PX_POP_PACK

}
} // namespace physx::apex

#endif // NX_MODULE_CLOTHING_H
