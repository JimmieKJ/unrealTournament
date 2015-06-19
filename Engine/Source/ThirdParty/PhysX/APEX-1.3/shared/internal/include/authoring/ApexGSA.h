/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef APEX_GSA_H
#define APEX_GSA_H


#include "PsShare.h"
#include "NxRenderMeshAsset.h"

#ifndef WITHOUT_APEX_AUTHORING

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#define USE_OPTIMIZED_COF	1

#ifdef PX_LINUX
#undef minor
#undef major
#endif

namespace ApexCSG
{
namespace GSA
{

/* Integer abbreviations */

typedef	physx::PxI8		i8;
typedef	physx::PxU8		u8;
typedef	physx::PxI16	i16;
typedef	physx::PxU16	u16;
typedef	physx::PxI32	i32;
typedef	physx::PxU32	u32;
typedef	physx::PxI64	i64;
typedef	physx::PxU64	u64;


/* Utilities */

// Square
template<typename T>
inline T		square(T x)
{
	return x * x;
}

// Sign
template<typename T>
inline T		sgn(T x)
{
	return x >= (T)0 ? (T)1 : -(T)1;
}

template<typename T>
inline T		sgnz(T x)
{
	return x == (T)0 ? (T)0 : (x > (T)0 ? (T)1 : -(T)1);
}

// Special real values
template<typename T>
inline T		max_real()
{
	return (T)0;
}
template<>
inline float	max_real()
{
	return FLT_MAX;
}
template<>
inline double	max_real()
{
	return DBL_MAX;
}

template<typename T>
inline T		min_real()
{
	return (T)0;
}
template<>
inline float	min_real()
{
	return FLT_MIN;
}
template<>
inline double	min_real()
{
	return DBL_MIN;
}

template<typename T>
inline T		eps_real()
{
	return (T)0;
}
template<>
inline float	eps_real()
{
	return FLT_EPSILON;
}
template<>
inline double	eps_real()
{
	return DBL_EPSILON;
}


/* Matrix */

template<int R, int C, typename F>
class Mat
{
public:
	Mat()
	{
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				(*this)(i, j) = (F)0;
			}
	}

	F&				operator()(int rowN, int colN)
	{
		return e[colN][rowN];
	}
	const F&		operator()(int rowN, int colN)	const
	{
		return e[colN][rowN];
	}

	void			setCol(int colN, const Mat<R, 1, F>& col)
	{
		for (int i = 0; i < R; ++i)
		{
			(*this)(i, colN) = col(i, 0);
		}
	}
	Mat<R, 1, F>		getCol(int colN)					const
	{
		Mat<R, 1, F> col;
		for (int i = 0; i < R; ++i)
		{
			col(i, 0) = (*this)(i, colN);
		}
		return col;
	}
	void			setRow(int rowN, const Mat<1, C, F>& row)
	{
		for (int i = 0; i < C; ++i)
		{
			(*this)(rowN, i) = row(0, i);
		}
	}
	Mat<1, C, F>		getRow(int rowN)					const
	{
		Mat<1, C, F> row;
		for (int i = 0; i < C; ++i)
		{
			row(0, i) = (*this)(rowN, i);
		}
		return row;
	}

	Mat<R, C, F>		operator -	()						const
	{
		Mat<R, C, F> r;
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				r(i, j) = -(*this)(i, j);
			}
		return r;
	}

	Mat<R, C, F>		operator *	(F s)					const
	{
		Mat<R, C, F> r;
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				r(i, j) = (*this)(i, j) * s;
			}
		return r;
	}
	Mat<R, C, F>		operator /	(F s)					const
	{
		return (*this) * ((F)1 / s);
	}
	Mat<R, C, F>&		operator *=	(F s)
	{
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				(*this)(i, j) *= s;
			}
		return *this;
	}
	Mat<R, C, F>&		operator /=	(F s)
	{
		*this *= ((F)1 / s);
		return *this;
	}

	Mat<R, C, F>		operator +	(const Mat<R, C, F>& m)	const
	{
		Mat<R, C, F> r;
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				r(i, j) = (*this)(i, j) + m(i, j);
			}
		return r;
	}
	Mat<R, C, F>		operator -	(const Mat<R, C, F>& m)	const
	{
		Mat<R, C, F> r;
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				r(i, j) = (*this)(i, j) - m(i, j);
			}
		return r;
	}
	Mat<R, C, F>&		operator +=	(const Mat<R, C, F>& m)
	{
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				(*this)(i, j) += m(i, j);
			}
		return *this;
	}
	Mat<R, C, F>&		operator -=	(const Mat<R, C, F>& m)
	{
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				(*this)(i, j) -= m(i, j);
			}
		return *this;
	}

	template<int S>
	Mat<R, S, F>		operator *	(const Mat<C, S, F>& m)	const
	{
		Mat<R, S, F> r;
		for (int j = 0; j < S; ++j) for (int i = 0; i < R; ++i) for (int k = 0; k < C; ++k)
				{
					r(i, j) += (*this)(i, k) * m(k, j);
				}
		return r;
	}

	Mat<C, R, F>		T()									const
	{
		Mat<C, R, F> t;
		for (int j = 0; j < C; ++j) for (int i = 0; i < R; ++i)
			{
				t(j, i) = (*this)(i, j);
			}
		return t;
	}

private:
	F	e[C][R];
};

template<int R, int C, typename F>
inline Mat<R, C, F>
operator * (F s, const Mat<R, C, F>& m)
{
	return m * s;
}


/* Square matrix operations */

// Unit matrix
template<int D, typename F>
inline Mat<D, D, F>
unit()
{
	Mat<D, D, F> u;
	for (int i = 0; i < D; ++i)
	{
		u(i, i) = (F)1;
	}
	return u;
}

// apparently minor() function (see man 3 minor) is implemented as a macro on Linux, so we have to undef it
#ifdef minor
  #undef minor
#endif // minor

// Minor of a square matrix element
template<int D, typename F>
inline Mat < D - 1, D - 1, F >
minor(const Mat<D, D, F>& m, int rowN, int colN)
{
	Mat < D - 1, D - 1, F > n;
	for (int i = 0, k = 0; i < D; ++i) if (i != rowN)
		{
			for (int j = 0, l = 0; j < D; ++j) if (j != colN)
				{
					n(k, l) = m(i, j);
					++l;
				} ++k;
		}
	return n;
}

