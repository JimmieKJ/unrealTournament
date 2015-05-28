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

#include "GuDistancePointTriangle.h"
#include "GuTriangleVertexPointers.h"
#include "GuContactMethodImpl.h"
#include "GuContactBuffer.h"
#include "GuGeometryUnion.h"
#include "GuFeatureCode.h"
#include "GuMidphase.h"
#include "CmScaling.h"
#include "GuEntityReport.h"
#include "GuHeightFieldUtil.h"

using namespace physx;
using namespace Gu;

#ifdef __SPU__
extern unsigned char HeightFieldBuffer[sizeof(Gu::HeightField)+16];
#include "CmMemFetch.h"
#endif

#ifdef __SPU__
	namespace physx
	{
	extern bool gSphereVsMeshContactLimitExceeded;
	}
#endif

static void outputErrorMessage()
{
#ifdef PX_CHECKED
	Ps::getFoundation().error(PxErrorCode::eINTERNAL_ERROR, __FILE__, __LINE__, "Dropping contacts in sphere vs mesh: exceeded limit of 64 ");
#elif defined(__SPU__)
	gSphereVsMeshContactLimitExceeded = true;
#endif
}

///////////////////////////////////////////////////////////////////////////////

// PT: a customized version that also returns the feature code
static PxVec3 closestPtPointTriangle(const PxVec3& p, const PxVec3& a, const PxVec3& b, const PxVec3& c, float& s, float& t, FeatureCode& fc)
{
	// Check if P in vertex region outside A
	const PxVec3 ab = b - a;
	const PxVec3 ac = c - a;
	const PxVec3 ap = p - a;
	const float d1 = ab.dot(ap);
	const float d2 = ac.dot(ap);
	if(d1<=0.0f && d2<=0.0f)
	{
		s = 0.0f;
		t = 0.0f;
		fc = FC_VERTEX0;
		return a;	// Barycentric coords 1,0,0
	}

	// Check if P in vertex region outside B
	const PxVec3 bp = p - b;
	const float d3 = ab.dot(bp);
	const float d4 = ac.dot(bp);
	if(d3>=0.0f && d4<=d3)
	{
		s = 1.0f;
		t = 0.0f;
		fc = FC_VERTEX1;
		return b;	// Barycentric coords 0,1,0
	}

	// Check if P in edge region of AB, if so return projection of P onto AB
	const float vc = d1*d4 - d3*d2;
	if(vc<=0.0f && d1>=0.0f && d3<=0.0f)
	{
		const float v = d1 / (d1 - d3);
		s = v;
		t = 0.0f;
		fc = FC_EDGE01;
		return a + v * ab;	// barycentric coords (1-v, v, 0)
	}

	// Check if P in vertex region outside C
	const PxVec3 cp = p - c;
	const float d5 = ab.dot(cp);
	const float d6 = ac.dot(cp);
	if(d6>=0.0f && d5<=d6)
	{
		s = 0.0f;
		t = 1.0f;
		fc = FC_VERTEX2;
		return c;	// Barycentric coords 0,0,1
	}

	// Check if P in edge region of AC, if so return projection of P onto AC
	const float vb = d5*d2 - d1*d6;
	if(vb<=0.0f && d2>=0.0f && d6<=0.0f)
	{
		const float w = d2 / (d2 - d6);
		s = 0.0f;
		t = w;
		fc = FC_EDGE20;
		return a + w * ac;	// barycentric coords (1-w, 0, w)
	}

	// Check if P in edge region of BC, if so return projection of P onto BC
	const float va = d3*d6 - d5*d4;
	if(va<=0.0f && (d4-d3)>=0.0f && (d5-d6)>=0.0f)
	{
		const float w = (d4-d3) / ((d4 - d3) + (d5-d6));
		s = 1.0f-w;
		t = w;
		fc = FC_EDGE12;
		return b + w * (c-b);	// barycentric coords (0, 1-w, w)
	}

	// P inside face region. Compute Q through its barycentric coords (u,v,w)
	const float denom = 1.0f / (va + vb + vc);
	const float v = vb * denom;
	const float w = vc * denom;
	s = v;
	t = w;
	fc = FC_FACE;
	return a + ab*v + ac*w;
}

///////////////////////////////////////////////////////////////////////////////

struct TriangleData
{
	PxVec3		delta;
	FeatureCode	fc;
	float		squareDist;
	PxU32		triangleIndex;
	PxU32		vref[3];
};

struct CachedTriangleIndices
{
	PxU32		vref[3];
};

