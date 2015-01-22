//
// paBlock.hpp
//
// Container class for audio blocks used in signal processing.
//
// This provides a friendly extra layer of indirection to the sample data,
// which is neccessary if we wish to establish signal connections between objects once,
// and later page-switch samples in the output paBlock of an object.
// The price of the extra indirection is not important since it is only incurred
// once per block operation.



#if !defined(__paBlock_hpp)
#define __paBlock_hpp



#include "Scene/paAudio.hpp"
#include "Utility/paObjectPool.hpp"


class PHYA_API paBlock;
//template class PHYA_API paObjectPool<paBlock>;		// For dll interface only.


class PHYA_API paBlock {

private:
	paFloat* m_samples;		// Location of this block's own samples.
	paFloat* m_start;			// Start of sample block, initially this block, but can point to other blocks.
//	bool m_zeroState;			// Experimental flag for reducing arithmetic operations. Probably more hassle than its worth.
				
public:

	static paObjectPool<paBlock> pool;
	static paBlock* newBlock();
	static int deleteBlock(paBlock*);
	static int nFrames;			// Working sample space can change size.
	static int nMaxFrames;	    // Changing this during execution could cause confusion.
	static int setnMaxFrames(int n);
	static int setnFrames(int n);

	int m_poolHandle;			// Used for freeing a block to the pool.
	
	paBlock();
	~paBlock();
	void zero();
//	void setZeroState(void) { m_zeroState = true; }	// Makes efficient buffer filling easy: If m_zeroState is set 'addWithMultiply' interpreted as 'copyWithMultiply' so 'zero' is avoided.
//	void clearZeroState(void) { m_zeroState = false; }	// Makes efficient buffer filling easy: If m_zeroState is set 'addWithMultiply' interpreted as 'copyWithMultiply' so 'zero' is avoided.
//	bool isZero() { return m_zeroState; }
	void copy(paBlock* input);			// Useful for creating extra 'buffer' blocks, when minimizing block memory usage.
	void multiplyBy(paFloat multfactor);	// Multiply block by a factor.
	void add(paBlock* toadd);			// Multiply toadd by multfactor then add to current value of this audio block.
	void addWithMultiply(paBlock* toadd, paFloat multfactor);		// Multiply toadd by multfactor then add to current value of this audio block.
	void copyWithMultiply(paBlock* tocopy, paFloat multfactor);	// Multiply 'tocopy' by 'multfactor' then copy to this audio block.
	void multiplyByNoise();
	paFloat* getStart() { return m_start; }
	void square();
	paFloat sum();				// Sum the samples in the block.
	void fadeout();
	void fillWithNoise();
	void fillWithNoise(paFloat amp);
	paFloat getLastSample() { return m_start[nFrames-1]; }
	void setStart(paFloat* s) { m_start = s; }
	void resetStart(void) { m_start = m_samples; }
	void limit();					// Ultra simple limiter. Better to use paLimiter

	void plot();
	void plot(paFloat scale);

};





#endif



