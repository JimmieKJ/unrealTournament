//
// paLowpass.hpp
//
// Note we can have the input and output pointing to the same
// paBlock
//


#if !defined (__paLowpass_hpp)
#define __paLowpass_hpp


#include "Signal/paBlock.hpp"



class PHYA_API paLowpass
{
private:

	paBlock* m_input;
	paBlock* m_output;

	// Buffered 'set' variables, for thread-safe operation, and efficiency.
	paFloat m_g_old;			// Gain at 0Hz.
	paFloat m_g;		// If gain changes, must interpolate to this value to avoid artifacts.
	paFloat m_b_old;			// Filter coefficients.
	paFloat m_b;

	paFloat m_cutoffFreq;
	// Used to signal coefficient update at start of next block.
	bool m_updateCutoffFreq;


	paFloat m_y;		// Sample output in the last frame.

	static paFloat twoPiDivFrameRate;
	static paFloat twoDivFrameRate;

public:
	paLowpass();
	~paLowpass();

	void setInput(paBlock* input) { m_input = input; };
	void setOutput(paBlock* output) { m_output = output; };  // Output can be set to input.
	void setIO(paBlock* buffer) { m_input = buffer; m_output = buffer; }

	void setPole(paFloat b) {
		m_b = b;
	}

	void setGain(paFloat g) {		// Gain is interpolated over the block.
		m_g = g;
	};

	void reset() {
//		m_g_old = 0.0f;
		m_y = 0.0f;  // Reset internal filter state!!
	};

	// Set -3dB cutoff frequency.
	void setCutoffFreq(paFloat f) {
		m_cutoffFreq = f;
		m_updateCutoffFreq = true;
	}

	// Filter one paBlock.
	paBlock* tick();
	paBlock* tick( paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tick();
	}

	paBlock* getOutput() { return m_output; };
};



#endif