// Cofactor of a square matrix element
template<int D, typename F>
inline F
cof(const Mat<D, D, F>& m, int rowN, int colN)
{
	const Mat < D - 1, D - 1, F > n = minor(m, rowN, colN);
	F detN = 0;
	for (int i = 0; i < D - 1; ++i)
	{
		detN += n(i, 0) * cof(n, i, 0);
	}
	return (F)((int)1 - (int)(((rowN + colN) & 1) << 1)) * detN;
}

// Cofactor of a square matrix element - specialization for 1x1 matrix
template<typename F>
inline F
cof(const Mat<1, 1, F>&, int, int)
{
	return (F)1;
}

// Cofactor matrix
template<int D, typename F>
inline Mat<D, D, F>
cof(const Mat<D, D, F>& m)
{
	Mat<D, D, F> c;
	for (int i = 0; i < D; ++i) for (int j = 0; j < D; ++j)
		{
			c(i, j) = cof(m, i, j);
		}
	return c;
}

// Determinant
template<int D, typename F>
inline F
det(const Mat<D, D, F>& m)
{
	F d = 0;
	for (int i = 0; i < D; ++i)
	{
		d += cof(m, i, D - 1) * m(i, D - 1);
	}
	return d;
}

// Inverse
template<int D, typename F>
inline Mat<D, D, F>
inv(const Mat<D, D, F>& m)
{
	const Mat<D, D, F> c = cof(m);
	F d = 0;
	for (int i = 0; i < D; ++i)
	{
		d += c(i, D - 1) * m(i, D - 1);
	}
	return d == 0 ? Mat<D, D, F>() : c.T() / d;
}


/* Column vector defined as a Dx1 matrix */

template<int D, typename F>
class Col : public Mat<D, 1, F>
{
public:
	Col()										{}
	Col(const Mat<D, 1, F>& v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i, 0);
		}
	}
	Col(const Mat < D + 1, 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i, 0);
		}
	}
	Col(const Col < D + 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}
	}

	Col<D, F>&			set(F c)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = c;
		}
		return *this;
	}

	F&					operator()(int i)
	{
		return Mat<D, 1, F>::operator()(i, 0);
	}
	const F&			operator()(int i)				const
	{
		return Mat<D, 1, F>::operator()(i, 0);
	}

	const Mat<1, D, F>&	T()									const
	{
		return *(const Mat<1, D, F>*)this;
	}

	F					operator | (const Col<D, F>& v)	const
	{
		return (T() * v)(0, 0);
	}

	F					lengthSquared()						const
	{
		return (*this) | (*this);
	}
	F					length()							const
	{
		return sqrt(lengthSquared());
	}

	F					normalize()
	{
		const F l2 = lengthSquared();
		const F recipL = l2 > (F)0 ? (F)1 / sqrt(l2) : (F)0;
		*this *= recipL;
		return recipL * l2;
	}

	Col<D, F>			normal()							const
	{
		Col<D, F> r = *this;
		r.normalize();
		return r;
	}

protected:
	/* Vector builder */
	class Builder
	{
	public:
		Builder(Col& v, const F& t) : m_v(v), m_index(1)
		{
			v(0) = t;
		}

		Builder&	operator , (const F& t)
		{
			if (m_index < D)
			{
				m_v(m_index++) = t;
			}
			return *this;
		}

	private:
		Builder& operator=(const Builder&);

		Col&	m_v;
		int		m_index;
	};

public:
	Builder		operator << (const F& t)
	{
		return Builder(*this, t);
	}
};

// Column vector build functions
template<typename F>
inline Col<2, F>	col2(F x, F y)
{
	Col<2, F> v;
	v << x, y;
	return v;
}
template<typename F>
inline Col<3, F>	col3(F x, F y, F z)
{
	Col<3, F> v;
	v << x, y, z;
	return v;
}
template<typename F>
inline Col<4, F>	col4(F x, F y, F z, F w)
{
	Col<4, F> v;
	v << x, y, z, w;
	return v;
}


/* Row vector defined as a 1xD matrix */

template<int D, typename F>
class Row : public Mat<1, D, F>
{
public:
	Row()										{}
	Row(const Mat<1, D, F>& v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(0, i);
		}
	}
	Row(const Mat < 1, D + 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(0, i);
		}
	}
	Row(const Row < D + 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}
	}

	Row<D, F>&			set(F c)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = c;
		}
		return *this;
	}

	F&					operator()(int i)
	{
		return Mat<1, D, F>::operator()(0, i);
	}
	const F&			operator()(int i)				const
	{
		return Mat<1, D, F>::operator()(0, i);
	}

	const Mat<D, 1, F>&	T()									const
	{
		return *(const Mat<D, 1, F>*)this;
	}

	F					operator | (const Row<D, F>& v)	const
	{
		return (v * T())(0, 0);
	}

	F					lengthSquared()						const
	{
		return (*this) | (*this);
	}
	F					length()							const
	{
		return sqrt(lengthSquared());
	}

	F					normalize()
	{
		const F l2 = lengthSquared();
		const F recipL = l2 > (F)0 ? (F)1 / sqrt(l2) : (F)0;
		*this *= recipL;
		return recipL * l2;
	}

	Row<D, F>			normal()							const
	{
		Row<D, F> r = *this;
		r.normalize();
		return r;
	}

protected:
	/* Vector builder */
	class Builder
	{
	public:
		Builder(Row& v, const F& t) : m_v(v), m_index(1)
		{
			v(0) = t;
		}

		Builder&	operator , (const F& t)
		{
			if (m_index < D)
			{
				m_v(m_index++) = t;
			}
			return *this;
		}

	private:
		Row&	m_v;
		int		m_index;
	};

public:
	Builder		operator << (const F& t)
	{
		return Builder(*this, t);
	}
};

// Row vector build functions
template<typename F>
inline Row<2, F>	row2(F x, F y)
{
	Row<2, F> v;
	v << x, y;
	return v;
}
template<typename F>
inline Row<3, F>	row3(F x, F y, F z)
{
	Row<3, F> v;
	v << x, y, z;
	return v;
}
template<typename F>
inline Row<4, F>	row4(F x, F y, F z, F w)
{
	Row<4, F> v;
	v << x, y, z, w;
	return v;
}


/* Direction, homogeneous representation */

template<int D, typename F>
class Dir : public Col < D + 1, F >
{
public:
	Dir()
	{
		set((F)0);
	}
	Dir(const Col<D, F>& v)
	{
		*this = v;
	}
	Dir(const Col < D + 1, F > & v)
	{
		*this = v;
	}
	Dir(const Mat < D + 1, 1, F > & v)
	{
		*this = v;
	}

