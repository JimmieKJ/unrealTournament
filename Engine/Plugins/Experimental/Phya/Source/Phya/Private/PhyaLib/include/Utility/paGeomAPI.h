//
//  paGeomAPI.h
//
//  Utility library for finding contact velocities from body data.
//



#ifndef paGeomAPI_h
#define paGeomAPI_h


#include "System/paFloat.h"



#define paGEOMPOINT 1E10
#define paGEOMFLAT 0

typedef paFloat* paVec3;  //! Create class with operators.



typedef struct
{
	bool isStill;
    paFloat position[3];
	paFloat velocity[3];
	paFloat angularVel[3];
	paFloat curvAtContact;

//	Full curvature info would include principal axes.

} paGeomBody;



typedef struct
{
	paGeomBody body1;
	paGeomBody body2;
	paFloat normal[3];
	paFloat contactPos[3];
	bool calcContactVel;		// Use curvature to find contactVel. Otherwise supply contactVel.
	paFloat contactVel[3];
} paGeomCollisionData;


typedef struct
{
	paFloat speedBody1RelBody2;			// Of CofMs.		//! rename  speedbody1CMrelbody2CM
	paFloat normalVelBody1RelBody2;		// At contact.
	paFloat normalSpeedBody1RelBody2;	
	paFloat tangentSpeedBody1RelBody2;  // The last three speeds are independent because the vels are not necc. colinear.
	paFloat speedContactRelBody1;
	paFloat speedContactRelBody2;
} paGeomCollisionResult;




PHYA_API void		paGeomBodyCalcVel(const paGeomBody *b, const paVec3 position, paVec3 velocity);
PHYA_API paFloat	paGeomCalcMag(const paVec3 v);
PHYA_API paFloat	paGeomCalcDistance(const paVec3 v1, const paVec3 v2);
PHYA_API void		paGeomCollisionCalc(paGeomCollisionData *d, paGeomCollisionResult *r);




#endif
