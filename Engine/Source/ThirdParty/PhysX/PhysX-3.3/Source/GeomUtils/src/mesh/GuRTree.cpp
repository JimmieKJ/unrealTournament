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

#define RTREE_TEXT_DUMP_ENABLE		0
#if defined(PX_P64)
#define RTREE_PAGES_PER_POOL_SLAB	16384 // preallocate all pages in first batch to make sure we stay within 32 bits for relative pointers.. this is 2 megs
#else
#define RTREE_PAGES_PER_POOL_SLAB	128
#endif

#define INSERT_SCAN_LOOKAHEAD		1 // enable one level lookahead scan for determining which child page is best to insert a node into

#include "GuRTree.h"
#include "PsSort.h"
#include "GuSerialize.h"
#include "CmMemFetch.h"
#include "CmUtils.h"

using namespace physx;
using namespace Ps::aos;
using Ps::Array;
using Ps::sort;
using namespace Gu;

namespace physx
{
namespace Gu {

/////////////////////////////////////////////////////////////////////////
#ifdef PX_X360
#define CONVERT_PTR_TO_INT PxU32
#else
#define CONVERT_PTR_TO_INT PxU64
#endif

#ifdef PX_P64
RTreePage* RTree::sFirstPoolPage = NULL; // used for relative addressing on 64-bit platforms
#endif

/////////////////////////////////////////////////////////////////////////
RTree::RTree()
{
	PX_ASSERT((Cm::MemFetchPtr(this) & 15) == 0);
	mFlags = 0;
	mPages = NULL;
	mTotalNodes = 0;
	mNumLevels = 0;
	mPageSize = RTreePage::SIZE;
}

/////////////////////////////////////////////////////////////////////////
PxU32 RTree::mVersion = 1;

bool RTree::save(PxOutputStream& stream) const
{
	// save the RTree root structure followed immediately by RTreePage pages to an output stream
	bool mismatch = (littleEndian() == 1);
	writeChunk('R', 'T', 'R', 'E', stream);
	writeDword(mVersion, mismatch, stream);
	writeFloatBuffer(&mBoundsMin.x, 4, mismatch, stream);
	writeFloatBuffer(&mBoundsMax.x, 4, mismatch, stream);
	writeFloatBuffer(&mInvDiagonal.x, 4, mismatch, stream);
	writeFloatBuffer(&mDiagonalScaler.x, 4, mismatch, stream);
	writeDword(mPageSize, mismatch, stream);
	writeDword(mNumRootPages, mismatch, stream);
	writeDword(mNumLevels, mismatch, stream);
	writeDword(mTotalNodes, mismatch, stream);
	writeDword(mTotalPages, mismatch, stream);
	writeDword(mUnused, mismatch, stream);
	for (PxU32 j = 0; j < mTotalPages; j++)
	{
		writeFloatBuffer(mPages[j].minx, RTreePage::SIZE, mismatch, stream);
		writeFloatBuffer(mPages[j].miny, RTreePage::SIZE, mismatch, stream);
		writeFloatBuffer(mPages[j].minz, RTreePage::SIZE, mismatch, stream);
		writeFloatBuffer(mPages[j].maxx, RTreePage::SIZE, mismatch, stream);
		writeFloatBuffer(mPages[j].maxy, RTreePage::SIZE, mismatch, stream);
		writeFloatBuffer(mPages[j].maxz, RTreePage::SIZE, mismatch, stream);
		WriteDwordBuffer(mPages[j].ptrs, RTreePage::SIZE, mismatch, stream);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////
bool RTree::load(PxInputStream& stream, PxU32 meshVersion)
{
	PX_ASSERT((mFlags & IS_DYNAMIC) == 0);
	PX_UNUSED(meshVersion);

	release();

	PxI8 a, b, c, d;
	readChunk(a, b, c, d, stream);
	if(a!='R' || b!='T' || c!='R' || d!='E')
		return false;

	bool mismatch = (littleEndian() == 1);
	if(readDword(mismatch, stream) != mVersion)
		return false;

	readFloatBuffer(&mBoundsMin.x, 4, mismatch, stream);
	readFloatBuffer(&mBoundsMax.x, 4, mismatch, stream);
	readFloatBuffer(&mInvDiagonal.x, 4, mismatch, stream);
	readFloatBuffer(&mDiagonalScaler.x, 4, mismatch, stream);
	mPageSize = readDword(mismatch, stream);
	mNumRootPages = readDword(mismatch, stream);
	mNumLevels = readDword(mismatch, stream);
	mTotalNodes = readDword(mismatch, stream);
	mTotalPages = readDword(mismatch, stream);
	mUnused = readDword(mismatch, stream);

	mPages = static_cast<RTreePage*>(
		Ps::AlignedAllocator<128>().allocate(sizeof(RTreePage)*mTotalPages, __FILE__, __LINE__));
	Cm::markSerializedMem(mPages, sizeof(RTreePage)*mTotalPages);
	for (PxU32 j = 0; j < mTotalPages; j++)
	{
		readFloatBuffer(mPages[j].minx, RTreePage::SIZE, mismatch, stream);
		readFloatBuffer(mPages[j].miny, RTreePage::SIZE, mismatch, stream);
		readFloatBuffer(mPages[j].minz, RTreePage::SIZE, mismatch, stream);
		readFloatBuffer(mPages[j].maxx, RTreePage::SIZE, mismatch, stream);
		readFloatBuffer(mPages[j].maxy, RTreePage::SIZE, mismatch, stream);
		readFloatBuffer(mPages[j].maxz, RTreePage::SIZE, mismatch, stream);
		ReadDwordBuffer(mPages[j].ptrs, RTreePage::SIZE, mismatch, stream);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////
PxU32 RTree::computeBottomLevelCount(PxU32 multiplier) const
{
	PX_ASSERT((mFlags & IS_DYNAMIC) == 0);
	PxU32 topCount = 0, curCount = mNumRootPages;
	const RTreePage* rightMostPage = &mPages[mNumRootPages-1];
	PX_ASSERT(rightMostPage);
	for (PxU32 level = 0; level < mNumLevels-1; level++)
	{
		topCount += curCount;
		PxU32 nc = rightMostPage->nodeCount();
		PX_ASSERT(nc > 0 && nc <= RTreePage::SIZE);
		// old version pointer, up to PX_MESH_VERSION 8
		PxU32 ptr = (rightMostPage->ptrs[nc-1]) * multiplier;
		PX_ASSERT(ptr % sizeof(RTreePage) == 0);
		const RTreePage* rightMostPageNext = mPages + (ptr / sizeof(RTreePage));
		curCount = PxU32(rightMostPageNext - rightMostPage);
		rightMostPage = rightMostPageNext;
	}

	return mTotalPages - topCount;
}

/////////////////////////////////////////////////////////////////////////
RTree::RTree(const PxEMPTY&)
{
	mFlags |= USER_ALLOCATED;
}

/////////////////////////////////////////////////////////////////////////
void RTree::release()
{
	if ((mFlags & USER_ALLOCATED) == 0 && mPages)
	{
		Ps::AlignedAllocator<128>().deallocate(mPages);
		mPages = NULL;
	}
}

// PX_SERIALIZATION
/////////////////////////////////////////////////////////////////////////
void RTree::exportExtraData(PxSerializationContext& stream)
{
	stream.alignData(128);
	stream.writeData(mPages, mTotalPages*sizeof(RTreePage));
}

/////////////////////////////////////////////////////////////////////////
void RTree::importExtraData(PxDeserializationContext& context)
{
	context.alignExtraData(128);
	mPages = context.readExtraData<RTreePage>(mTotalPages);
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE PxU32 RTreePage::nodeCount() const
{
	for (int j = 0; j < RTreePage::SIZE; j ++)
		if (minx[j] == MX)
			return (PxU32)j;

	return RTreePage::SIZE;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::clearNode(PxU32 nodeIndex)
{
	PX_ASSERT(nodeIndex < RTreePage::SIZE);
	minx[nodeIndex] = miny[nodeIndex] = minz[nodeIndex] = MX; // initialize empty node with sentinels
	maxx[nodeIndex] = maxy[nodeIndex] = maxz[nodeIndex] = MN;
	ptrs[nodeIndex] = 0;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::getNode(const PxU32 nodeIndex, RTreeNodeQ& r) const
{
	PX_ASSERT(nodeIndex < RTreePage::SIZE);
	r.minx = minx[nodeIndex];
	r.miny = miny[nodeIndex];
	r.minz = minz[nodeIndex];
	r.maxx = maxx[nodeIndex];
	r.maxy = maxy[nodeIndex];
	r.maxz = maxz[nodeIndex];
	r.ptr  = ptrs[nodeIndex];
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE PxU32 RTreePage::getNodeHandle(PxU32 index) const
{
	PX_ASSERT(index < RTreePage::SIZE);
	PX_ASSERT(CONVERT_PTR_TO_INT(reinterpret_cast<size_t>(this)) % sizeof(RTreePage) == 0);
	PxU32 result = (RTree::pagePtrTo32Bits(this) | index);
	return result;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::setEmpty(PxU32 startIndex)
{
	PX_ASSERT(startIndex < RTreePage::SIZE);
	for (PxU32 j = startIndex; j < RTreePage::SIZE; j ++)
		clearNode(j);
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::computeBounds(RTreeNodeQ& newBounds)
{
	RTreeValue _minx = MX, _miny = MX, _minz = MX, _maxx = MN, _maxy = MN, _maxz = MN;
	for (PxU32 j = 0; j < RTreePage::SIZE; j++)
	{
		if (isEmpty(j))
			continue;
		_minx = PxMin(_minx, minx[j]);
		_miny = PxMin(_miny, miny[j]);
		_minz = PxMin(_minz, minz[j]);
		_maxx = PxMax(_maxx, maxx[j]);
		_maxy = PxMax(_maxy, maxy[j]);
		_maxz = PxMax(_maxz, maxz[j]);
	}
	newBounds.minx = _minx;
	newBounds.miny = _miny;
	newBounds.minz = _minz;
	newBounds.maxx = _maxx;
	newBounds.maxy = _maxy;
	newBounds.maxz = _maxz;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::adjustChildBounds(PxU32 index, const RTreeNodeQ& adjChild)
{
	PX_ASSERT(index < RTreePage::SIZE);
	minx[index] = adjChild.minx;
	miny[index] = adjChild.miny;
	minz[index] = adjChild.minz;
	maxx[index] = adjChild.maxx;
	maxy[index] = adjChild.maxy;
	maxz[index] = adjChild.maxz;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::growChildBounds(PxU32 index, const RTreeNodeQ& child)
{
	PX_ASSERT(index < RTreePage::SIZE);
	minx[index] = PxMin(minx[index], child.minx);
	miny[index] = PxMin(miny[index], child.miny);
	minz[index] = PxMin(minz[index], child.minz);
	maxx[index] = PxMax(maxx[index], child.maxx);
	maxy[index] = PxMax(maxy[index], child.maxy);
	maxz[index] = PxMax(maxz[index], child.maxz);
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::copyNode(PxU32 targetIndex, const RTreePage& sourcePage, PxU32 sourceIndex)
{
	PX_ASSERT(targetIndex < RTreePage::SIZE);
	PX_ASSERT(sourceIndex < RTreePage::SIZE);
	minx[targetIndex] = sourcePage.minx[sourceIndex];
	miny[targetIndex] = sourcePage.miny[sourceIndex];
	minz[targetIndex] = sourcePage.minz[sourceIndex];
	maxx[targetIndex] = sourcePage.maxx[sourceIndex];
	maxy[targetIndex] = sourcePage.maxy[sourceIndex];
	maxz[targetIndex] = sourcePage.maxz[sourceIndex];
	ptrs[targetIndex] = sourcePage.ptrs[sourceIndex];
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreePage::setNode(PxU32 targetIndex, const RTreeNodeQ& sourceNode)
{
	PX_ASSERT(targetIndex < RTreePage::SIZE);
	minx[targetIndex] = sourceNode.minx;
	miny[targetIndex] = sourceNode.miny;
	minz[targetIndex] = sourceNode.minz;
	maxx[targetIndex] = sourceNode.maxx;
	maxy[targetIndex] = sourceNode.maxy;
	maxz[targetIndex] = sourceNode.maxz;
	ptrs[targetIndex] = sourceNode.ptr;
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreeNodeQ::grow(const RTreePage& page, int nodeIndex)
{
	PX_ASSERT(nodeIndex < RTreePage::SIZE);
	minx = PxMin(minx, page.minx[nodeIndex]);
	miny = PxMin(miny, page.miny[nodeIndex]);
	minz = PxMin(minz, page.minz[nodeIndex]);
	maxx = PxMax(maxx, page.maxx[nodeIndex]);
	maxy = PxMax(maxy, page.maxy[nodeIndex]);
	maxz = PxMax(maxz, page.maxz[nodeIndex]);
}

/////////////////////////////////////////////////////////////////////////
PX_FORCE_INLINE void RTreeNodeQ::grow(const RTreeNodeQ& node)
{
	minx = PxMin(minx, node.minx); miny = PxMin(miny, node.miny); minz = PxMin(minz, node.minz);
	maxx = PxMax(maxx, node.maxx); maxy = PxMax(maxy, node.maxy); maxz = PxMax(maxz, node.maxz);
}

/////////////////////////////////////////////////////////////////////////
void RTree::validateRecursive(PxU32 level, RTreeNodeQ parentBounds, RTreePage* page, CallbackRefit* cbLeaf)
{
	PX_UNUSED(parentBounds);

	static PxU32 validateCounter = 0; // this is to suppress a warning that recursive call has no side effects
	validateCounter++;

	RTreeNodeQ n;
	PxU32 pageNodeCount = page->nodeCount();
	for (PxU32 j = 0; j < pageNodeCount; j++)
	{
		page->getNode(j, n);
		if (page->isEmpty(j))
			continue;
		PX_ASSERT(n.minx >= parentBounds.minx); PX_ASSERT(n.miny >= parentBounds.miny); PX_ASSERT(n.minz >= parentBounds.minz);
		PX_ASSERT(n.maxx <= parentBounds.maxx); PX_ASSERT(n.maxy <= parentBounds.maxy); PX_ASSERT(n.maxz <= parentBounds.maxz);
		if (!n.isLeaf())
		{
			PX_ASSERT((n.ptr&1) == 0);
            RTreePage* childPage = (RTreePage*)(size_t(CONVERT_PTR_TO_INT(reinterpret_cast<size_t>(get64BitBasePage()))) + n.ptr);
			validateRecursive(level+1, n, childPage, cbLeaf);
		} else if (cbLeaf)
		{
			Vec3V mnv, mxv;
			cbLeaf->recomputeBounds(page->ptrs[j] & ~1, mnv, mxv);
			PxVec3 mn3, mx3; V3StoreU(mnv, mn3); V3StoreU(mxv, mx3);
			const PxBounds3 lb(mn3, mx3);
			const PxVec3& mn = lb.minimum; const PxVec3& mx = lb.maximum; PX_UNUSED(mn); PX_UNUSED(mx);
			PX_ASSERT(mn.x >= n.minx); PX_ASSERT(mn.y >= n.miny); PX_ASSERT(mn.z >= n.minz);
			PX_ASSERT(mx.x <= n.maxx); PX_ASSERT(mx.y <= n.maxy); PX_ASSERT(mx.z <= n.maxz);
		}
	}
	RTreeNodeQ recomputedBounds;
	page->computeBounds(recomputedBounds);
	PX_ASSERT((recomputedBounds.minx - parentBounds.minx)<=RTREE_INFLATION_EPSILON);
	PX_ASSERT((recomputedBounds.miny - parentBounds.miny)<=RTREE_INFLATION_EPSILON);
	PX_ASSERT((recomputedBounds.minz - parentBounds.minz)<=RTREE_INFLATION_EPSILON);
	PX_ASSERT((recomputedBounds.maxx - parentBounds.maxx)<=RTREE_INFLATION_EPSILON);
	PX_ASSERT((recomputedBounds.maxy - parentBounds.maxy)<=RTREE_INFLATION_EPSILON);
	PX_ASSERT((recomputedBounds.maxz - parentBounds.maxz)<=RTREE_INFLATION_EPSILON);
}

/////////////////////////////////////////////////////////////////////////
void RTree::validate(CallbackRefit* cbLeaf)
{
	for (PxU32 j = 0; j < mNumRootPages; j++)
	{
		RTreeNodeQ rootBounds;
		mPages[j].computeBounds(rootBounds);
		validateRecursive(0, rootBounds, mPages+j, cbLeaf);
	}
}

/////////////////////////////////////////////////////////////////////////
PxBounds3 RTree::refitRecursive(PxU8* treeNodes8, PxU32 top, RTreePage* parentPage, PxU32 parentSubIndex, CallbackRefit& cb)
{
	const PxReal eps = RTREE_INFLATION_EPSILON;
	RTreePage* tn = (RTreePage*)(treeNodes8 + top);
	PxBounds3 pageBound;
	for (PxU32 i = 0; i < RTREE_PAGE_SIZE; i++)
	{
		if (tn->isEmpty(i))
			continue;
		PxU32 ptr = tn->ptrs[i];
		Vec3V childMn, childMx;
		PxBounds3 childBound;
		if (ptr & 1)
		{
			// (ptr-1) clears the isLeaf bit (lowest bit)
			cb.recomputeBounds(ptr-1, childMn, childMx); // compute the bound around triangles
			V3StoreU(childMn, childBound.minimum);
			V3StoreU(childMx, childBound.maximum);
			// AP: doesn't seem worth vectorizing because of transposed layout
			tn->minx[i] = childBound.minimum.x - eps; // update page bounds for this leaf
			tn->miny[i] = childBound.minimum.y - eps;
			tn->minz[i] = childBound.minimum.z - eps;
			tn->maxx[i] = childBound.maximum.x + eps;
			tn->maxy[i] = childBound.maximum.y + eps;
			tn->maxz[i] = childBound.maximum.z + eps;
		} else
			childBound = refitRecursive(treeNodes8, ptr, tn, i, cb);
		if (i == 0)
			pageBound = childBound;
		else
			pageBound.include(childBound);
	}

	if (parentPage)
	{
		parentPage->minx[parentSubIndex] = pageBound.minimum.x - eps;
		parentPage->miny[parentSubIndex] = pageBound.minimum.y - eps;
		parentPage->minz[parentSubIndex] = pageBound.minimum.z - eps;
		parentPage->maxx[parentSubIndex] = pageBound.maximum.x + eps;
		parentPage->maxy[parentSubIndex] = pageBound.maximum.y + eps;
		parentPage->maxz[parentSubIndex] = pageBound.maximum.z + eps;
	}

	return pageBound;
}

#define CAST_U8(a) reinterpret_cast<PxU8*>(a)

PxBounds3 RTree::refitAll(CallbackRefit& cb)
{
	PxU8* treeNodes8 = PX_IS_X64 ? CAST_U8(get64BitBasePage()) : CAST_U8((mFlags & IS_DYNAMIC) ? NULL : mPages);
	PxBounds3 meshBounds = refitRecursive(treeNodes8, 0, NULL, 0, cb);
	for (PxU32 j = 1; j<mNumRootPages; j++)
		meshBounds.include(   refitRecursive(treeNodes8, j, NULL, 0, cb)   );

#ifdef PX_CHECKED
	validate(&cb);
#endif
	return meshBounds;
}

void RTree::refitAllStaticTree(CallbackRefit& cb, PxBounds3* retBounds)
{
	PxU8* treeNodes8 = PX_IS_X64 ? CAST_U8(get64BitBasePage()) : CAST_U8((mFlags & IS_DYNAMIC) ? NULL : mPages);

	// since pages are ordered we can scan back to front and the hierarchy will be updated
	for (PxI32 iPage = PxI32(mTotalPages)-1; iPage>=0; iPage--)
	{
		RTreePage& page = mPages[iPage];
		for (PxU32 j = 0; j < RTREE_PAGE_SIZE; j++)
		{
			if (page.isEmpty(j))
				continue;
			if (page.isLeaf(j))
			{
				Vec3V childMn, childMx;
				cb.recomputeBounds(page.ptrs[j]-1, childMn, childMx); // compute the bound around triangles
				PxVec3 mn3, mx3;
				V3StoreU(childMn, mn3);
				V3StoreU(childMx, mx3);
				page.minx[j] = mn3.x; page.miny[j] = mn3.y; page.minz[j] = mn3.z;
				page.maxx[j] = mx3.x; page.maxy[j] = mx3.y; page.maxz[j] = mx3.z;
			} else
			{
				const RTreePage* child = (const RTreePage*)(treeNodes8 + page.ptrs[j]);
				PX_COMPILE_TIME_ASSERT(RTREE_PAGE_SIZE == 4);
				bool first = true;
				for (PxU32 k = 0; k < RTREE_PAGE_SIZE; k++)
				{
					if (child->isEmpty(k))
						continue;
					if (first)
					{
						page.minx[j] = child->minx[k]; page.miny[j] = child->miny[k]; page.minz[j] = child->minz[k];
						page.maxx[j] = child->maxx[k]; page.maxy[j] = child->maxy[k]; page.maxz[j] = child->maxz[k];
						first = false;
					} else
					{
						page.minx[j] = PxMin(page.minx[j], child->minx[k]);
						page.miny[j] = PxMin(page.miny[j], child->miny[k]);
						page.minz[j] = PxMin(page.minz[j], child->minz[k]);
						page.maxx[j] = PxMax(page.maxx[j], child->maxx[k]);
						page.maxy[j] = PxMax(page.maxy[j], child->maxy[k]);
						page.maxz[j] = PxMax(page.maxz[j], child->maxz[k]);
					}
				}
			}
		}
	}

	if (retBounds)
	{
		RTreeNodeQ bound1;
		for (PxU32 ii = 0; ii<mNumRootPages; ii++)
		{
			mPages[ii].computeBounds(bound1);
			if (ii == 0)
			{
				retBounds->minimum = PxVec3(bound1.minx, bound1.miny, bound1.minz);
				retBounds->maximum = PxVec3(bound1.maxx, bound1.maxy, bound1.maxz);
			} else
			{
				retBounds->minimum = retBounds->minimum.minimum(PxVec3(bound1.minx, bound1.miny, bound1.minz));
				retBounds->maximum = retBounds->maximum.maximum(PxVec3(bound1.maxx, bound1.maxy, bound1.maxz));
			}
		}
	}

#ifdef PX_CHECKED
	validate(&cb);
#endif
}


//~PX_SERIALIZATION
const RTreeValue RTreePage::MN = -PX_MAX_REAL;
const RTreeValue RTreePage::MX = PX_MAX_REAL;

} // namespace Gu

}