	Dir<D, F>&	operator = (const Col<D, F>& v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}(*this)(D) = (F)0;
		return *this;
	}
	Dir<D, F>&	operator = (const Col < D + 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}(*this)(D) = (F)0;
		return *this;
	}
	Dir<D, F>&	operator = (const Mat < D + 1, 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i, 0);
		}(*this)(D) = (F)0;
		return *this;
	}

	Dir<D, F>&	set(F c)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = c;
		}(*this)(D) = (F)0;
		return *this;
	}
};


/* Position, homogeneous representation */

template<int D, typename F>
class Pos : public Col < D + 1, F >
{
public:
	Pos()
	{
		set((F)0);
	}
	Pos(const Col<D, F>& v)
	{
		*this = v;
	}
	Pos(const Col < D + 1, F > & v)
	{
		*this = v;
	}
	Pos(const Mat < D + 1, 1, F > & v)
	{
		*this = v;
	}

	Pos<D, F>&	operator = (const Col<D, F>& v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}(*this)(D) = (F)1;
		return *this;
	}
	Pos<D, F>&	operator = (const Col < D + 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i);
		}(*this)(D) = (F)1;
		return *this;
	}
	Pos<D, F>&	operator = (const Mat < D + 1, 1, F > & v)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = v(i, 0);
		}(*this)(D) = (F)1;
		return *this;
	}

	Pos<D, F>&	set(F c)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = c;
		}(*this)(D) = (F)1;
		return *this;
	}
};


/* Specializations */

template<int D, typename F>
inline Dir<D, F>
operator ^(const Dir<D, F>& a, const Dir<D, F>& b)
{
	PX_UNUSED(a);
	PX_UNUSED(b);
	return Dir<D, F>();
}

// 3D cross product
template<typename F>
inline Dir<3, F>
operator ^(const Dir<3, F>& a, const Dir<3, F>& b)
{
	Dir<3, F> c;
	c << (a(1)*b(2) - a(2)*b(1)), (a(2)*b(0) - a(0)*b(2)), (a(0)*b(1) - a(1)*b(0));
	return c;
}


/* Plane equation defined as a (D+1)-dimensional column vector */

template<int D, typename F>
class Plane : public Col < D + 1, F >
{
public:
	Plane()												{}
	Plane(const Dir<D, F>& n, F d)
	{
		set(n, d);
	}
	Plane(const Dir<D, F>& n, const Pos<D, F>& p)
	{
		set(n, p);
	}
	Plane(const Col < D + 1, F > & v)
	{
		for (int i = 0; i < D + 1; ++i)
		{
			(*this)(i) = v(i);
		}
	}
	Plane(const Mat < D + 1, 1, F > & v)
	{
		for (int i = 0; i < D + 1; ++i)
		{
			(*this)(i) = v(i, 0);
		}
	}

	void		set(const Dir<D, F>& n, F d)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = n(i);
		}(*this)(D) = d;
	}
	void		set(const Dir<D, F>& n, const Pos<D, F>& p)
	{
		for (int i = 0; i < D; ++i)
		{
			(*this)(i) = n(i);
		}(*this)(D) = -(n | p);
	}

	Dir<D, F>	n()											const
	{
		return Dir<D, F>(*this);
	}
	F			d()											const
	{
		return (*this)(D);
	}

	Pos<D, F>	project(const Pos<D, F>& p)				const
	{
		return p - n() * (p | *this);
	}

	F			normalize()
	{
		const F l2 = n().lengthSquared();
		const F recipL = l2 > (F)0 ? (F)1 / sqrt(l2) : (F)0;
		*this *= recipL;
		return recipL * l2;
	}
};


/* Implementation of the Guiding Simplex Algorithm */

template<typename F>
PX_INLINE F
gsa_eps()
{
	return (sizeof(F) == 8 ? 1000000 : 100) * eps_real<F>();
}

/*
	"reduce_simplex" functions take D columns from a D+1 x D+1 singular simplex matrix,
	and produce a D x D simplex matrix in a subspace spanned by D of the normals from the original
	matrix.  The D normals are chosen by colToEliminate, which is the normal _not_ considered for
	the new subspace.
	***In addition, these functions assume that the D normals chosen do NOT span D dimensions!!!***
	... that is, these are very specialized functions for the findCondition function, which are
	called when this condition is met.  A more accurate function name would be
	"reduce_simplex_in_rank_deficient_subspace"
*/

// Reduce 2x2 to 1x1
template<typename F>
PX_INLINE void
reduce_simplex(Mat< 1, 1, F >& reducedS, const Mat< 2, 2, F >& S, int colToEliminate)
{
	// This function should never be called - it's explicitly guarded against in findCondition,
	// and in addition calling it would mean we had a 1-D "plane" with a normal that didn't
	// span one dimension.  (The normal's length would be zero.)
	(void)S;
	(void)colToEliminate;
	reducedS(0,0) = (F)0;
}

// Reduce 3x3 to 2x2
template<typename F>
PX_INLINE void
reduce_simplex(Mat< 2, 2, F >& reducedS, const Mat< 3, 3, F >& S, int colToEliminate)
{
	// The columns used from S
	const int readCol1 = (1 << colToEliminate) & 3;
	const int readCol2 = (3 >> colToEliminate) ^ 1;

	// Rank deficiency implies that the subspace is 1-dimensional.  We'll let it be the first used column normal.

	reducedS(0,0) = (F)1;
	reducedS(1,0) = S(2, readCol1);
	reducedS(0,1) = S(0, readCol1)*S(0, readCol2) + S(1, readCol1)*S(1, readCol2);	// Should be +/- 1
	reducedS(1,1) = S(2, readCol2);
}