static PX_FORCE_INLINE bool validateSquareDist(PxReal squareDist)
{
	return squareDist>0.0001f;
}

static bool validateEdge(PxU32 vref0, PxU32 vref1, const CachedTriangleIndices* cachedTris, PxU32 nbCachedTris)
{
	while(nbCachedTris--)
	{
		const CachedTriangleIndices& inds = *cachedTris++;
		const PxU32 vi0 = inds.vref[0];
		const PxU32 vi1 = inds.vref[1];
		const PxU32 vi2 = inds.vref[2];

		if(vi0==vref0)
		{
			if(vi1==vref1 || vi2==vref1)
				return false;
		}
		else if(vi1==vref0)
		{
			if(vi0==vref1 || vi2==vref1)
				return false;
		}
		else if(vi2==vref0)
		{
			if(vi1==vref1 || vi0==vref1)
				return false;
		}
	}
	return true;
}

static bool validateVertex(PxU32 vref, const CachedTriangleIndices* cachedTris, PxU32 nbCachedTris)
{
	while(nbCachedTris--)
	{
		const CachedTriangleIndices& inds = *cachedTris++;
		if(inds.vref[0]==vref || inds.vref[1]==vref || inds.vref[2]==vref)
			return false;
	}
	return true;
}


namespace
{

struct SphereMeshContactGeneration
{
	const PxSphereGeometry&	mShapeSphere;
	const PxTransform&		mTransform0;
	const PxTransform&		mTransform1;
	ContactBuffer&			mContactBuffer;
	const PxVec3&			mSphereCenterShape1Space;
	PxF32					mInflatedRadius;
	PxU32					mNbDelayed;
	TriangleData			mSavedData[ContactBuffer::MAX_CONTACTS];
	PxU32					mNbCachedTris;
	CachedTriangleIndices	mCachedTris[ContactBuffer::MAX_CONTACTS];

	SphereMeshContactGeneration(const PxSphereGeometry& shapeSphere, const PxTransform& transform0, const PxTransform& transform1,
		ContactBuffer& contactBuffer, const PxVec3& sphereCenterShape1Space, PxF32 inflatedRadius) :
		mShapeSphere				(shapeSphere),
		mTransform0					(transform0),
		mTransform1					(transform1),
		mContactBuffer				(contactBuffer),
		mSphereCenterShape1Space	(sphereCenterShape1Space),
		mInflatedRadius				(inflatedRadius),
		mNbDelayed					(0),
		mNbCachedTris				(0)
	{
	}

	PX_FORCE_INLINE void cacheTriangle(PxU32 ref0, PxU32 ref1, PxU32 ref2)
	{
		const PxU32 nb = mNbCachedTris++;
		mCachedTris[nb].vref[0] = ref0;
		mCachedTris[nb].vref[1] = ref1;
		mCachedTris[nb].vref[2] = ref2;
	}

	PX_FORCE_INLINE void addContact(const PxVec3& d, PxReal squareDist, PxU32 triangleIndex)
	{
		float dist;
		PxVec3 delta;
		if(validateSquareDist(squareDist))
		{
			// PT: regular contact. Normalize 'delta'.
			dist = PxSqrt(squareDist);
			delta = d / dist;
		}
		else
		{
			// PT: singular contact: 'd' is the non-unit triangle's normal in this case.
			dist = 0.0f;
			delta = -d.getNormalized();
		}

		const PxVec3 worldNormal = -mTransform1.rotate(delta);

		const PxVec3 localHit = mSphereCenterShape1Space + mShapeSphere.radius*delta;
		const PxVec3 hit = mTransform1.transform(localHit);

		if(!mContactBuffer.contact(hit, worldNormal, dist - mShapeSphere.radius, PXC_CONTACT_NO_FACE_INDEX, triangleIndex))
			outputErrorMessage();
	}

