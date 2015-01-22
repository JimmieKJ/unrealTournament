//
//    paGeom.cpp
//    
//    Utility library for finding contact velocities from body data.
//!   Could be tidied up by defining vector ops on paVec3.
//

#include "PhyaPluginPrivatePCH.h"

//#include <math.h>

#include "Utility/paGeomAPI.h"

#include <stdio.h>

// Find body velocity at position x.

void
paGeomBodyCalcVel(const paGeomBody *b, const paVec3 x, paVec3 v)
{
	paFloat* ov = (paFloat*)b->velocity;
	paFloat* av = (paFloat*)b->angularVel;
	paFloat* o = (paFloat*)b->position;

	paFloat r[3];

	r[0] = x[0] - o[0];
	r[1] = x[1] - o[1];
	r[2] = x[2] - o[2];

	v[0] = ov[0] +   av[1] * r[2] - av[2] * r[1];
	v[1] = ov[1] +   av[2] * r[0] - av[0] * r[2];
	v[2] = ov[2] +   av[0] * r[1] - av[1] * r[0];
}


paFloat
paGeomCalcMag(const paVec3 v)
{
	return (paFloat)sqrt( v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
}

paFloat
paGeomCalcDistance(const paVec3 v1, const paVec3 v2)
{
	paFloat v[3];

	v[0] = v2[0]-v1[0];
	v[1] = v2[1]-v1[1];
	v[2] = v2[2]-v1[2];

	return paGeomCalcMag(v);
}



void
paGeomCollisionCalc(paGeomCollisionData *data, paGeomCollisionResult *result)
{
	paFloat v1[3];
	paFloat v2[3];
	paFloat v[3];
	paFloat* vc;
	paFloat* n;
	paFloat nvel;
	paGeomBody* b1 = &data->body1;
	paGeomBody* b2 = &data->body2;
	int i;

	// Simple processing of body.isStill flag.

	if (b1->isStill) 
		for(i=0; i<3; i++) {
			b1->angularVel[i] = 0;
			b1->position[i] = 0;
			b1->velocity[i] = 0;
		}

	if (b2->isStill) 
		for(i=0; i<3; i++) {
			b2->angularVel[i] = 0;
			b2->position[i] = 0;
			b2->velocity[i] = 0;
		}

	v[0] = b1->velocity[0] - b2->velocity[0];
	v[1] = b1->velocity[1] - b2->velocity[1];
	v[2] = b1->velocity[2] - b2->velocity[2];

	result->speedBody1RelBody2 = sqrtf( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );


	paGeomBodyCalcVel(b1, (paFloat*)data->contactPos, v1);
	paGeomBodyCalcVel(b2, (paFloat*)data->contactPos, v2);

//printf("b1 %f  %f  %f\n", v1[0],v1[1],v1[2]);
//printf("b2 %f  %f  %f\n", v2[0],v2[1],v2[2]);

	v[0] = v2[0]-v1[0];
	v[1] = v2[1]-v1[1];
	v[2] = v2[2]-v1[2];

	n = (paFloat*)data->normal;

	// Calculate relative speed of bodies in normal direction.
	nvel = v[0]*n[0] + v[1]*n[1] + v[2]*n[2];
	result->normalVelBody1RelBody2 = nvel;
	if (nvel > 0) result->normalSpeedBody1RelBody2 = nvel;
	else result->normalSpeedBody1RelBody2 = -nvel;

	v[0] -= n[0]*nvel;
	v[1] -= n[1]*nvel;
	v[2] -= n[2]*nvel;

	// Calculate relative tangent speed of bodies.
	result->tangentSpeedBody1RelBody2 = sqrtf( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );



	// Calculate contact velocity.

	if (data->calcContactVel) {
		//! Could calculate contact vel from curvature and angvel data.
		//! For now just assume point on plane:
		vc = v1;
	}
	// else the user has supplied valid contact vel already, 
	// calculated eg using numerical diff.
	else {
		vc = (paFloat*)data->contactVel;
	}


	v[0] = v1[0]-vc[0];
	v[1] = v1[1]-vc[1];
	v[2] = v1[2]-vc[2];

	// Remove normal component of velocity : possibly not neccessary.
	nvel = v[0]*n[0] + v[1]*n[1] + v[2]*n[2];
	v[0] -= n[0]*nvel;
	v[1] -= n[1]*nvel;
	v[2] -= n[2]*nvel;

	result->speedContactRelBody1 = sqrtf( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );


	v[0] = v2[0]-vc[0];
	v[1] = v2[1]-vc[1];
	v[2] = v2[2]-vc[2];

	// Ditto above
	nvel = v[0]*n[0] + v[1]*n[1] + v[2]*n[2];
	v[0] -= n[0]*nvel;
	v[1] -= n[1]*nvel;
	v[2] -= n[2]*nvel;

	result->speedContactRelBody2 = sqrtf( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );

}