// Reduce 4x4 to 3x3
template<typename F>
PX_INLINE void
reduce_simplex(Mat< 3, 3, F >& reducedS, const Mat< 4, 4, F >& S, int colToEliminate)
{
	// Iterate over columns of S, skipping colToEliminate
	const int readCol1 = (colToEliminate + 1) & 3;
	const int readCol2 = (colToEliminate + 2) & 3;
	const int readCol3 = (colToEliminate + 3) & 3;

	// Rank deficiency implies that the subspace is 1- or 2-dimensional.  Find the cosines between the
	// used columns to determine which is the case.
	const F cosTheta12 = S(0, readCol1)*S(0, readCol2) + S(1, readCol1)*S(1, readCol2) + S(2, readCol1)*S(2, readCol2);
	const F cosTheta23 = S(0, readCol2)*S(0, readCol3) + S(1, readCol2)*S(1, readCol3) + S(2, readCol2)*S(2, readCol3);
	const F cosTheta31 = S(0, readCol3)*S(0, readCol1) + S(1, readCol3)*S(1, readCol1) + S(2, readCol3)*S(2, readCol1);

	int basisCol0;
	int basisCol1;
	int basisCol2;
	F basis1Cosine;
	F basis2Cosine;
	if (physx::PxAbs(cosTheta12) < physx::PxAbs(cosTheta23))
	{
		if (physx::PxAbs(cosTheta12) < physx::PxAbs(cosTheta31))
		{
			// Minimum physx::PxAbs cosine is 1-2
			basisCol0 = readCol1;
			basisCol1 = readCol2;
			basisCol2 = readCol3;
			basis1Cosine = cosTheta12;
			basis2Cosine = cosTheta31;
		}
		else
		{
			// Minimum physx::PxAbs cosine is 3-1
			basisCol0 = readCol3;
			basisCol1 = readCol1;
			basisCol2 = readCol2;
			basis1Cosine = cosTheta31;
			basis2Cosine = cosTheta23;
		}
	}
	else
	{
		if (physx::PxAbs(cosTheta23) < physx::PxAbs(cosTheta31))
		{
			// Minimum physx::PxAbs cosine is 2-3
			basisCol0 = readCol2;
			basisCol1 = readCol3;
			basisCol2 = readCol1;
			basis1Cosine = cosTheta23;
			basis2Cosine = cosTheta12;
		}
		else
		{
			// Minimum physx::PxAbs cosine is 3-1
			basisCol0 = readCol3;
			basisCol1 = readCol1;
			basisCol2 = readCol2;
			basis1Cosine = cosTheta31;
			basis2Cosine = cosTheta23;
		}
	}

	Dir< 3, F > perp = Dir< 3, F >(S.getCol(basisCol1)) - basis1Cosine*Dir< 3, F >(S.getCol(basisCol0));
	const F basis1Sine = perp.normalize();
	const F basis2Sine = Dir< 3, F >(S.getCol(basisCol2)) | perp;

	reducedS(0,0) = (F)1;
	reducedS(1,0) = (F)0;
	reducedS(2,0) = S(3, basisCol0);
	reducedS(0,1) = basis1Cosine;
	reducedS(1,1) = basis1Sine;
	reducedS(2,1) = S(3, basisCol1);
	reducedS(0,2) = basis2Cosine;
	reducedS(1,2) = basis2Sine;
	reducedS(2,2) = S(3, basisCol2);
}

/*
	"cof_last_row" functions give the last row of the cofactor matrix of a D x D matrix M,
	along with the determinant of M (as the return value).
*/

// 1x1
template<typename F>
PX_INLINE F
cof_last_row(Row < 1, F > & cofMRowD, const Mat < 1, 1, F > & M)
{
	cofMRowD(0) = (F)1;
	return M(0,0);
}

// 2x2
template<typename F>
PX_INLINE F
cof_last_row(Row < 2, F > & cofMRowD, const Mat < 2, 2, F > & M)
{
	cofMRowD(0) = -M(0,1);
	cofMRowD(1) = M(0,0);
	return M(1,0)*cofMRowD(0) + M(1,1)*cofMRowD(1);
}

// 3x3
template<typename F>
PX_INLINE F
cof_last_row(Row < 3, F > & cofMRowD, const Mat < 3, 3, F > & M)
{
	cofMRowD(0) = M(0,1)*M(1,2) - M(1,1)*M(0,2);
	cofMRowD(1) = M(0,2)*M(1,0) - M(1,2)*M(0,0);
	cofMRowD(2) = M(0,0)*M(1,1) - M(1,0)*M(0,1);
	return M(2,0)*cofMRowD(0) + M(2,1)*cofMRowD(1) + M(2,2)*cofMRowD(2);
}

// 4x4
template<typename F>
PX_INLINE F
cof_last_row(Row < 4, F > & cofMRowD, const Mat < 4, 4, F > & M)
{
	cofMRowD(0) = M(0,3)*(M(1,2)*M(2,1) - M(1,1)*M(2,2)) + M(0,2)*(M(1,1)*M(2,3) - M(1,3)*M(2,1)) + M(0,1)*(M(1,3)*M(2,2) - M(1,2)*M(2,3));
	cofMRowD(1) = M(0,0)*(M(1,2)*M(2,3) - M(1,3)*M(2,2)) + M(0,2)*(M(1,3)*M(2,0) - M(1,0)*M(2,3)) + M(0,3)*(M(1,0)*M(2,2) - M(1,2)*M(2,0));
	cofMRowD(2) = M(0,3)*(M(1,1)*M(2,0) - M(1,0)*M(2,1)) + M(0,1)*(M(1,0)*M(2,3) - M(1,3)*M(2,0)) + M(0,0)*(M(1,3)*M(2,1) - M(1,1)*M(2,3));
	cofMRowD(3) = M(0,0)*(M(1,1)*M(2,2) - M(1,2)*M(2,1)) + M(0,1)*(M(1,2)*M(2,0) - M(1,0)*M(2,2)) + M(0,2)*(M(1,0)*M(2,1) - M(1,1)*M(2,0));
	return M(3,0)*cofMRowD(0) + M(3,1)*cofMRowD(1) + M(3,2)*cofMRowD(2) + M(3,3)*cofMRowD(3);
}

