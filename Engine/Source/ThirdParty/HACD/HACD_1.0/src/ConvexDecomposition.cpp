#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include "ConvexDecomposition.h"
#include "ConvexHull.h"
#include "dgConvexHull3d.h"

#pragma warning(disable:4702)
#pragma warning(disable:4996 4100)
#pragma warning(disable:4267)

static const hacd::HaF32 EPSILON=0.0001f;

const hacd::HaF32 FM_PI = 3.1415926535897932384626433832795028841971693993751f;
const hacd::HaF32 FM_DEG_TO_RAD = ((2.0f * FM_PI) / 360.0f);
const hacd::HaF32 FM_RAD_TO_DEG = (360.0f / (2.0f * FM_PI));



typedef hacd::vector< hacd::HaU32 > UintVector;

namespace CONVEX_DECOMPOSITION_STANDALONE
{
class ConvexDecompInterface
{
public:
		virtual void ConvexDecompResult(const ConvexResult &result) = 0;
};



class Cdesc
{
public:
  ConvexDecompInterface *mCallback;
  hacd::HaF32			mMeshVolumePercent;
  hacd::HaF32			mMasterVolume;
  hacd::HaU32			mMaxDepth;
  hacd::HaF32			mConcavePercent;
  hacd::HaU32			mOutputCount;
  hacd::HaU32			mOutputPow2;
  hacd::ICallback		*mICallback;
};

static hacd::HaF32 fm_computePlane(const hacd::HaF32 *A,const hacd::HaF32 *B,const hacd::HaF32 *C,hacd::HaF32 *n) // returns D
{
	hacd::HaF32 vx = (B[0] - C[0]);
	hacd::HaF32 vy = (B[1] - C[1]);
	hacd::HaF32 vz = (B[2] - C[2]);

	hacd::HaF32 wx = (A[0] - B[0]);
	hacd::HaF32 wy = (A[1] - B[1]);
	hacd::HaF32 wz = (A[2] - B[2]);

	hacd::HaF32 vw_x = vy * wz - vz * wy;
	hacd::HaF32 vw_y = vz * wx - vx * wz;
	hacd::HaF32 vw_z = vx * wy - vy * wx;

	hacd::HaF32 mag = ::sqrt((vw_x * vw_x) + (vw_y * vw_y) + (vw_z * vw_z));

	if ( mag < 0.000001f )
	{
		mag = 0;
	}
	else
	{
		mag = 1.0f/mag;
	}

	hacd::HaF32 x = vw_x * mag;
	hacd::HaF32 y = vw_y * mag;
	hacd::HaF32 z = vw_z * mag;


	hacd::HaF32 D = 0.0f - ((x*A[0])+(y*A[1])+(z*A[2]));

	n[0] = x;
	n[1] = y;
	n[2] = z;

	return D;
}

static hacd::HaF32 fm_dot(const hacd::HaF32 *p1,const hacd::HaF32 *p2)
{
	return p1[0]*p2[0]+p1[1]*p2[1]+p1[2]*p2[2];
}



static bool fm_samePlane(const hacd::HaF32 p1[4],const hacd::HaF32 p2[4],hacd::HaF32 normalEpsilon,hacd::HaF32 dEpsilon,bool doubleSided)
{
	bool ret = false;

	hacd::HaF32 diff = (hacd::HaF32) fabs(p1[3]-p2[3]);
	if ( diff < dEpsilon ) // if the plane -d  co-efficient is within our epsilon
	{
		hacd::HaF32 dot = fm_dot(p1,p2); // compute the dot-product of the vector normals.
		if ( doubleSided ) dot = (hacd::HaF32)fabs(dot);
		hacd::HaF32 dmin = 1 - normalEpsilon;
		hacd::HaF32 dmax = 1 + normalEpsilon;
		if ( dot >= dmin && dot <= dmax )
		{
			ret = true; // then the plane equation is for practical purposes identical.
		}
	}

	return ret;
}

static bool    fm_isMeshCoplanar(hacd::HaU32 tcount,const hacd::HaU32 *indices,const hacd::HaF32 *vertices,bool doubleSided) // returns true if this collection of indexed triangles are co-planar!
{
	bool ret = true;

	if ( tcount > 0 )
	{
		hacd::HaU32 i1 = indices[0];
		hacd::HaU32 i2 = indices[1];
		hacd::HaU32 i3 = indices[2];
		const hacd::HaF32 *p1 = &vertices[i1*3];
		const hacd::HaF32 *p2 = &vertices[i2*3];
		const hacd::HaF32 *p3 = &vertices[i3*3];
		hacd::HaF32 plane[4];
		plane[3] = fm_computePlane(p1,p2,p3,plane);
		const hacd::HaU32 *scan = &indices[3];
		for (hacd::HaU32 i=1; i<tcount; i++)
		{
			i1 = *scan++;
			i2 = *scan++;
			i3 = *scan++;
			p1 = &vertices[i1*3];
			p2 = &vertices[i2*3];
			p3 = &vertices[i3*3];
			hacd::HaF32 _plane[4];
			_plane[3] = fm_computePlane(p1,p2,p3,_plane);
			if ( !fm_samePlane(plane,_plane,0.01f,0.001f,doubleSided) )
			{
				ret = false;
				break;
			}
		}
	}
	return ret;
}


static hacd::HaF32 fm_distToPlane(const hacd::HaF32 *plane,const hacd::HaF32 *p) // computes the distance of this point from the plane.
{
	return p[0]*plane[0]+p[1]*plane[1]+p[2]*plane[2]+plane[3];
}

static void fm_cross(hacd::HaF32 *cross,const hacd::HaF32 *a,const hacd::HaF32 *b)
{
	cross[0] = a[1]*b[2] - a[2]*b[1];
	cross[1] = a[2]*b[0] - a[0]*b[2];
	cross[2] = a[0]*b[1] - a[1]*b[0];
}

/* a = b - c */
#define vector(a,b,c) \
	(a)[0] = (b)[0] - (c)[0];	\
	(a)[1] = (b)[1] - (c)[1];	\
	(a)[2] = (b)[2] - (c)[2];



#define innerProduct(v,q) \
	((v)[0] * (q)[0] + \
	(v)[1] * (q)[1] + \
	(v)[2] * (q)[2])

#define crossProduct(a,b,c) \
	(a)[0] = (b)[1] * (c)[2] - (c)[1] * (b)[2]; \
	(a)[1] = (b)[2] * (c)[0] - (c)[2] * (b)[0]; \
	(a)[2] = (b)[0] * (c)[1] - (c)[0] * (b)[1];


inline hacd::HaF32 det(const hacd::HaF32 *p1,const hacd::HaF32 *p2,const hacd::HaF32 *p3)
{
	return  p1[0]*p2[1]*p3[2] + p2[0]*p3[1]*p1[2] + p3[0]*p1[1]*p2[2] -p1[0]*p3[1]*p2[2] - p2[0]*p1[1]*p3[2] - p3[0]*p2[1]*p1[2];
}


static hacd::HaF32  fm_computeMeshVolume(const hacd::HaF32 *vertices,hacd::HaU32 tcount,const hacd::HaU32 *indices)
{
	hacd::HaF32 volume = 0;

	for (hacd::HaU32 i=0; i<tcount; i++,indices+=3)
	{
		const hacd::HaF32 *p1 = &vertices[ indices[0]*3 ];
		const hacd::HaF32 *p2 = &vertices[ indices[1]*3 ];
		const hacd::HaF32 *p3 = &vertices[ indices[2]*3 ];
		volume+=det(p1,p2,p3); // compute the volume of the tetrahedran relative to the origin.
	}

	volume*=(1.0f/6.0f);
	if ( volume < 0 )
		volume*=-1;
	return volume;
}



// assumes that the points are on opposite sides of the plane!
static void fm_intersectPointPlane(const hacd::HaF32 *p1,const hacd::HaF32 *p2,hacd::HaF32 *split,const hacd::HaF32 *plane)
{

	hacd::HaF32 dp1 = fm_distToPlane(plane,p1);

	hacd::HaF32 dir[3];

	dir[0] = p2[0] - p1[0];
	dir[1] = p2[1] - p1[1];
	dir[2] = p2[2] - p1[2];

	hacd::HaF32 dot1 = dir[0]*plane[0] + dir[1]*plane[1] + dir[2]*plane[2];
	hacd::HaF32 dot2 = dp1 - plane[3];

	hacd::HaF32    t = -(plane[3] + dot2 ) / dot1;

	split[0] = (dir[0]*t)+p1[0];
	split[1] = (dir[1]*t)+p1[1];
	split[2] = (dir[2]*t)+p1[2];

}


static void  fm_transform(const hacd::HaF32 matrix[16],const hacd::HaF32 v[3],hacd::HaF32 t[3]) // rotate and translate this point
{
	if ( matrix )
	{
		hacd::HaF32 tx = (matrix[0*4+0] * v[0]) +  (matrix[1*4+0] * v[1]) + (matrix[2*4+0] * v[2]) + matrix[3*4+0];
		hacd::HaF32 ty = (matrix[0*4+1] * v[0]) +  (matrix[1*4+1] * v[1]) + (matrix[2*4+1] * v[2]) + matrix[3*4+1];
		hacd::HaF32 tz = (matrix[0*4+2] * v[0]) +  (matrix[1*4+2] * v[1]) + (matrix[2*4+2] * v[2]) + matrix[3*4+2];
		t[0] = tx;
		t[1] = ty;
		t[2] = tz;
	}
	else
	{
		t[0] = v[0];
		t[1] = v[1];
		t[2] = v[2];
	}
}

static void  fm_rotate(const hacd::HaF32 matrix[16],const hacd::HaF32 v[3],hacd::HaF32 t[3]) // rotate and translate this point
{
	if ( matrix )
	{
		hacd::HaF32 tx = (matrix[0*4+0] * v[0]) +  (matrix[1*4+0] * v[1]) + (matrix[2*4+0] * v[2]);
		hacd::HaF32 ty = (matrix[0*4+1] * v[0]) +  (matrix[1*4+1] * v[1]) + (matrix[2*4+1] * v[2]);
		hacd::HaF32 tz = (matrix[0*4+2] * v[0]) +  (matrix[1*4+2] * v[1]) + (matrix[2*4+2] * v[2]);
		t[0] = tx;
		t[1] = ty;
		t[2] = tz;
	}
	else
	{
		t[0] = v[0];
		t[1] = v[1];
		t[2] = v[2];
	}
}




static void fm_inverseRT(const hacd::HaF32 matrix[16],const hacd::HaF32 pos[3],hacd::HaF32 t[3]) // inverse rotate translate the point.
{

	hacd::HaF32 _x = pos[0] - matrix[3*4+0];
	hacd::HaF32 _y = pos[1] - matrix[3*4+1];
	hacd::HaF32 _z = pos[2] - matrix[3*4+2];

	// Multiply inverse-translated source vector by inverted rotation transform

	t[0] = (matrix[0*4+0] * _x) + (matrix[0*4+1] * _y) + (matrix[0*4+2] * _z);
	t[1] = (matrix[1*4+0] * _x) + (matrix[1*4+1] * _y) + (matrix[1*4+2] * _z);
	t[2] = (matrix[2*4+0] * _x) + (matrix[2*4+1] * _y) + (matrix[2*4+2] * _z);

}

template <class Type> class Eigen
{
public:


	void DecrSortEigenStuff(void)
	{
		Tridiagonal(); //diagonalize the matrix.
		QLAlgorithm(); //
		DecreasingSort();
		GuaranteeRotation();
	}

	void Tridiagonal(void)
	{
		Type fM00 = mElement[0][0];
		Type fM01 = mElement[0][1];
		Type fM02 = mElement[0][2];
		Type fM11 = mElement[1][1];
		Type fM12 = mElement[1][2];
		Type fM22 = mElement[2][2];

		m_afDiag[0] = fM00;
		m_afSubd[2] = 0;
		if (fM02 != (Type)0.0)
		{
			Type fLength = ::sqrt(fM01*fM01+fM02*fM02);
			Type fInvLength = ((Type)1.0)/fLength;
			fM01 *= fInvLength;
			fM02 *= fInvLength;
			Type fQ = ((Type)2.0)*fM01*fM12+fM02*(fM22-fM11);
			m_afDiag[1] = fM11+fM02*fQ;
			m_afDiag[2] = fM22-fM02*fQ;
			m_afSubd[0] = fLength;
			m_afSubd[1] = fM12-fM01*fQ;
			mElement[0][0] = (Type)1.0;
			mElement[0][1] = (Type)0.0;
			mElement[0][2] = (Type)0.0;
			mElement[1][0] = (Type)0.0;
			mElement[1][1] = fM01;
			mElement[1][2] = fM02;
			mElement[2][0] = (Type)0.0;
			mElement[2][1] = fM02;
			mElement[2][2] = -fM01;
			m_bIsRotation = false;
		}
		else
		{
			m_afDiag[1] = fM11;
			m_afDiag[2] = fM22;
			m_afSubd[0] = fM01;
			m_afSubd[1] = fM12;
			mElement[0][0] = (Type)1.0;
			mElement[0][1] = (Type)0.0;
			mElement[0][2] = (Type)0.0;
			mElement[1][0] = (Type)0.0;
			mElement[1][1] = (Type)1.0;
			mElement[1][2] = (Type)0.0;
			mElement[2][0] = (Type)0.0;
			mElement[2][1] = (Type)0.0;
			mElement[2][2] = (Type)1.0;
			m_bIsRotation = true;
		}
	}

	bool QLAlgorithm(void)
	{
		const hacd::HaI32 iMaxIter = 32;

		for (hacd::HaI32 i0 = 0; i0 <3; i0++)
		{
			hacd::HaI32 i1;
			for (i1 = 0; i1 < iMaxIter; i1++)
			{
				hacd::HaI32 i2;
				for (i2 = i0; i2 <= (3-2); i2++)
				{
					Type fTmp = fabs(m_afDiag[i2]) + fabs(m_afDiag[i2+1]);
					if ( fabs(m_afSubd[i2]) + fTmp == fTmp )
						break;
				}
				if (i2 == i0)
				{
					break;
				}

				Type fG = (m_afDiag[i0+1] - m_afDiag[i0])/(((Type)2.0) * m_afSubd[i0]);
				Type fR = ::sqrt(fG*fG+(Type)1.0);
				if (fG < (Type)0.0)
				{
					fG = m_afDiag[i2]-m_afDiag[i0]+m_afSubd[i0]/(fG-fR);
				}
				else
				{
					fG = m_afDiag[i2]-m_afDiag[i0]+m_afSubd[i0]/(fG+fR);
				}
				Type fSin = (Type)1.0, fCos = (Type)1.0, fP = (Type)0.0;
				for (hacd::HaI32 i3 = i2-1; i3 >= i0; i3--)
				{
					Type fF = fSin*m_afSubd[i3];
					Type fB = fCos*m_afSubd[i3];
					if (fabs(fF) >= fabs(fG))
					{
						fCos = fG/fF;
						fR = ::sqrt(fCos*fCos+(Type)1.0);
						m_afSubd[i3+1] = fF*fR;
						fSin = ((Type)1.0)/fR;
						fCos *= fSin;
					}
					else
					{
						fSin = fF/fG;
						fR = ::sqrt(fSin*fSin+(Type)1.0);
						m_afSubd[i3+1] = fG*fR;
						fCos = ((Type)1.0)/fR;
						fSin *= fCos;
					}
					fG = m_afDiag[i3+1]-fP;
					fR = (m_afDiag[i3]-fG)*fSin+((Type)2.0)*fB*fCos;
					fP = fSin*fR;
					m_afDiag[i3+1] = fG+fP;
					fG = fCos*fR-fB;
					for (hacd::HaI32 i4 = 0; i4 < 3; i4++)
					{
						fF = mElement[i4][i3+1];
						mElement[i4][i3+1] = fSin*mElement[i4][i3]+fCos*fF;
						mElement[i4][i3] = fCos*mElement[i4][i3]-fSin*fF;
					}
				}
				m_afDiag[i0] -= fP;
				m_afSubd[i0] = fG;
				m_afSubd[i2] = (Type)0.0;
			}
			if (i1 == iMaxIter)
			{
				return false;
			}
		}
		return true;
	}

	void DecreasingSort(void)
	{
		//sort eigenvalues in decreasing order, e[0] >= ... >= e[iSize-1]
		for (hacd::HaI32 i0 = 0, i1; i0 <= 3-2; i0++)
		{
			// locate maximum eigenvalue
			i1 = i0;
			Type fMax = m_afDiag[i1];
			hacd::HaI32 i2;
			for (i2 = i0+1; i2 < 3; i2++)
			{
				if (m_afDiag[i2] > fMax)
				{
					i1 = i2;
					fMax = m_afDiag[i1];
				}
			}

			if (i1 != i0)
			{
				// swap eigenvalues
				m_afDiag[i1] = m_afDiag[i0];
				m_afDiag[i0] = fMax;
				// swap eigenvectors
				for (i2 = 0; i2 < 3; i2++)
				{
					Type fTmp = mElement[i2][i0];
					mElement[i2][i0] = mElement[i2][i1];
					mElement[i2][i1] = fTmp;
					m_bIsRotation = !m_bIsRotation;
				}
			}
		}
	}


	void GuaranteeRotation(void)
	{
		if (!m_bIsRotation)
		{
			// change sign on the first column
			for (hacd::HaI32 iRow = 0; iRow <3; iRow++)
			{
				mElement[iRow][0] = -mElement[iRow][0];
			}
		}
	}

	Type mElement[3][3];
	Type m_afDiag[3];
	Type m_afSubd[3];
	bool m_bIsRotation;
};


static bool fm_computeBestFitPlane(hacd::HaU32 vcount,
							const hacd::HaF32 *points,
							hacd::HaU32 vstride,
							const hacd::HaF32 *weights,
							hacd::HaU32 wstride,
							hacd::HaF32 *plane)
{
	bool ret = false;

	hacd::HaF32 kOrigin[3] = { 0, 0, 0 };

	hacd::HaF32 wtotal = 0;

	{
		const char *source  = (const char *) points;
		const char *wsource = (const char *) weights;

		for (hacd::HaU32 i=0; i<vcount; i++)
		{

			const hacd::HaF32 *p = (const hacd::HaF32 *) source;

			hacd::HaF32 w = 1;

			if ( wsource )
			{
				const hacd::HaF32 *ws = (const hacd::HaF32 *) wsource;
				w = *ws; //
				wsource+=wstride;
			}

			kOrigin[0]+=p[0]*w;
			kOrigin[1]+=p[1]*w;
			kOrigin[2]+=p[2]*w;

			wtotal+=w;

			source+=vstride;
		}
	}

	hacd::HaF32 recip = 1.0f / wtotal; // reciprocol of total weighting

	kOrigin[0]*=recip;
	kOrigin[1]*=recip;
	kOrigin[2]*=recip;


	hacd::HaF32 fSumXX=0;
	hacd::HaF32 fSumXY=0;
	hacd::HaF32 fSumXZ=0;

	hacd::HaF32 fSumYY=0;
	hacd::HaF32 fSumYZ=0;
	hacd::HaF32 fSumZZ=0;


	{
		const char *source  = (const char *) points;
		const char *wsource = (const char *) weights;

		for (hacd::HaU32 i=0; i<vcount; i++)
		{

			const hacd::HaF32 *p = (const hacd::HaF32 *) source;

			hacd::HaF32 w = 1;

			if ( wsource )
			{
				const hacd::HaF32 *ws = (const hacd::HaF32 *) wsource;
				w = *ws; //
				wsource+=wstride;
			}

			hacd::HaF32 kDiff[3];

			kDiff[0] = w*(p[0] - kOrigin[0]); // apply vertex weighting!
			kDiff[1] = w*(p[1] - kOrigin[1]);
			kDiff[2] = w*(p[2] - kOrigin[2]);

			fSumXX+= kDiff[0] * kDiff[0]; // sume of the squares of the differences.
			fSumXY+= kDiff[0] * kDiff[1]; // sume of the squares of the differences.
			fSumXZ+= kDiff[0] * kDiff[2]; // sume of the squares of the differences.

			fSumYY+= kDiff[1] * kDiff[1];
			fSumYZ+= kDiff[1] * kDiff[2];
			fSumZZ+= kDiff[2] * kDiff[2];


			source+=vstride;
		}
	}

	fSumXX *= recip;
	fSumXY *= recip;
	fSumXZ *= recip;
	fSumYY *= recip;
	fSumYZ *= recip;
	fSumZZ *= recip;

	// setup the eigensolver
	Eigen<hacd::HaF32> kES;

	kES.mElement[0][0] = fSumXX;
	kES.mElement[0][1] = fSumXY;
	kES.mElement[0][2] = fSumXZ;

	kES.mElement[1][0] = fSumXY;
	kES.mElement[1][1] = fSumYY;
	kES.mElement[1][2] = fSumYZ;

	kES.mElement[2][0] = fSumXZ;
	kES.mElement[2][1] = fSumYZ;
	kES.mElement[2][2] = fSumZZ;

	// compute eigenstuff, smallest eigenvalue is in last position
	kES.DecrSortEigenStuff();

	hacd::HaF32 kNormal[3];

	kNormal[0] = kES.mElement[0][2];
	kNormal[1] = kES.mElement[1][2];
	kNormal[2] = kES.mElement[2][2];

	// the minimum energy
	plane[0] = kNormal[0];
	plane[1] = kNormal[1];
	plane[2] = kNormal[2];

	plane[3] = 0 - fm_dot(kNormal,kOrigin);

	ret = true;

	return ret;
}




// computes the OBB for this set of points relative to this transform matrix.
void computeOBB(hacd::HaU32 vcount,const hacd::HaF32 *points,hacd::HaU32 pstride,hacd::HaF32 *sides,hacd::HaF32 *matrix)
{
	const char *src = (const char *) points;

	hacd::HaF32 bmin[3] = { 1e9, 1e9, 1e9 };
	hacd::HaF32 bmax[3] = { -1e9, -1e9, -1e9 };

	for (hacd::HaU32 i=0; i<vcount; i++)
	{
		const hacd::HaF32 *p = (const hacd::HaF32 *) src;
		hacd::HaF32 t[3];

		fm_inverseRT(matrix, p, t ); // inverse rotate translate

		if ( t[0] < bmin[0] ) bmin[0] = t[0];
		if ( t[1] < bmin[1] ) bmin[1] = t[1];
		if ( t[2] < bmin[2] ) bmin[2] = t[2];

		if ( t[0] > bmax[0] ) bmax[0] = t[0];
		if ( t[1] > bmax[1] ) bmax[1] = t[1];
		if ( t[2] > bmax[2] ) bmax[2] = t[2];

		src+=pstride;
	}

	hacd::HaF32 center[3];

	sides[0] = bmax[0]-bmin[0];
	sides[1] = bmax[1]-bmin[1];
	sides[2] = bmax[2]-bmin[2];

	center[0] = sides[0]*0.5f+bmin[0];
	center[1] = sides[1]*0.5f+bmin[1];
	center[2] = sides[2]*0.5f+bmin[2];

	hacd::HaF32 ocenter[3];

	fm_rotate(matrix,center,ocenter);

	matrix[12]+=ocenter[0];
	matrix[13]+=ocenter[1];
	matrix[14]+=ocenter[2];

}

// Reference, from Stan Melax in Game Gems I
//  Quaternion q;
//  vector3 c = CrossProduct(v0,v1);
//  hacd::HaF32   d = DotProduct(v0,v1);
//  hacd::HaF32   s = (hacd::HaF32)::sqrt((1+d)*2);
//  q.x = c.x / s;
//  q.y = c.y / s;
//  q.z = c.z / s;
//  q.w = s /2.0f;
//  return q;
static void fm_rotationArc(const hacd::HaF32 *v0,const hacd::HaF32 *v1,hacd::HaF32 *quat)
{
	hacd::HaF32 cross[3];

	fm_cross(cross,v0,v1);
	hacd::HaF32 d = fm_dot(v0,v1);

	if(d<=-1.0f) // 180 about x axis
	{
		quat[0] = 1.0f;
		quat[1] = quat[2] = quat[3] =0.0f;
		return;
	}
	else
	{
		hacd::HaF32 s = ::sqrt((1+d)*2);
		hacd::HaF32 recip = 1.0f / s;

		quat[0] = cross[0] * recip;
		quat[1] = cross[1] * recip;
		quat[2] = cross[2] * recip;
		quat[3] = s * 0.5f;
	}
}

static void  fm_setTranslation(const hacd::HaF32 *translation,hacd::HaF32 *matrix)
{
	matrix[12] = translation[0];
	matrix[13] = translation[1];
	matrix[14] = translation[2];
}


static void fm_eulerToQuat(hacd::HaF32 roll,hacd::HaF32 pitch,hacd::HaF32 yaw,hacd::HaF32 *quat) // convert euler angles to quaternion.
{
	roll  *= 0.5f;
	pitch *= 0.5f;
	yaw   *= 0.5f;

	hacd::HaF32 cr = ::cos(roll);
	hacd::HaF32 cp = ::cos(pitch);
	hacd::HaF32 cy = ::cos(yaw);

	hacd::HaF32 sr = ::sin(roll);
	hacd::HaF32 sp = ::sin(pitch);
	hacd::HaF32 sy = ::sin(yaw);

	hacd::HaF32 cpcy = cp * cy;
	hacd::HaF32 spsy = sp * sy;
	hacd::HaF32 spcy = sp * cy;
	hacd::HaF32 cpsy = cp * sy;

	quat[0]   = ( sr * cpcy - cr * spsy);
	quat[1]   = ( cr * spcy + sr * cpsy);
	quat[2]   = ( cr * cpsy - sr * spcy);
	quat[3]   = cr * cpcy + sr * spsy;
}


static void fm_quatToMatrix(const hacd::HaF32 *quat,hacd::HaF32 *matrix) // convert quaterinion rotation to matrix, zeros out the translation component.
{

	hacd::HaF32 xx = quat[0]*quat[0];
	hacd::HaF32 yy = quat[1]*quat[1];
	hacd::HaF32 zz = quat[2]*quat[2];
	hacd::HaF32 xy = quat[0]*quat[1];
	hacd::HaF32 xz = quat[0]*quat[2];
	hacd::HaF32 yz = quat[1]*quat[2];
	hacd::HaF32 wx = quat[3]*quat[0];
	hacd::HaF32 wy = quat[3]*quat[1];
	hacd::HaF32 wz = quat[3]*quat[2];

	matrix[0*4+0] = 1 - 2 * ( yy + zz );
	matrix[1*4+0] =     2 * ( xy - wz );
	matrix[2*4+0] =     2 * ( xz + wy );

	matrix[0*4+1] =     2 * ( xy + wz );
	matrix[1*4+1] = 1 - 2 * ( xx + zz );
	matrix[2*4+1] =     2 * ( yz - wx );

	matrix[0*4+2] =     2 * ( xz - wy );
	matrix[1*4+2] =     2 * ( yz + wx );
	matrix[2*4+2] = 1 - 2 * ( xx + yy );

	matrix[3*4+0] = matrix[3*4+1] = matrix[3*4+2] = (hacd::HaF32) 0.0f;
	matrix[0*4+3] = matrix[1*4+3] = matrix[2*4+3] = (hacd::HaF32) 0.0f;
	matrix[3*4+3] =(hacd::HaF32) 1.0f;

}

static void  fm_matrixMultiply(const hacd::HaF32 *pA,const hacd::HaF32 *pB,hacd::HaF32 *pM)
{
	hacd::HaF32 a = pA[0*4+0] * pB[0*4+0] + pA[0*4+1] * pB[1*4+0] + pA[0*4+2] * pB[2*4+0] + pA[0*4+3] * pB[3*4+0];
	hacd::HaF32 b = pA[0*4+0] * pB[0*4+1] + pA[0*4+1] * pB[1*4+1] + pA[0*4+2] * pB[2*4+1] + pA[0*4+3] * pB[3*4+1];
	hacd::HaF32 c = pA[0*4+0] * pB[0*4+2] + pA[0*4+1] * pB[1*4+2] + pA[0*4+2] * pB[2*4+2] + pA[0*4+3] * pB[3*4+2];
	hacd::HaF32 d = pA[0*4+0] * pB[0*4+3] + pA[0*4+1] * pB[1*4+3] + pA[0*4+2] * pB[2*4+3] + pA[0*4+3] * pB[3*4+3];

	hacd::HaF32 e = pA[1*4+0] * pB[0*4+0] + pA[1*4+1] * pB[1*4+0] + pA[1*4+2] * pB[2*4+0] + pA[1*4+3] * pB[3*4+0];
	hacd::HaF32 f = pA[1*4+0] * pB[0*4+1] + pA[1*4+1] * pB[1*4+1] + pA[1*4+2] * pB[2*4+1] + pA[1*4+3] * pB[3*4+1];
	hacd::HaF32 g = pA[1*4+0] * pB[0*4+2] + pA[1*4+1] * pB[1*4+2] + pA[1*4+2] * pB[2*4+2] + pA[1*4+3] * pB[3*4+2];
	hacd::HaF32 h = pA[1*4+0] * pB[0*4+3] + pA[1*4+1] * pB[1*4+3] + pA[1*4+2] * pB[2*4+3] + pA[1*4+3] * pB[3*4+3];

	hacd::HaF32 i = pA[2*4+0] * pB[0*4+0] + pA[2*4+1] * pB[1*4+0] + pA[2*4+2] * pB[2*4+0] + pA[2*4+3] * pB[3*4+0];
	hacd::HaF32 j = pA[2*4+0] * pB[0*4+1] + pA[2*4+1] * pB[1*4+1] + pA[2*4+2] * pB[2*4+1] + pA[2*4+3] * pB[3*4+1];
	hacd::HaF32 k = pA[2*4+0] * pB[0*4+2] + pA[2*4+1] * pB[1*4+2] + pA[2*4+2] * pB[2*4+2] + pA[2*4+3] * pB[3*4+2];
	hacd::HaF32 l = pA[2*4+0] * pB[0*4+3] + pA[2*4+1] * pB[1*4+3] + pA[2*4+2] * pB[2*4+3] + pA[2*4+3] * pB[3*4+3];

	hacd::HaF32 m = pA[3*4+0] * pB[0*4+0] + pA[3*4+1] * pB[1*4+0] + pA[3*4+2] * pB[2*4+0] + pA[3*4+3] * pB[3*4+0];
	hacd::HaF32 n = pA[3*4+0] * pB[0*4+1] + pA[3*4+1] * pB[1*4+1] + pA[3*4+2] * pB[2*4+1] + pA[3*4+3] * pB[3*4+1];
	hacd::HaF32 o = pA[3*4+0] * pB[0*4+2] + pA[3*4+1] * pB[1*4+2] + pA[3*4+2] * pB[2*4+2] + pA[3*4+3] * pB[3*4+2];
	hacd::HaF32 p = pA[3*4+0] * pB[0*4+3] + pA[3*4+1] * pB[1*4+3] + pA[3*4+2] * pB[2*4+3] + pA[3*4+3] * pB[3*4+3];

	pM[0] = a;
	pM[1] = b;
	pM[2] = c;
	pM[3] = d;

	pM[4] = e;
	pM[5] = f;
	pM[6] = g;
	pM[7] = h;

	pM[8] = i;
	pM[9] = j;
	pM[10] = k;
	pM[11] = l;

	pM[12] = m;
	pM[13] = n;
	pM[14] = o;
	pM[15] = p;
}




static void fm_planeToMatrix(const hacd::HaF32 *plane,hacd::HaF32 *matrix) // convert a plane equation to a 4x4 rotation matrix
{
	hacd::HaF32 ref[3] = { 0, 1, 0 };
	hacd::HaF32 quat[4];
	fm_rotationArc(ref,plane,quat);
	fm_quatToMatrix(quat,matrix);
	hacd::HaF32 origin[3] = { 0, -plane[3], 0 };
	hacd::HaF32 center[3];
	fm_transform(matrix,origin,center);
	fm_setTranslation(center,matrix);
}


static void fm_computeBestFitOBB(hacd::HaU32 vcount,
						  const hacd::HaF32 *points,
						  hacd::HaU32 pstride,
						  hacd::HaF32 *sides,
						  hacd::HaF32 *matrix,
						  bool bruteForce)
{
	hacd::HaF32 plane[4];
	fm_computeBestFitPlane(vcount,points,pstride,0,0,plane);
	fm_planeToMatrix(plane,matrix);
	computeOBB( vcount, points, pstride, sides, matrix );

	hacd::HaF32 refmatrix[16];
	memcpy(refmatrix,matrix,16*sizeof(hacd::HaF32));

	hacd::HaF32 volume = sides[0]*sides[1]*sides[2];
	if ( bruteForce )
	{
		for (hacd::HaF32 a=10; a<180; a+=10)
		{
			hacd::HaF32 quat[4];
			fm_eulerToQuat(0,a*FM_DEG_TO_RAD,0,quat);
			hacd::HaF32 temp[16];
			hacd::HaF32 pmatrix[16];
			fm_quatToMatrix(quat,temp);
			fm_matrixMultiply(temp,refmatrix,pmatrix);
			hacd::HaF32 psides[3];
			computeOBB( vcount, points, pstride, psides, pmatrix );
			hacd::HaF32 v = psides[0]*psides[1]*psides[2];
			if ( v < volume )
			{
				volume = v;
				memcpy(matrix,pmatrix,sizeof(hacd::HaF32)*16);
				sides[0] = psides[0];
				sides[1] = psides[1];
				sides[2] = psides[2];
			}
		}
	}
}

template <class T> class Rect3d
{
public:
	Rect3d(void) { };

	Rect3d(const T *bmin,const T *bmax)
	{

		mMin[0] = bmin[0];
		mMin[1] = bmin[1];
		mMin[2] = bmin[2];

		mMax[0] = bmax[0];
		mMax[1] = bmax[1];
		mMax[2] = bmax[2];

	}

	void SetMin(const T *bmin)
	{
		mMin[0] = bmin[0];
		mMin[1] = bmin[1];
		mMin[2] = bmin[2];
	}

	void SetMax(const T *bmax)
	{
		mMax[0] = bmax[0];
		mMax[1] = bmax[1];
		mMax[2] = bmax[2];
	}

	void SetMin(T x,T y,T z)
	{
		mMin[0] = x;
		mMin[1] = y;
		mMin[2] = z;
	}

	void SetMax(T x,T y,T z)
	{
		mMax[0] = x;
		mMax[1] = y;
		mMax[2] = z;
	}

	T mMin[3];
	T mMax[3];
};

void splitRect(hacd::HaU32 axis,
			   const Rect3d<hacd::HaF32> &source,
			   Rect3d<hacd::HaF32> &b1,
			   Rect3d<hacd::HaF32> &b2,
			   const hacd::HaF32 *midpoint)
{
	switch ( axis )
	{
	case 0:
		b1.SetMin(source.mMin);
		b1.SetMax( midpoint[0], source.mMax[1], source.mMax[2] );

		b2.SetMin( midpoint[0], source.mMin[1], source.mMin[2] );
		b2.SetMax(source.mMax);

		break;
	case 1:
		b1.SetMin(source.mMin);
		b1.SetMax( source.mMax[0], midpoint[1], source.mMax[2] );

		b2.SetMin( source.mMin[0], midpoint[1], source.mMin[2] );
		b2.SetMax(source.mMax);

		break;
	case 2:
		b1.SetMin(source.mMin);
		b1.SetMax( source.mMax[0], source.mMax[1], midpoint[2] );

		b2.SetMin( source.mMin[0], source.mMin[1], midpoint[2] );
		b2.SetMax(source.mMax);

		break;
	}
}



static bool fm_computeSplitPlane(hacd::HaU32 vcount,
						  const hacd::HaF32 *vertices,
						  hacd::HaU32 /* tcount */,
						  const hacd::HaU32 * /* indices */,
						  hacd::HaF32 *plane)
{

	hacd::HaF32 sides[3];
	hacd::HaF32 matrix[16];

	fm_computeBestFitOBB( vcount, vertices, sizeof(hacd::HaF32)*3, sides, matrix, false );

	hacd::HaF32 bmax[3];
	hacd::HaF32 bmin[3];

	bmax[0] = sides[0]*0.5f;
	bmax[1] = sides[1]*0.5f;
	bmax[2] = sides[2]*0.5f;

	bmin[0] = -bmax[0];
	bmin[1] = -bmax[1];
	bmin[2] = -bmax[2];


	hacd::HaF32 dx = sides[0];
	hacd::HaF32 dy = sides[1];
	hacd::HaF32 dz = sides[2];


	hacd::HaU32 axis = 0;

	if ( dy > dx )
	{
		axis = 1;
	}

	if ( dz > dx && dz > dy )
	{
		axis = 2;
	}

	hacd::HaF32 p1[3];
	hacd::HaF32 p2[3];
	hacd::HaF32 p3[3];

	p3[0] = p2[0] = p1[0] = bmin[0] + dx*0.5f;
	p3[1] = p2[1] = p1[1] = bmin[1] + dy*0.5f;
	p3[2] = p2[2] = p1[2] = bmin[2] + dz*0.5f;

	Rect3d<hacd::HaF32> b(bmin,bmax);

	Rect3d<hacd::HaF32> b1,b2;

	splitRect(axis,b,b1,b2,p1);


	switch ( axis )
	{
	case 0:
		p2[1] = bmin[1];
		p2[2] = bmin[2];

		if ( dz > dy )
		{
			p3[1] = bmax[1];
			p3[2] = bmin[2];
		}
		else
		{
			p3[1] = bmin[1];
			p3[2] = bmax[2];
		}

		break;
	case 1:
		p2[0] = bmin[0];
		p2[2] = bmin[2];

		if ( dx > dz )
		{
			p3[0] = bmax[0];
			p3[2] = bmin[2];
		}
		else
		{
			p3[0] = bmin[0];
			p3[2] = bmax[2];
		}

		break;
	case 2:
		p2[0] = bmin[0];
		p2[1] = bmin[1];

		if ( dx > dy )
		{
			p3[0] = bmax[0];
			p3[1] = bmin[1];
		}
		else
		{
			p3[0] = bmin[0];
			p3[1] = bmax[1];
		}

		break;
	}

	hacd::HaF32 tp1[3];
	hacd::HaF32 tp2[3];
	hacd::HaF32 tp3[3];

	fm_transform(matrix,p1,tp1);
	fm_transform(matrix,p2,tp2);
	fm_transform(matrix,p3,tp3);

	plane[3] = fm_computePlane(tp1,tp2,tp3,plane);

	return true;

}



#define FM_DEFAULT_GRANULARITY 0.001f  // 1 millimeter is the default granularity

class fm_VertexIndex
{
public:
	virtual hacd::HaU32          getIndex(const hacd::HaF32 pos[3],bool &newPos) = 0;  // get welded index for this hacd::HaF32 vector[3]
	virtual hacd::HaU32          getIndex(const hacd::HaF64 pos[3],bool &newPos) = 0;  // get welded index for this hacd::HaF64 vector[3]
	virtual const hacd::HaF32 *   getVerticesFloat(void) const = 0;
	virtual const hacd::HaF64 *  getVerticesDouble(void) const = 0;
	virtual const hacd::HaF32 *   getVertexFloat(hacd::HaU32 index) const = 0;
	virtual const hacd::HaF64 *  getVertexDouble(hacd::HaU32 index) const = 0;
	virtual hacd::HaU32          getVcount(void) const = 0;
	virtual bool            isDouble(void) const = 0;
	virtual bool            saveAsObj(const char *fname,hacd::HaU32 tcount,hacd::HaU32 *indices) = 0;
};

static fm_VertexIndex * fm_createVertexIndex(hacd::HaF64 granularity,bool snapToGrid); // create an indexed vertex system for doubles
static void             fm_releaseVertexIndex(fm_VertexIndex *vindex);



class KdTreeNode;

typedef hacd::vector< KdTreeNode * > KdTreeNodeVector;

enum Axes
{
	X_AXIS = 0,
	Y_AXIS = 1,
	Z_AXIS = 2
};

class KdTreeFindNode
{
public:
	KdTreeFindNode(void)
	{
		mNode = 0;
		mDistance = 0;
	}
	KdTreeNode  *mNode;
	hacd::HaF64        mDistance;
};

class KdTreeInterface
{
public:
	virtual const hacd::HaF64 * getPositionDouble(hacd::HaU32 index) const = 0;
	virtual const hacd::HaF32  * getPositionFloat(hacd::HaU32 index) const = 0;
};

class KdTreeNode
{
public:
	KdTreeNode(void)
	{
		mIndex = 0;
		mLeft = 0;
		mRight = 0;
	}

