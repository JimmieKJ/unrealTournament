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


#include "PsIntrinsics.h"
#include "GuMeshFactory.h"

#include "PxHeightFieldDesc.h"

#include "GuTriangleMesh.h"
#include "GuConvexMesh.h"
#include "GuHeightField.h"
#include "CmUtils.h"

#if PX_SUPPORT_GPU_PHYSX
	#define	GU_MESH_FACTORY_GPU_NOTIFICATION(notificationMethod, argument) notificationMethod(argument);
#else
	#define GU_MESH_FACTORY_GPU_NOTIFICATION(notificationMethod, argument)
#endif

using namespace physx;

// PT: TODO: refactor all this with a dedicated container

GuMeshFactory::~GuMeshFactory()
{
}

///////////////////////////////////////////////////////////////////////////////

void GuMeshFactory::release()
{
	// Release all objects in case the user didn't do it

	while(mTriangleMeshArray.size() > 0)
	{
		// force destruction
		PX_ASSERT(mTriangleMeshArray[0]->getRefCount()==1);
		GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseTriangleMesh, *mTriangleMeshArray[0])
		mTriangleMeshArray[0]->release();
	}

	while(mConvexMeshArray.size() > 0)
	{
		// force destruction
		PX_ASSERT(mConvexMeshArray[0]->getRefCount()==1);
		GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseConvexMesh, *mConvexMeshArray[0])
		mConvexMeshArray[0]->release();
	}

	while(mHeightFieldArray.size() > 0)
	{
		// force destruction
		PX_ASSERT(mHeightFieldArray[0]->getRefCount()==1);
		GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseHeightField, *mHeightFieldArray[0])
		mHeightFieldArray[0]->release();
	}

	PX_DELETE(this);
}

namespace
{
	template<typename TDataType>
	inline void notifyReleaseFactoryItem( Ps::Array<GuMeshFactoryListener*>& listeners, const TDataType* type, PxType typeID, bool memRelease )
	{
		PxU32 numListeners = listeners.size();
		for ( PxU32 idx = 0; idx < numListeners; ++idx )
			listeners[idx]->onGuMeshFactoryBufferRelease( type, typeID, memRelease );
	}

	template <typename T> void addToArray(Ps::Array<T*>& array, T* element, Ps::Mutex* mutex)
	{
		if(!element)
			return;

		if(mutex)
			mutex->lock();
		{
			if(!array.size())
				array.reserve(64);
			array.pushBack(element);
		}
		if(mutex)
			mutex->unlock();
	}
}

///////////////////////////////////////////////////////////////////////////////

void GuMeshFactory::addTriangleMesh(Gu::TriangleMesh* np, bool lock)
{
	addToArray(mTriangleMeshArray, np, lock ? &mTrackingMutex : NULL);
}

PxTriangleMesh* GuMeshFactory::createTriangleMesh(PxInputStream& desc)
{	
	Gu::TriangleMesh* np;
	PX_NEW_SERIALIZED(np,Gu::TriangleMesh);
	if(!np)
		return NULL;

	np->setMeshFactory(this);

	if(!np->load(desc))
	{
		np->decRefCount();
		return NULL;
	}

	addTriangleMesh(np);
	return np;
}

bool GuMeshFactory::removeTriangleMesh(PxTriangleMesh& m)
{
	Ps::Mutex::ScopedLock lock(mTrackingMutex);
	Gu::TriangleMesh* gu = static_cast<Gu::TriangleMesh*>(&m);

	for(PxU32 i=0; i<mTriangleMeshArray.size(); i++)
	{
		if(mTriangleMeshArray[i]==gu)
		{
			GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseTriangleMesh, m)
			mTriangleMeshArray.replaceWithLast(i);
			return true;
		}
	}
	return false;
}

PxU32 GuMeshFactory::getNbTriangleMeshes() const
{
	return mTriangleMeshArray.size();
}

PxU32 GuMeshFactory::getTriangleMeshes(PxTriangleMesh** userBuffer, PxU32 bufferSize, PxU32 startIndex) const
{
	const PxU32 size = mTriangleMeshArray.size();

	const PxU32 remainder = (PxU32)PxMax<PxI32>(PxI32(size - startIndex), 0);
	const PxU32 writeCount = PxMin(remainder, bufferSize);
	for(PxU32 i=0; i<writeCount; i++)
		userBuffer[i] = mTriangleMeshArray[i+startIndex];

	return writeCount;
}

///////////////////////////////////////////////////////////////////////////////

void GuMeshFactory::addConvexMesh(Gu::ConvexMesh* np, bool lock)
{
	addToArray(mConvexMeshArray, np, lock ? &mTrackingMutex : NULL);
}

