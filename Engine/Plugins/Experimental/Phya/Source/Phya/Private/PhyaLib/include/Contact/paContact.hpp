//
// paContact.hpp
//
// Describes the state of a continuous contact between two acoustic bodies.
//

#if !defined(__paContact_hpp)
#define __paContact_hpp

#include "System/paConfig.h"

#include "Contact/paContactGen.hpp"
#include "Utility/paObjectPool.hpp"
#include "Utility/paGeomAPI.h"
#include "Contact/paContactDynamicDataAPI.h"
#include "Scene/paFlags.hpp"
class paSurface;
class paRes;
class paBody;

//PHYA_API paBlock* paTick(void);

class PHYA_API paContact
{
private:
	friend PHYA_API paBlock* paTick(void);		// Main audio tick function which updates the resonators and surface generators.

	paBody* m_body1;
	paBody* m_body2;

	paRes* m_resonator1;
	paRes* m_resonator2;

	paSurface* m_surface1;			// Used to provide a surface type for
	paSurface* m_surface2;			// freeing contactGens when the contact is freed.

	paContactGen* m_contactGen1;
	paContactGen* m_contactGen2;

	paContactDynamicData m_dynamicData;

	// Contact couplings.
	// These are copied from the owning paBody, and can be altered.

	paFloat m_surface1ContactCrossGain;
	paFloat m_surface2ContactCrossGain;
	paFloat m_surface1ContactDirectGain;
	paFloat m_surface2ContactDirectGain;

	bool m_isReady;			// Used to prevent the audiothread processing the contact before any parameters have been set.
	int m_poolHandle;		// Handle used by the scene contact manager to release the contact.
	bool m_used;			// Can be used to track when contacts are not used and can be released.

	paFloat m_lastPosition[3];	// Can be used to find contact vel by numerical diff.

	void* m_userData;

public:

	static paObjectPool<paContact> pool;

	paContact();

	int initialize();	// Use after pulling something from the pool.
	int terminate();	// Use before releasing something back to the pool.

	static paContact* newContact();
	static int deleteContact(paContact*);
	static int deleteRandomContact();

	bool isActive() { return pool.isObjectActive(m_poolHandle); };

	static int setAllUnused();			// Facility for deleting contacts that are not referenced in contact callback.
	int setUsed() { m_used = true; return 0; };	// Set in the contact callback.
	static int deleteUnused();			// Run after dynamic step, to clear unused contacts.

	int fadeAndDelete();
	bool m_fadeAndDelete;	// Signals that paTick() should fade out excitations from this contact then delete.


	// Coupling set functions. Use straight after assigning bodies.
	// These couplings overide the coupling setting in the assigned bodies.

	int setBody1(paBody*);
	int setBody2(paBody*);

	paBody* getBody1() { return m_body1; }
	paBody* getBody2() { return m_body2; }
	paRes* getRes1() { return m_resonator1; }
	paRes* getRes2() { return m_resonator2; }
	paSurface* getSurface1() { return m_surface1; }
	paSurface* getSurface2() { return m_surface2; }
	paContactGen* getGen1() { return m_contactGen1; }
	paContactGen* getGen2() { return m_contactGen2; }


	// Coupling set functions. Use straight after obtaining a new contact.

	int setSurface1ContactGain(paFloat s) {
		paAssert( m_contactGen1 && "Body1 has not been assigned with 'setBody1()' " );
		m_contactGen1->setGain(s);
		return 0;
	}

	int setSurface2ContactGain(paFloat s) {
		paAssert( m_contactGen2 && "Body2 has not been assigned with 'setBody2()' " );
		m_contactGen2->setGain(s);
		return 0;
	}

	int setSurface1ContactAmpLimit(paFloat s) {
		paAssert( m_contactGen1 && "Body1 has not been assigned with 'setBody1()' " );
		m_contactGen1->setAmpMax(s);
		return 0;
	}

	int setSurface2ContactAmpLimit(paFloat s) {
		paAssert( m_contactGen2 && "Body2 has not been assigned with 'setBody2()' " );
		m_contactGen2->setAmpMax(s);
		return 0;
	}

	int setSurface1ContactCrossGain(paFloat s) { m_surface1ContactCrossGain = s; return 0; }
	int setSurface2ContactCrossGain(paFloat s) { m_surface2ContactCrossGain = s; return 0; }

	int setSurface1ContactDirectGain(paFloat s) { m_surface1ContactDirectGain = s; return 0; }
	int setSurface2ContactDirectGain(paFloat s) { m_surface2ContactDirectGain = s; return 0; }



	// Regular dynamic data update.

	int setDynamicData(paContactDynamicData* d);
//	int setPhysCollisionData(paGeomCollisionData* d);

	void* getUserData() { return m_userData; }
	int setUserData(void* d) { m_userData = d; return 0; }


	paFloat* getLastPosition() { return m_lastPosition; }
	void setLastPostion(paFloat *p) {
		m_lastPosition[0] = p[0];
		m_lastPosition[1] = p[1];
		m_lastPosition[2] = p[2];
	}

};





#endif
