//
// paHighpass.hpp
//
// Complement to paLowpass.
// Note we can have the input and output pointing to the same
// paBlock
//


#if !defined (__paHighpass_hpp)
#define __paHighpass_hpp


#include "Signal/paBlock.hpp"
#include "Signal/paLowpass.hpp"


class PHYA_API paHighpass
{
private:

	paBlock* m_input;
	paBlock* m_output;

	paLowpass m_lowpass;
	paBlock m_temp;

public:
	paHighpass();
	~paHighpass();

	void setInput(paBlock* input) { m_input = input; };
	void setOutput(paBlock* output) { m_output = output; };  // Output can be set to input.

	void setGain(paFloat gain) {
		m_lowpass.setGain(gain);
	};

	void reset() {
		m_lowpass.reset();
	};

	void setCutoffFreq(paFloat f) {
		m_lowpass.setCutoffFreq(f);
	};

	// Filter one paBlock.
	paBlock* tick();
	paBlock* tick( paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tick();
	};

	paBlock* getOutput() { return m_output; };
};



#endif


