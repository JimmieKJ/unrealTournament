/* Copyright (c) <2003-2011> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "dgGoogol.h"
#include "dgIntersections.h"

#pragma warning(disable:4244)

#define USE_FLOAT_VERSION

//#define DG_RAY_TOL_ERROR (hacd::HaF32 (-1.0e-5f))
#define DG_RAY_TOL_ERROR (hacd::HaF32 (-1.0e-3f))
//#define DG_RAY_TOL_ERROR (hacd::HaF32 (-1.0e-2f))
//#define DG_RAY_TOL_ERROR (hacd::HaF32 (-1.0e-1f))

dgFastRayTest::dgFastRayTest(const dgVector& l0, const dgVector& l1)
	:m_p0 (l0), m_p1(l1), m_diff (l1 - l0)
	,m_minT(hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f))  
	,m_maxT(hacd::HaF32 (1.0f), hacd::HaF32 (1.0f), hacd::HaF32 (1.0f), hacd::HaF32 (1.0f))
	,m_tolerance (DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR)
	,m_zero(hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f)) 
{
	m_diff.m_w = hacd::HaF32 (0.0f);
	m_isParallel[0] = (dgAbsf (m_diff.m_x) > hacd::HaF32 (1.0e-8f)) ? 0 : hacd::HaI32 (0xffffffff); 
	m_isParallel[1] = (dgAbsf (m_diff.m_y) > hacd::HaF32 (1.0e-8f)) ? 0 : hacd::HaI32 (0xffffffff); 
	m_isParallel[2] = (dgAbsf (m_diff.m_z) > hacd::HaF32 (1.0e-8f)) ? 0 : hacd::HaI32 (0xffffffff); 
	m_isParallel[3] = 0;

	m_dpInv.m_x = (!m_isParallel[0]) ? (hacd::HaF32 (1.0f) / m_diff.m_x) : hacd::HaF32 (1.0e20f);
	m_dpInv.m_y = (!m_isParallel[1]) ? (hacd::HaF32 (1.0f) / m_diff.m_y) : hacd::HaF32 (1.0e20f);
	m_dpInv.m_z = (!m_isParallel[2]) ? (hacd::HaF32 (1.0f) / m_diff.m_z) : hacd::HaF32 (1.0e20f);
	m_dpInv.m_w = hacd::HaF32 (0.0f);
	m_dpBaseInv = m_dpInv;

	m_dirError = -hacd::HaF32 (0.0175f) * dgSqrt (m_diff % m_diff);

//	tollerance = simd_set (DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR);
//	m_tolerance = dgVector (DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, DG_RAY_TOL_ERROR, hacd::HaF32 (0.0f));

}

hacd::HaI32 dgFastRayTest::BoxTest (const dgVector& minBox, const dgVector& maxBox) const
{
	hacd::HaF32 tmin = 0.0f;          
	hacd::HaF32 tmax = 1.0f;

	for (hacd::HaI32 i = 0; i < 3; i++) {
		if (m_isParallel[i]) {
			if (m_p0[i] <= minBox[i] || m_p0[i] >= maxBox[i]) {
				return 0;
			}
		} else {
			hacd::HaF32 t1 = (minBox[i] - m_p0[i]) * m_dpInv[i];
			hacd::HaF32 t2 = (maxBox[i] - m_p0[i]) * m_dpInv[i];

			if (t1 > t2) {
				Swap(t1, t2);
			}
			if (t1 > tmin) {
				tmin = t1;
			}
			if (t2 < tmax) {
				tmax = t2;
			}
			if (tmin > tmax) {
				return 0;
			}
		}
	}
	return 0x1;
}


hacd::HaF32 dgFastRayTest::PolygonIntersect (const dgVector& normal, const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes, const hacd::HaI32* const indexArray, hacd::HaI32 indexCount) const
{
	HACD_ASSERT (m_p0.m_w == m_p1.m_w);


	#ifndef __USE_DOUBLE_PRECISION__
	hacd::HaF32 unrealible = hacd::HaF32 (1.0e10f);
	#endif

	hacd::HaF32 dist = normal % m_diff;
	if (dist < m_dirError) {

		hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF32));

		dgVector v0 (&polygon[indexArray[indexCount - 1] * stride]);
		dgVector p0v0 (v0 - m_p0);
		hacd::HaF32 tOut = normal % p0v0;
		// this only work for convex polygons and for single side faces 
		// walk the polygon around the edges and calculate the volume 
		if ((tOut < hacd::HaF32 (0.0f)) && (tOut > dist)) {
			for (hacd::HaI32 i = 0; i < indexCount; i ++) {
				hacd::HaI32 i2 = indexArray[i] * stride;
				dgVector v1 (&polygon[i2]);
				dgVector p0v1 (v1 - m_p0);
				// calculate the volume formed by the line and the edge of the polygon
				hacd::HaF32 alpha = (m_diff * p0v1) % p0v0;
				// if a least one volume is negative it mean the line cross the polygon outside this edge and do not hit the face
				if (alpha < DG_RAY_TOL_ERROR) {
					#ifdef __USE_DOUBLE_PRECISION__
						return 1.2f;
					#else
						unrealible = alpha;
						break;
					#endif
				}
				p0v0 = p0v1;
			}

			#ifndef __USE_DOUBLE_PRECISION__ 
				if ((unrealible  < hacd::HaF32 (0.0f)) && (unrealible > (DG_RAY_TOL_ERROR * hacd::HaF32 (10.0f)))) {
					// the edge is too close to an edge float is not reliable, do the calculation with double
					dgBigVector v0_ (v0);
					dgBigVector m_p0_ (m_p0);
					//dgBigVector m_p1_ (m_p1);
					dgBigVector p0v0_ (v0_ - m_p0_);
					dgBigVector normal_ (normal);
					dgBigVector diff_ (m_diff);
					hacd::HaF64 tOut_ = normal_ % p0v0_;
					//hacd::HaF64 dist_ = normal_ % diff_;
					if ((tOut < hacd::HaF64 (0.0f)) && (tOut > dist)) {
						for (hacd::HaI32 i = 0; i < indexCount; i ++) {
							hacd::HaI32 i2 = indexArray[i] * stride;
							dgBigVector v1 (&polygon[i2]);
							dgBigVector p0v1_ (v1 - m_p0_);
							// calculate the volume formed by the line and the edge of the polygon
							hacd::HaF64 alpha = (diff_ * p0v1_) % p0v0_;
							// if a least one volume is negative it mean the line cross the polygon outside this edge and do not hit the face
							if (alpha < DG_RAY_TOL_ERROR) {
								return 1.2f;
							}
							p0v0_ = p0v1_;
						}

						tOut = hacd::HaF32 (tOut_);
					}
				}
			#endif

			//the line is to the left of all the polygon edges, 
			//then the intersection is the point we the line intersect the plane of the polygon
			tOut = tOut / dist;
			HACD_ASSERT (tOut >= hacd::HaF32 (0.0f));
			HACD_ASSERT (tOut <= hacd::HaF32 (1.0f));
			return tOut;
		}
	}
	return hacd::HaF32 (1.2f);

}




bool  dgRayBoxClip (dgVector& p0, dgVector& p1, const dgVector& boxP0, const dgVector& boxP1) 
{	
	for (int i = 0; i < 3; i ++) {
		hacd::HaF32 tmp0;
		hacd::HaF32 tmp1;

		tmp0 = boxP1[i] - p0[i];
		if (tmp0 > hacd::HaF32 (0.0f)) {
			tmp1 = boxP1[i] - p1[i];
			if (tmp1 < hacd::HaF32 (0.0f)) {
				p1 = p0 + (p1 - p0).Scale (tmp0 / (p1[i] - p0[i])); 
				p1[i] = boxP1[i];
			}
		} else {
			tmp1 = boxP1[i] - p1[i];
			if (tmp1 > hacd::HaF32 (0.0f)) {
				p0 += (p1 - p0).Scale (tmp0 / (p1[i] - p0[i])); 
				p0[i] = boxP1[i];
			} else {
				return false;
			}
		}

		tmp0 = boxP0[i] - p0[i];
		if (tmp0 < hacd::HaF32 (0.0f)) {
			tmp1 = boxP0[i] - p1[i];
			if (tmp1 > hacd::HaF32 (0.0f)) {
				p1 = p0 + (p1 - p0).Scale (tmp0 / (p1[i] - p0[i])); 
				p1[i] = boxP0[i];
			}
		} else {
			tmp1 = boxP0[i] - p1[i];
			if (tmp1 < hacd::HaF32 (0.0f)) {
				p0 += (p1 - p0).Scale (tmp0 / (p1[i] - p0[i])); 
				p0[i] = boxP0[i];
			} else {
				return false;
			}
		}
	}
	return true;
}


dgVector  dgPointToRayDistance (const dgVector& point, const dgVector& ray_p0, const dgVector& ray_p1)
{
	hacd::HaF32 t;
	dgVector dp (ray_p1 - ray_p0);
	t = ClampValue (((point - ray_p0) % dp) / (dp % dp), hacd::HaF32(hacd::HaF32 (0.0f)), hacd::HaF32(hacd::HaF32 (1.0f)));
	return ray_p0 + dp.Scale (t);
}

void  dgRayToRayDistance (const dgVector& ray_p0, const dgVector& ray_p1, const dgVector& ray_q0, const dgVector& ray_q1, dgVector& pOut, dgVector& qOut)
{
	hacd::HaF32 sN;
	hacd::HaF32 tN;

	dgVector u (ray_p1 - ray_p0);
	dgVector v (ray_q1 - ray_q0);
	dgVector w (ray_p0 - ray_q0);

	hacd::HaF32 a = u % u;        // always >= 0
	hacd::HaF32 b = u % v;
	hacd::HaF32 c = v % v;        // always >= 0
	hacd::HaF32 d = u % w;
	hacd::HaF32 e = v % w;
	hacd::HaF32 D = a*c - b*b;   // always >= 0
	hacd::HaF32 sD = D;			// sc = sN / sD, default sD = D >= 0
	hacd::HaF32 tD = D;			// tc = tN / tD, default tD = D >= 0

	// compute the line parameters of the two closest points
	if (D < hacd::HaF32 (1.0e-8f)) { // the lines are almost parallel
		sN = hacd::HaF32 (0.0f);        // force using point P0 on segment S1
		sD = hacd::HaF32 (1.0f);        // to prevent possible division by 0.0 later
		tN = e;
		tD = c;
	} else {                // get the closest points on the infinite lines
		sN = (b*e - c*d);
		tN = (a*e - b*d);
		if (sN < hacd::HaF32 (0.0f)) {       // sc < 0 => the s=0 edge is visible
			sN = hacd::HaF32 (0.0f);
			tN = e;
			tD = c;
		}
		else if (sN > sD) {  // sc > 1 => the s=1 edge is visible
			sN = sD;
			tN = e + b;
			tD = c;
		}
	}


	if (tN < hacd::HaF32 (0.0f)) {           // tc < 0 => the t=0 edge is visible
		tN = hacd::HaF32 (0.0f);
		// recompute sc for this edge
		if (-d < hacd::HaF32 (0.0f))
			sN = hacd::HaF32 (0.0f);
		else if (-d > a)
			sN = sD;
		else {
			sN = -d;
			sD = a;
		}
	}
	else if (tN > tD) {      // tc > 1 => the t=1 edge is visible
		tN = tD;
		// recompute sc for this edge
		if ((-d + b) < hacd::HaF32 (0.0f))
			sN = hacd::HaF32 (0.0f);
		else if ((-d + b) > a)
			sN = sD;
		else {
			sN = (-d + b);
			sD = a;
		}
	}

	// finally do the division to get sc and tc
	hacd::HaF32 sc = (dgAbsf(sN) < hacd::HaF32(1.0e-8f) ? hacd::HaF32 (0.0f) : sN / sD);
	hacd::HaF32 tc = (dgAbsf(tN) < hacd::HaF32(1.0e-8f) ? hacd::HaF32 (0.0f) : tN / tD);

	pOut = ray_p0 + u.Scale (sc);
	qOut = ray_q0 + v.Scale (tc);
}




dgVector dgPointToTriangleDistance (const dgVector& point, const dgVector& p0, const dgVector& p1, const dgVector& p2)
{
	//	const dgVector p (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f));
	const dgVector p10 (p1 - p0);
	const dgVector p20 (p2 - p0);
	const dgVector p_p0 (point - p0);

	hacd::HaF32 alpha1 = p10 % p_p0;
	hacd::HaF32 alpha2 = p20 % p_p0;
	if ((alpha1 <= hacd::HaF32 (0.0f)) && (alpha2 <= hacd::HaF32 (0.0f))) {
		return p0;
	}

	dgVector p_p1 (point - p1);
	hacd::HaF32 alpha3 = p10 % p_p1;
	hacd::HaF32 alpha4 = p20 % p_p1;
	if ((alpha3 >= hacd::HaF32 (0.0f)) && (alpha4 <= alpha3)) {
		return p1;
	}

	hacd::HaF32 vc = alpha1 * alpha4 - alpha3 * alpha2;
	if ((vc <= hacd::HaF32 (0.0f)) && (alpha1 >= hacd::HaF32 (0.0f)) && (alpha3 <= hacd::HaF32 (0.0f))) {
		hacd::HaF32 t = alpha1 / (alpha1 - alpha3);
		HACD_ASSERT (t >= hacd::HaF32 (0.0f));
		HACD_ASSERT (t <= hacd::HaF32 (1.0f));
		return p0 + p10.Scale (t);
	}


	dgVector p_p2 (point - p2);
	hacd::HaF32 alpha5 = p10 % p_p2;
	hacd::HaF32 alpha6 = p20 % p_p2;
	if ((alpha6 >= hacd::HaF32 (0.0f)) && (alpha5 <= alpha6)) {
		return p2;
	}


	hacd::HaF32 vb = alpha5 * alpha2 - alpha1 * alpha6;
	if ((vb <= hacd::HaF32 (0.0f)) && (alpha2 >= hacd::HaF32 (0.0f)) && (alpha6 <= hacd::HaF32 (0.0f))) {
		hacd::HaF32 t = alpha2 / (alpha2 - alpha6);
		HACD_ASSERT (t >= hacd::HaF32 (0.0f));
		HACD_ASSERT (t <= hacd::HaF32 (1.0f));
		return p0 + p20.Scale (t);
	}


	hacd::HaF32 va = alpha3 * alpha6 - alpha5 * alpha4;
	if ((va <= hacd::HaF32 (0.0f)) && ((alpha4 - alpha3) >= hacd::HaF32 (0.0f)) && ((alpha5 - alpha6) >= hacd::HaF32 (0.0f))) {
		hacd::HaF32 t = (alpha4 - alpha3) / ((alpha4 - alpha3) + (alpha5 - alpha6));
		HACD_ASSERT (t >= hacd::HaF32 (0.0f));
		HACD_ASSERT (t <= hacd::HaF32 (1.0f));
		return p1 + (p2 - p1).Scale (t);
	}

	hacd::HaF32 den = float(hacd::HaF32 (1.0f)) / (va + vb + vc);
	hacd::HaF32 t = vb * den;
	hacd::HaF32 s = vc * den;
	HACD_ASSERT (t >= hacd::HaF32 (0.0f));
	HACD_ASSERT (s >= hacd::HaF32 (0.0f));
	HACD_ASSERT (t <= hacd::HaF32 (1.0f));
	HACD_ASSERT (s <= hacd::HaF32 (1.0f));
	return p0 + p10.Scale (t) + p20.Scale (s);
}

dgBigVector dgPointToTriangleDistance (const dgBigVector& point, const dgBigVector& p0, const dgBigVector& p1, const dgBigVector& p2)
{
	//	const dgBigVector p (hacd::HaF64 (0.0f), hacd::HaF64 (0.0f), hacd::HaF64 (0.0f));
	const dgBigVector p10 (p1 - p0);
	const dgBigVector p20 (p2 - p0);
	const dgBigVector p_p0 (point - p0);

	hacd::HaF64 alpha1 = p10 % p_p0;
	hacd::HaF64 alpha2 = p20 % p_p0;
	if ((alpha1 <= hacd::HaF64 (0.0f)) && (alpha2 <= hacd::HaF64 (0.0f))) {
		return p0;
	}

	dgBigVector p_p1 (point - p1);
	hacd::HaF64 alpha3 = p10 % p_p1;
	hacd::HaF64 alpha4 = p20 % p_p1;
	if ((alpha3 >= hacd::HaF64 (0.0f)) && (alpha4 <= alpha3)) {
		return p1;
	}

	hacd::HaF64 vc = alpha1 * alpha4 - alpha3 * alpha2;
	if ((vc <= hacd::HaF64 (0.0f)) && (alpha1 >= hacd::HaF64 (0.0f)) && (alpha3 <= hacd::HaF64 (0.0f))) {
		hacd::HaF64 t = alpha1 / (alpha1 - alpha3);
		HACD_ASSERT (t >= hacd::HaF64 (0.0f));
		HACD_ASSERT (t <= hacd::HaF64 (1.0f));
		return p0 + p10.Scale (t);
	}


	dgBigVector p_p2 (point - p2);
	hacd::HaF64 alpha5 = p10 % p_p2;
	hacd::HaF64 alpha6 = p20 % p_p2;
	if ((alpha6 >= hacd::HaF64 (0.0f)) && (alpha5 <= alpha6)) {
		return p2;
	}


	hacd::HaF64 vb = alpha5 * alpha2 - alpha1 * alpha6;
	if ((vb <= hacd::HaF64 (0.0f)) && (alpha2 >= hacd::HaF64 (0.0f)) && (alpha6 <= hacd::HaF64 (0.0f))) {
		hacd::HaF64 t = alpha2 / (alpha2 - alpha6);
		HACD_ASSERT (t >= hacd::HaF64 (0.0f));
		HACD_ASSERT (t <= hacd::HaF64 (1.0f));
		return p0 + p20.Scale (t);
	}


	hacd::HaF64 va = alpha3 * alpha6 - alpha5 * alpha4;
	if ((va <= hacd::HaF64 (0.0f)) && ((alpha4 - alpha3) >= hacd::HaF64 (0.0f)) && ((alpha5 - alpha6) >= hacd::HaF64 (0.0f))) {
		hacd::HaF64 t = (alpha4 - alpha3) / ((alpha4 - alpha3) + (alpha5 - alpha6));
		HACD_ASSERT (t >= hacd::HaF64 (0.0f));
		HACD_ASSERT (t <= hacd::HaF64 (1.0f));
		return p1 + (p2 - p1).Scale (t);
	}

	hacd::HaF64 den = float(hacd::HaF64 (1.0f)) / (va + vb + vc);
	hacd::HaF64 t = vb * den;
	hacd::HaF64 s = vc * den;
	HACD_ASSERT (t >= hacd::HaF64 (0.0f));
	HACD_ASSERT (s >= hacd::HaF64 (0.0f));
	HACD_ASSERT (t <= hacd::HaF64 (1.0f));
	HACD_ASSERT (s <= hacd::HaF64 (1.0f));
	return p0 + p10.Scale (t) + p20.Scale (s);
}


bool  dgPointToPolygonDistance (const dgVector& p, const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes,
									 const hacd::HaI32* const indexArray, hacd::HaI32 indexCount, hacd::HaF32 bailDistance, dgVector& out)
{
	HACD_ALWAYS_ASSERT();
	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF32));

	hacd::HaI32 i0 = indexArray[0] * stride;
	hacd::HaI32 i1 = indexArray[1] * stride;

	const dgVector v0 (&polygon[i0]);
	dgVector v1 (&polygon[i1]);
	dgVector closestPoint (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f));
	hacd::HaF32 minDist = hacd::HaF32 (1.0e20f);
	for (hacd::HaI32 i = 2; i < indexCount; i ++) {
		hacd::HaI32 i2 = indexArray[i] * stride;
		const dgVector v2 (&polygon[i2]);
		const dgVector q (dgPointToTriangleDistance (p, v0, v1, v2));
		const dgVector error (q - p);
		hacd::HaF32 dist = error % error;
		if (dist < minDist) {
			minDist = dist;
			closestPoint = q;
		}
		v1 = v2;
	}

	if (minDist > (bailDistance * bailDistance)) {
		return false;
	}

	out = closestPoint;
	return true;
}



dgBigVector LineTriangleIntersection (const dgBigVector& p0, const dgBigVector& p1, const dgBigVector& A, const dgBigVector& B, const dgBigVector& C)
{
	dgHugeVector ph0 (p0);
	dgHugeVector ph1 (p1);
	dgHugeVector Ah (A);
	dgHugeVector Bh (B);
	dgHugeVector Ch (C);

	dgHugeVector p1p0 (ph1 - ph0);
	dgHugeVector Ap0 (Ah - ph0);
	dgHugeVector Bp0 (Bh - ph0);
	dgHugeVector Cp0 (Ch - ph0);

	dgGoogol t0 ((Bp0 * Cp0) % p1p0);
	//hacd::HaF64 val0 = t0.GetAproximateValue();	
	//if (val0 < hacd::HaF64 (0.0f)) {
	if (hacd::HaF64(t0) < hacd::HaF64(0.0f)) {
		return dgBigVector (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (-1.0f));
	}

	dgGoogol t1 ((Cp0 * Ap0) % p1p0);
//	hacd::HaF64 val1 = t1.GetAproximateValue();	
//	if (val1 < hacd::HaF64 (0.0f)) {
	if (hacd::HaF64 (t1) < hacd::HaF64 (0.0f)) {
		return dgBigVector (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (-1.0f));
	}

	dgGoogol t2 ((Ap0 * Bp0) % p1p0);
	//hacd::HaF64 val2 = t2.GetAproximateValue();	
	//if (val2 < hacd::HaF64 (0.0f)) {
	if (hacd::HaF64 (t2) < hacd::HaF64 (0.0f)) {
		return dgBigVector (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (-1.0f));
	}

	dgGoogol sum = t0 + t1 + t2;
	//hacd::HaF64 den = sum.GetAproximateValue();

#ifdef _DEBUG
	//dgBigVector testpoint (A.Scale (val0 / den) + B.Scale (val1 / den) + C.Scale(val2 / den));
	dgBigVector testpoint (A.Scale (t0 / sum) + B.Scale (t1 / sum) + C.Scale(t2 / sum));
	hacd::HaF64 volume = ((B - A) * (C - A)) % (testpoint - A);
	HACD_ASSERT (fabs (volume) < hacd::HaF64 (1.0e-12f));
#endif
//	return dgBigVector (val0 / den, val1 / den, val2 / den, hacd::HaF32 (0.0f));
	return dgBigVector (t0 / sum, t1 / sum, t2 / sum, hacd::HaF32 (0.0f));
}








