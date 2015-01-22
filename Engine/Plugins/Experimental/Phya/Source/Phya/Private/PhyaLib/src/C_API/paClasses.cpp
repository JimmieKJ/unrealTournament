//
// paClasses.cpp
//
// Wrapper for the accessing class methods.
//
//! This is not up to date.
#include "PhyaPluginPrivatePCH.h"

#if 0

#include "Surface/paSurface.hpp"
#include "Resonator/paModalData.hpp"
#include "Resonator/paModalRes.hpp"
#include "Body/paBody.hpp"
#include "Contact/paContact.hpp"
#include "Impact/paImpact.hpp"

typedef class paRes paRes;
typedef class paModalData paModalData;
typedef class paModalRes paModalRes;
typedef class paSurface paSurface;
typedef class paSeg paSeg;
typedef class paSegSurface paSegSurface;
typedef class paBody paBody;
typedef class paContact paContact;
typedef class paImpact paImpact;
typedef class paBlock paBlock;

extern "C" {


// Res group.


// Resource pool required for resonators that become active.

void             paResActivationManagerInit(int n) { paRes::activeResList.allocate(n); }
int              paResActivationManagerGetNumInUse() { return paRes::activeResList.getnMembers(); }
int              paResActivationManagerGetNumUsed() { return paRes::activeResList.getnMaxAllocationsUsed(); }

// Functions shared by all resonators.

int              paResDeactivate(paRes *r) { return r->deactivate(); }
int              paResSetQuietLevel(paRes *r, paFloat l) { return r->setQuietLevel(l); }
int              paResMakeQuiet(paRes *r) { return r->makeQuiet(); };
int              paResSetAuxAmpScale(paRes *r, paFloat s) { return r->setAuxAmpScale(s); }
int              paResSetAuxFreqScale(paRes *r, paFloat s) { return r->setAuxFreqScale(s); }
int              paResSetAuxDampScale(paRes *r, paFloat s) { return r->setAuxDampScale(s); }
int              paResSetMaxContactDamping(paRes *r, paFloat d) { return r->setMaxContactDamping(d); }

// paModalData : Data type used by paModalRes.

paModalData*   paModalDataCreate() { return new paModalData; }
void             paModalDataDestroy(paModalData *d) { delete d; }
int              paModalDataReadDatafile(paModalData *d, char* filename) { return d->readDatafile(filename); }
int              paModalDataWritePackedDatafile(paModalData *d, char* filename) { return d->writeDatafile(filename); }
int              paModalDataGetNumModes(paModalData *d) { return d->getnModes(); }



// paModalRes : A resonator type using modal synthesis.

paRes*         paModalResQuaRes(paModalRes *r) { return (paRes*)r; }
paModalRes*    paModalResCreate() { return new paModalRes; }
void             paModalResDestroy(paModalRes *r) { delete r; }
int              paModalResSetData(paModalRes *r, paModalData *d) { return r->setData(d); }
int              paModalResSetNumActiveModes(paModalRes *r, int n) { return r->setnActiveModes(n); }





// Surface group

int              paSurfaceSetHardness(paSurface *s, paFloat h) { return s->setHardness(h); }
int              paSurfaceSetContactDamping(paSurface *s, paFloat d) { return s->setContactDamping(d); }
int              paSurfaceSetContactPostMasterGainLimit(paSurface *s, paFloat l) { return s->setContactPostMasterGainLimit(l); }
int              paSurfaceSetContactMasterGain(paSurface *s, paFloat g) { return s->setContactMasterGain(g); }
int              paSurfaceSetContactToOtherResGain(paSurface *s, paFloat g) { return s->setContactToOtherResGain(g); }
int              paSurfaceSetContactDirectGain(paSurface *s, paFloat g) { return s->setContactDirectGain(g); }
int              paSurfaceSetImpactPostMasterGainLimit(paSurface *s, paFloat l) { return s->setImpactPostMasterGainLimit(l); }
int              paSurfaceSetImpactMasterGain(paSurface *s, paFloat g) { return s->setImpactMasterGain(g); }
int              paSurfaceSetImpactToOtherResGain(paSurface *s, paFloat g) { return s->setImpactToOtherResGain(g); }
int              paSurfaceSetImpactDirectGain(paSurface *s, paFloat g) { return s->setImpactDirectGain(g); }


// paSeg : An audio recording used by paSegSurface.

paSeg*         paSegCreate() { return new paSeg; }
void             paSegDestroy(paSeg *s) { delete s; }
int              paSegReadAudioFile(paSeg *s, char *filename) { return s->readAudioFile(filename); }
int              paSegFillWhiteNoise(paSeg *s, int length) { return s->fillWhiteNoise(length); }
short*           paSegGetStart(paSeg *s) { return s->getStart(); }
long             paSegGetNumFrames(paSeg *s) { return s->getnFrames(); }


// paSegSurface : An actual surface description based on an audio recording of surface texture.

paSurface*     paSegSurfaceQuaSurface(paSegSurface* s) { return (paSurface*)s; }
paSegSurface*  paSegSurfaceCreate() { return new paSegSurface; }
void             paSegSurfaceDestroy(paSegSurface *s) { delete s; }
int              paSegSurfaceSetSeg(paSegSurface *s, paSeg *seg) { return s->setSeg(seg); }
paSeg*         paSegSurfaceGetSeg(paSegSurface *s) { return s->getSeg(); }
int              paSegSurfaceSetCutoffFreqAtZeroSlipSpeed(paSegSurface *s, paFloat f) { return s->setCutoffFreqAtZeroSlipSpeed(f); }
int              paSegSurfaceSetCutoffFreqAtNonzeroSlipSpeed(paSegSurface *s, paFloat f, paFloat speed) { return s->setCutoffFreqAtNonzeroSlipSpeed(f, speed); }
int              paSegSurfaceSetRateAtSpeed(paSegSurface *s, paFloat r, paFloat speed) { return s->setRateAtSpeed(r, speed); }
int              paSegSurfaceSetSkidGain(paSegSurface *s, paFloat g) { return s->setSkidGain(g); }
int              paSegSurfaceSetSkidImpulseToForceRatio(paSegSurface *s, paFloat r) { return s->setSkidImpulseToForceRatio(r); }
int              paSegSurfaceSetSkidTime(paSegSurface *s, paFloat skidTime) { return s->setSkidTime(skidTime); }
int              paSegSurfaceSetSkidThickness(paSegSurface *s, paFloat t) { return s->setSkidThickness(t); }
int              paSegSurfaceSetSkidMaxTime(paSegSurface *s, paFloat t) { return s->setSkidMaxTime(t); }
int              paSegSurfaceSetSkidMinTime(paSegSurface *s, paFloat t) { return s->setSkidMinTime(t); }
int              paSegSurfaceSetQuietSpeed(paSegSurface *s, paFloat speed) { return s->setQuietSpeed(speed); }
int              paSegSurfaceSetHardness(paSegSurface *s, paFloat h) { return s->setHardness(h); }

void			 paSegSurfaceContactGenPoolInit(int numContacts) { paSegSurface::allocateContactGens(numContacts); }
void			 paSegSurfaceImpactGenPoolInit(int numContacts) { paSegSurface::allocateImpactGens(numContacts); }
int              paSegSurfaceContactGenPoolGetNumInUse() { return paSegSurface::getnActiveContactGens(); }
int              paSegSurfaceContactGenPoolGetNumUsed() { return paSegSurface::getnMaxActiveContactGens(); }
int              paSegSurfaceImpactGenPoolGetNumInUse() { return paSegSurface::getnActiveImpactGens(); }
int              paSegSurfaceImpactGenPoolGetNumUsed() { return paSegSurface::getnMaxActiveImpactGens(); }



// Body group

// paBody : Describes the acoustic properties and state of a body.

paBody*        paBodyCreate() { return new paBody; }
void             paBodyDestroy(paBody *b) { delete b; }
int              paBodySetRes(paBody *b, paRes *r) { return b->setRes(r); }
int              paBodySetModalRes(paBody *b, paModalRes *r) { return b->setRes(r); }
int              paBodySetSurface(paBody *b, paSurface *s) { return b->setSurface(s); }
int              paBodySetSegSurface(paBody *b, paSegSurface *s) { return b->setSurface(s); }
paRes*         paBodyGetRes(paBody *b) { return b->getRes(); }
paSurface*     paBodyGetSurface(paBody *b) { return b->getSurface(); }
int              paBodySetContactMasterGain(paBody *b, paFloat g) { return b->setContactMasterGain(g); }
int              paBodySetContactToOtherResGain(paBody *b, paFloat g) { return b->setContactToOtherResGain(g); }
int              paBodySetContactDirectGain(paBody *b, paFloat g) { return b->setContactDirectGain(g); }
int              paBodySetImpactMasterGain(paBody *b, paFloat g) { return b->setImpactMasterGain(g); }
int              paBodySetImpactToOtherResGain(paBody *b, paFloat g) { return b->setImpactToOtherResGain(g); }
int              paBodySetImpactDirectGain(paBody *b, paFloat g) { return b->setImpactDirectGain(g); }
int              paBodyEnable(paBody *b) { return b->enable(); }
int              paBodyDisable(paBody *b) { return b->disable(); }
int              paBodyIsEnabled(paBody *b) { return (int)b->isEnabled(); }

// Contact group

// paContact : Describes the acoustic properties and state of a contact between two bodies. */

void			 paContactPoolInit(int numContacts) { paContact::pool.allocate(numContacts); }
paContact*     paContactPoolCreateContact() { return paContact::newContact(); }
void             paContactPoolFadeAndDestroyContact(paContact *c) { c->fadeAndDeleteContact(c); }
void             paContactPoolDestroyContact(paContact *c) { paContact::deleteContact(c); }
void             paContactPoolDestroyRandomContact() { paContact::deleteRandomContact(); }
void             paContactPoolActiveContactIteratorReset() { paContact::pool.firstActiveObject(); }
paContact*     paContactPoolActiveContactIteratorGetNext() { return paContact::pool.getNextActiveObject(); }
int              paContactPoolGetNumInUse() { return paContact::pool.getnActiveObjects(); }
int              paContactPoolGetNumUsed() { return paContact::pool.getnMaxAllocationsUsed(); }

paContact*     paContactCreate() { return paContact::newContact(); }
int	             paContactDestroy(paContact* c) { return paContact::deleteContact(c); }
int	             paContactSetBody1(paContact *c, paBody *b) { c->setBody1(b); return 0; }
int	             paContactSetBody2(paContact *c, paBody *b) { c->setBody2(b); return 0; }
int              paContactSetSurface1ContactMasterGain(paContact *c, paFloat g) { c->setSurface1ContactMasterGain(g); return 0; }
int              paContactSetSurface2ContactMasterGain(paContact *c, paFloat g) { c->setSurface2ContactMasterGain(g); return 0; }
int              paContactSetSurface1ToRes2ContactGain(paContact *c, paFloat g) { c->setSurface1ContactToRes2Gain(g); return 0; }
int              paContactSetSurface2ToRes1ContactGain(paContact *c, paFloat g) { c->setSurface2ContactToRes1Gain(g); return 0; }
int              paContactSetSurface1DirectContactGain(paContact *c, paFloat g) { c->setSurface1ContactDirectGain(g); return 0; }
int              paContactSetSurface2DirectContactGain(paContact *c, paFloat g) { c->setSurface2ContactDirectGain(g); return 0; }
int              paContactSetDynamicData(paContact *c, paContactDynamicData* d) { c->setDynamicData(d); return 0; }
int              paContactSetData1(paContact *c, void *d) { c->setData1(d); return 0; }
void*            paContactGetData1(paContact *c) { return c->getData1(); }

// Impact group

// paImpact : Describes the acoustic properties and state of an impact between two bodies.

void			 paImpactPoolInit(int numImpacts) { paImpact::pool.allocate(numImpacts); }
paImpact*      paImpactPoolCreateImpact() { return paImpact::newImpact(); }
void             paImpactPoolDestroyImpact(paImpact *c) { paImpact::deleteImpact(c); }
void             paImpactPoolActiveImpactIteratorReset() { paImpact::pool.firstActiveObject(); }
paImpact*      paImpactPoolActiveImpactIteratorGetNext() { return paImpact::pool.getNextActiveObject(); }
int              paImpactPoolGetNumInUse() { return paImpact::pool.getnActiveObjects(); }
int              paImpactPoolGetNumUsed() { return paImpact::pool.getnMaxAllocationsUsed(); }

paImpact*      paImpactCreate() { return paImpact::newImpact(); }
int	             paImpactDestroy(paImpact* i) { return paImpact::deleteImpact(i); }
int	             paImpactSetBody1(paImpact *i, paBody *b) { return i->setBody1(b); }
int	             paImpactSetBody2(paImpact *i, paBody *b) { return i->setBody2(b); }
int              paImpactSetSurface1ImpactMasterGain(paImpact *i, paFloat g) { return i->setSurface1ImpactMasterGain(g); }
int              paImpactSetSurface2ImpactMasterGain(paImpact *i, paFloat g) { return i->setSurface2ImpactMasterGain(g); }
int              paImpactSetSurface1ToRes2ImpactGain(paImpact *i, paFloat g) { return i->setSurface1ImpactToRes2Gain(g); }
int              paImpactSetSurface2ToRes1ImpactGain(paImpact *i, paFloat g) { return i->setSurface2ImpactToRes1Gain(g); }
int              paImpactSetSurface1DirectImpactGain(paImpact *i, paFloat g) { return i->setSurface1ImpactDirectGain(g); }
int              paImpactSetSurface2DirectImpactGain(paImpact *i, paFloat g) { return i->setSurface2ImpactDirectGain(g); }
//int              paImpactSetSurface1SkidContactGain(paImpact *i, paFloat g){ return i->setSurface1SkidContactGain(g); }
//int              paImpactSetSurface2SkidContactGain(paImpact *i, paFloat g){ return i->setSurface2SkidContactGain(g); }
int              paImpactSetDynamicData(paImpact *i, paImpactDynamicData* d) { return i->setDynamicData(d); }
int              paImpactGetDyanmicDataIsSet(paImpact *i) { return (i->getDyanmicDataIsSet()); }
int              paImpactSetData1(paImpact *i, void *d) { i->setData1(d); return 0; }
void*            paImpactGetData1(paImpact *i) { return i->getData1(); }


// Block Group

// A Block is collection of samples used in signal processing.
// A pool of blocks must be initialized for internal use. First set the maximum
// number if samples per Block.

int              paBlockSetMaxNumSamples(int n) { return paBlock::setnMaxFrames(n); }
paBlock*       paBlockCreate() { return new paBlock; }
void			 paBlockPoolInit(int numContacts);
paBlock*       paBlockPoolCreateBlock() { return paBlock::pool.newActiveObject(); }
void			 paBlockPoolInit(int n) { paBlock::pool.allocate(n); }
int              paBlockPoolGetNumInUse() { return paBlock::pool.getnActiveObjects(); }
int              paBlockPoolGetNumUsed() { return paBlock::pool.getnMaxAllocationsUsed(); }

// The number of block samples actually used can be varied below MaxNumSamples,
// in order to dynamically improve perforance.

int              paBlockSetNumSamples(int n) { return paBlock::setnFrames(n); }


} // Matches   extern "C"

#endif
