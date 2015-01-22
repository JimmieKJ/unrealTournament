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

#ifndef NX_APEX_H
#define NX_APEX_H

/**
\file
\brief The top level include file for all of the APEX API.

Include this whenever you want to use anything from the APEX API
in a source file.
*/

#include "foundation/Px.h"

#include "NxApexUsingNamespace.h"


namespace NxParameterized
{
class Traits;
class Interface;
class Serializer;
};


#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxAssert.h"
#include "PxFileBuf.h"
#include "foundation/PxBounds3.h"
#include "foundation/PxVec2.h"
#include "foundation/PxVec3.h"


// APEX public API:
// In general, APEX public headers will not be included 'alone', so they
// should not include their prerequisites.

#include "NxApexDefs.h"
#include "NxApexDesc.h"
#include "NxApexInterface.h"
#include "NxApexSDK.h"

#include "NxApexActor.h"
#include "NxApexContext.h"
#include "NxApexNameSpace.h"
#include "NxApexPhysXObjectDesc.h"
#include "NxApexRenderDataProvider.h"
#include "NxApexRenderable.h"
#include "NxApexAssetPreview.h"
#include "NxApexAsset.h"
#include "NxApexRenderContext.h"
#include "NxApexScene.h"
#include "NxApexSDKCachedData.h"
#include "NxApexUserProgress.h"
#include "NxModule.h"
#include "NxInstancedObjectSimulationAsset.h"

#include "NxApexRenderDataFormat.h"
#include "NxApexRenderBufferData.h"
#include "NxUserRenderResourceManager.h"
#include "NxUserRenderVertexBufferDesc.h"
#include "NxUserRenderInstanceBufferDesc.h"
#include "NxUserRenderSpriteBufferDesc.h"
#include "NxUserRenderIndexBufferDesc.h"
#include "NxUserRenderBoneBufferDesc.h"
#include "NxUserRenderResourceDesc.h"
#include "NxUserRenderSurfaceBufferDesc.h"
#include "NxUserRenderSurfaceBuffer.h"
#include "NxUserRenderResource.h"
#include "NxUserRenderVertexBuffer.h"
#include "NxUserRenderInstanceBuffer.h"
#include "NxUserRenderSpriteBuffer.h"
#include "NxUserRenderIndexBuffer.h"
#include "NxUserRenderBoneBuffer.h"
#include "NxUserRenderer.h"

#include "NxVertexFormat.h"
#include "NxRenderMesh.h"
#include "NxRenderMeshActorDesc.h"
#include "NxRenderMeshActor.h"
#include "NxRenderMeshAsset.h"
#include "NxResourceCallback.h"
#include "NxResourceProvider.h"

#endif // NX_APEX_H
