/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Lockless.h
Content     :   Lock-less producer/consumer communication
Created     :   November 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_Lockless_h
#define OVR_Lockless_h

#include "OVR_Atomic.h"

// Define this to compile-in Lockless test logic
//#define OVR_LOCKLESS_TEST

namespace OVR {

// ***** LocklessUpdater

// For single producer cases where you only care about the most recent update, not
// necessarily getting every one that happens (vsync timing, SensorFusion updates).
//
// This is multiple consumer safe.
//
// TODO: This is Android specific

template<class T>
class LocklessUpdater
{
public:
	LocklessUpdater() : UpdateBegin( 0 ), UpdateEnd( 0 ) {}

	T		GetState() const
	{
		// Copy the state out, then retry with the alternate slot
		// if we determine that our copy may have been partially
		// stepped on by a new update.
		T	state;
		int	begin, end, final;

		for(;;)
		{
			// We are adding 0, only using these as atomic memory barriers, so it
			// is ok to cast off the const, allowing GetState() to remain const.
            end   = UpdateEnd.ExchangeAdd_Sync(0);
            state = Slots[ end & 1 ];
            begin = UpdateBegin.ExchangeAdd_Sync(0);
			if ( begin == end ) {
				return state;
			}

			// The producer is potentially blocked while only having partially
			// written the update, so copy out the other slot.
            state = Slots[ (begin & 1) ^ 1 ];
            final = UpdateBegin.ExchangeAdd_NoSync(0);
			if ( final == begin ) {
				return state;
			}

			// The producer completed the last update and started a new one before
			// we got it copied out, so try fetching the current buffer again.
		}
		return state;
	}

	void	SetState( T state )
	{
        const int slot = UpdateBegin.ExchangeAdd_Sync(1) & 1;
        // Write to (slot ^ 1) because ExchangeAdd returns 'previous' value before add.
        Slots[slot ^ 1] = state;
        UpdateEnd.ExchangeAdd_Sync(1);
	}

    mutable AtomicInt<int> UpdateBegin;
    mutable AtomicInt<int> UpdateEnd;
    T		               Slots[2];
};


#ifdef OVR_LOCKLESS_TEST
void StartLocklessTest();
#endif


} // namespace OVR

#endif // OVR_Lockless_h