// Creating a class for this function since partial specialization requires it(!?)
template<int D, typename F>
struct SimplexTools
{
	PX_INLINE int
	findCondition(F& detS, Row < D + 1, F > & cofSRowD, const Mat < D + 1, D + 1, F > & S) const
	{
#if USE_OPTIMIZED_COF
		detS = cof_last_row<F>(cofSRowD, S);
#else
		for (int col = 0; col < D + 1; ++col)
		{
			cofSRowD(col) = cof(S, D, col);
		}
		detS = cofSRowD | S.getRow(D);
#endif

		if (physx::PxAbs(detS) > gsa_eps<F>())
		{
			// w_i = -det(S)/cof(S)_{D,i}
			for (int i = 0; i < D + 1; ++i)
			{
				if (-cofSRowD(i) / detS > gsa_eps<F>())	// m_recipWidths[i] = -cofSRowD(i)/m_detS; condition += (int)(m_recipWidths[i] > 0);
				{
					return D;
				}
			}
			return 0;
		}

		int dim = 1;

		// det(S) = 0.  If any of the cofactors of the plane displacements are zero, we have a degenerate subspace to test.
		int cofactorSigns = 0;	// (+/-/0) => bit(0,1,2)
		for (int i = 0; i < D + 1; ++i)
		{
			if (physx::PxAbs(cofSRowD(i)) < gsa_eps<F>())
			{
				Mat<D, D, F> reducedS;
				reduce_simplex<F>(reducedS, S, i);
				F detReducedS = 0;
				(void)detReducedS;
				Row<D, F> cofReducedSRowD;
				int sub_dim = 1;
				SimplexTools<D-1, F> st;
				if (D > 1 && (sub_dim = st.findCondition(detReducedS, cofReducedSRowD, reducedS)) == 0)
				{
					return 0;
				}
				if (sub_dim > dim)
				{
					dim = sub_dim;
				}
				cofactorSigns |= 4;
			}
			else
			{
				dim = D;
				cofactorSigns |= 1 << (int)(cofSRowD(i) < 0);
			}
		}

		if( dim == D && cofactorSigns < 3 )	// no zero cofactors found, and cofactors are all the same sign
		{
			dim = 0;
		}

		return dim;
	}
};

// Specialization for D = 1
template<typename F>
struct SimplexTools<1, F>
{
	PX_INLINE int
	findCondition(F& detS, Row < 2, F > & cofSRowD, const Mat < 2, 2, F > & S) const
	{
#if USE_OPTIMIZED_COF
		detS = cof_last_row<F>(cofSRowD, S);
#else
		for (int col = 0; col < 2; ++col)
		{
			cofSRowD(col) = cof(S, 1, col);
		}
		detS = cofSRowD | S.getRow(1);
#endif

		if (physx::PxAbs(detS) > gsa_eps<F>())
		{
			// w_i = -det(S)/cof(S)_{1,i}
			for (int i = 0; i < 2; ++i)
			{
				if (-cofSRowD(i) / detS > gsa_eps<F>())	// m_recipWidths[i] = -cofSRowD(i)/m_detS; condition += (int)(m_recipWidths[i] > 0);
				{
					return 1;
				}
			}
			return 0;
		}

		// det(S) = 0.  For D = 1, none of the cofactors are zero.  We are simply checking for cofactors that are all the same sign.
		return cofSRowD(0)*cofSRowD(1) < (F)0 ? 1 : 0;
	}
};


struct GSA_State
{
	enum Enum
	{
		Error_Flag = PX_MIN_I32,
		Error_Mask = ~Error_Flag,

		Invalid = Error_Flag,
		Maximum_GSA_Iterations_Reached,
		Maximum_TOI_Iterations_Reached,
	};
};


template<int D, typename F>
class GSA
{
public:
	struct LineIntersect
	{
		Plane<D, F>	m_plane;
		F			m_s;
	};

	class ConvexShape
	{
	public:
		virtual	unsigned	initialize_tangent_planes(Plane<D, F>* planes, unsigned plane_count) const = 0;
		virtual	void		intersect_line(LineIntersect& in, LineIntersect& ex, const Pos<D, F>& orig, const Dir<D, F>& dir, F time) const = 0;
		virtual Dir<D, F>	get_linear_velocity() const = 0;

		virtual				~ConvexShape() {}
	};

	GSA()
	{
		init();
	}

	int		state() const
	{
		return m_state;
	}

	void	init(int maxIterationCount = 1000);
	bool	set_shapes(const ConvexShape* shape1, const ConvexShape* shape2 = NULL);

	bool	intersect(F time = 0);
	bool	time_of_intersection(F& time, Dir<D, F>& normal, Pos<D, F> points[1 << (D - 1)], F timeMin = -1, F timeMax = 1);
	int		contacts(F& depth, Dir<D, F>& normal, Pos<D, F> points[1 << (D - 1)]);

protected:
	void	advance_simplex(F time);
	void	solve_simplex();
	void	create_search_line();
	bool	intersect_search_line(F time);
	void	update_simplex();

	int					m_maxIterationCount;

	int					m_state;

	const ConvexShape*	m_shape[2];

	Pos<D, F>			m_searchLineOrig;
	Dir<D, F>			m_searchLineDir;

	// Current simplex (and derived values)
	Mat < D + 1, D + 1, F >		m_S;	// Simplex matrix - note, planes are stored in columns
	Row < D + 1, F >			m_cofSRowD;
	F					m_detS;
	int					m_normalSpan;
	F					m_time;	// Time at which current simplex is captured
	Col < D + 1, u8 >			m_shapeIndices;	// The shape index for each plane (column) of m_S.
};


/* GSA methods */

template<int D, typename F>
void
GSA<D, F>::init(int maxIterationCount)
{
	m_maxIterationCount = maxIterationCount;
	m_state = GSA_State::Invalid;
	m_shape[0] = m_shape[1] = NULL;
	m_searchLineOrig.set((F)0);
	m_searchLineDir.set((F)0);
	m_searchLineDir(0) = (F)1;
	m_S = Mat < D + 1, D + 1, F > ();
	m_cofSRowD.set((F)0);
	m_detS = (F)0;
	m_normalSpan = 0;
	m_time = (F)0;
	m_shapeIndices.set(0);
}

template<int D, typename F>
bool
GSA<D, F>::set_shapes(const ConvexShape* shape1, const ConvexShape* shape2)
{
	if (shape1 == NULL)
	{
		init(m_maxIterationCount);
		return false;
	}

	m_state = 0;

	m_shape[0] = shape1;
	m_shape[1] = shape2;
	m_shapeIndices.set(0);

	Plane<D, F> planes[D + 1];
	unsigned planeCount = 0;
	unsigned colN = 0;
	if (m_shape[1] != NULL)
	{
		planeCount += m_shape[1]->initialize_tangent_planes(planes, (D + 1) / 2);
		for (; colN < planeCount; ++colN)
		{
			m_S.setCol((int)colN, planes[colN]);
			m_shapeIndices((int)colN) = 1;
		}
	}
	planeCount += m_shape[0]->initialize_tangent_planes(planes + planeCount, D + 1 - planeCount);
	for (; colN < planeCount; ++colN)
	{
		m_S.setCol((int)colN, planes[colN]);
		m_shapeIndices((int)colN) = 0;
	}
	for (; colN < D + 1; ++colN)
	{
		m_S.setCol((int)colN, m_S.getCol(0));
	}

	m_time = (F)0;

	return true;
}

