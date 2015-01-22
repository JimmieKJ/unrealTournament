//
//	paContactDynamicDataAPI.h
//



#ifndef _paContactDynamicDataAPI_h
#define _paContactDynamicDataAPI_h


//#ifdef __cpluplus
//extern "C" {
//#endif


typedef
struct 
{
	paFloat speedContactRelBody1;
	paFloat speedContactRelBody2;
	paFloat speedBody1RelBody2;				//! speedBody1RelBody2AtContact
	paFloat contactForce;	
}paContactDynamicData;



//#ifdef __cpluplus
//}
//#endif


#endif