PxConvexMesh* GuMeshFactory::createConvexMesh(PxInputStream& desc)
{
	Gu::ConvexMesh* np;
	PX_NEW_SERIALIZED(np,Gu::ConvexMesh);
	if(!np)
		return NULL;

	np->setMeshFactory(this);

	if(!np->load(desc))
	{
		np->decRefCount();
		return NULL;
	}

	addConvexMesh(np);
	return np;
}

bool GuMeshFactory::removeConvexMesh(PxConvexMesh& m)
{
	Gu::ConvexMesh* gu = static_cast<Gu::ConvexMesh*>(&m);

	Ps::Mutex::ScopedLock lock(mTrackingMutex);
	for(PxU32 i=0; i<mConvexMeshArray.size(); i++)
	{
		if(mConvexMeshArray[i]==gu)
		{
			GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseConvexMesh, m)
			mConvexMeshArray.replaceWithLast(i);
			return true;
		}
	}

	return false;
}

PxU32 GuMeshFactory::getNbConvexMeshes() const
{
	return mConvexMeshArray.size();
}

PxU32 GuMeshFactory::getConvexMeshes(PxConvexMesh** userBuffer, PxU32 bufferSize, PxU32 startIndex) const
{
	const PxU32 size = mConvexMeshArray.size();

	const PxU32 remainder = (PxU32)PxMax<PxI32>(PxI32(size - startIndex), 0);
	const PxU32 writeCount = PxMin(remainder, bufferSize);
	for(PxU32 i=0; i<writeCount; i++)
		userBuffer[i] = mConvexMeshArray[i+startIndex];

	return writeCount;
}

///////////////////////////////////////////////////////////////////////////////

void GuMeshFactory::addHeightField(Gu::HeightField* np, bool lock)
{
	addToArray(mHeightFieldArray, np, lock ? &mTrackingMutex : NULL);
}

PxHeightField* GuMeshFactory::createHeightField(const PxHeightFieldDesc& desc)
{
	Gu::HeightField* np;
	PX_NEW_SERIALIZED(np,Gu::HeightField)(this);
	if(!np)
		return NULL;

	if(!np->loadFromDesc(desc))
	{
		np->decRefCount();
		return NULL;
	}

	addHeightField(np);
	return np;
}

PxHeightField* GuMeshFactory::createHeightField(PxInputStream& stream)
{
	Gu::HeightField* np;
	PX_NEW_SERIALIZED(np,Gu::HeightField)(this);
	if(!np)
		return NULL;

	if(!np->load(stream))
	{
		np->decRefCount();
		return NULL;
	}

	addHeightField(np);
	return np;
}

bool GuMeshFactory::removeHeightField(PxHeightField& hf)
{
	Gu::HeightField* gu = static_cast<Gu::HeightField*>(&hf);

	Ps::Mutex::ScopedLock lock(mTrackingMutex);

	for(PxU32 i=0; i<mHeightFieldArray.size(); i++)
	{
		if(mHeightFieldArray[i]==gu)
		{
			GU_MESH_FACTORY_GPU_NOTIFICATION(notifyReleaseHeightField, hf)
			mHeightFieldArray.replaceWithLast(i);
			return true;
		}
	}
	return false;
}

PxU32 GuMeshFactory::getNbHeightFields() const
{
	return mHeightFieldArray.size();
}

PxU32 GuMeshFactory::getHeightFields(PxHeightField** userBuffer, PxU32 bufferSize, PxU32 startIndex) const
{
	const PxU32 size = mHeightFieldArray.size();

	const PxU32 remainder = (PxU32)PxMax<PxI32>(PxI32(size - startIndex), 0);
	const PxU32 writeCount = PxMin(remainder, bufferSize);
	for(PxU32 i=0; i<writeCount; i++)
		userBuffer[i] = mHeightFieldArray[i+startIndex];

	return writeCount;
}


void GuMeshFactory::addFactoryListener( GuMeshFactoryListener& listener )
{
	mFactoryListeners.pushBack( &listener );
}
void GuMeshFactory::removeFactoryListener( GuMeshFactoryListener& listener )
{
	for ( PxU32 idx = 0; idx < mFactoryListeners.size(); ++idx )
	{
		if ( mFactoryListeners[idx] == &listener )
		{
			mFactoryListeners.replaceWithLast( idx );
			--idx;
		}
	}
}

void GuMeshFactory::notifyFactoryListener(const PxBase* base, PxType typeID, bool memRelease)
{
	notifyReleaseFactoryItem(mFactoryListeners, base, typeID, memRelease);
}

///////////////////////////////////////////////////////////////////////////////
