//
// paTick.cpp
//
// Calculates one paBlock of audio output, for the whole scene,
// and possibly calls output callbacks.
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paConfig.h"


#include "Signal/paBlock.hpp"
#include "Surface/paSurface.hpp"
#include "Resonator/paRes.hpp"
#include "Body/paBody.hpp"
#include "Contact/paContact.hpp"
#include "Impact/paImpact.hpp"
#include "Signal/paHighpass.hpp"


static paBlock* _output = 0;
static paBlock* _tempBlock = 0;

static int _maxTimeCost = 100000;
static int _timeCost = 0;
static void (*_multipleOutputCallback)(paRes* res, paFloat* output) = 0;


static paHighpass* _highpass = 0;
//static paLowpass* _lowpass = 0;


// The time cost must be increased when tick starts using an object,
// and decremented when tick stops using it.
//! It would be better if _timeCost were a static part of a class inherited by costing objects,
//! so that the activate / deactivate methods encapsulate load management,
//! and probably block pull/push as well.

PHYA_API
int
paSetMaxTimeCost(int c)
{
	_maxTimeCost = c;
	return 0;
}


PHYA_API
int
paSetMultipleOutputCallback( void (*cb)(paRes* res, paFloat* output) )
{
	_multipleOutputCallback = cb;
	return 0;
}



// Creation of blocks delayed until after user sets block size.
PHYA_API
int
paTickInit()
{
//	if (_lowpass == 0) _lowpass = new paLowpass;
	if (_highpass == 0) _highpass = new paHighpass;
	if (_tempBlock == 0) _tempBlock = paBlock::newBlock(); else return -1;
	if (_output == 0) _output = paBlock::newBlock(); else return -1;
	return 0;
}



static
bool
_tryActivateResIfInactive(paRes* res)
{
	if (!res->isActive())
	{
		int newCost;
		if ( (newCost = _timeCost+res->getTimeCost()) <= _maxTimeCost )
		{
			_timeCost = newCost;
			res->setInput(paBlock::newBlock());
			res->getInput()->zero();		// This resonator won't have been zeroed at the start of paTick()
			res->resetContactDamping();
			res->activate();
			return true;
		}
		else return false;
	}
	else return true;
}