	void processTriangle(PxU32 triangleIndex, const PxVec3& v0, const PxVec3& v1, const PxVec3& v2, const PxU32* vertInds)
	{
		const PxF32 r2 = mInflatedRadius * mInflatedRadius;

		// PT: compute closest point between sphere center and triangle
		PxReal u, v;
		FeatureCode fc;
		const PxVec3 cp = closestPtPointTriangle(mSphereCenterShape1Space, v0, v1, v2, u, v, fc);

		// PT: compute 'delta' vector between closest point and sphere center
		PxVec3 delta = cp - mSphereCenterShape1Space;
		const PxReal squareDist = delta.magnitudeSquared();
		if(squareDist >= r2)
			return;

		// PT: backface culling without the normalize
		// PT: TODO: consider doing before the pt-triangle distance test if it's cheaper
		const PxVec3 e0 = v1 - v0;
		const PxVec3 e1 = v2 - v0;
		const PxVec3 planeNormal = e0.cross(e1);
		const PxF32 planeD = planeNormal.dot(v0);	// PT: actually -d compared to PxcPlane
		if(planeNormal.dot(mSphereCenterShape1Space) < planeD)
			return;

		// PT: for a regular contact, 'delta' is non-zero (and so is 'squareDist'). However when the sphere's center exactly touches
		// the triangle, then both 'delta' and 'squareDist' become zero. This needs to be handled as a special case to avoid dividing
		// by zero. We will use the triangle's normal as a contact normal in this special case.
		//
		// 'validateSquareDist' is called twice because there are conflicting goals here. We could call it once now and already
		// compute the proper data for generating the contact. But this would mean doing a square-root and a division right here,
		// even when the contact is not actually needed in the end. We could also call it only once in "addContact', but the plane's
		// normal would not always be available (in case of delayed contacts), and thus it would need to be either recomputed (slower)
		// or stored within 'TriangleData' (using more memory). Calling 'validateSquareDist' twice is a better option overall.
		PxVec3 d;
		if(validateSquareDist(squareDist))
			d = delta;
		else
			d = planeNormal;

		if(fc==FC_FACE)
		{
			addContact(d, squareDist, triangleIndex);

			if(mNbCachedTris<ContactBuffer::MAX_CONTACTS)
				cacheTriangle(vertInds[0], vertInds[1], vertInds[2]);
		}
		else
		{
			if(mNbDelayed<ContactBuffer::MAX_CONTACTS)
			{
				TriangleData* saved = mSavedData + mNbDelayed++;
				saved->delta			= d;
				saved->vref[0]			= vertInds[0];
				saved->vref[1]			= vertInds[1];
				saved->vref[2]			= vertInds[2];
				saved->fc				= fc;
				saved->squareDist		= squareDist;
				saved->triangleIndex	= triangleIndex;
			}
			else outputErrorMessage();
		}
	}

	void generateLastContacts()
	{
		const PxU32 count = mNbDelayed;
		if(!count)
			return;

		struct Local
		{
			static int compare(const void* c0, const void* c1)
			{
				const TriangleData* mc0 = (const TriangleData*)c0;
				const TriangleData* mc1 = (const TriangleData*)c1;
				return mc0->squareDist > mc1->squareDist;
			}
		};
		TriangleData* touchedTris = mSavedData;
		qsort(touchedTris, (size_t)count, sizeof(TriangleData), Local::compare);

		// PT: the single pass works thanks to the initial sorting, which makes us process edges
		// before vertices in cases that would need a separate pass for vertices...
		for(PxU32 i=0;i<count;i++)
		{
			const PxU32 ref0 = touchedTris[i].vref[0];
			const PxU32 ref1 = touchedTris[i].vref[1];
			const PxU32 ref2 = touchedTris[i].vref[2];

			bool generateContact = false;

			switch(touchedTris[i].fc)
			{
				case FC_VERTEX0:
					generateContact = ::validateVertex(ref0, mCachedTris, mNbCachedTris);
					break;

				case FC_VERTEX1:
					generateContact = ::validateVertex(ref1, mCachedTris, mNbCachedTris);
					break;

				case FC_VERTEX2:
					generateContact = ::validateVertex(ref2, mCachedTris, mNbCachedTris);
					break;

				case FC_EDGE01:
					generateContact = ::validateEdge(ref0, ref1, mCachedTris, mNbCachedTris);
					break;

				case FC_EDGE12:
					generateContact = ::validateEdge(ref1, ref2, mCachedTris, mNbCachedTris);
					break;

				case FC_EDGE20:
					generateContact = ::validateEdge(ref0, ref2, mCachedTris, mNbCachedTris);
					break;

				case FC_FACE:
				case FC_UNDEFINED:
					PX_ASSERT(0);	// PT: should not be possible
				default:
					break;
			};
	
			if(generateContact)
				addContact(touchedTris[i].delta, touchedTris[i].squareDist, touchedTris[i].triangleIndex);

			if(mNbCachedTris<ContactBuffer::MAX_CONTACTS)
				cacheTriangle(ref0, ref1, ref2);
			else
				outputErrorMessage();
		}
	}

private:
	SphereMeshContactGeneration& operator=(const SphereMeshContactGeneration&);
};

struct SphereMeshContactGenerationCallback_NoScale : MeshHitCallback<PxRaycastHit>
{
	SphereMeshContactGeneration			mGeneration;
	const InternalTriangleMeshData&		mMeshData;