template<int D, typename F>
bool
GSA<D, F>::intersect(F time)
{
	if (m_state == GSA_State::Invalid)
	{
		return false;
	}

	m_state = 0;

	// (0) Bring the cached simplex into the current time, and record current time
	advance_simplex(time);

	// (1) Initial Test for void simplex
	solve_simplex();
	if (m_normalSpan == 0)
	{
		// Void simplex
		return false;
	}

	// Iterate
	for (;;)
	{
		// (2) Create search line
		create_search_line();

		// (3) Search and test for intersection
		if (intersect_search_line(time))
		{
			// Proved intersection
			return true;
		}
		if (m_state > m_maxIterationCount)
		{
			// Iteration limit reached
			m_state = GSA_State::Maximum_GSA_Iterations_Reached;
			return false;
		}

		// (4) Test for disjointness
		solve_simplex();
		if (m_normalSpan == 0)
		{
			// Proved disjointness
			return false;
		}

		// (5) Update simplex
		update_simplex();
	}
}

template<int D, typename F>
bool
GSA<D, F>::time_of_intersection(F& time, Dir<D, F>& normal, Pos<D, F> points[1 << (D - 1)], F timeMin, F timeMax)
{
	PX_UNUSED(normal);
	PX_UNUSED(points);
	if (m_state == GSA_State::Invalid)
	{
		return false;
	}

	// Initialize time
	time = 0;

	// Determine if there is initial intersection
	bool intersecting = intersect(0);
	if (m_state < 0)
	{
		return false;	// Error
	}
	int totalIterations = m_state;

	for (int temporalIterations = 0; temporalIterations < m_maxIterationCount; ++temporalIterations)
	{
		// Displacement velocity vector
		Row < D + 1, F > displacementVelocity;
		for (int i = 0; i < D + 1; ++i)
		{
			displacementVelocity(i) = m_shape[m_shapeIndices(i)]->get_linear_velocity() | m_S.getCol(i);
		}

		// Calculate -d(|S|)/dt
		const F minusDetSDot = displacementVelocity | m_cofSRowD;

		// Calculate delta time to intersection
		F dt;
		if (physx::PxAbs(minusDetSDot) > gsa_eps<F>())
		{
			dt = m_detS / minusDetSDot;	// DIVIDE
		}
		else
		{
			// minusDetSDot = 0.  If m_detS = 0 as well, the time to intersection is indeterminant.  Use L'Hopital's rule.
			bool dtFound = false;
			if (physx::PxAbs(m_detS) < gsa_eps<F>())
			{
				// Find the row k and column l of the greatest magnitude cofactor C_kl of the normal components of S.
				// This is d(|S|)/d(S_kl), and will be the numerator of dt.
				int k = 0;
				int l = 0;
				F C_kl = (F)0;
				for (int i = 0; i < D; ++i)
				{
					for (int j = 0; j < D + 1; ++j)
					{
						const F c = cof(m_S, i, j);
						if (physx::PxAbs(c) >= physx::PxAbs(C_kl))
						{
							C_kl = c;
							k = i;
							l = j;
						}
					}
				}

				// The denominator of dt is d(minusDetSDot)/d(S_kl) = displacementVelocity | d(m_cofSRowD)/d(S_kl).
				F d_minusDetSDot_d_S_kl = (F)0;
				for (int j = 0; j < D + 1; ++j)
				{
					if (j != l)
					{
						const Mat<D, D, F> M = minor(m_S, D, j);		// Optimization opportunity: this minor has already been calculated when finding m_cofSRowD
						const F sign = (F)((int)1 - (int)(((D + j) & 1) << 1));	// ... and so has this sign
						d_minusDetSDot_d_S_kl += displacementVelocity(j) * sign * cof(M, k, (j > l) ? l : (l - 1));
					}
				}

				if (physx::PxAbs(d_minusDetSDot_d_S_kl) > gsa_eps<F>())
				{
					dtFound = true;
					dt = C_kl / d_minusDetSDot_d_S_kl;
				}
			}

			if (!dtFound)
			{
				// Still indeterminant or undefined.
				time = intersecting ? timeMin : timeMax;	// if already intersecting, no known negative TOI, otherwise there is no TOI
				return intersecting;
			}
		}

		if (!intersecting && dt <= gsa_eps<F>()*physx::PxAbs(time))
		{
			// Widths are non-positive.  If the time derivatives of the widths are non-positive, there will never be an intersection.
			// d(w_i)/dt = minusDetSDot/minusDetSDot[i].  This has the same sign as minusDetSDot*minusDetSDot[i].
			for (int i = 0; i < D + 1; ++i)
			{
				if (minusDetSDot * m_cofSRowD(i) <= 0)
				{
					time = timeMax;
					return false;	// No TOI.
				}
			}
			return true;	// Intersection found at current TOI
		}

		if (intersecting && dt >= 0)
		{
			time = timeMin;
			return true;	// Already intersecting, no known negative TOI
		}

		// Increment TOI
		time += dt;

		// Clamp time to given range
		if (time < timeMin)
		{
			time = timeMin;
			return false;
		}

		if (time > timeMax)
		{
			time = timeMax;
			return false;
		}

		// Ideally we have moved the shapes out of intersection.  If there is intersection, it should be barely touching within roundoff error, meaning we have found the TOI.
		intersecting = intersect(time);
		if (m_state < 0)
		{
			return false;	// Error
		}
		totalIterations += m_state;
		m_state = totalIterations;
		if (intersecting)
		{
			return true;	// Found TOI
		}
		if (totalIterations > m_maxIterationCount)
		{
			// GSA iteration limit reached
			m_state = GSA_State::Maximum_GSA_Iterations_Reached;
			return false;
		}
	}

	// Temporal iteration limit reached
	m_state = GSA_State::Maximum_TOI_Iterations_Reached;
	return false;
}

