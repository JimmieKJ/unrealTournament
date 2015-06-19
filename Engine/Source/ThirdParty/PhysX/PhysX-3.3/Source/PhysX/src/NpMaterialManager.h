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


#ifndef NP_MATERIALMANAGER
#define NP_MATERIALMANAGER

#include "NpMaterial.h"
#ifdef PX_PS3
#include "ps3/PxPS3Config.h"
#endif


namespace physx
{


	class NpMaterialHandleManager
	{
	public:
		NpMaterialHandleManager() : currentID(0), freeIDs(PX_DEBUG_EXP("NpMaterialHandleManager"))	{}

		void	freeID(PxU32 id)
		{
			// Allocate on first call
			// Add released ID to the array of free IDs
			if(id == (currentID - 1))
				--currentID;
			else
				freeIDs.pushBack(id);
		}

		void	freeAll()
		{
			currentID = 0;
			freeIDs.clear();
		}

		PxU32	getNewID()
		{
			// If recycled IDs are available, use them
			const PxU32 size = freeIDs.size();
			if(size)
			{
				const PxU32 id = freeIDs[size-1]; // Recycle last ID
				freeIDs.popBack();
				return id;
			}
			// Else create a new ID
			return currentID++;
		}

		PxU32 getNumMaterials()
		{
			return currentID - freeIDs.size();
		}
	private:
		PxU32				currentID;
		Ps::Array<PxU32>	freeIDs;
	};

	class NpMaterialManager 
	{
	public:
		NpMaterialManager()
		{
			const PxU32 matCount = 
#ifdef PX_PS3
			PX_PS3_MAX_MATERIAL_COUNT;
#else
			128;
#endif
			//we allocate +1 for a space for a dummy material for PS3 when we allocate a material > 128 and need to clear it immediately after
			materials = (NpMaterial**)PX_ALLOC(sizeof(NpMaterial*)* matCount,  PX_DEBUG_EXP("NpMaterialManager::initialise"));
			maxMaterials = matCount;
			PxMemZero(materials, sizeof(NpMaterial*)*maxMaterials);
		}

		~NpMaterialManager() {}

		void releaseMaterials()
		{
			for(PxU32 i=0; i<maxMaterials; ++i)
			{
				if(materials[i])
				{
					PxU32 handle = materials[i]->getHandle();
					handleManager.freeID(handle);
					materials[i]->release();
					materials[i] = NULL;
					
				}
					//PX_DELETE(materials[i]);
			}
			PX_FREE(materials);
		}

		bool setMaterial(NpMaterial* mat)
		{
			PxU32 materialIndex = handleManager.getNewID();

			if(materialIndex >= maxMaterials)
			{
#ifdef	PX_PS3
				handleManager.freeID(materialIndex);
				return false;
#else
				resize();
#endif
			}

			materials[materialIndex] = mat;
			materials[materialIndex]->setHandle(materialIndex);
			return true;
		}

		void updateMaterial(NpMaterial* mat)
		{
			materials[mat->getHandle()] = mat;
		}

		PxU32 getNumMaterials()
		{
			return handleManager.getNumMaterials();
		}

		void removeMaterial(NpMaterial* mat)
		{
			PxU32 handle = mat->getHandle();
			if(handle != MATERIAL_INVALID_HANDLE)
			{
				materials[handle] = NULL;
				handleManager.freeID(handle);
			}
		}

		NpMaterial* getMaterial(const PxU32 index)const
		{
			PX_ASSERT(index <  maxMaterials);
			return materials[index];
		}

		PxU32 getMaxSize()const 
		{
			return maxMaterials;
		}

		NpMaterial** getMaterials() const
		{
			return materials;
		}

	private:
		void resize()
		{
			const PxU32 numMaterials = maxMaterials;
			maxMaterials = maxMaterials*2;

			NpMaterial** mat = (NpMaterial**)PX_ALLOC(sizeof(NpMaterial*)*maxMaterials,  PX_DEBUG_EXP("NpMaterialManager::resize"));
			PxMemZero(mat, sizeof(NpMaterial*)*maxMaterials);
			for(PxU32 i=0; i<numMaterials; ++i)
			{
				mat[i] = materials[i];
			}


			PX_FREE(materials);

			materials = mat;
		}

		NpMaterialHandleManager		handleManager;
		NpMaterial**				materials;
		PxU32						maxMaterials;
		
	};

	class NpMaterialManagerIterator
	{
	
	public:
		NpMaterialManagerIterator(const NpMaterialManager& manager) : mManager(manager), index(0)
		{
		}

		bool hasNextMaterial()
		{
			while(index < mManager.getMaxSize() && mManager.getMaterial(index)==NULL)
				index++;
			return index < mManager.getMaxSize();
		}

		NpMaterial* getNextMaterial()
		{
			PX_ASSERT(index < mManager.getMaxSize());
			return mManager.getMaterial(index++);
		}

		PxU32 getNumMaterial()
		{
			PxU32 numMaterials = 0;
			PxU32 i = 0;
			while(i < mManager.getMaxSize())
			{
				if(mManager.getMaterial(i++)!=NULL)//mManager.getMaterial(i++)->getHandle() != MATERIAL_INVALID_HANDLE)
					numMaterials++;
			}
			return numMaterials;
		}

	private:
		NpMaterialManagerIterator& operator=(const NpMaterialManagerIterator&);
		const NpMaterialManager& mManager;
		PxU32 index;

	};

}

#endif