	SphereMeshContactGenerationCallback_NoScale(const InternalTriangleMeshData& meshData, const PxSphereGeometry& shapeSphere,
		const PxTransform& transform0, const PxTransform& transform1, ContactBuffer& contactBuffer,
		const PxVec3& sphereCenterShape1Space, PxF32 inflatedRadius
	) : MeshHitCallback<PxRaycastHit>	(CallbackMode::eMULTIPLE),
		mGeneration						(shapeSphere, transform0, transform1, contactBuffer, sphereCenterShape1Space, inflatedRadius),
		mMeshData						(meshData)
	{
	}

	virtual ~SphereMeshContactGenerationCallback_NoScale()
	{
		mGeneration.generateLastContacts();
	}

	virtual PxAgain processHit(
		const PxRaycastHit& hit, const PxVec3& v0, const PxVec3& v1, const PxVec3& v2, PxReal&, const PxU32* vinds)
	{
		mGeneration.processTriangle(hit.faceIndex, v0, v1, v2, vinds);
		return true;
	}

protected:
	SphereMeshContactGenerationCallback_NoScale &operator=(const SphereMeshContactGenerationCallback_NoScale &);
};

struct SphereMeshContactGenerationCallback_Scale : SphereMeshContactGenerationCallback_NoScale
{
	const Cm::FastVertex2ShapeScaling&	mMeshScaling;

	SphereMeshContactGenerationCallback_Scale(const InternalTriangleMeshData& meshData, const PxSphereGeometry& shapeSphere,
		const PxTransform& transform0, const PxTransform& transform1, const Cm::FastVertex2ShapeScaling& meshScaling,
		ContactBuffer& contactBuffer, const PxVec3& sphereCenterShape1Space, PxF32 inflatedRadius
	) : SphereMeshContactGenerationCallback_NoScale(meshData, shapeSphere,
		transform0, transform1, contactBuffer, sphereCenterShape1Space, inflatedRadius),
		mMeshScaling	(meshScaling)
	{
	}

	virtual ~SphereMeshContactGenerationCallback_Scale() {}

	virtual PxAgain processHit(
		const PxRaycastHit& hit, const PxVec3& v0, const PxVec3& v1, const PxVec3& v2, PxReal&, const PxU32* vinds)
	{
		const PxVec3 v0b = mMeshScaling * v0;
		const PxVec3 v1b = mMeshScaling * v1;
		const PxVec3 v2b = mMeshScaling * v2;
		mGeneration.processTriangle(hit.faceIndex, v0b, v1b, v2b, vinds);
		return true;
	}
protected:
	SphereMeshContactGenerationCallback_Scale &operator=(const SphereMeshContactGenerationCallback_Scale &);
};

}

namespace physx
{
bool Gu::contactSphereMesh(GU_CONTACT_METHOD_ARGS)
{
	PX_UNUSED(cache);

	const PxSphereGeometry& shapeSphere = shape0.get<const PxSphereGeometry>();
	const PxTriangleMeshGeometryLL& shapeMesh = shape1.get<const PxTriangleMeshGeometryLL>();

#ifdef __SPU__
	// PT: TODO: cache this one
	// fetch meshData to temp buffer
	PX_ALIGN_PREFIX(16) char meshDataBuf[sizeof(InternalTriangleMeshData)] PX_ALIGN_SUFFIX(16);
	Cm::memFetchAlignedAsync(PxU64(meshDataBuf), PxU64(shapeMesh.meshData), sizeof(InternalTriangleMeshData), 5);
#endif

	Cm::FastVertex2ShapeScaling meshScaling;	// PT: TODO: get rid of default ctor :(
	const bool idtMeshScale = shapeMesh.scale.isIdentity();
	if(!idtMeshScale)
		meshScaling.init(shapeMesh.scale);

	// We must be in local space to use the cache
	const PxVec3 sphereCenterShape1Space = transform1.transformInv(transform0.p);
	const PxReal inflatedRadius = shapeSphere.radius + contactDistance;

#ifdef __SPU__
	Cm::memFetchWait(5);
	const InternalTriangleMeshData* meshData = reinterpret_cast<const InternalTriangleMeshData*>(meshDataBuf);
#else
	const InternalTriangleMeshData* meshData = shapeMesh.meshData;
#endif

	Gu::RTreeMidphaseData hmd;
	meshData->mCollisionModel.getRTreeMidphaseData(hmd);

	PxVec3 obbCenter = sphereCenterShape1Space;
	PxVec3 obbExtents = PxVec3(inflatedRadius);
	PxMat33 obbRot(PxIdentity);
	if(!idtMeshScale)
		meshScaling.transformQueryBounds(obbCenter, obbExtents, obbRot);

	const Gu::Box obb(obbCenter, obbExtents, obbRot);
	MPT_SET_CONTEXT("cosm", transform1, shapeMesh.scale);
	// mesh scale is not baked into cached verts
	if(!idtMeshScale)
	{
		SphereMeshContactGenerationCallback_Scale callback(
			*meshData, shapeSphere,
			transform0, transform1,
			meshScaling, contactBuffer,
			sphereCenterShape1Space, inflatedRadius);
		Gu::MeshRayCollider::collideOBB(obb, true, hmd, callback);
	}
	else
	{
		SphereMeshContactGenerationCallback_NoScale callback(
			*meshData, shapeSphere,
			transform0, transform1,
			contactBuffer, sphereCenterShape1Space, inflatedRadius);
		Gu::MeshRayCollider::collideOBB(obb, true, hmd, callback);
	}
	return (contactBuffer.count > 0);
}
}

