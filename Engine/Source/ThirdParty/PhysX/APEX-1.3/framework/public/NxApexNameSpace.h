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
#ifndef NX_APEX_NAME_SPACE_H
#define NX_APEX_NAME_SPACE_H

#include "NxApexUsingNamespace.h"

/*!
\file
\brief Defines APEX namespace strings

These are predefined framework namespaces in the named resource provider
*/

/*!
\brief A namespace for names of collision groups (NxCollisionGroup).

Each name in this namespace must map to a 5-bit integer in the range [0..31] (stored in a void*).
The NRP will not automatically generate release requests for names in this namespace.
*/
#define APEX_COLLISION_GROUP_NAME_SPACE				"NSCollisionGroup"

/*!
\brief A namespace for names of NxGroupsMasks

Each name in this namespace must map to a pointer to a persistent 128-bit NxGroupsMask type.
The NRP will not automatically generate release requests for names in this namespace.
*/
#define APEX_COLLISION_GROUP_128_NAME_SPACE			"NSCollisionGroup128"

/*!
\brief A namespace for names of NxGroupsMasks64

Each name in this namespace must map to a pointer to a persistent 64-bit NxGroupsMask64 type.
The NRP will not automatically generate release requests for names in this namespace.
*/
#define APEX_COLLISION_GROUP_64_NAME_SPACE			"NSCollisionGroup64"

/*!
\brief A namespace for names of collision group masks

Each name in this namespace must map to a 32-bit integer (stored in a void*), wherein each
bit represents a collision group (NxCollisionGroup). The NRP will not automatically generate
release requests for names in this namespace.
*/
#define APEX_COLLISION_GROUP_MASK_NAME_SPACE		"NSCollisionGroupMask"

/*!
\brief Internal namespace for authorable asset types

For examples, there are entries in this namespace for "ApexRenderMesh", "NxClothingAsset",
"DestructibleAsset", etc...
The values stored in this namespace are the namespace IDs of the authorable asset types.
So if your module needs to get a pointer to a FooAsset created by module Foo, you can ask
the ApexSDK for that asset's namespace ID.
*/
#define APEX_AUTHORABLE_ASSETS_TYPES_NAME_SPACE		"AuthorableAssetTypes"

/*!
\brief Internal namespace for parameterized authorable assets
*/
#define APEX_NX_PARAM_AUTH_ASSETS_TYPES_NAME_SPACE	"NxParamAuthorableAssetTypes"

/*!
\brief A namespace for graphical material names

Each name in this namespace maps to a pointer to a game-engine defined graphical material data
structure. APEX does not interpret or dereference this pointer in any way. APEX provides this
pointer to the NxUserRenderResource::setMaterial(void *material) callback to the rendering engine.
This mapping allows APEX assets to refer to game engine materials (e.g.: texture maps and shader
programs) without imposing any limitations on what a game engine graphical material can contain.
The NRP will not automatically generate release requests for names in this namespace.
*/
#define APEX_MATERIALS_NAME_SPACE					"ApexMaterials"

/*!
\brief A namespace for volumetric rendering material names

*/
#define APEX_VOLUME_RENDER_MATERIALS_NAME_SPACE					"ApexVolumeRenderMaterials"


/*!
\brief A namespace for physical material names

Each name in this namespace maps to NxMaterialID, which is a data type defined
by the PhysX SDK. The NRP will not automatically generate release requests for
names in this namespace.
*/
#define APEX_PHYSICS_MATERIAL_NAME_SPACE			"NSPhysicalMaterial"

/*!
\brief A namespace for custom vertex buffer semantics names

Each name in this namespace maps to a pointer to a game-engine defined data structure identifying
a custom vertex buffer semantic. APEX does not interpret or dereference this pointer in any way.
APEX provides an array of these pointers in NxUserRenderVertexBufferDesc::customBuffersIdents,
which is passed the rendering engine when requesting allocation of vertex buffers.
*/
#define APEX_CUSTOM_VB_NAME_SPACE                   "NSCustomVBNames"

#endif // NX_APEX_NAME_SPACE_H
