//
// paContact.cpp
//
// Describes the state of a continuous contact between two acoustic bodies.
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include "Contact/paContact.hpp"
#include "Surface/paSurface.hpp"
#include "Resonator/paRes.hpp"
#include "Body/paBody.hpp"
#include "Scene/paLockCheck.hpp"

paObjectPool<paContact> paContact :: pool;

//int gpc = 0; //contact counting test

int paContact :: initialize()
{
	m_resonator1 = 0;
	m_resonator2 = 0;
	m_contactGen1 = 0;
	m_contactGen2 = 0;
	m_surface1 = 0;
	m_surface2 = 0;

	m_dynamicData.speedContactRelBody1 = 0;
	m_dynamicData.speedContactRelBody2 = 0;
	m_dynamicData.speedBody1RelBody2 = 0;
	m_dynamicData.contactForce = 0;

	m_isReady = false;
	m_fadeAndDelete = false;

	return 0;
}


paContact :: paContact()
{
	m_poolHandle = -1;
	initialize();
}



int
paContact :: terminate()
{
	if (m_surface1) m_surface1->deleteContactGen(m_contactGen1);
	if (m_surface2) m_surface2->deleteContactGen(m_contactGen2);
	return 0;
}


paContact*
paContact :: newContact()
{
	paLOCKCHECK

	paContact* c = paContact::pool.newActiveObject();

	if (c == 0) return c;
//gpc++;
	c->m_poolHandle = paContact::pool.getHandle();

	c->initialize();
	return c;
}

// static
int
paContact :: deleteContact(paContact* c)
{
//	paLOCKCHECK				// Can't use flag checking inside audio thread.

	paAssert(c && "Contact not valid.");
//	if (c==0) return -1;

	paAssert(c->m_poolHandle != -1 && "Contact not in use.");
//	if (c->m_poolHandle == -1) return -1;	//error - contact not being used.

	c->terminate();
	pool.deleteActiveObject(c->m_poolHandle);
//gpc--;
//printf("%d\n",gpc);
	c->m_poolHandle = -1;
	return 0;
}




int
paContact :: fadeAndDelete()
{
	m_fadeAndDelete = true;				// Signals paTick() to fade out over one block, then delete.
	return 0;
}

// static
int
paContact :: deleteRandomContact()
{
	paLOCKCHECK

	if (pool.getnActiveObjects() == 0) return -1;
	return deleteContact(pool.getRandomObject());
}


// static
int
paContact :: setAllUnused()			// Facility for deleting contacts that are not referenced in contact callback.
{
	pool.firstActiveObject();
	paContact* c;
	while( (c = pool.getNextActiveObject()) != 0 ) { c->m_used = false; }
	return 0;
}


// static
int
paContact :: deleteUnused()		// Run after dynamic step, to clear unused contacts. Might also need to clear refs to acontact from contact in dynamics engine however.
{
	pool.firstActiveObject();
	paContact* c;
	while( (c = pool.getNextActiveObject()) != 0 ) { if (!c->m_used) c->fadeAndDelete(); }
	return 0;
}



int
paContact :: setBody1(paBody* b)
{
	paLOCKCHECK

	if (b == 0) { m_body1 = 0; return -1; }
	if (!b->isEnabled()) { m_body1 = 0; return -1; }

	paAssert(!m_contactGen1);

	m_body1 = b;
	m_resonator1 = b->getRes();
	m_surface1 = b->getSurface();

	if (!m_surface1) return -1;

	m_contactGen1 = m_surface1->newContactGen();

	if (!m_contactGen1) return -1;

	// Activate direct-mix output streamFeed

	// Copy couplings from the body.

	setSurface1ContactGain(b->m_contactGain);
	setSurface1ContactAmpLimit(b->m_contactAmpMax);
	setSurface1ContactCrossGain(b->m_contactCrossGain);
	setSurface1ContactDirectGain(b->m_contactDirectGain);

	return 0;
}




int
paContact :: setBody2(paBody* b)
{
	paLOCKCHECK

	if (b == 0) { m_body2 = 0; return -1; }
	if (!b->isEnabled()) { m_body2 = 0; return -1; }

	paAssert(!m_contactGen2);

	m_body2 = b;
	m_resonator2 = b->getRes();
	m_surface2 = b->getSurface();

	if (!m_surface2) return -1;

	m_contactGen2 = m_surface2->newContactGen();

	if (!m_contactGen2) return -1;		// No point having a contact.

	// Copy couplings from the body.

	setSurface2ContactAmpLimit(b->m_contactAmpMax);
	setSurface2ContactGain(b->m_contactGain);
	setSurface2ContactCrossGain(b->m_contactCrossGain);
	setSurface2ContactDirectGain(b->m_contactDirectGain);

	return 0;
}



int
paContact :: setDynamicData(paContactDynamicData* d)
{
	paLOCKCHECK

	m_dynamicData = *d;
	m_isReady = true;
	return 0;
}
