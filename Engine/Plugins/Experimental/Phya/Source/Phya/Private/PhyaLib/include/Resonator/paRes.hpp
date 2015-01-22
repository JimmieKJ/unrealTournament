//
// paRes.hpp
//
// Abstract class from which resonators derive.
//



#if !defined(__paRes_hpp)
#define __paRes_hpp



#include "Signal/paBlock.hpp"
#include "Utility/paObjectList.hpp"

class paBody;



class paRes;
//template class PHYA_API paObjectList<paRes*>;		// For dll interface only.


class PHYA_API paRes
{

protected:
	friend PHYA_API paBlock* paTick();

	paBlock* m_input;
	paBlock* m_output;				// Output block currently in use.

	paFloat m_levelSqr;				// Last calculated level squared.

	int m_activityHandle;			// Used when deleting this resonator from the
									// resonator activity list in paScene.
	bool m_makeQuiet;				// Force resonator to be quiet. Processed in paTick();

	paFloat m_auxFreqScale;			// Adjusts for generic resonator properties.
	paFloat m_auxDampScale;
	paFloat m_auxAmpScale;

	paFloat m_contactDamping;		// Damping multiplier, used to accumulate damping from different contacts in paTick();
	paFloat m_maxContactDamping;	// Limit combined contact damping.
	paFloat m_contactDampingOld;	// Record of total contact damping, last frame.

	bool m_paramsChanged;			// Can be used eg to signal an update of resonator parameters in current frame.

	bool m_inContact;				// True if resonator has any continuous contacts.
									// Used to prevent premature quieting.

	void* m_userData;
	paBody* m_body;					// Points back to parent body.

public :


	static paObjectList<paRes*> activeResList;

	paRes();
	virtual ~paRes() { if( isActive() ) deactivate(); }

	virtual int zero() = 0;			// Zero internal audio state.

	void setInput(paBlock* input) { m_input = input; };
	paBlock* getInput() { return m_input; };

	void setOutput(paBlock* output) { m_output = output; };
	paBlock* getOutput() { return m_output; };
	void zeroOutput() { m_output->zero(); }

	// Used to determine when a resonator should be switched off.
	virtual int setQuietLevel(paFloat l) = 0;
	virtual bool isQuiet() = 0;

	// MakeQuiet is used to force a isQuiet() when next appropriate.
	int makeQuiet() { m_makeQuiet = true; return 0; }

	// Process input, put result in output.
	virtual paBlock* tick() = 0;
	virtual paBlock* tickAdd() = 0;

	paBlock* tick(paBlock* input) {
		m_input = input;
		return tick();
	}

	paBlock* tickAdd(paBlock* input) {
		m_input = input;
		return tickAdd();
	}

	paBlock* tick(paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tick();
	}

	paBlock* tickAdd(paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tickAdd();
	}

	int activate();
	bool isActive() { return (m_activityHandle != -1); }
	int deactivate();

	void* getUserData() { return m_userData; }
	int setUserData(void* d) { m_userData = d; return 0; }

	virtual int getTimeCost() = 0;

	// These parameters should do something reasonable in a particular resonator,
	// so that resonators can be interchanged.
	// Could be virtualized.

	virtual paFloat getAuxAmpScale(void) const { return m_auxAmpScale; }
	virtual int setAuxAmpScale(paFloat s) {
		if (m_auxAmpScale != s) {
			m_auxAmpScale = s;
			m_paramsChanged = true;
		}
		return 0;
	};

	paFloat getAuxFreqScale(void) const
	{
		return m_auxFreqScale;
	}

	int setAuxFreqScale(paFloat s) {
		if (m_auxFreqScale != s) {
			m_auxFreqScale = s;
			m_paramsChanged = true;
		}
		return 0;
	};

	paFloat getAuxDampScale(void) const
	{
		return m_auxDampScale;
	}

	int setAuxDampScale(paFloat s) {
		if (m_auxDampScale != s) {
			m_auxDampScale = s;
			m_paramsChanged = true;
		}
		return 0;
	};

	int setMaxContactDamping(paFloat d) { m_maxContactDamping = d; return 0; }
	int resetContactDamping() { m_contactDamping = 1.0f; m_paramsChanged = true; return 0; }
	int addContactDamping(paFloat d) { m_contactDamping *= d; return 0; }

	int updateContactDamping()		// If overall contact damping changed, signal param change.
	{
		if (m_contactDamping > m_maxContactDamping) m_contactDamping = m_maxContactDamping;
		if (m_contactDamping != m_contactDampingOld)
		{
			m_contactDampingOld = m_contactDamping;
			m_paramsChanged = true;
		}
		return 0;
	}

	bool isInContact() { return m_inContact; }
	void setInContact() { m_inContact = true; }
	void clearInContact() { m_inContact = false; }
};



#endif
