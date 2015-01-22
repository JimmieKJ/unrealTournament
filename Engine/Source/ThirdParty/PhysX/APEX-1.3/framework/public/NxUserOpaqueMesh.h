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

#ifndef NX_USER_OPAQUE_MESH_H
#define NX_USER_OPAQUE_MESH_H

/*!
\file
\brief class NxUserOpaqueMesh
*/

#include "NxApexUsingNamespace.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

//! \brief Name of NxOpaqueMesh authoring type namespace
#define APEX_OPAQUE_MESH_NAME_SPACE "ApexOpaqueMesh"

/**
\brief Opaque mesh description
*/
class NxUserOpaqueMeshDesc
{
public:
	///the name of the opaque mesh
	const char* mMeshName;
};

/**
\brief An abstract interface to an opaque mesh
*
* An 'opaque' mesh is a binding between the 'name' of a mesh and some internal mesh representation used by the
* application.  This allows the application to refer to meshes by name without involving duplciation of index buffer and
* vertex buffer data declarations.
*/
class NxUserOpaqueMesh
{
public:
	virtual ~NxUserOpaqueMesh() {}
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif
