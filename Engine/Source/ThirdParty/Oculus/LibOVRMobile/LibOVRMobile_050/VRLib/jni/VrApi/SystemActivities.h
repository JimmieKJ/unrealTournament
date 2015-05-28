/************************************************************************************

Filename    :   SystemActivities.h
Content     :   Event handling for system activities
Created     :   February 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_SystemActivities_h
#define OVR_SystemActivities_h

namespace OVR {

//==============================================================================================
// Internal to VrAPI
//==============================================================================================
void SystemActivities_InitEventQueues();
void SystemActivities_ShutdownEventQueues();
void SystemActivities_AddEvent( char const * data );
void SystemActivities_AddInternalEvent( char const * data );
eVrApiEventStatus SystemActivities_GetNextPendingInternalEvent( char * buffer, unsigned int const bufferSize );
eVrApiEventStatus SystemActivities_GetNextPendingMainEvent( char * buffer, unsigned int const bufferSize );

} // namespace OVR

#endif // OVR_SystemActivities_h