PHYA_API
paBlock*
paTick()
{

	// Overview:
	// Clear resonator inputs.
	// Tick contact generators where not quiet, add to resonator inputs.
	// (wake up affected resonators that were previously quiet.)
	// (Free up resources used by contacts that have been marked for deletion.)
	// Tick resonators.



	//// Clear Res inputs.

	paRes* res;
	paRes::activeResList.firstMember();
	while( (res = paRes::activeResList.getNextMember()) != 0)
	{
		res->getInput()->zero();
		res->resetContactDamping();
		res->clearInContact();
	}


	//// Clear main output.
    paAssert(_output && "Main output not set. Try paInit ?");
	_output->zero();


	//// Tick contacts.

	paContact* ac;
//int acc=0;
	paContact::pool.firstActiveObject();
	while( (ac = paContact::pool.getNextActiveObject()) != 0 ) {
//acc++;
//printf("%d\n",acc);
		if (ac->m_isReady) {
			// And generate surface excitations from valid surfaces.

			paBlock* surfaceOutput = _tempBlock;
			paRes* res1 = ac->m_resonator1;
			paRes* res2 = ac->m_resonator2;
			paContactGen* gen1 = ac->m_contactGen1;
			paContactGen* gen2 = ac->m_contactGen2;
			//paSurface* surface1 = c->m_surface1;
			//paSurface* surface2 = c->m_surface2;


			// Contact damping multiplicative 'accumulator'
			paFloat contactDamping = (paFloat)1.0;

			if (res1) res1->setInContact();
			if (res2) res2->setInContact();

			if (gen1) {
				// If a body is disabled it will stop generating contact sound immediately.
				// (Valid contactGen implies a valid body)
				if (ac->m_body1->isEnabled()) {
					contactDamping *= ac->m_surface1->m_contactDamping;
						gen1->setSpeedBody1RelBody2(
							ac->m_dynamicData.speedBody1RelBody2);
						gen1->setSpeedContactRelBody(
							ac->m_dynamicData.speedContactRelBody1);
						gen1->setContactForce(
							ac->m_dynamicData.contactForce);

					// ( Otherwise we continue using previous data set. )

					// Test that surface generates non-zero output.
					// If it doesn't then we don't need to calculate it explicitly.
					if (gen1->isActive()) {
						gen1->setOutput(surfaceOutput);
						gen1->tick();
						if (ac->m_fadeAndDelete  || gen1->isQuiet())
						{
							surfaceOutput->fadeout();
							gen1->setInactive();
						}

						// Direct output.
						_output->addWithMultiply(surfaceOutput, ac->m_surface1ContactDirectGain);

						if (res1)
						{
							if (_tryActivateResIfInactive(res1))		// If res active, or can be made active.
								// Direct addition onto resonator input:
								res1->getInput()->add(surfaceOutput);
						}

						if (res2)
						{
							if (_tryActivateResIfInactive(res2))
								// Cross coupling to the other resonator.
								res2->getInput()->addWithMultiply(surfaceOutput, ac->m_surface1ContactCrossGain);
						}

					}
				}
			}

			if (gen2) {
				// If a body is disabled it will stop generating contact sound immediately.
				// (Valid contactGen implies a valid body)
				if (ac->m_body2->isEnabled()) {
					contactDamping *= ac->m_surface2->m_contactDamping;
						gen2->setSpeedBody1RelBody2(
							ac->m_dynamicData.speedBody1RelBody2);
						gen2->setSpeedContactRelBody(
							ac->m_dynamicData.speedContactRelBody2);
						gen2->setContactForce(
							ac->m_dynamicData.contactForce);



					if (gen2->isActive()) {
						gen2->setOutput(surfaceOutput);
						gen2->tick();
//printf("gen2 ");
						if (ac->m_fadeAndDelete || gen2->isQuiet())	
						{
							surfaceOutput->fadeout();
							gen2->setInactive();
//printf("f %d %d\n", ac, ac->getUserData());
						}

						// Direct output.
						_output->addWithMultiply(surfaceOutput, ac->m_surface2ContactDirectGain);

						if (res2) {
							if (_tryActivateResIfInactive(res2))
								res2->getInput()->add(surfaceOutput);
						}

						if (res1) {
							if (_tryActivateResIfInactive(res1))
	 							res1->getInput()->addWithMultiply(surfaceOutput, ac->m_surface2ContactCrossGain);
						}

					}
				}
			}


		if (res1) res1->addContactDamping(contactDamping);
		if (res2) res2->addContactDamping(contactDamping);
		}

		if (ac->m_fadeAndDelete)
		{
			paContact::deleteContact(ac);
//printf("- %d %d\n", ac, ac->getUserData());
		}
	}



	//// Tick impacts.

	paImpact* ai;

	paImpact::pool.firstActiveObject();
	while((ai = paImpact::pool.getNextActiveObject()) != 0 ) {
		// If an impact is locked it cannot be ready - you only write params once.
		if (ai->m_isReady) {
			// Pass impact data to the impact.
			// And generate surface excitations from valid surfaces.

			paBlock* surfaceOutput = _tempBlock;
			paRes* res1 = ai->m_resonator1;
			paRes* res2 = ai->m_resonator2;
			paImpactGen* gen1 = ai->m_impactGen1;
			paImpactGen* gen2 = ai->m_impactGen2;
			paSurface* surface1 = ai->m_surface1;
			paSurface* surface2 = ai->m_surface2;

			bool impactHasFinished = true;	// Will be false if either impact generator still makes sound.

			if (surface1 && gen1) {

				gen1->setRelTangentSpeedAtImpact(ai->m_dynamicData.relTangentSpeedAtImpact);
				gen1->setRelNormalSpeedAtImpact(ai->m_dynamicData.relNormalSpeedAtImpact);
				gen1->setImpactImpulse(ai->m_dynamicData.impactImpulse);

				gen1->setOutput(surfaceOutput);
				gen1->tick();


				// Direct output. (mostly for testing)
				_output->addWithMultiply(surfaceOutput, ai->m_surface1ImpactDirectGain);


				if (res1) {
					if (_tryActivateResIfInactive(res1))
						res1->getInput()->add(surfaceOutput);
				}

				if (res2) {
					if (_tryActivateResIfInactive(res2))
						res2->getInput()->addWithMultiply(surfaceOutput, (paFloat)ai->m_surface1ImpactCrossGain);
				}

				if (!gen1->isQuiet())
					impactHasFinished = false;
				else {		// Shutdown this generator.
					surface1->deleteImpactGen(ai->m_impactGen1);
					ai->m_surface1 = 0;
				}
			}


			if (surface2 && gen2) {

				gen2->setRelTangentSpeedAtImpact(ai->m_dynamicData.relTangentSpeedAtImpact);
				gen2->setRelNormalSpeedAtImpact(ai->m_dynamicData.relNormalSpeedAtImpact);
				gen2->setImpactImpulse(ai->m_dynamicData.impactImpulse);

				gen2->setOutput(surfaceOutput);
				gen2->tick();

				// Direct output.
				_output->addWithMultiply(surfaceOutput, ai->m_surface2ImpactDirectGain);

				if (res2) {
					if (_tryActivateResIfInactive(res2))
						res2->getInput()->add(surfaceOutput);
				}

				if (res1) {
					if (_tryActivateResIfInactive(res1))
						res1->getInput()->addWithMultiply(surfaceOutput, (paFloat)ai->m_surface2ImpactCrossGain);
				}

				if (!gen2->isQuiet())
					impactHasFinished = false;
				else {		// Shutdown this generator.
					surface2->deleteImpactGen(ai->m_impactGen2);
					ai->m_surface2 = 0;
				}


			}

			// Delete the impact when both impact generators are off.
			if (impactHasFinished)
			{
				paImpact::pool.deleteActiveObject(ai->m_poolHandle);
//				printf("d");
			}

		}
	}


	//// Tick resonators.

	paRes::activeResList.firstMember();
	paBlock* resOutput = paBlock::newBlock();		// Temp buffer
	while( (res = paRes::activeResList.getNextMember()) != 0 )
	{
		res->setOutput(resOutput);
		res->tick();

		if (res->isQuiet()) { //! maybe important, or relic from pre gen->isQuiet?? :     && !res->isInContact()) {
			resOutput->fadeout();	// Fade over one block to smooth out stop-glitch.
			res->deactivate();
			_timeCost -= res->getTimeCost();
			//! Potential bug if cost of res changes while res activated.
			//! Would be better if load management were in res, with state record.

			paBlock::deleteBlock(res->m_input);	// Free input block to the pool.
		}

		if (_multipleOutputCallback) _multipleOutputCallback(res, resOutput->getStart());
		else _output->add(resOutput);
	}

	paBlock::deleteBlock(resOutput);

//	_output->multiplyBy(0.5f);


	// Remove DC due to random nature of collisions.
//	_highpass->setGain(1.0);
//	_highpass->setCutoffFreq(1.0);
//	_highpass->tick(_output,_output);

//	_lowpass->setGain(1.0);
//	_lowpass->setCutoffFreq(1.0);
//	_lowpass->tick(_output, _output);

	return _output;
}


