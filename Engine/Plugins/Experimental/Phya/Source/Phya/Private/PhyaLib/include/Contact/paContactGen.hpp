//
// paContactGen.hpp
//
// Abstract base class for generating sound from a continuous contact.
// Referenced by active paContact objects.
//



#if !defined(__paContactGen_hpp)
#define __paContactGen_hpp


#include "Signal/paBlock.hpp"


class PHYA_API paContactGen
{
protected:
	paBlock* m_output;

	paFloat m_quietSpeed;
	paFloat m_gain;
	paFloat m_limit;

	paFloat m_speedContactRelBody;
	paFloat m_speedBody1RelBody2;
	paFloat m_contactForce;

	bool m_isActive;

public:


	paContactGen()
	{
		m_poolHandle = -1;
		m_output = 0;
		m_speedContactRelBody = 0;
		m_speedBody1RelBody2 = 0;
		m_contactForce = 0;
	}

	virtual ~paContactGen() {}

	virtual paBlock* tick() = 0;



	int m_poolHandle;		// Handle used by the scene contact manager to release the object.

	virtual bool isActive() = 0;	// Indicates if gen is currently generating output. Should wake gen if should be generating output.
	virtual bool isQuiet() = 0;		// Indicates when gen is quiet enough to be faded out and made inactive.
	virtual int setInactive() { m_isActive = false; return 0; };


	// The following could possibly be replaced with automatic param update
	// in gen->tick(), using a refernence to the owning paContact.

	void setSpeedContactRelBody(paFloat s) {
		m_speedContactRelBody = s; };

	void setSpeedBody1RelBody2(paFloat s) {
		m_speedBody1RelBody2 = s; };

	void setContactForce(paFloat f) {
		m_contactForce = f; }

	void setGain(paFloat g) {
		m_gain = g; }

	void setAmpMax(paFloat l) {
		m_limit = l; }


	virtual void setOutput(paBlock* output) = 0;	// Virtual so that internal settings can be made.
	paBlock* getOutput() { return m_output; };
};



#endif

