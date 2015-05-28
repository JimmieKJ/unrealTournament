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


#ifndef PX_PHYSICS_SC_PHYSICS
#define PX_PHYSICS_SC_PHYSICS

#include "PxPhysics.h"
#include "PxScene.h"
#include "PsUserAllocated.h"
#include "CmPhysXCommon.h"

namespace physx
{

namespace cloth
{
class Factory;
}


class PxMaterial;
class PxTolerancesScale;

#if PX_SUPPORT_GPU_PHYSX
class PxPhysXGpu;
#endif

namespace Sc
{

	class Scene;

	class Physics : public Ps::UserAllocated
	{
	public:
		PX_INLINE static Physics&						getInstance()	{ return *mInstance; }

														Physics(const PxTolerancesScale&);
														~Physics(); // use release() instead
	public:
						void							release();

		PX_FORCE_INLINE	const PxTolerancesScale&		getTolerancesScale()								const	{ return mScale;	}

#if PX_USE_CLOTH_API
		void						registerCloth();
		PX_INLINE bool				hasLowLevelClothFactory() const { return mLowLevelClothFactory != 0; }
		PX_INLINE cloth::Factory&	getLowLevelClothFactory() { PX_ASSERT(mLowLevelClothFactory); return *mLowLevelClothFactory; }
#endif

	private:
						PxTolerancesScale			mScale;
		static			Physics*					mInstance;
						cloth::Factory*				mLowLevelClothFactory;

	public:
		static			const PxReal				sWakeCounterOnCreation;
	};

} // namespace Sc

}

#endif