template<int D, typename F>
int
GSA<D, F>::contacts(F& depth, Dir<D, F>& normal, Pos<D, F> points[1 << (D - 1)])
{
	if ((m_state & GSA_State::Error_Flag) != 0)
	{
		return 0;
	}

	int pointCount = 0;

	switch (D)
	{
	case 1: // 1D - Trivial case
		depth = -m_detS / m_cofSRowD(0);	// = m_recipWidths[ m_simplex[0] ];
		normal = Plane<D, F>(m_S.getCol(0)).n();
		points[pointCount++] = Plane<D, F>(m_S.getCol(1)).project(Pos<D, F>());
		break;
	case 2: // 2D
	{
		u8 planeSort[D + 1];
		u32 partitions[2] = {0, D};
		for (u32 i = 0; i < D + 1; ++i)
		{
			const i32 shapeIndex = (i32)m_shapeIndices(i);
			u32& partition = partitions[shapeIndex];
			planeSort[partition] = i;
			partition += 1 - (shapeIndex << 1);
		}
		u8 normalCol;
		switch (partitions[0])
		{
		case 1:
			normalCol = planeSort[0];
			break;
		case 2:
			normalCol = planeSort[2];
			break;
		default:
			return 0;
		}
		if (physx::PxAbs(m_cofSRowD(normalCol)) < eps_real<F>())
		{
			return 0;
		}
		const F recipW = -(F)1 / m_cofSRowD(normalCol);
		depth = recipW * m_detS;	// = m_recipWidths[ m_simplex[0] ];
		normal = m_S.getCol(normalCol);
		points[pointCount++] << -cof(m_S, 0, normalCol)*recipW, -cof(m_S, 1, normalCol)*recipW, (F)1;
	}
	break;
	case 3: // 3D
		break;
	default: // D > 3 not handled
		break;
	}

	return pointCount;
}

template<int D, typename F>
void
GSA<D, F>::advance_simplex(F time)
{
	const F dt = time - m_time;
	for (int i = 0; i < D + 1; ++i)
	{
		Plane<D, F> plane = m_S.getCol(i);
		plane(D) -= dt * (plane.n() | m_shape[m_shapeIndices(i)]->get_linear_velocity());
		m_S.setCol(i, plane);
	}
	m_time = time;
}

template<int D, typename F>
void
GSA<D, F>::solve_simplex()
{
	SimplexTools<D, F> st;
	m_normalSpan = st.findCondition(m_detS, m_cofSRowD, m_S);
}

template<int D, typename F>
void
GSA<D, F>::create_search_line()
{
	switch (D)
	{
	case 1: // 1D - Trivial case
		m_searchLineOrig.set((F)0);
		m_searchLineDir.set((F)1);
		break;
	case 2: // 2D - The search line is m_simplex[0]
	{
		const Plane<D, F> plane0 = Plane<D, F>(m_S.getCol(0));
		m_searchLineDir = plane0.n();
		physx::swap(m_searchLineDir(0), m_searchLineDir(1));
		m_searchLineDir(0) = -m_searchLineDir(0);
		m_searchLineOrig = plane0.project(Pos<D, F>());
	}
	break;
	case 3: // 3D - The search line is the intersection of m_simplex[0] and m_simplex[1]
		switch (m_normalSpan)
		{
		case 1:
		{
			const Plane<D, F> plane0 = Plane<D, F>(m_S.getCol(0));
			m_searchLineOrig = plane0.project(Pos<D, F>());
			m_searchLineDir = plane0.n();
		}
		break;
		case 2:
		{
			const Plane<D, F> plane0 = Plane<D, F>(m_S.getCol(0));
			Plane<D, F> planeResolve = Plane<D, F>(m_S.getCol(1));
			Dir<D, F> planeResolveDir = plane0.n() ^ planeResolve.n();
			F planeResolveDir2 = planeResolveDir.lengthSquared();
			for (int i = 2; i < D + 1; ++i)
			{
				const Plane<D, F> planeI = Plane<D, F>(m_S.getCol(i));
				const Dir<D, F> dir = plane0.n() ^ planeI.n();
				const F dir2 = dir.lengthSquared();
				if (dir2 > planeResolveDir2)
				{
					planeResolve = planeI;
					planeResolveDir = dir;
					planeResolveDir2 = dir2;
				}
			}
			if (planeResolveDir2 > square(gsa_eps<F>()))
			{
				m_searchLineOrig = Pos<D, F>() - (plane0.d() * (planeResolve.n() ^ planeResolveDir) + planeResolve.d() * (planeResolveDir ^ plane0.n())) / planeResolveDir2;	// DIVIDE
				m_searchLineDir = (plane0.n() ^ planeResolveDir) / sqrt(planeResolveDir2);	// DIVIDE
			}
			else
			{
				m_searchLineOrig = plane0.project(Pos<D, F>());
				m_searchLineDir = plane0.n();
			}
		}
		break;
		case 3:
		{
			const Plane<D, F> plane0 = Plane<D, F>(m_S.getCol(0));
			const Plane<D, F> plane1 = Plane<D, F>(m_S.getCol(1));
			Dir<D, F> dir = plane0.n() ^ plane1.n();
			const F dir2 = dir.lengthSquared();
			if (dir.lengthSquared() > square(gsa_eps<F>()))
			{
				m_searchLineOrig = Pos<D, F>() - (plane0.d() * (plane1.n() ^ dir) + plane1.d() * (dir ^ plane0.n())) / dir2;	// DIVIDE
				m_searchLineDir = dir / sqrt(dir2);	// DIVIDE
			}
			else
			{
				// Set line to an arbitrary line in one of the planes (using plane0)
				int minIndex = 0;
				for (int i = 1; i < D; ++i)
				{
					if (physx::PxAbs(plane0(i)) < physx::PxAbs(plane0(minIndex)))
					{
						minIndex = i;
					}
				}
				Dir<D, F> x;
				x(minIndex) = (F)1;
				m_searchLineDir = plane0.n() ^ x;
				m_searchLineDir.normalize();
				m_searchLineOrig = plane0.project(m_searchLineOrig);
			}
		}
		break;
		}
		break;
	default: // D > 3 not handled
		break;
	}
}

