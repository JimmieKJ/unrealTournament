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

#ifndef PX_VEHICLE_LINEAR_MATH_H
#define PX_VEHICLE_LINEAR_MATH_H
/** \addtogroup vehicle
  @{
*/

#include "PxVehicleSDK.h"

#ifndef PX_DOXYGEN
namespace physx
{
#endif

#define MAX_VECTORN_SIZE (PX_MAX_NB_WHEELS+3)

class VectorN
{
public:

	VectorN(const PxU32 size)
		: mSize(size)
	{
		PX_ASSERT(mSize<=MAX_VECTORN_SIZE);
	}
	~VectorN()
	{
	}

	VectorN(const VectorN& src)
	{
		for(PxU32 i=0;i<src.mSize;i++)
		{
			mValues[i]=src.mValues[i];
		}
		mSize=src.mSize;
	}

	PX_FORCE_INLINE VectorN& operator=(const VectorN& src)
	{
		for(PxU32 i=0;i<src.mSize;i++)
		{
			mValues[i]=src.mValues[i];
		}
		mSize=src.mSize;
		return *this;
	}

	PX_FORCE_INLINE PxF32& operator[] (const PxU32 i)
	{
		PX_ASSERT(i<mSize);
		return (mValues[i]);
	}

	PX_FORCE_INLINE const PxF32& operator[] (const PxU32 i) const
	{
		PX_ASSERT(i<mSize);
		return (mValues[i]);
	}

	PX_FORCE_INLINE PxU32 getSize() const {return mSize;}

private:

	PxF32 mValues[MAX_VECTORN_SIZE];
	PxU32 mSize;
};

class MatrixNN
{
public:

	MatrixNN()
		: mSize(0)
	{
	}
	MatrixNN(const PxU32 size)
		: mSize(size)
	{
		PX_ASSERT(mSize<=MAX_VECTORN_SIZE);
	}
	MatrixNN(const MatrixNN& src)
	{
		for(PxU32 i=0;i<src.mSize;i++)
		{
			for(PxU32 j=0;j<src.mSize;j++)
			{
				mValues[i][j]=src.mValues[i][j];
			}
		}
		mSize=src.mSize;
	}
	~MatrixNN()
	{
	}

	PX_FORCE_INLINE MatrixNN& operator=(const MatrixNN& src)
	{
		for(PxU32 i=0;i<src.mSize;i++)
		{
			for(PxU32 j=0;j<src.mSize;j++)
			{
				mValues[i][j]=src.mValues[i][j];
			}
		}
		mSize=src.mSize;
		return *this;
	}

	PX_FORCE_INLINE PxF32 get(const PxU32 i, const PxU32 j) const
	{
		PX_ASSERT(i<mSize);
		PX_ASSERT(j<mSize);
		return mValues[i][j];
	}
	PX_FORCE_INLINE void set(const PxU32 i, const PxU32 j, const PxF32 val)
	{
		PX_ASSERT(i<mSize);
		PX_ASSERT(j<mSize);
		mValues[i][j]=val;
	}

	PX_FORCE_INLINE PxU32 getSize() const {return mSize;}

	PX_FORCE_INLINE void setSize(const PxU32 size)
	{
		PX_ASSERT(size <= MAX_VECTORN_SIZE);
		mSize = size;
	}

public:

	PxF32 mValues[MAX_VECTORN_SIZE][MAX_VECTORN_SIZE];
	PxU32 mSize;
};

class MatrixNNLUSolver
{
private:

	PxU32 mIndex[MAX_VECTORN_SIZE];
	MatrixNN mLU;

public:

	MatrixNNLUSolver(){}
	~MatrixNNLUSolver(){}

	//Algorithm taken from Numerical Recipes in Fortran 77, page 38

	void decomposeLU(const MatrixNN& a)
	{
#define TINY (1.0e-20f)

		const PxU32 size=a.mSize;

		//Copy a into result then work exclusively on the result matrix.
		MatrixNN LU=a;

		//Initialise index swapping values.
		for(PxU32 i=0;i<size;i++)
		{
			mIndex[i]=0xffffffff;
		}

		PxU32 imax=0;
		PxF32 big,dum;
		PxF32 vv[MAX_VECTORN_SIZE];
		PxF32 d=1.0f;

		for(PxU32 i=0;i<=(size-1);i++)
		{
			big=0.0f;
			for(PxU32 j=0;j<=(size-1);j++)
			{
				const PxF32 temp=PxAbs(LU.get(i,j));
				big = temp>big ? temp : big;
			}
			PX_ASSERT(big!=0.0f);
			vv[i]=1.0f/big;
		}

		for(PxU32 j=0;j<=(size-1);j++)
		{
			for(PxU32 i=0;i<j;i++)
			{
				PxF32 sum=LU.get(i,j);
				for(PxU32 k=0;k<i;k++)
				{
					sum-=LU.get(i,k)*LU.get(k,j);
				}
				LU.set(i,j,sum);
			}

			big=0.0f;
			for(PxU32 i=j;i<=(size-1);i++)
			{
				PxF32 sum=LU.get(i,j);
				for(PxU32 k=0;k<j;k++)
				{
					sum-=LU.get(i,k)*LU.get(k,j);
				}
				LU.set(i,j,sum);
				dum=vv[i]*PxAbs(sum);
				if(dum>=big)
				{
					big=dum;
					imax=i;
				}
			}

			if(j!=imax)
			{
				for(PxU32 k=0;k<size;k++)
				{
					dum=LU.get(imax,k);
					LU.set(imax,k,LU.get(j,k));
					LU.set(j,k,dum);
				}
				d=-d;
				vv[imax]=vv[j];
			}
			mIndex[j]=imax;

			if(LU.get(j,j)==0)
			{
				LU.set(j,j,TINY);
			}

			if(j!=(size-1))
			{
				dum=1.0f/LU.get(j,j);
				for(PxU32 i=j+1;i<=(size-1);i++)
				{
					LU.set(i,j,LU.get(i,j)*dum);
				}
			}
		}

		//Store the result.
		mLU=LU;
	}

