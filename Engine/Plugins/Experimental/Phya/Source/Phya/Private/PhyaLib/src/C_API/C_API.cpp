//
// paClasses.cpp
//
// Wrapper for the accessing class methods.
//
//! This is not up to date.

#include "PhyaPluginPrivatePCH.h"


#include "Phya.hpp"

#if 0

typedef class paRes paRes;
typedef class paModalData paModalData;
typedef class paModalRes paModalRes;
typedef class paSurface paSurface;
typedef class paSeg paSeg;
typedef class paSegSurface paSegSurface;
typedef class paBody paBody;
typedef class paContact paContact;
typedef class paImpact paImpact;

extern "C" {


// Res group.

// paModalData : Data type used by paModalRes.

paModalData*   paModalDataCreate() { return new paModalData; }
void             paModalDataDestroy(paModalData *d) { delete d; }
int              paModalDataReadDatafile(paModalData *d, char* filename) { return d->read(filename); }
int              paModalDataWritePackedDatafile(paModalData *d, char* filename) { return /*d->writePackedDatafile(filename)*/0; }
int              paModalDataGetNumModes(paModalData *d) { return d->getnModes(); }


// Resource pool required for resonators that become active.
void             paResManagerPoolInit(int n) { paRes::activeResList.allocate(n); }

// paModalRes : An actual resonator type using modal synthesis.

paRes*         paModalResQuaRes(paModalRes *r) { return (paRes*)r; }
paModalRes*    paModalResCreate() { return new paModalRes; }
void             paModalResDestroy(paModalRes *r) { delete r; }
int              paModalResSetData(paModalRes *r, paModalData *d) { return r->setData(d); }
int              paModalResSetNumActiveModes(paModalRes *r, int n) { return r->setnActiveModes(n); }
int              paModalResSetQuietLevel(paModalRes *r, paFloat l) { return r->setQuietLevel(l); }
int              paModalResSetAuxAmpScale(paModalRes *r, paFloat s) { return r->setAuxAmpScale(s); }
int              paModalResSetAuxFreqScale(paModalRes *r, paFloat s) { return r->setAuxFreqScale(s); }
int              paModalResSetAuxDampScale(paModalRes *r, paFloat s) { return r->setAuxDampScale(s); }


// Surface group

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
paSeg*	     paSegSurfaceGetSeg(paSegSurface *s) { return s->getSeg(); }
int              paSegSurfaceSetZeroSlipSpeedCutoffFrequency(paSegSurface *s, paFloat f) { return s->setZeroSlipSpeedCutoffFrequency(f); }
int              paSegSurfaceSetNonzeroSlipSpeedCutoffFrequency(paSegSurface *s, paFloat f, paFloat speed) { return s->setNonzeroSlipSpeedCutoffFrequency(f, speed); }
int              paSegSurfaceSetImpulseToSkidForceRatio(paSegSurface *s, paFloat r) { return s->setImpulseToSkidForceRatio(r); }
int              paSegSurfaceSetSkidSeconds(paSegSurface *s, paFloat skidTime) { return s->setSkidSeconds(skidTime); }

void			 paSegSurfaceContactGenPoolInit(int numContacts) { paSegSurface::allocateContactGens(numContacts); }
void			 paSegSurfaceImpactGenPoolInit(int numContacts) { paSegSurface::allocateImpactGens(numContacts); }



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
int              paBodySetSurfaceContactMasterGain(paBody *b, paFloat g) { return b->setSurfaceContactMasterGain(g); }
int              paBodySetSurfaceToOtherResContactGain(paBody *b, paFloat g) { return b->setSurfaceToOtherResContactGain(g); }
int              paBodySetSurfaceImpactMasterGain(paBody *b, paFloat g) { return b->setSurfaceImpactMasterGain(g); }
int              paBodySetSurfaceToOtherResImpactGain(paBody *b, paFloat g) { return b->setSurfaceToOtherResImpactGain(g); }


// Contact group

// paContact : Describes the acoustic properties and state of a contact between two bodies. */

void			 paContactPoolInit(int numContacts) { paContact::pool.allocate(numContacts); }
paContact*     paContactPoolCreateContact() { return paContact::newContact(); }
void             paContactPoolDestroyContact(paContact *c) { paContact::deleteContact(c); }
void             paContactPoolDestroyRandomContact() { paContact::

paContact*     paContactCreate() { return paContact::newContact(); }
int	             paContactDestroy(paContact* c) { return paContact::deleteContact(c); }
int	             paContactSetBody1(paContact *c, paBody *b) { c->setBody1(b); return 0; }
int	             paContactSetBody2(paContact *c, paBody *b) { c->setBody2(b); return 0; }
int              paContactSetSurface1ContactMasterGain(paContact *c, paFloat g) { c->setSurface1ContactMasterGain(g); return 0; }
int              paContactSetSurface2ContactMasterGain(paContact *c, paFloat g) { c->setSurface2ContactMasterGain(g); return 0; }
int              paContactSetSurface1ToRes2ContactGain(paContact *c, paFloat g) { c->setSurface1ToRes2ContactGain(g); return 0; }
int              paContactSetSurface2ToRes1ContactGain(paContact *c, paFloat g) { c->setSurface2ToRes1ContactGain(g); return 0; }
int              paContactSetSurface1DirectContactGain(paContact *c, paFloat g) { c->setSurface1DirectContactGain(g); return 0; }
int              paContactSetSurface2DirectContactGain(paContact *c, paFloat g) { c->setSurface2DirectContactGain(g); return 0; }
int              paContactSetDynamicData(paContact *c, paContactDynamicData* d) { c->setDynamicData(d); return 0; }


// Impact group

// paImpact : Describes the acoustic properties and state of an impact between two bodies.

void			 paImpactPoolInit(int numImpacts) { paImpact::pool.allocate(numImpacts); }
paImpact*      paImpactPoolCreateImpact() { return paImpact::newImpact(); }
void             paImpactPoolDestroyImpact(paImpact *c) { paImpact::deleteImpact(c); }

paImpact*      paImpactCreate() { return paImpact::newImpact(); }
int	             paImpactDestroy(paImpact* i) { return paImpact::deleteImpact(i); }
int	             paImpactSetBody1(paImpact *i, paBody *b) { return i->setBody1(b); }
int	             paImpactSetBody2(paImpact *i, paBody *b) { return i->setBody2(b); }
int              paImpactSetSurface1ImpactMasterGain(paImpact *i, paFloat g) { return i->setSurface1ImpactMasterGain(g); }
int              paImpactSetSurface2ImpactMasterGain(paImpact *i, paFloat g) { return i->setSurface2ImpactMasterGain(g); }
int              paImpactSetSurface1ToRes2ImpactGain(paImpact *i, paFloat g) { return i->setSurface1ToRes2ImpactGain(g); }
int              paImpactSetSurface2ToRes1ImpactGain(paImpact *i, paFloat g) { return i->setSurface2ToRes1ImpactGain(g); }
int              paImpactSetSurface1DirectImpactGain(paImpact *i, paFloat g) { return i->setSurface1DirectImpactGain(g); }
int              paImpactSetSurface2DirectImpactGain(paImpact *i, paFloat g) { return i->setSurface2DirectImpactGain(g); }
int              paImpactSetSurface1SkidContactGain(paImpact *i, paFloat g){ return i->setSurface1SkidContactGain(g); }
int              paImpactSetSurface2SkidContactGain(paImpact *i, paFloat g){ return i->setSurface2SkidContactGain(g); }
int              paImpactSetDynamicData(paImpact *i, paImpactDynamicData* d) { return i->setDynamicData(d); }


// Block Group

// A Block is collection of samples used in signal processing.
// A pool of blocks must be initialized for internal use. First set the maximum
// number if samples per Block.

int              paBlockSetMaxNumSamples(int n) { return paBlock::setnMaxFrames(n); }
void			 paBlockPoolInit(int n) { paBlock::pool.allocate(n); }

// The number of block samples actually used can be varied below MaxNumSamples,
// in order to dynamically improve perforance.

int              paBlockSetNumSamples(int n) { return paBlock::setnFrames(n); }


} // Matches   extern "C"

#endif
