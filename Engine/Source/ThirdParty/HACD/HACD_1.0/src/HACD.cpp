// Modifications copyright (c) 2014-2015 Epic Games, Inc. All rights reserved.

#include "HACD.h"
#include <stdlib.h>
#include <string.h>
#include "PlatformConfigHACD.h"

#include "dgMeshEffect.h"
#include "dgConvexHull3d.h"
#include "MergeHulls.h"
#include "ConvexDecomposition.h"
#include "WuQuantizer.h"

#ifdef _DEBUG
#define DEBUG_WAVEFRONT 0 // if true, will save the input convex decomposition to disk as a wavefront OBJ file for debugging purposes.
#endif

/*!
**
** Copyright (c) 20011 by John W. Ratcliff mailto:jratcliffscarab@gmail.com
**
**
** The MIT license:
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is furnished
** to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.

** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
** WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma warning(disable:4100 4996)

using namespace hacd;

namespace HACD_STANDALONE
{

class MyHACD_API : public HACD_API, public UANS::UserAllocated
{
public:
	class Vec3
	{
	public:
		Vec3(void)
		{

		}
		Vec3(HaF32 _x,HaF32 _y,HaF32 _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}
		HaF32 x;
		HaF32 y;
		HaF32 z;
	};

	MyHACD_API(void)
	{
		
	}
	virtual ~MyHACD_API(void)
	{
		releaseHACD();
	}

	void normalizeInputMesh(Desc &desc,Vec3 &inputScale,Vec3 &inputCenter)
	{
		const HaF32 *source = desc.mVertices;

		Vec3 bmin(0,0,0),bmax(0,0,0);

		for (HaU32 i=0; i<desc.mVertexCount; i++)
		{
			const Vec3 &v = *(const Vec3 *)source;
			if ( i == 0 )
			{
				bmin = v;
				bmax = v;
			}
			else
			{
				if ( v.x < bmin.x ) bmin.x = v.x;
				if ( v.y < bmin.y ) bmin.y = v.y;
				if ( v.z < bmin.z ) bmin.z = v.z;

				if ( v.x > bmax.x ) bmax.x = v.x;
				if ( v.y > bmax.y ) bmax.y = v.y;
				if ( v.z > bmax.z ) bmax.z = v.z;

			}
			source+=3;
		}
		inputCenter.x = (bmin.x+bmax.x)*0.5f;
		inputCenter.y = (bmin.y+bmax.y)*0.5f;
		inputCenter.z = (bmin.z+bmax.z)*0.5f;

		HaF32 dx = bmax.x - bmin.x;
		HaF32 dy = bmax.y - bmin.y;
		HaF32 dz = bmax.z - bmin.z;

		if ( dx > 0 )
		{
			inputScale.x = 1.0f / dx;
		}
		else
		{
			inputScale.x = 1;
		}

		if ( dy > 0 )
		{
			inputScale.y = 1.0f / dy;
		}
		else
		{
			inputScale.y = 1;
		}

		if ( dz > 0 )
		{
			inputScale.z = 1.0f / dz;
		}
		else
		{
			inputScale.z = 1;
		}

		source = desc.mVertices;
		desc.mVertices = (const HaF32 *)HACD_ALLOC( sizeof(HaF32)*3*desc.mVertexCount );
		HaF32 *dest = (HaF32 *)desc.mVertices;
		for (HaU32 i=0; i<desc.mVertexCount; i++)
		{
			dest[0] = (source[0]-inputCenter.x)*inputScale.x;
			dest[1] = (source[1]-inputCenter.y)*inputScale.y;
			dest[2] = (source[2]-inputCenter.z)*inputScale.z;
			dest+=3;
			source+=3;
		}
		inputScale.x = dx;
		inputScale.y = dy;
		inputScale.z = dz;
	}

	void releaseNormalizedInputMesh(Desc &desc)
	{
		HACD_FREE( (void *)desc.mVertices );
	}

	virtual hacd::HaU32	performHACD(const Desc &_desc) 
	{
		hacd::HaU32 ret = 0;

		if ( _desc.mCallback )
		{
			_desc.mCallback->ReportProgress("Starting HACD",1);
		}

		TIMEIT("PerformHACD");

#if DEBUG_WAVEFRONT
		{
			static hacd::HaU32 saveCount=0;
			saveCount++;
			char scratch[512];
#if !PLATFORM_MAC && !PLATFORM_LINUX // @todo Mac
			sprintf_s(scratch,512,"HACD_DEBUG_%d.obj", saveCount );
#else
			snprintf(scratch,512,"HACD_DEBUG_%d.obj", saveCount );
#endif
			FILE *fph = fopen(scratch,"wb");
			if ( fph )
			{
				fprintf(fph,"# NormalizeInputMesh:         %s\r\n", _desc.mNormalizeInputMesh ? "true" : "false");
				fprintf(fph,"# UseFastVersion:             %s\r\n", _desc.mUseFastVersion ? "true" : "false" );
				fprintf(fph,"# TriangleCount:              %d\r\n", _desc.mTriangleCount);
				fprintf(fph,"# VertexCount:                %d\r\n", _desc.mVertexCount);
				fprintf(fph,"# MaxHullCount:               %d\r\n", _desc.mMaxHullCount);
				fprintf(fph,"# MaxMergeHullCount:          %d\r\n", _desc.mMaxMergeHullCount);
				fprintf(fph,"# MaxHullVertices:            %d\r\n", _desc.mMaxHullVertices);
				fprintf(fph,"# Concavity:                  %0.4f\r\n", _desc.mConcavity);
				fprintf(fph,"# SmallClusterThreshold:      %0.4f\r\n", _desc.mSmallClusterThreshold);
				fprintf(fph,"# BackFaceDistanceFactor:     %0.4f\r\n", _desc.mBackFaceDistanceFactor);
				fprintf(fph,"# DecompositionDepth:         %d\r\n", _desc.mDecompositionDepth);
				fprintf(fph,"# JobSwarmContext:            %s\r\n", _desc.mJobSwarmContext ? "true" : "false");
				fprintf(fph,"# Callback:                   %s\r\n", _desc.mCallback ? "true" : "false");

				for (hacd::HaU32 i=0; i<_desc.mVertexCount; i++)
				{
					const hacd::HaF32 *p = &_desc.mVertices[i*3];
					fprintf(fph,"v %0.9f %0.9f %0.9f\r\n", p[0], p[1], p[2] );
				}
				for (hacd::HaU32 i=0; i<_desc.mTriangleCount; i++)
				{
					hacd::HaU32 i1 = _desc.mIndices[i*3+0];
					hacd::HaU32 i2 = _desc.mIndices[i*3+1];
					hacd::HaU32 i3 = _desc.mIndices[i*3+2];
					fprintf(fph,"f %d %d %d\r\n", i1+1, i2+1, i3+1 );
				}
				fclose(fph);
			}
		}
#endif

		releaseHACD();
		Desc desc = _desc;

		hacd::HaF32 *tempPositions = NULL; // temp memory holding remapped vertex positions
		hacd::HaU32 *tempIndices = NULL; // temp memory holding remapped triangle indices
		// This method scans the input mesh for duplicate vertices.
		if ( desc.mRemoveDuplicateVertices )
		{
			if ( desc.mCallback )
			{
				desc.mCallback->ReportProgress("Removing duplicate vertices",1);
			}

			tempPositions = (hacd::HaF32 *)HACD_ALLOC(sizeof(hacd::HaF32)*desc.mVertexCount*3); // room to hold all of the input vertex positions
			tempIndices = (hacd::HaU32 *)HACD_ALLOC(sizeof(hacd::HaU32)*desc.mTriangleCount*3); // room to hold all of the triangle indices

			desc.mVertices	= tempPositions;	// the remapped vertex position data
			desc.mIndices	= tempIndices;	// the remapped triangle indices

			hacd::HaU32 removeCount = 0;

			desc.mVertexCount = 0;
			hacd::HaU32 *remapPositions = (hacd::HaU32 *)HACD_ALLOC(sizeof(hacd::HaU32)*_desc.mVertexCount);

			// Scan each input position and see if it duplicates an already defined vertex position
			for (hacd::HaU32 i=0; i<_desc.mVertexCount; i++)
			{

				const float *p1 = &_desc.mVertices[i*3]; // see if this position is already represented in out vertex list.
				// Iterate through all positions we have already defined

				bool found = false;
				for (hacd::HaU32 j=0; j<desc.mVertexCount; j++)
				{
					const float *p2 = &desc.mVertices[j*3];		// an existing psotion

					float dx = p1[0] - p2[0];
					float dy = p1[1] - p2[1];
					float dz = p1[2] - p2[2];

					float dist = dx*dx+dy*dy+dz*dz;	// Compute teh squared distance between this position and a previously defined position

					if ( dist < (0.001f*0.001f)) // if the position is essentially identical; less than 1mm different location then we do not add it.
					{
						found = true;
						remapPositions[i] = j;	// remap the original source position I to the new index position J
						removeCount++;	// increment the counter indicating the number of duplicates we have fou8nd
					}
				}
				if ( !found ) // if no duplicate was found; then this is a unique input position and we add it to the output.
				{
					remapPositions[i] = desc.mVertexCount;		// This input position 'I' remaps to the current output position location desc.mVertexCount
					float *p2 = &tempPositions[desc.mVertexCount*3]; // This is the destination for the unique input position.
					p2[0] = p1[0];	// copy X
					p2[1] = p1[1];	// copy Y
					p2[2] = p1[2];	// copy Z
					desc.mVertexCount++;	// increment the number of vertices in the new output
				}
			}
			// now we need to build the remapped index table.
			for (hacd::HaU32 i=0; i<desc.mTriangleCount*3; i++)
			{
				tempIndices[i] = remapPositions[ _desc.mIndices[i] ];
			}
			HACD_FREE(remapPositions);
			if ( desc.mCallback )
			{
				char scratch[512];
				HACD_SPRINTF_S(scratch,512,"Removed %d duplicate vertices.", removeCount );
				desc.mCallback->ReportProgress(scratch,1);
			}
		}
		if ( desc.mVertexCount )
		{

			if ( desc.mDecompositionDepth ) // if using legacy ACD
			{
				TIMEIT("Perform ACD");

				CONVEX_DECOMPOSITION_STANDALONE::ConvexDecomposition *cd = CONVEX_DECOMPOSITION_STANDALONE::createConvexDecomposition();

				CONVEX_DECOMPOSITION_STANDALONE::DecompDesc dcompDesc;

				dcompDesc.mIndices		= desc.mIndices;
				dcompDesc.mVertices		= desc.mVertices;
				dcompDesc.mTcount		= desc.mTriangleCount;
				dcompDesc.mVcount		= desc.mVertexCount;
				dcompDesc.mMaxVertices	= desc.mMaxHullVertices;
				dcompDesc.mDepth		= desc.mDecompositionDepth;
				dcompDesc.mCpercent		= desc.mConcavity*10;
				dcompDesc.mMeshVolumePercent = desc.mSmallClusterThreshold;
				dcompDesc.mCallback		= desc.mCallback;

				if ( desc.mMaxMergeHullCount == 1 ) // if we only want a single hull output then set the decomposition depth to zero!
				{
					dcompDesc.mDepth = 0;
				}

				ret = cd->performConvexDecomposition(dcompDesc);

				for (hacd::HaU32 i=0; i<ret; i++)
				{
					CONVEX_DECOMPOSITION_STANDALONE::ConvexResult *result =cd->getConvexResult(i,true);
					Hull h;
					h.mVertices = result->mHullVertices;
					h.mIndices  = result->mHullIndices;
					h.mTriangleCount = result->mHullTcount;
					h.mVertexCount = result->mHullVcount;
					mHulls.push_back(h);
				}
			}
			else
			{
				Vec3 inputScale(1,1,1);
				Vec3 inputCenter(0,0,0);

				if ( desc.mNormalizeInputMesh )
				{
					if ( desc.mCallback )
					{
						desc.mCallback->ReportProgress("Normalizing Input Mesh",1);
					}
					normalizeInputMesh(desc,inputScale,inputCenter);
				}

				{
					dgMeshEffect mesh(true);

					float normal[3] = { 0,1,0 };
					float uv[2] = { 0,0 };

					hacd::HaI32 *faceIndexCount = (hacd::HaI32 *)HACD_ALLOC(sizeof(hacd::HaI32)*desc.mTriangleCount);
					hacd::HaI32 *dummyIndex = (hacd::HaI32 *)HACD_ALLOC(sizeof(hacd::HaI32)*desc.mTriangleCount*3);

					for (hacd::HaU32 i=0; i<desc.mTriangleCount; i++)
					{
						faceIndexCount[i] = 3;
						dummyIndex[i*3+0] = 0;
						dummyIndex[i*3+1] = 0;
						dummyIndex[i*3+2] = 0;
					}

					if ( desc.mCallback )
					{
						desc.mCallback->ReportProgress("Building Mesh from Vertex Index List",1);
					}
					mesh.BuildFromVertexListIndexList(desc.mTriangleCount,faceIndexCount,dummyIndex,
						desc.mVertices,sizeof(hacd::HaF32)*3,(const hacd::HaI32 *const)desc.mIndices,
						normal,sizeof(hacd::HaF32)*3,dummyIndex,
						uv,sizeof(hacd::HaF32)*2,dummyIndex,
						uv,sizeof(hacd::HaF32)*2,dummyIndex);

					dgMeshEffect *result;
					{
						TIMEIT("ConvexApproximation");
						if ( desc.mCallback )
						{
							desc.mCallback->ReportProgress("Begin HACD",1);
						}
						if ( desc.mUseFastVersion )
						{
							result = mesh.CreateConvexApproximationFast(desc.mConcavity,desc.mMaxHullCount,desc.mCallback);
						}
						else
						{
							result = mesh.CreateConvexApproximation(desc.mConcavity,desc.mBackFaceDistanceFactor,desc.mMaxHullCount,desc.mMaxHullVertices,desc.mJobSwarmContext, desc.mCallback);
						}
					}

					if ( result )
					{
						// now we build hulls for each connected surface...
						if ( desc.mCallback )
						{
							desc.mCallback->ReportProgress("Getting connected surfaces",1);
						}
						dgPolyhedra segment;
						result->BeginConectedSurface();

						if ( result->GetConectedSurface(segment))
						{
							dgMeshEffect *solid = HACD_NEW(dgMeshEffect)(segment,*result);
							while ( solid )
							{
								dgConvexHull3d *hull = solid->CreateConvexHull(0.00001,desc.mMaxHullVertices);
								if ( hull )
								{
									Hull h;
									h.mVertexCount = hull->GetVertexCount();
									h.mVertices = (hacd::HaF32 *)HACD_ALLOC( sizeof(hacd::HaF32)*3*h.mVertexCount);
									for (hacd::HaU32 i=0; i<h.mVertexCount; i++)
									{
										hacd::HaF32 *dest = (hacd::HaF32 *)&h.mVertices[i*3];
										const dgBigVector &source = hull->GetVertex(i);
										dest[0] = (hacd::HaF32)source.m_x*inputScale.x+inputCenter.x;
										dest[1] = (hacd::HaF32)source.m_y*inputScale.y+inputCenter.y;
										dest[2] = (hacd::HaF32)source.m_z*inputScale.z+inputCenter.z;
									}

									h.mTriangleCount = hull->GetCount();
									hacd::HaU32 *destIndices = (hacd::HaU32 *)HACD_ALLOC(sizeof(hacd::HaU32)*3*h.mTriangleCount);
									h.mIndices = destIndices;
				
									dgList<dgConvexHull3DFace>::Iterator iter(*hull);
									for (iter.Begin(); iter; iter++)
									{
										dgConvexHull3DFace &face = (*iter);
										destIndices[0] = face.m_index[0];
										destIndices[1] = face.m_index[1];
										destIndices[2] = face.m_index[2];
										destIndices+=3;
									}

									mHulls.push_back(h);

									// save it!
									delete hull;
								}

								delete solid;
								solid = NULL;
								dgPolyhedra nextSegment;
								hacd::HaI32 moreSegments = result->GetConectedSurface(nextSegment);
								if ( moreSegments )
								{
									solid = HACD_NEW(dgMeshEffect)(nextSegment,*result);
								}
								else
								{
									result->EndConectedSurface();
								}
							}
						}

						delete result;
					}
				}
				ret= (HaU32)mHulls.size();
			}

			if ( desc.mNormalizeInputMesh && desc.mDecompositionDepth == 0 )
			{
				releaseNormalizedInputMesh(desc);
			}
		}

		if ( ret && ((ret > desc.mMaxMergeHullCount) || 
			(desc.mSmallClusterThreshold != 0.0f)) )
		{
			MergeHullsInterface *mhi = createMergeHullsInterface();
			if ( mhi )
			{
				if ( desc.mCallback )
				{
					desc.mCallback->ReportProgress("Gathering Input Hulls",1);
				}
				MergeHullVector inputHulls;
				MergeHullVector outputHulls;
				for (hacd::HaU32 i=0; i<ret; i++)
				{
					Hull &h = mHulls[i];
					MergeHull mh;
					mh.mTriangleCount = h.mTriangleCount;
					mh.mVertexCount = h.mVertexCount;
					mh.mVertices = h.mVertices;
					mh.mIndices = h.mIndices;
					inputHulls.push_back(mh);
				}

				{
					TIMEIT("MergeHulls");
					ret = mhi->mergeHulls(inputHulls,outputHulls,desc.mMaxMergeHullCount, desc.mSmallClusterThreshold + FLT_EPSILON, desc.mMaxHullVertices, desc.mCallback, desc.mJobSwarmContext);
				}

				for (HaU32 i=0; i<ret; i++)
				{
					Hull &h = mHulls[i];
					releaseHull(h);
				}
				mHulls.clear();

				if ( desc.mCallback )
				{
					desc.mCallback->ReportProgress("Gathering Merged Hulls",1);
				}
				for (hacd::HaU32 i=0; i<outputHulls.size(); i++)
				{
					Hull h;
					const MergeHull &mh = outputHulls[i];
					h.mTriangleCount =  mh.mTriangleCount;
					h.mVertexCount = mh.mVertexCount;
					h.mIndices = (HaU32 *)HACD_ALLOC(sizeof(HaU32)*3*h.mTriangleCount);
					h.mVertices = (HaF32 *)HACD_ALLOC(sizeof(HaF32)*3*h.mVertexCount);
					memcpy((HaU32 *)h.mIndices,mh.mIndices,sizeof(HaU32)*3*h.mTriangleCount);
					memcpy((HaF32 *)h.mVertices,mh.mVertices,sizeof(HaF32)*3*h.mVertexCount);
					mHulls.push_back(h);
				}

				ret = (HaU32)mHulls.size();

				mhi->release();
			}
			HACD_FREE(tempIndices);
			HACD_FREE(tempPositions);
		}

		return ret;
	}

	void releaseHull(Hull &h)
	{
		HACD_FREE((void *)h.mIndices);
		HACD_FREE((void *)h.mVertices);
		h.mIndices = NULL;
		h.mVertices = NULL;
	}

	virtual const Hull		*getHull(hacd::HaU32 index)  const
	{
		const Hull *ret = NULL;
		if ( index < mHulls.size() )
		{
			ret = &mHulls[index];
		}
		return ret;
	}

	virtual void	releaseHACD(void) // release memory associated with the last HACD request
	{
		for (hacd::HaU32 i=0; i<mHulls.size(); i++)
		{
			releaseHull(mHulls[i]);
		}
		mHulls.clear();
	}


	virtual void release(void) // release the HACD_API interface
	{
		delete this;
	}

	virtual hacd::HaU32	getHullCount(void)
	{
		return (hacd::HaU32) mHulls.size(); 
	}

private:
	hacd::vector< Hull >	mHulls;
};

HACD_API * createHACD_API(void)
{
	MyHACD_API *m = HACD_NEW(MyHACD_API);
	return static_cast<HACD_API *>(m);
}


};