template<int D, typename F>
bool
GSA<D, F>::intersect_search_line(F time)
{
	LineIntersect in, ex;
	u8 inShapeIndex = 0;
	u8 exShapeIndex = 0;

	for (int attemptN = 0; attemptN < 2; ++attemptN)
	{
		m_shape[0]->intersect_line(in, ex, m_searchLineOrig, m_searchLineDir, time);
		inShapeIndex = 0;
		exShapeIndex = 0;
		if (m_shape[1] != NULL)
		{
			LineIntersect in1, ex1;
			m_shape[1]->intersect_line(in1, ex1, m_searchLineOrig, m_searchLineDir, time);
			if (in1.m_s > in.m_s)
			{
				in = in1;
				inShapeIndex = 1;
			}
			if (ex1.m_s < ex.m_s)
			{
				ex = ex1;
				exShapeIndex = 1;
			}
		}

		// Update GSA iteration count
		++m_state;

		if (in.m_s - ex.m_s <= 10 * gsa_eps<F>())
		{
			// Intersection
			return true;
		}

		if (attemptN == 1)
		{
			break;
		}

		// Test for fixed point
		if ((Dir < D + 1, F > (ex.m_plane - in.m_plane).lengthSquared() > square(gsa_eps<F>()) && Dir < D + 1, F > (ex.m_plane + in.m_plane).lengthSquared() > square(gsa_eps<F>())) ||
		        physx::PxAbs(m_searchLineDir | ex.m_plane) > gsa_eps<F>() || physx::PxAbs(m_searchLineOrig | ex.m_plane) > gsa_eps<F>())
		{
			// Not a fixed point
			break;
		}

		// Need to reset line
		m_searchLineOrig += m_searchLineDir * ((F)0.5 * (in.m_s + ex.m_s));
		m_searchLineDir = ex.m_plane.n();
	}

	// Put results in last two columns of the simplex matrix
	m_S.setCol(D - 1, in.m_plane);
	m_shapeIndices(D - 1) = inShapeIndex;
	m_S.setCol(D, ex.m_plane);
	m_shapeIndices(D) = exShapeIndex;

	return false;
}

template<int D, typename F>
void
GSA<D, F>::update_simplex()
{
	switch (D)
	{
	case 1: // 1D - Trivial case
		break;
	case 2: // 2D - Use line with normal that most opposes current line
	{
		const int col = (Plane<D, F>(m_S.getCol(0)).n() | m_S.getCol(1)) < (Plane<D, F>(m_S.getCol(0)).n() | m_S.getCol(2)) ? 1 : 2;
		m_S.setCol(0, m_S.getCol(col));
		m_shapeIndices(0) = m_shapeIndices(col);
	}
	break;
	case 3: // 3D - Use 2D GSA in plane of m_simplex[0]
	{
		if (m_cofSRowD(1)*m_detS >= (F)0)	// if( m_recipWidths[1] <= (F)0 )
		{
			// 2D GSA void simplex (but not 3D void simplex).  Change m_simplex[0]
			m_S.setCol(0, m_S.getCol(1));
			m_shapeIndices(0) = m_shapeIndices(1);
		}
		const int col = (Plane<D, F>(m_S.getCol(1)).n() | m_S.getCol(2)) < (Plane<D, F>(m_S.getCol(1)).n() | m_S.getCol(3)) ? 2 : 3;
		m_S.setCol(1, m_S.getCol(col));
		m_shapeIndices(1) = m_shapeIndices(col);
	}
	break;
	default: // D > 3 not handled
		break;
	}
}


/*
	Utility class derived from GSA::ConvexShape, to handle common implementations

	PlaneIterator must have:
		1) a constructor which takes an object of type IteratorInitValues (either by value or refrence) in its constructor,
		2) a valid() method which returns a bool (true iff the plane() function can return a valid plane, see below),
		3) an inc() method to advance to the next plane, and
		4) a plane() method which returns a plane of type ApexCSG::GSA::Plane<3, float>, either by value or reference (the plane will be copied).
*/
template<class PlaneIterator, class IteratorInitValues, typename F>
class StaticConvexPolyhedron : public ApexCSG::GSA::GSA<3, F>::ConvexShape
{
public:
	// Not overloaded in this class, still virtual
	virtual	unsigned	initialize_tangent_planes(ApexCSG::GSA::Plane<3, F>* planes, unsigned plane_count) const
	{
		unsigned nPlanes = 0;

		for (PlaneIterator it(m_initValues); it.valid() && nPlanes < plane_count; it.inc())
		{
			planes[nPlanes++] = it.plane();
		}

		return nPlanes;
	}

	virtual	void intersect_line(typename ApexCSG::GSA::GSA<3, F>::LineIntersect& in, typename ApexCSG::GSA::GSA<3, F>::LineIntersect& ex, const ApexCSG::GSA::Pos<3, F>& orig, const ApexCSG::GSA::Dir<3, F>& dir, F time) const
	{
		(void)time;	// Not used
		F in_num = -(F)1;
		F in_den = (F)0;
		F ex_num = (F)1;
		F ex_den = (F)0;
		in.m_plane.set(ApexCSG::GSA::Dir<3, F>(), (F)0);
		ex.m_plane.set(ApexCSG::GSA::Dir<3, F>(), (F)0);

		for (PlaneIterator it(m_initValues); it.valid(); it.inc())
		{
			const ApexCSG::GSA::Plane<3, F> plane = it.plane();
			const F num = -(plane | orig);
			const F den = (plane | dir);
			if (den > ApexCSG::GSA::gsa_eps<F>())
			{
				// Exit
				if (num * ex_den < ex_num * den)	// num/den < ex_num/ex_den
				{
					ex_num = num;
					ex_den = den;
					ex.m_plane = plane;
				}
			}
			else if (den < -ApexCSG::GSA::gsa_eps<F>())
			{
				// Entrance
				if (num * in_den < in_num * den)	// num/den > in_num/in_den
				{
					in_num = -num;
					in_den = -den;
					in.m_plane = plane;
				}
			}
			else if (num < ApexCSG::GSA::gsa_eps<F>())
			{
				// Parallel and coincident or outside
				if (in_den == (F)0)
				{
					in.m_plane = plane;
				}
				if (ex_den == (F)0)
				{
					ex.m_plane = plane;
				}
				if (num < -ApexCSG::GSA::gsa_eps<F>())
				{
					// Outside
					if (ex_den > ApexCSG::GSA::eps_real<F>() || num < ex_num)
					{
						in_num = -num;
						ex_num = num;
						in_den = ex_den = ApexCSG::GSA::eps_real<F>();
						in.m_plane = ex.m_plane = plane;
					}
				}
			}
		}

		in.m_s = in_den > (F)0 ? in_num / in_den : -ApexCSG::GSA::max_real<F>();
		ex.m_s = ex_den > (F)0 ? ex_num / ex_den : ApexCSG::GSA::max_real<F>();
	}

	// Returns a zero vector, thus the "Static" prefix in this class name
	virtual ApexCSG::GSA::Dir<3, F>	get_linear_velocity() const
	{
		return ApexCSG::GSA::Dir<3, F>();
	}

	void	setPlaneInitValues(const IteratorInitValues& initValues)
	{
		m_initValues = initValues;
	}

protected:
	IteratorInitValues	m_initValues;
};

};	// namespace GSA
};	// namespace ApexCSG

#endif

#endif // #ifndef APEX_GSA_H