	KdTreeNode(hacd::HaU32 index)
	{
		mIndex = index;
		mLeft = 0;
		mRight = 0;
	};

	~KdTreeNode(void)
	{
	}


	void addDouble(KdTreeNode *node,Axes dim,const KdTreeInterface *iface)
	{
		const hacd::HaF64 *nodePosition = iface->getPositionDouble( node->mIndex );
		const hacd::HaF64 *position     = iface->getPositionDouble( mIndex );
		switch ( dim )
		{
		case X_AXIS:
			if ( nodePosition[0] <= position[0] )
			{
				if ( mLeft )
					mLeft->addDouble(node,Y_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addDouble(node,Y_AXIS,iface);
				else
					mRight = node;
			}
			break;
		case Y_AXIS:
			if ( nodePosition[1] <= position[1] )
			{
				if ( mLeft )
					mLeft->addDouble(node,Z_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addDouble(node,Z_AXIS,iface);
				else
					mRight = node;
			}
			break;
		case Z_AXIS:
			if ( nodePosition[2] <= position[2] )
			{
				if ( mLeft )
					mLeft->addDouble(node,X_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addDouble(node,X_AXIS,iface);
				else
					mRight = node;
			}
			break;
		}

	}


	void addFloat(KdTreeNode *node,Axes dim,const KdTreeInterface *iface)
	{
		const hacd::HaF32 *nodePosition = iface->getPositionFloat( node->mIndex );
		const hacd::HaF32 *position     = iface->getPositionFloat( mIndex );
		switch ( dim )
		{
		case X_AXIS:
			if ( nodePosition[0] <= position[0] )
			{
				if ( mLeft )
					mLeft->addFloat(node,Y_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addFloat(node,Y_AXIS,iface);
				else
					mRight = node;
			}
			break;
		case Y_AXIS:
			if ( nodePosition[1] <= position[1] )
			{
				if ( mLeft )
					mLeft->addFloat(node,Z_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addFloat(node,Z_AXIS,iface);
				else
					mRight = node;
			}
			break;
		case Z_AXIS:
			if ( nodePosition[2] <= position[2] )
			{
				if ( mLeft )
					mLeft->addFloat(node,X_AXIS,iface);
				else
					mLeft = node;
			}
			else
			{
				if ( mRight )
					mRight->addFloat(node,X_AXIS,iface);
				else
					mRight = node;
			}
			break;
		}

	}


	hacd::HaU32 getIndex(void) const { return mIndex; };

	void search(Axes axis,const hacd::HaF64 *pos,hacd::HaF64 radius,hacd::HaU32 &count,hacd::HaU32 maxObjects,KdTreeFindNode *found,const KdTreeInterface *iface)
	{

		const hacd::HaF64 *position = iface->getPositionDouble(mIndex);

		hacd::HaF64 dx = pos[0] - position[0];
		hacd::HaF64 dy = pos[1] - position[1];
		hacd::HaF64 dz = pos[2] - position[2];

		KdTreeNode *search1 = 0;
		KdTreeNode *search2 = 0;

		switch ( axis )
		{
		case X_AXIS:
			if ( dx <= 0 )     // JWR  if we are to the left
			{
				search1 = mLeft; // JWR  then search to the left
				if ( -dx < radius )  // JWR  if distance to the right is less than our search radius, continue on the right as well.
					search2 = mRight;
			}
			else
			{
				search1 = mRight; // JWR  ok, we go down the left tree
				if ( dx < radius ) // JWR  if the distance from the right is less than our search radius
					search2 = mLeft;
			}
			axis = Y_AXIS;
			break;
		case Y_AXIS:
			if ( dy <= 0 )
			{
				search1 = mLeft;
				if ( -dy < radius )
					search2 = mRight;
			}
			else
			{
				search1 = mRight;
				if ( dy < radius )
					search2 = mLeft;
			}
			axis = Z_AXIS;
			break;
		case Z_AXIS:
			if ( dz <= 0 )
			{
				search1 = mLeft;
				if ( -dz < radius )
					search2 = mRight;
			}
			else
			{
				search1 = mRight;
				if ( dz < radius )
					search2 = mLeft;
			}
			axis = X_AXIS;
			break;
		}

		hacd::HaF64 r2 = radius*radius;
		hacd::HaF64 m  = dx*dx+dy*dy+dz*dz;

		if ( m < r2 )
		{
			switch ( count )
			{
			case 0:
				found[count].mNode = this;
				found[count].mDistance = m;
				break;
			case 1:
				if ( m < found[0].mDistance )
				{
					if ( maxObjects == 1 )
					{
						found[0].mNode = this;
						found[0].mDistance = m;
					}
					else
					{
						found[1] = found[0];
						found[0].mNode = this;
						found[0].mDistance = m;
					}
				}
				else if ( maxObjects > 1)
				{
					found[1].mNode = this;
					found[1].mDistance = m;
				}
				break;
			default:
				{
					bool inserted = false;

					for (hacd::HaU32 i=0; i<count; i++)
					{
						if ( m < found[i].mDistance ) // if this one is closer than a pre-existing one...
						{
							// insertion sort...
							hacd::HaU32 scan = count;
							if ( scan >= maxObjects ) scan=maxObjects-1;
							for (hacd::HaU32 j=scan; j>i; j--)
							{
								found[j] = found[j-1];
							}
							found[i].mNode = this;
							found[i].mDistance = m;
							inserted = true;
							break;
						}
					}

					if ( !inserted && count < maxObjects )
					{
						found[count].mNode = this;
						found[count].mDistance = m;
					}
				}
				break;
			}
			count++;
			if ( count > maxObjects )
			{
				count = maxObjects;
			}
		}


		if ( search1 )
			search1->search( axis, pos,radius, count, maxObjects, found, iface);

		if ( search2 )
			search2->search( axis, pos,radius, count, maxObjects, found, iface);

	}

	void search(Axes axis,const hacd::HaF32 *pos,hacd::HaF32 radius,hacd::HaU32 &count,hacd::HaU32 maxObjects,KdTreeFindNode *found,const KdTreeInterface *iface)
	{

		const hacd::HaF32 *position = iface->getPositionFloat(mIndex);

		hacd::HaF32 dx = pos[0] - position[0];
		hacd::HaF32 dy = pos[1] - position[1];
		hacd::HaF32 dz = pos[2] - position[2];

		KdTreeNode *search1 = 0;
		KdTreeNode *search2 = 0;

		switch ( axis )
		{
		case X_AXIS:
			if ( dx <= 0 )     // JWR  if we are to the left
			{
				search1 = mLeft; // JWR  then search to the left
				if ( -dx < radius )  // JWR  if distance to the right is less than our search radius, continue on the right as well.
					search2 = mRight;
			}
			else
			{
				search1 = mRight; // JWR  ok, we go down the left tree
				if ( dx < radius ) // JWR  if the distance from the right is less than our search radius
					search2 = mLeft;
			}
			axis = Y_AXIS;
			break;
		case Y_AXIS:
			if ( dy <= 0 )
			{
				search1 = mLeft;
				if ( -dy < radius )
					search2 = mRight;
			}
			else
			{
				search1 = mRight;
				if ( dy < radius )
					search2 = mLeft;
			}
			axis = Z_AXIS;
			break;
		case Z_AXIS:
			if ( dz <= 0 )
			{
				search1 = mLeft;
				if ( -dz < radius )
					search2 = mRight;
			}
			else
			{
				search1 = mRight;
				if ( dz < radius )
					search2 = mLeft;
			}
			axis = X_AXIS;
			break;
		}

		hacd::HaF32 r2 = radius*radius;
		hacd::HaF32 m  = dx*dx+dy*dy+dz*dz;

		if ( m < r2 )
		{
			switch ( count )
			{
			case 0:
				found[count].mNode = this;
				found[count].mDistance = m;
				break;
			case 1:
				if ( m < found[0].mDistance )
				{
					if ( maxObjects == 1 )
					{
						found[0].mNode = this;
						found[0].mDistance = m;
					}
					else
					{
						found[1] = found[0];
						found[0].mNode = this;
						found[0].mDistance = m;
					}
				}
				else if ( maxObjects > 1)
				{
					found[1].mNode = this;
					found[1].mDistance = m;
				}
				break;
			default:
				{
					bool inserted = false;

					for (hacd::HaU32 i=0; i<count; i++)
					{
						if ( m < found[i].mDistance ) // if this one is closer than a pre-existing one...
						{
							// insertion sort...
							hacd::HaU32 scan = count;
							if ( scan >= maxObjects ) scan=maxObjects-1;
							for (hacd::HaU32 j=scan; j>i; j--)
							{
								found[j] = found[j-1];
							}
							found[i].mNode = this;
							found[i].mDistance = m;
							inserted = true;
							break;
						}
					}

					if ( !inserted && count < maxObjects )
					{
						found[count].mNode = this;
						found[count].mDistance = m;
					}
				}
				break;
			}
			count++;
			if ( count > maxObjects )
			{
				count = maxObjects;
			}
		}


		if ( search1 )
			search1->search( axis, pos,radius, count, maxObjects, found, iface);

		if ( search2 )
			search2->search( axis, pos,radius, count, maxObjects, found, iface);

	}

private:

	void setLeft(KdTreeNode *left) { mLeft = left; };
	void setRight(KdTreeNode *right) { mRight = right; };

	KdTreeNode *getLeft(void)         { return mLeft; }
	KdTreeNode *getRight(void)        { return mRight; }

	hacd::HaU32          mIndex;
	KdTreeNode     *mLeft;
	KdTreeNode     *mRight;
};


#define MAX_BUNDLE_SIZE 1024  // 1024 nodes at a time, to minimize memory allocation and guarentee that pointers are persistent.

class KdTreeNodeBundle : public UANS::UserAllocated
{
public:

	KdTreeNodeBundle(void)
	{
		mNext = 0;
		mIndex = 0;
	}

	bool isFull(void) const
	{
		return (bool)( mIndex == MAX_BUNDLE_SIZE );
	}

	KdTreeNode * getNextNode(void)
	{
		assert(mIndex<MAX_BUNDLE_SIZE);
		KdTreeNode *ret = &mNodes[mIndex];
		mIndex++;
		return ret;
	}

	KdTreeNodeBundle  *mNext;
	hacd::HaU32             mIndex;
	KdTreeNode         mNodes[MAX_BUNDLE_SIZE];
};


typedef hacd::vector< hacd::HaF64 > DoubleVector;
typedef hacd::vector< hacd::HaF32 >  FloatVector;

class KdTree : public KdTreeInterface, public UANS::UserAllocated
{
public:
	KdTree(void)
	{
		mRoot = 0;
		mBundle = 0;
		mVcount = 0;
		mUseDouble = false;
	}

	virtual ~KdTree(void)
	{
		reset();
	}

	const hacd::HaF64 * getPositionDouble(hacd::HaU32 index) const
	{
		assert( mUseDouble );
		assert ( index < mVcount );
		return  &mVerticesDouble[index*3];
	}

	const hacd::HaF32 * getPositionFloat(hacd::HaU32 index) const
	{
		assert( !mUseDouble );
		assert ( index < mVcount );
		return  &mVerticesFloat[index*3];
	}

	hacd::HaU32 search(const hacd::HaF64 *pos,hacd::HaF64 radius,hacd::HaU32 maxObjects,KdTreeFindNode *found) const
	{
		assert( mUseDouble );
		if ( !mRoot )	return 0;
		hacd::HaU32 count = 0;
		mRoot->search(X_AXIS,pos,radius,count,maxObjects,found,this);
		return count;
	}

	hacd::HaU32 search(const hacd::HaF32 *pos,hacd::HaF32 radius,hacd::HaU32 maxObjects,KdTreeFindNode *found) const
	{
		assert( !mUseDouble );
		if ( !mRoot )	return 0;
		hacd::HaU32 count = 0;
		mRoot->search(X_AXIS,pos,radius,count,maxObjects,found,this);
		return count;
	}

	void reset(void)
	{
		mRoot = 0;
		mVerticesDouble.clear();
		mVerticesFloat.clear();
		KdTreeNodeBundle *bundle = mBundle;
		while ( bundle )
		{
			KdTreeNodeBundle *next = bundle->mNext;
			delete bundle;
			bundle = next;
		}
		mBundle = 0;
		mVcount = 0;
	}

	hacd::HaU32 add(hacd::HaF64 x,hacd::HaF64 y,hacd::HaF64 z)
	{
		assert(mUseDouble);
		hacd::HaU32 ret = mVcount;
		mVerticesDouble.push_back(x);
		mVerticesDouble.push_back(y);
		mVerticesDouble.push_back(z);
		mVcount++;
		KdTreeNode *node = getNewNode(ret);
		if ( mRoot )
		{
			mRoot->addDouble(node,X_AXIS,this);
		}
		else
		{
			mRoot = node;
		}
		return ret;
	}

	hacd::HaU32 add(hacd::HaF32 x,hacd::HaF32 y,hacd::HaF32 z)
	{
		assert(!mUseDouble);
		hacd::HaU32 ret = mVcount;
		mVerticesFloat.push_back(x);
		mVerticesFloat.push_back(y);
		mVerticesFloat.push_back(z);
		mVcount++;
		KdTreeNode *node = getNewNode(ret);
		if ( mRoot )
		{
			mRoot->addFloat(node,X_AXIS,this);
		}
		else
		{
			mRoot = node;
		}
		return ret;
	}

	KdTreeNode * getNewNode(hacd::HaU32 index)
	{
		if ( mBundle == 0 )
		{
			mBundle = HACD_NEW(KdTreeNodeBundle);
		}
		if ( mBundle->isFull() )
		{
			KdTreeNodeBundle *bundle = HACD_NEW(KdTreeNodeBundle);
			mBundle->mNext = bundle;
			mBundle = bundle;
		}
		KdTreeNode *node = mBundle->getNextNode();
		new ( node ) KdTreeNode(index);
		return node;
	}

	hacd::HaU32 getNearest(const hacd::HaF64 *pos,hacd::HaF64 radius,bool &_found) const // returns the nearest possible neighbor's index.
	{
		assert( mUseDouble );
		hacd::HaU32 ret = 0;

		_found = false;
		KdTreeFindNode found[1];
		hacd::HaU32 count = search(pos,radius,1,found);
		if ( count )
		{
			KdTreeNode *node = found[0].mNode;
			ret = node->getIndex();
			_found = true;
		}
		return ret;
	}

	hacd::HaU32 getNearest(const hacd::HaF32 *pos,hacd::HaF32 radius,bool &_found) const // returns the nearest possible neighbor's index.
	{
		assert( !mUseDouble );
		hacd::HaU32 ret = 0;

		_found = false;
		KdTreeFindNode found[1];
		hacd::HaU32 count = search(pos,radius,1,found);
		if ( count )
		{
			KdTreeNode *node = found[0].mNode;
			ret = node->getIndex();
			_found = true;
		}
		return ret;
	}

	const hacd::HaF64 * getVerticesDouble(void) const
	{
		assert( mUseDouble );
		const hacd::HaF64 *ret = 0;
		if ( !mVerticesDouble.empty() )
		{
			ret = &mVerticesDouble[0];
		}
		return ret;
	}

	const hacd::HaF32 * getVerticesFloat(void) const
	{
		assert( !mUseDouble );
		const hacd::HaF32 * ret = 0;
		if ( !mVerticesFloat.empty() )
		{
			ret = &mVerticesFloat[0];
		}
		return ret;
	}

	hacd::HaU32 getVcount(void) const { return mVcount; };

	void setUseDouble(bool useDouble)
	{
		mUseDouble = useDouble;
	}

private:
	bool                    mUseDouble;
	KdTreeNode             *mRoot;
	KdTreeNodeBundle       *mBundle;
	hacd::HaU32                  mVcount;
	DoubleVector            mVerticesDouble;
	FloatVector             mVerticesFloat;
};

class MyVertexIndex : public fm_VertexIndex, public UANS::UserAllocated
{
public:
	MyVertexIndex(hacd::HaF64 granularity,bool snapToGrid)
	{
		mDoubleGranularity = granularity;
		mFloatGranularity  = (hacd::HaF32)granularity;
		mSnapToGrid        = snapToGrid;
		mUseDouble         = true;
		mKdTree.setUseDouble(true);
	}

	MyVertexIndex(hacd::HaF32 granularity,bool snapToGrid)
	{
		mDoubleGranularity = granularity;
		mFloatGranularity  = (hacd::HaF32)granularity;
		mSnapToGrid        = snapToGrid;
		mUseDouble         = false;
		mKdTree.setUseDouble(false);
	}

	virtual ~MyVertexIndex(void)
	{

	}


	hacd::HaF64 snapToGrid(hacd::HaF64 p)
	{
		hacd::HaF64 m = fmod(p,mDoubleGranularity);
		p-=m;
		return p;
	}

	hacd::HaF32 snapToGrid(hacd::HaF32 p)
	{
		hacd::HaF32 m = fmodf(p,mFloatGranularity);
		p-=m;
		return p;
	}

	hacd::HaU32    getIndex(const hacd::HaF32 *_p,bool &newPos)  // get index for a vector hacd::HaF32
	{
		hacd::HaU32 ret;

		if ( mUseDouble )
		{
			hacd::HaF64 p[3];
			p[0] = _p[0];
			p[1] = _p[1];
			p[2] = _p[2];
			return getIndex(p,newPos);
		}

		newPos = false;

		hacd::HaF32 p[3];

		if ( mSnapToGrid )
		{
			p[0] = snapToGrid(_p[0]);
			p[1] = snapToGrid(_p[1]);
			p[2] = snapToGrid(_p[2]);
		}
		else
		{
			p[0] = _p[0];
			p[1] = _p[1];
			p[2] = _p[2];
		}

		bool found;
		ret = mKdTree.getNearest(p,mFloatGranularity,found);
		if ( !found )
		{
			newPos = true;
			ret = mKdTree.add(p[0],p[1],p[2]);
		}


		return ret;
	}

	hacd::HaU32    getIndex(const hacd::HaF64 *_p,bool &newPos)  // get index for a vector hacd::HaF64
	{
		hacd::HaU32 ret;

		if ( !mUseDouble )
		{
			hacd::HaF32 p[3];
			p[0] = (hacd::HaF32)_p[0];
			p[1] = (hacd::HaF32)_p[1];
			p[2] = (hacd::HaF32)_p[2];
			return getIndex(p,newPos);
		}

		newPos = false;

		hacd::HaF64 p[3];

		if ( mSnapToGrid )
		{
			p[0] = snapToGrid(_p[0]);
			p[1] = snapToGrid(_p[1]);
			p[2] = snapToGrid(_p[2]);
		}
		else
		{
			p[0] = _p[0];
			p[1] = _p[1];
			p[2] = _p[2];
		}

		bool found;
		ret = mKdTree.getNearest(p,mDoubleGranularity,found);
		if ( !found )
		{
			newPos = true;
			ret = mKdTree.add(p[0],p[1],p[2]);
		}


		return ret;
	}

	const hacd::HaF32 *   getVerticesFloat(void) const
	{
		const hacd::HaF32 * ret = 0;

		assert( !mUseDouble );

		ret = mKdTree.getVerticesFloat();

		return ret;
	}

	const hacd::HaF64 *  getVerticesDouble(void) const
	{
		const hacd::HaF64 * ret = 0;

		assert( mUseDouble );

		ret = mKdTree.getVerticesDouble();

		return ret;
	}

	const hacd::HaF32 *   getVertexFloat(hacd::HaU32 index) const
	{
		const hacd::HaF32 * ret  = 0;
		assert( !mUseDouble );
#ifdef _DEBUG
		hacd::HaU32 vcount = mKdTree.getVcount();
		assert( index < vcount );
#endif
		ret =  mKdTree.getVerticesFloat();
		ret = &ret[index*3];
		return ret;
	}

	const hacd::HaF64 *   getVertexDouble(hacd::HaU32 index) const
	{
		const hacd::HaF64 * ret = 0;
		assert( mUseDouble );
#ifdef _DEBUG
		hacd::HaU32 vcount = mKdTree.getVcount();
		assert( index < vcount );
#endif
		ret =  mKdTree.getVerticesDouble();
		ret = &ret[index*3];

		return ret;
	}

	hacd::HaU32    getVcount(void) const
	{
		return mKdTree.getVcount();
	}

	bool isDouble(void) const
	{
		return mUseDouble;
	}


	bool            saveAsObj(const char *fname,hacd::HaU32 tcount,hacd::HaU32 *indices)
	{
		bool ret = false;


		FILE *fph = fopen(fname,"wb");
		if ( fph )
		{
			ret = true;

			hacd::HaU32 vcount    = getVcount();
			if ( mUseDouble )
			{
				const hacd::HaF64 *v  = getVerticesDouble();
				for (hacd::HaU32 i=0; i<vcount; i++)
				{
					fprintf(fph,"v %0.9f %0.9f %0.9f\r\n", (hacd::HaF32)v[0], (hacd::HaF32)v[1], (hacd::HaF32)v[2] );
					v+=3;
				}
			}
			else
			{
				const hacd::HaF32 *v  = getVerticesFloat();
				for (hacd::HaU32 i=0; i<vcount; i++)
				{
					fprintf(fph,"v %0.9f %0.9f %0.9f\r\n", v[0], v[1], v[2] );
					v+=3;
				}
			}

			for (hacd::HaU32 i=0; i<tcount; i++)
			{
				hacd::HaU32 i1 = *indices++;
				hacd::HaU32 i2 = *indices++;
				hacd::HaU32 i3 = *indices++;
				fprintf(fph,"f %d %d %d\r\n", i1+1, i2+1, i3+1 );
			}
			fclose(fph);
		}

		return ret;
	}

private:
	bool    mUseDouble:1;
	bool    mSnapToGrid:1;
	hacd::HaF64  mDoubleGranularity;
	hacd::HaF32   mFloatGranularity;
	KdTree  mKdTree;
};

static fm_VertexIndex * fm_createVertexIndex(hacd::HaF32 granularity,bool snapToGrid)  // create an indexed vertext system for floats
{
	MyVertexIndex *ret = HACD_NEW(MyVertexIndex)(granularity,snapToGrid);
	return static_cast< fm_VertexIndex *>(ret);
}

static void          fm_releaseVertexIndex(fm_VertexIndex *vindex)
{
	MyVertexIndex *m = static_cast< MyVertexIndex *>(vindex);
	delete m;
}


//

// Split Mesh

class SimpleMesh
{
public:
	SimpleMesh(void)
	{
		mVcount = 0;
		mTcount = 0;
		mVertices = NULL;
		mIndices = NULL;
	}
	SimpleMesh(hacd::HaU32 vcount,hacd::HaU32 tcount,const hacd::HaF32 *vertices,const hacd::HaU32 *indices)
	{
		mVcount = 0;
		mTcount = 0;
		mVertices = NULL;
		mIndices = NULL;
		set(vcount,tcount,vertices,indices);
	}

	void set(hacd::HaU32 vcount,hacd::HaU32 tcount,const hacd::HaF32 *vertices,const hacd::HaU32 *indices)
	{
		release();
		mVcount = vcount;
		mTcount = tcount;
		mVertices = (hacd::HaF32 *)HACD_ALLOC(sizeof(hacd::HaF32)*3*mVcount);
		memcpy(mVertices,vertices,sizeof(hacd::HaF32)*3*mVcount);
		mIndices = (hacd::HaU32 *)HACD_ALLOC(sizeof(hacd::HaU32)*3*mTcount);
		memcpy(mIndices,indices,sizeof(hacd::HaU32)*3*mTcount);
	}


	~SimpleMesh(void)
	{
		release();
	}

	void release(void)
	{
		HACD_FREE(mVertices);
		HACD_FREE(mIndices);
		mVertices = NULL;
		mIndices = NULL;
		mVcount = 0;
		mTcount = 0;
	}


	hacd::HaU32	mVcount;
	hacd::HaU32	mTcount;
	hacd::HaF32	*mVertices;
	hacd::HaU32	*mIndices;
};


void splitMesh(const hacd::HaF32 *planeEquation,const SimpleMesh &input,SimpleMesh &left,SimpleMesh &right,bool closedMesh);

static void addTri(const hacd::HaF32 *p1,
				   const hacd::HaF32 *p2,
				   const hacd::HaF32 *p3,
				   hacd::vector< hacd::HaU32 > &indices,
				   fm_VertexIndex *vertices)
{
	bool newPos;

	hacd::HaU32 i1 = vertices->getIndex(p1,newPos);
	hacd::HaU32 i2 = vertices->getIndex(p2,newPos);
	hacd::HaU32 i3 = vertices->getIndex(p3,newPos);

	indices.push_back(i1);
	indices.push_back(i2);
	indices.push_back(i3);
}

enum PlaneTriResult
{
	PTR_ON_PLANE,
	PTR_FRONT,
	PTR_BACK,
	PTR_SPLIT,
};

static PlaneTriResult fm_getSidePlane(const hacd::HaF32 *p,const hacd::HaF32 *plane,hacd::HaF32 epsilon)
{
	PlaneTriResult ret = PTR_ON_PLANE;

	hacd::HaF32 d = fm_distToPlane(plane,p);

	if ( d < -epsilon || d > epsilon )
	{
		if ( d > 0 )
			ret =  PTR_FRONT; // it is 'in front' within the provided epsilon value.
		else
			ret = PTR_BACK;
	}

	return ret;
}




static PlaneTriResult fm_planeTriIntersection(const hacd::HaF32 plane[4],    // the plane equation in Ax+By+Cz+D format
									   const hacd::HaF32 *triangle, // the source triangle.
									   hacd::HaU32 tstride,  // stride in bytes of the input and output *vertices*
									   hacd::HaF32        epsilon,  // the co-planer epsilon value.
									   hacd::HaF32       *front,    // the triangle in front of the
									   hacd::HaU32 &fcount,  // number of vertices in the 'front' triangle
									   hacd::HaF32       *back,     // the triangle in back of the plane
									   hacd::HaU32 &bcount); // the number of vertices in the 'back' triangle.

static inline void add(const hacd::HaF32 *p,hacd::HaF32 *dest,hacd::HaU32 tstride,hacd::HaU32 &pcount)
{
	char *d = (char *) dest;
	d = d + pcount*tstride;
	dest = (hacd::HaF32 *) d;
	dest[0] = p[0];
	dest[1] = p[1];
	dest[2] = p[2];
	pcount++;
	HACD_ASSERT( pcount <= 4 );
}

#define MAXPTS 256

template <class Type> class point
{
public:

	void set(const Type *p)
	{
		x = p[0];
		y = p[1];
		z = p[2];
	}

	Type x;
	Type y;
	Type z;
};

template <class Type> class plane
{
public:
	plane(const Type *p)
	{
		normal.x = p[0];
		normal.y = p[1];
		normal.z = p[2];
		D        = p[3];
	}

	Type Classify_Point(const point<Type> &p)
	{
		return p.x*normal.x + p.y*normal.y + p.z*normal.z + D;
	}

	point<Type> normal;
	Type  D;
};

template <class Type> class polygon
{
public:
	polygon(void)
	{
		mVcount = 0;
	}

	polygon(const Type *p1,const Type *p2,const Type *p3)
	{
		mVcount = 3;
		mVertices[0].set(p1);
		mVertices[1].set(p2);
		mVertices[2].set(p3);
	}


	hacd::HaI32 NumVertices(void) const { return mVcount; };

	const point<Type>& Vertex(hacd::HaI32 index)
	{
		if ( index < 0 ) index+=mVcount;
		return mVertices[index];
	};


	void set(const point<Type> *pts,hacd::HaI32 count)
	{
		for (hacd::HaI32 i=0; i<count; i++)
		{
			mVertices[i] = pts[i];
		}
		mVcount = count;
	}


	void Split_Polygon(polygon<Type> *poly,plane<Type> *part, polygon<Type> &front, polygon<Type> &back)
	{
		hacd::HaI32   count = poly->NumVertices ();
		hacd::HaI32   out_c = 0, in_c = 0;
		point<Type> ptA, ptB,outpts[MAXPTS],inpts[MAXPTS];
		Type sideA, sideB;
		ptA = poly->Vertex (count - 1);
		sideA = part->Classify_Point (ptA);
		for (hacd::HaI32 i = -1; ++i < count;)
		{
			ptB = poly->Vertex(i);
			sideB = part->Classify_Point(ptB);
			if (sideB > 0)
			{
				if (sideA < 0)
				{
					point<Type> v;
					fm_intersectPointPlane(&ptB.x, &ptA.x, &v.x, &part->normal.x );
					outpts[out_c++] = inpts[in_c++] = v;
				}
				outpts[out_c++] = ptB;
			}
			else if (sideB < 0)
			{
				if (sideA > 0)
				{
					point<Type> v;
					fm_intersectPointPlane(&ptB.x, &ptA.x, &v.x, &part->normal.x );
					outpts[out_c++] = inpts[in_c++] = v;
				}
				inpts[in_c++] = ptB;
			}
			else
				outpts[out_c++] = inpts[in_c++] = ptB;
			ptA = ptB;
			sideA = sideB;
		}

		front.set(&outpts[0], out_c);
		back.set(&inpts[0], in_c);
	}

	hacd::HaI32           mVcount;
	point<Type>   mVertices[MAXPTS];
};
/* a = b - c */
#define vector(a,b,c) \
	(a)[0] = (b)[0] - (c)[0];	\
	(a)[1] = (b)[1] - (c)[1];	\
	(a)[2] = (b)[2] - (c)[2];



#define innerProduct(v,q) \
	((v)[0] * (q)[0] + \
	(v)[1] * (q)[1] + \
	(v)[2] * (q)[2])

#define crossProduct(a,b,c) \
	(a)[0] = (b)[1] * (c)[2] - (c)[1] * (b)[2]; \
	(a)[1] = (b)[2] * (c)[0] - (c)[2] * (b)[0]; \
	(a)[2] = (b)[0] * (c)[1] - (c)[0] * (b)[1];


static bool fm_rayIntersectsTriangle(const hacd::HaF32 *p,const hacd::HaF32 *d,const hacd::HaF32 *v0,const hacd::HaF32 *v1,const hacd::HaF32 *v2,hacd::HaF32 &t)
{
	hacd::HaF32 e1[3],e2[3],h[3],s[3],q[3];
	hacd::HaF32 a,f,u,v;

	vector(e1,v1,v0);
	vector(e2,v2,v0);
	crossProduct(h,d,e2);
	a = innerProduct(e1,h);

	if (a > -0.00001 && a < 0.00001)
		return(false);

	f = 1/a;
	vector(s,p,v0);
	u = f * (innerProduct(s,h));

	if (u < 0.0 || u > 1.0)
		return(false);

	crossProduct(q,s,e1);
	v = f * innerProduct(d,q);
	if (v < 0.0 || u + v > 1.0)
		return(false);
	// at this stage we can compute t to find out where
	// the intersection point is on the line
	t = f * innerProduct(e2,q);
	if (t > 0) // ray intersection
		return(true);
	else // this means that there is a line intersection
		// but not a ray intersection
		return (false);
}



static PlaneTriResult fm_planeTriIntersection(const hacd::HaF32 *_plane,    // the plane equation in Ax+By+Cz+D format
									   const hacd::HaF32 *triangle, // the source triangle.
									   hacd::HaU32 tstride,  // stride in bytes of the input and output *vertices*
									   hacd::HaF32        epsilon,  // the co-planar epsilon value.
									   hacd::HaF32       *front,    // the triangle in front of the
									   hacd::HaU32 &fcount,  // number of vertices in the 'front' triangle
									   hacd::HaF32       *back,     // the triangle in back of the plane
									   hacd::HaU32 &bcount) // the number of vertices in the 'back' triangle.
{

	fcount = 0;
	bcount = 0;

	const char *tsource = (const char *) triangle;

	// get the three vertices of the triangle.
	const hacd::HaF32 *p1     = (const hacd::HaF32 *) (tsource);
	const hacd::HaF32 *p2     = (const hacd::HaF32 *) (tsource+tstride);
	const hacd::HaF32 *p3     = (const hacd::HaF32 *) (tsource+tstride*2);


	PlaneTriResult r1   = fm_getSidePlane(p1,_plane,epsilon); // compute the side of the plane each vertex is on
	PlaneTriResult r2   = fm_getSidePlane(p2,_plane,epsilon);
	PlaneTriResult r3   = fm_getSidePlane(p3,_plane,epsilon);

	// If any of the points lay right *on* the plane....
	if ( r1 == PTR_ON_PLANE || r2 == PTR_ON_PLANE || r3 == PTR_ON_PLANE )
	{
		// If the triangle is completely co-planar, then just treat it as 'front' and return!
		if ( r1 == PTR_ON_PLANE && r2 == PTR_ON_PLANE && r3 == PTR_ON_PLANE )
		{
			add(p1,front,tstride,fcount);
			add(p2,front,tstride,fcount);
			add(p3,front,tstride,fcount);
			return PTR_FRONT;
		}
		// Decide to place the co-planar points on the same side as the co-planar point.
		PlaneTriResult r= PTR_ON_PLANE;
		if ( r1 != PTR_ON_PLANE )
			r = r1;
		else if ( r2 != PTR_ON_PLANE )
			r = r2;
		else if ( r3 != PTR_ON_PLANE )
			r = r3;

		if ( r1 == PTR_ON_PLANE ) r1 = r;
		if ( r2 == PTR_ON_PLANE ) r2 = r;
		if ( r3 == PTR_ON_PLANE ) r3 = r;

	}

	if ( r1 == r2 && r1 == r3 ) // if all three vertices are on the same side of the plane.
	{
		if ( r1 == PTR_FRONT ) // if all three are in front of the plane, then copy to the 'front' output triangle.
		{
			add(p1,front,tstride,fcount);
			add(p2,front,tstride,fcount);
			add(p3,front,tstride,fcount);
		}
		else
		{
			add(p1,back,tstride,bcount); // if all three are in 'back' then copy to the 'back' output triangle.
			add(p2,back,tstride,bcount);
			add(p3,back,tstride,bcount);
		}
		return r1; // if all three points are on the same side of the plane return result
	}


	polygon<hacd::HaF32> pi(p1,p2,p3);
	polygon<hacd::HaF32>  pfront,pback;

	plane<hacd::HaF32>    part(_plane);

	pi.Split_Polygon(&pi,&part,pfront,pback);

	for (hacd::HaI32 i=0; i<pfront.mVcount; i++)
	{
		add( &pfront.mVertices[i].x, front, tstride, fcount );
	}

	for (hacd::HaI32 i=0; i<pback.mVcount; i++)
	{
		add( &pback.mVertices[i].x, back, tstride, bcount );
	}

	PlaneTriResult ret = PTR_SPLIT;

	if ( fcount < 3 ) fcount = 0;
	if ( bcount < 3 ) bcount = 0;

	if ( fcount == 0 && bcount )
		ret = PTR_BACK;

	if ( bcount == 0 && fcount )
		ret = PTR_FRONT;


	return ret;
}



void splitMesh(const hacd::HaF32 *planeEquation,const SimpleMesh &input,SimpleMesh &leftMesh,SimpleMesh &rightMesh)
{
	hacd::vector< hacd::HaU32 > leftIndices;
	hacd::vector< hacd::HaU32 > rightIndices;

	fm_VertexIndex *leftVertices = fm_createVertexIndex(0.00001f,false);
	fm_VertexIndex *rightVertices = fm_createVertexIndex(0.00001f,false);

	{
		for (hacd::HaU32 i=0; i<input.mTcount; i++)
		{
			hacd::HaU32 i1 = input.mIndices[i*3+0];
			hacd::HaU32 i2 = input.mIndices[i*3+1];
			hacd::HaU32 i3 = input.mIndices[i*3+2];

			hacd::HaF32 *p1 = &input.mVertices[i1*3];
			hacd::HaF32 *p2 = &input.mVertices[i2*3];
			hacd::HaF32 *p3 = &input.mVertices[i3*3];

			hacd::HaF32 tri[3*3];

			tri[0] = p1[0];
			tri[1] = p1[1];
			tri[2] = p1[2];

			tri[3] = p2[0];
			tri[4] = p2[1];
			tri[5] = p2[2];

			tri[6] = p3[0];
			tri[7] = p3[1];
			tri[8] = p3[2];

			hacd::HaF32	front[3*5];
			hacd::HaF32	back[3*5];

			hacd::HaU32 fcount,bcount;

			PlaneTriResult result = fm_planeTriIntersection(planeEquation,tri,sizeof(hacd::HaF32)*3,0.00001f,front,fcount,back,bcount);

			switch ( result )
			{
			case PTR_FRONT:
				addTri(p1,p2,p3,leftIndices,leftVertices);
				break;
			case PTR_BACK:
				addTri(p1,p2,p3,rightIndices,rightVertices);
				break;
			case PTR_SPLIT:
				if ( fcount )
				{
					addTri(&front[0],&front[3],&front[6],leftIndices,leftVertices);
					if ( fcount == 4 )
					{
						addTri(&front[0],&front[6],&front[9],leftIndices,leftVertices);
					}
				}
				if ( bcount )
				{
					addTri(&back[0],&back[3],&back[6],rightIndices,rightVertices);
					if ( bcount == 4 )
					{
						addTri(&back[0],&back[6],&back[9],rightIndices,rightVertices);
					}
				}
				break;
			case PTR_ON_PLANE: // Make compiler happy
				break;
			}
		}
	}

	if ( !leftIndices.empty() )
	{
		leftMesh.set(leftVertices->getVcount(),leftIndices.size()/3,leftVertices->getVerticesFloat(),&leftIndices[0]);
	}

	if ( !rightIndices.empty() )
	{
		rightMesh.set(rightVertices->getVcount(),rightIndices.size()/3,rightVertices->getVerticesFloat(),&rightIndices[0]);
	}
	fm_releaseVertexIndex(leftVertices);
	fm_releaseVertexIndex(rightVertices);
}


//

static hacd::HaF32 enorm0_3d ( hacd::HaF32 x0, hacd::HaF32 y0, hacd::HaF32 z0, hacd::HaF32 x1, hacd::HaF32 y1, hacd::HaF32 z1 )

/**********************************************************************/

/*
Purpose:

ENORM0_3D computes the Euclidean norm of (P1-P0) in 3D.

Modified:

18 April 1999

Author:

John Burkardt

Parameters:

Input, hacd::HaF32 X0, Y0, Z0, X1, Y1, Z1, the coordinates of the points 
P0 and P1.

Output, hacd::HaF32 ENORM0_3D, the Euclidean norm of (P1-P0).
*/
{
  hacd::HaF32 value;

  value = ::sqrt (
    ( x1 - x0 ) * ( x1 - x0 ) + 
    ( y1 - y0 ) * ( y1 - y0 ) + 
    ( z1 - z0 ) * ( z1 - z0 ) );

  return value;
}


static hacd::HaF32 triangle_area_3d ( hacd::HaF32 x1, hacd::HaF32 y1, hacd::HaF32 z1, hacd::HaF32 x2,hacd::HaF32 y2, hacd::HaF32 z2, hacd::HaF32 x3, hacd::HaF32 y3, hacd::HaF32 z3 )

                        /**********************************************************************/

                        /*
                        Purpose:

                        TRIANGLE_AREA_3D computes the area of a triangle in 3D.

                        Modified:

                        22 April 1999

                        Author:

                        John Burkardt

                        Parameters:

                        Input, hacd::HaF32 X1, Y1, Z1, X2, Y2, Z2, X3, Y3, Z3, the (X,Y,Z)
                        coordinates of the corners of the triangle.

                        Output, hacd::HaF32 TRIANGLE_AREA_3D, the area of the triangle.
                        */
{
  hacd::HaF32 a;
  hacd::HaF32 alpha;
  hacd::HaF32 area;
  hacd::HaF32 b;
  hacd::HaF32 base;
  hacd::HaF32 c;
  hacd::HaF32 dot;
  hacd::HaF32 height;
  /*
  Find the projection of (P3-P1) onto (P2-P1).
  */
  dot = 
    ( x2 - x1 ) * ( x3 - x1 ) +
    ( y2 - y1 ) * ( y3 - y1 ) +
    ( z2 - z1 ) * ( z3 - z1 );

  base = enorm0_3d ( x1, y1, z1, x2, y2, z2 );
  /*
  The height of the triangle is the length of (P3-P1) after its
  projection onto (P2-P1) has been subtracted.
  */
  if ( base == 0.0 ) {

    height = 0.0;

  }
  else {

    alpha = dot / ( base * base );

    a = x3 - x1 - alpha * ( x2 - x1 );
    b = y3 - y1 - alpha * ( y2 - y1 );
    c = z3 - z1 - alpha * ( z2 - z1 );

    height = ::sqrt ( a * a + b * b + c * c );

  }

  area = 0.5f * base * height;

  return area;
}


hacd::HaF32 fm_computeArea(const hacd::HaF32 *p1,const hacd::HaF32 *p2,const hacd::HaF32 *p3)
{
  hacd::HaF32 ret = 0;

  ret = triangle_area_3d(p1[0],p1[1],p1[2],p2[0],p2[1],p2[2],p3[0],p3[1],p3[2]);

  return ret;
}

void validate(hacd::HaF32 v)
{
	HACD_ASSERT( v >= -5000 && v < 5000 );
}

void validate(const hacd::HaF32 *v)
{
	validate(v[0]);
	validate(v[1]);
	validate(v[2]);
}


void addVertex(const hacd::HaF32 *p,hacd::HaF32 *dest,hacd::HaU32 index)
{
	dest[index*3+0] = p[0];
	dest[index*3+1] = p[1];
	dest[index*3+2] = p[2];

	validate( &dest[index*3]);

}

void addTriangle(hacd::HaU32 *indices,hacd::HaU32 i1,hacd::HaU32 i2,hacd::HaU32 i3,hacd::HaU32 index)
{
	indices[index*3+0] = i1;
	indices[index*3+1] = i2;
	indices[index*3+2] = i3;
}

bool projectRay(const hacd::HaF32 *p,const hacd::HaF32 *n,hacd::HaF32 *t,const HACD_STANDALONE::HullResult &hull)
{
	bool ret = false;

	t[0] = p[0];
	t[1] = p[1];
	t[2] = p[2];
	validate(p);
	validate(n);

	for (hacd::HaU32 i=0; i<hull.mNumTriangles; i++)
	{
		hacd::HaI32 i1 = hull.mIndices[i*3+0];
		hacd::HaI32 i2 = hull.mIndices[i*3+1];
		hacd::HaI32 i3 = hull.mIndices[i*3+2];

		const hacd::HaF32 *p1 = &hull.mOutputVertices[i1*3];
		const hacd::HaF32 *p2 = &hull.mOutputVertices[i2*3];
		const hacd::HaF32 *p3 = &hull.mOutputVertices[i3*3];

		hacd::HaF32 tm;
		if ( fm_rayIntersectsTriangle(p,n,p1,p2,p3,tm))
		{
			if ( tm > 100 )
			{
				fm_rayIntersectsTriangle(p,n,p1,p2,p3,tm);
			}
			t[0] = p[0]+n[0]*tm;
			t[1] = p[1]+n[1]*tm;
			t[2] = p[2]+n[2]*tm;
			ret = true;
			break;
		}
	}

	if ( ret )
	{
		validate(t);
	}

	return ret;
}

hacd::HaF32 computeProjectedVolume(const hacd::HaF32 *p1,const hacd::HaF32 *p2,const hacd::HaF32 *p3,const HACD_STANDALONE::HullResult &hull)
{
	hacd::HaF32 ret = 0;

	hacd::HaF32 area = fm_computeArea(p1,p2,p3);
	if ( area <= 0 ) 
	{
		return 0;
	}

	hacd::HaF32 normal[3];
	fm_computePlane(p3,p2,p1,normal);

	hacd::HaF32 t1[3];
	hacd::HaF32 t2[3];
	hacd::HaF32 t3[3];

	bool hit1 = projectRay(p1,normal,t1,hull);
	bool hit2 = projectRay(p2,normal,t2,hull);
	bool hit3 = projectRay(p3,normal,t3,hull);

	if ( hit1 || hit2 || hit3 )
	{
		// now we build the little triangle mesh piece...
		hacd::HaU32 indices[8*3];

		hacd::HaF32 vertices[6*3];
		addVertex(p1,vertices,0);
		addVertex(p2,vertices,1);
		addVertex(p3,vertices,2);
		addVertex(t1,vertices,3);
		addVertex(t2,vertices,4);
		addVertex(t3,vertices,5);

		addTriangle(indices,2,1,0,0);
		addTriangle(indices,3,4,5,1);

		addTriangle(indices,0,3,4,2);
		addTriangle(indices,0,4,1,3);

		addTriangle(indices,2,5,3,4);
		addTriangle(indices,2,3,0,5);

		addTriangle(indices,1,4,5,6);
		addTriangle(indices,1,5,2,7);

		ret = fm_computeMeshVolume(vertices,8,indices);

#if 0
		static FILE *fph = fopen("project.obj", "wb" );
		static int baseVertex = 1;
		for (int i=0; i<6; i++)
		{
			fprintf(fph,"v %0.9f %0.9f %0.9f\r\n", vertices[i*3+0], vertices[i*3+1], vertices[i*3+2] );
		}
		for (int i=0; i<8; i++)
		{
			fprintf(fph,"f %d %d %d\r\n", indices[i*3+0]+baseVertex, indices[i*3+1]+baseVertex, indices[i*3+2]+baseVertex );
		}
		fflush(fph);
		baseVertex+=6;
#endif
	}

	return ret;
}
hacd::HaF32 computeConcavityVolume(hacd::HaU32 vcount,
						   const hacd::HaF32 *vertices,
						   hacd::HaU32 tcount,
						   const hacd::HaU32 *indices,
						   const HACD_STANDALONE::HullResult &result)
{
	hacd::HaF32 ret = 0;

	for (hacd::HaU32 i=0; i<tcount; i++)
	{
		hacd::HaU32 i1 = indices[i*3+0];
		hacd::HaU32 i2 = indices[i*3+1];
		hacd::HaU32 i3 = indices[i*3+2];

		const hacd::HaF32 *p1 = &vertices[i1*3];
		const hacd::HaF32 *p2 = &vertices[i2*3];
		const hacd::HaF32 *p3 = &vertices[i3*3];

		ret+=computeProjectedVolume(p1,p2,p3,result);

	}

	return ret;
}

//


typedef hacd::vector< ConvexResult > ConvexResultVector;

class ConvexBuilder : public ConvexDecompInterface, public ConvexDecomposition, public UANS::UserAllocated
{
public:
	ConvexBuilder(void)
	{
	};

	virtual ~ConvexBuilder(void)
	{
		for (hacd::HaU32 i=0; i<mResults.size(); i++)
		{
			ConvexResult &r = mResults[i];
			HACD_FREE(r.mHullIndices);
			HACD_FREE(r.mHullVertices);
		}
	}

	virtual void ConvexDecompResult(const ConvexResult &result)
	{
		ConvexResult r;
		r.mHullTcount = result.mHullTcount;
		r.mHullVcount = result.mHullVcount;
		r.mHullIndices = (hacd::HaU32 *)HACD_ALLOC(sizeof(hacd::HaU32)*r.mHullTcount*3);
		memcpy(r.mHullIndices,result.mHullIndices,sizeof(hacd::HaU32)*r.mHullTcount*3);
		r.mHullVertices = (hacd::HaF32 *)HACD_ALLOC(sizeof(hacd::HaF32)*r.mHullVcount*3);
		memcpy(r.mHullVertices,result.mHullVertices,sizeof(hacd::HaF32)*r.mHullVcount*3);
		mResults.push_back(r);
	}

void doConvexDecomposition(hacd::HaU32 vcount,
							const hacd::HaF32 *vertices,
							hacd::HaU32 tcount,
							const hacd::HaU32 *indices,
							Cdesc &cdesc,
							hacd::HaU32 depth)
{

	// first see if the input mesh is co-planar.
	// If it is, then we return because we can't do anything with a co-planer mesh
	bool isCoplanar = fm_isMeshCoplanar(tcount,indices,vertices,true);
	if ( isCoplanar ) return;


	// Next build a convex hull for the input vertices for this mesh fragment
	HACD_STANDALONE::HullResult result;
	HACD_STANDALONE::HullLibrary hl;
	HACD_STANDALONE::HullDesc   desc;
	desc.mVcount = vcount;
	desc.mVertices = vertices;
	desc.mVertexStride = sizeof(hacd::HaF32)*3;
	HACD_STANDALONE::HullError ret = hl.CreateConvexHull(desc,result);
	if ( ret != HACD_STANDALONE::QE_OK )
	{
		return; // unable to build a hull for this remaining piece of mesh; so return.
	}

	bool split = false;
	if ( depth < cdesc.mMaxDepth ) // if have not reached the maximum depth
	{
		// compute the volume of the convex hull prior to the plist.
		hacd::HaF32 hullVolume = fm_computeMeshVolume(result.mOutputVertices,result.mNumTriangles,result.mIndices);
		if (depth == 0 )
		{
			cdesc.mMasterVolume = hullVolume;
		}
		hacd::HaF32 percent = (hullVolume*100)/cdesc.mMasterVolume;
		// if this convex hull is still considered significant enough in size to keep splitting...
		if ( percent > cdesc.mMeshVolumePercent ) // if not too small of a feature...
		{
			// find the split plane by computing the OBB and slicing in half
			hacd::HaF32 plane[4];
			split = fm_computeSplitPlane(result.mNumOutputVertices,result.mOutputVertices,result.mNumTriangles,result.mIndices,plane);
			if ( split )
			{
				{
					hacd::HaF32 concaveVolume = computeConcavityVolume(vcount,vertices,tcount,indices,result);
					hacd::HaF32 percentVolume = concaveVolume*100 / hullVolume;

					if ( percentVolume < cdesc.mConcavePercent )
					{
						split = false;
					}
				}

				SimpleMesh mesh(vcount, tcount, vertices, indices);
				SimpleMesh leftMesh;
				SimpleMesh rightMesh;
				splitMesh(plane,mesh,leftMesh,rightMesh);

				if ( split )
				{

					if ( leftMesh.mTcount )
					{
						doConvexDecomposition(leftMesh.mVcount, leftMesh.mVertices, leftMesh.mTcount,leftMesh.mIndices,cdesc,depth+1);
					}
					if ( rightMesh.mTcount )
					{
						doConvexDecomposition(rightMesh.mVcount, rightMesh.mVertices, rightMesh.mTcount,rightMesh.mIndices, cdesc, depth+1);
					}
				}
			}
		}
	}
	if ( !split )
	{
		ConvexResult r;
		r.mHullIndices = result.mIndices;
		r.mHullVertices = result.mOutputVertices;
		r.mHullTcount = result.mNumTriangles;
		r.mHullVcount = result.mNumOutputVertices;
		cdesc.mCallback->ConvexDecompResult(r);
		hl.ReleaseResult(result); // do not release the result!

		if ( cdesc.mICallback )
		{
			hacd::HaF32 progress = (hacd::HaF32)cdesc.mOutputCount / (hacd::HaF32)cdesc.mOutputPow2;
			cdesc.mOutputCount++;
			cdesc.mICallback->ReportProgress("SplittingMesh", progress );
		}


	}
}

	virtual hacd::HaU32 performConvexDecomposition(const DecompDesc &desc) // returns the number of hulls produced.
	{
		Cdesc cdesc;
		cdesc.mMaxDepth			= desc.mDepth;
		cdesc.mConcavePercent	= desc.mCpercent;
		cdesc.mMeshVolumePercent= desc.mMeshVolumePercent;
		cdesc.mCallback			= this;
		cdesc.mICallback		= desc.mCallback;
		cdesc.mOutputCount = 0;
		hacd::HaU32 p2[17] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
		if ( cdesc.mMaxDepth > 10 )
			cdesc.mMaxDepth = 10;
		cdesc.mOutputPow2 = p2[ cdesc.mMaxDepth];
		doConvexDecomposition(desc.mVcount, desc.mVertices, desc.mTcount, desc.mIndices, cdesc, 0);
		return mResults.size();
	}

  virtual void release(void)
  {
	  delete this;
  }

  virtual ConvexResult * getConvexResult(hacd::HaU32 index,bool takeMemoryOwnership)
  {
	  ConvexResult *ret = NULL;
	  if ( index < mResults.size() )
	  {
		  ret = &mResults[index];
		  if ( takeMemoryOwnership )
		  {
			  mTempResult = *ret;
			  ret->mHullIndices = NULL;
			  ret->mHullVertices = NULL;
			  ret = &mTempResult;
		  }
	  }
	  return ret;
  }

	ConvexResult		mTempResult;
	ConvexResultVector	mResults;
};

ConvexDecomposition * createConvexDecomposition(void)
{
	ConvexBuilder *m = HACD_NEW(ConvexBuilder);
	return static_cast<ConvexDecomposition *>(m);
}


}; // end of namespace

