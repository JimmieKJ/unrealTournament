/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  


#ifndef PXD_INIT_H
#define PXD_INIT_H

#include "PxvConfig.h"

namespace physx
{

/*!
\file
PhysX Low-level, Memory management
*/

/************************************************************************/
/* Error Handling                                                       */
/************************************************************************/


enum PxvErrorCode
{
	PXD_ERROR_NO_ERROR = 0,
	PXD_ERROR_INVALID_PARAMETER,
	PXD_ERROR_INVALID_PARAMETER_SIZE,
	PXD_ERROR_INTERNAL_ERROR,
	PXD_ERROR_NOT_IMPLEMENTED,
	PXD_ERROR_NO_CONTEXT,
	PXD_ERROR_NO_TASK_MANAGER,
	PXD_ERROR_WARNING
};
	
/*!
Initialize low-level implementation.
*/

void PxvInit();


/*!
Shut down low-level implementation.
*/
void PxvTerm();

/*!
Initialize low-level implementation.
*/

void PxvRegisterArticulations();

void PxvRegisterHeightFields();

void PxvRegisterLegacyHeightFields();

#if PX_USE_PARTICLE_SYSTEM_API
void PxvRegisterParticles();
#endif

#if PX_SUPPORT_GPU_PHYSX
class PxPhysXGpu* PxvGetPhysXGpu(bool createIfNeeded);
#endif

}

#endif