/////

namespace
{
struct SphereHeightfieldContactGenerationCallback : Gu::EntityReport<PxU32>
{
	SphereMeshContactGeneration mGeneration;
	Gu::HeightFieldUtil&		mHfUtil;

	SphereHeightfieldContactGenerationCallback(
		Gu::HeightFieldUtil&	hfUtil,
		const PxSphereGeometry&	shapeSphere,
		const PxTransform&		transform0,
		const PxTransform&		transform1,
		ContactBuffer&			contactBuffer,
		const PxVec3&			sphereCenterShape1Space,
		PxF32					inflatedRadius
	) :
		mGeneration	(shapeSphere, transform0, transform1, contactBuffer, sphereCenterShape1Space, inflatedRadius),
		mHfUtil		(hfUtil)
	{
	}

	virtual bool onEvent(PxU32 nb, PxU32* indices)
	{
		while(nb--)
		{
			const PxU32 triangleIndex = *indices++;
			PxU32 vertIndices[3];
			PxTriangle currentTriangle;
			mHfUtil.getTriangle(mGeneration.mTransform1, currentTriangle, vertIndices, NULL, triangleIndex, false, false);

			mGeneration.processTriangle(triangleIndex, currentTriangle.verts[0], currentTriangle.verts[1], currentTriangle.verts[2], vertIndices);
		}
		return true;
	}
protected:
	SphereHeightfieldContactGenerationCallback &operator=(const SphereHeightfieldContactGenerationCallback &);
};
}

namespace physx
{
bool Gu::contactSphereHeightField(GU_CONTACT_METHOD_ARGS)
{
	PX_UNUSED(cache);

	const PxSphereGeometry& shapeSphere = shape0.get<const PxSphereGeometry>();
	const PxHeightFieldGeometryLL& shapeMesh = shape1.get<const PxHeightFieldGeometryLL>();

#ifdef __SPU__
		const Gu::HeightField& hf = *Cm::memFetchAsync<const Gu::HeightField>(HeightFieldBuffer, Cm::MemFetchPtr(static_cast<Gu::HeightField*>(shapeMesh.heightField)), sizeof(Gu::HeightField), 1);
		Cm::memFetchWait(1);
#if HF_TILED_MEMORY_LAYOUT
		g_sampleCache.init((uintptr_t)(hf.getData().samples), hf.getData().tilesU);
#endif
#else
	const Gu::HeightField& hf = *static_cast<Gu::HeightField*>(shapeMesh.heightField);
#endif
	Gu::HeightFieldUtil hfUtil(shapeMesh, hf);

	const PxVec3 sphereCenterShape1Space = transform1.transformInv(transform0.p);
	PxReal inflatedRadius = shapeSphere.radius + contactDistance;
	PxVec3 inflatedRV3(inflatedRadius);

	PxBounds3 bounds(sphereCenterShape1Space - inflatedRV3, sphereCenterShape1Space + inflatedRV3);

	SphereHeightfieldContactGenerationCallback blockCallback(hfUtil, shapeSphere, transform0, transform1, contactBuffer, sphereCenterShape1Space, inflatedRadius);

	MPT_SET_CONTEXT("cosh", PxTransform(), PxMeshScale());
	hfUtil.overlapAABBTriangles(transform1, bounds, 0, &blockCallback);

	blockCallback.mGeneration.generateLastContacts();

	return (contactBuffer.count > 0);
}

}