	void solve(const VectorN& input, VectorN& result) const
	{
		const PxU32 size=input.getSize();

		result=input;

		PxU32 ip;
		PxU32 ii=0xffffffff;
		PxF32 sum;

		for(PxU32 i=0;i<size;i++)
		{
			ip=mIndex[i];
			sum=result[ip];
			result[ip]=result[i];
            if(ii!=(PxU32)-1)
			{
				for(PxU32 j=ii;j<=(i-1);j++)
				{
					sum-=mLU.get(i,j)*result[j];
				}
			}
			else if(sum!=0)
			{
				ii=i;
			}
			result[i]=sum;
		}
		for(PxI32 i=PxI32(size-1);i>=0;i--)
		{
			sum=result[(PxU32)i];
			for(PxU32 j=PxU32(i+1);j<=(size-1);j++)
			{
				sum-=mLU.get((PxU32)i,j)*result[j];
			}
			result[(PxU32)i]=sum/mLU.get((PxU32)i,(PxU32)i);
		}
	}
};

class MatrixNNDeterminant
{
public:

	static PxF32 compute(const MatrixNN& A)
	{
		if(2==A.getSize())
		{
			const PxF32 a = A.get(0,0);
			const PxF32 b = A.get(0,1);
			const PxF32 c = A.get(1,0);
			const PxF32 d = A.get(1,1);
			const PxF32 det = a*d - b*c;
			return det;
		}
		else
		{
			const PxU32 N = A.getSize();
			PxF32 det = 0.0f;
			PxF32 sigma = 1;
			for(PxU32 k=0;k<N;k++)
			{
				//Construct a matrix without column 0 and row k
				MatrixNN cofactor(N-1);
				for(PxU32 i=1;i<N;i++)
				{
					for(PxU32 j=0;j<k;j++)
					{
						cofactor.set(i-1,j,A.get(i,j));
					}
					for(PxU32 j=k+1;j<N;j++)
					{
						cofactor.set(i-1,j-1,A.get(i,j));
					}
				}

				const PxF32 detsub = compute(cofactor);
				det += A.get(0,k) * sigma * detsub;
				sigma *= -1.0f;
			}
			return det;
		}
	}
};

class MatrixNGaussSeidelSolver
{
public:

	void solve(const PxU32 maxIterations, const PxF32 tolerance, const MatrixNN& A, const VectorN& b, VectorN& result) const
	{
		const PxU32 N = A.getSize();

		VectorN DInv(N);
		PxF32 bLength2 = 0.0f;
		for(PxU32 i = 0; i < N; i++)
		{
			DInv[i] = 1.0f/A.get(i,i);
			bLength2 += (b[i] * b[i]);
		}

		PxU32 iteration = 0;
		PxF32 error = PX_MAX_F32;
		while(iteration < maxIterations && tolerance < error)
		{
			for(PxU32 i = 0; i < N; i++)
			{
				PxF32 l = 0.0f;
				for(PxU32 j = 0; j < i; j++)
				{
					l += A.get(i,j) * result[j];
				}

				PxF32 u = 0.0f;
				for(PxU32 j = i + 1; j < N; j++)
				{
					u += A.get(i,j) * result[j];
				}

				result[i] = DInv[i] * (b[i] - l - u);
			}

			//Compute the error.
			PxF32 rLength2 = 0;
			for(PxU32 i = 0; i < N; i++)
			{
				PxF32 e = -b[i];
				for(PxU32 j = 0; j < N; j++)
				{
					e += A.get(i,j) * result[j];
				}
				rLength2 += e * e;
			}
			error = (rLength2 / (bLength2 + 1e-10f));

			iteration++;
		}
	}
};

class Matrix33Solver
{
public:

	bool solve(const MatrixNN& _A_, const VectorN& _b_, VectorN& result) const
	{
		const PxF32 a = _A_.get(0,0);
		const PxF32 b = _A_.get(0,1);
		const PxF32 c = _A_.get(0,2);

		const PxF32 d = _A_.get(1,0);
		const PxF32 e = _A_.get(1,1);
		const PxF32 f = _A_.get(1,2);

		const PxF32 g = _A_.get(2,0);
		const PxF32 h = _A_.get(2,1);
		const PxF32 k = _A_.get(2,2);

		const PxF32 detA = a*(e*k - f*h) - b*(k*d - f*g) + c*(d*h - e*g);
		if(0.0 == detA)
		{
			return false;
		}
		const PxF32 detAInv = 1.0f/detA;

		const PxF32 A = (e*k - f*h);
		const PxF32 D = -(b*k - c*h);
		const PxF32 G = (b*f - c*e);
		const PxF32 B = -(d*k - f*g);
		const PxF32 E = (a*k - c*g);
		const PxF32 H = -(a*f - c*d);
		const PxF32 C = (d*h - e*g);
		const PxF32 F = -(a*h - b*g);
		const PxF32 K = (a*e - b*d); 

		result[0] = detAInv*(A*_b_[0] + D*_b_[1] + G*_b_[2]);
		result[1] = detAInv*(B*_b_[0] + E*_b_[1] + H*_b_[2]);
		result[2] = detAInv*(C*_b_[0] + F*_b_[1] + K*_b_[2]);

		return true;
	}
};




#ifndef PX_DOXYGEN
} // namespace physx
#endif

#endif //PX_VEHICLE_LINEAR_MATH_H
