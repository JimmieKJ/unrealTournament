// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Sound quality preview helper thread.
 *
 */

#ifndef __SOUNDPREVIETHREAD_H__
#define __SOUNDPREVIETHREAD_H__

class FSoundPreviewThread : public FRunnable
{
public:
	/**
	 * @param PreviewCount	number of sounds to preview
	 * @param Node			sound node to perform compression/decompression
	 * @param Info			preview info class
	 */
	FSoundPreviewThread( int32 PreviewCount, USoundWave *Node, FPreviewInfo *Info );

	virtual bool Init( void );
	virtual uint32 Run( void );
	virtual void Stop( void );
	virtual void Exit( void );

	/**
	 * @return true if job is finished
	 */
	bool IsTaskFinished( void ) const 
	{ 
		return TaskFinished; 
	}

	/**
	 * @return number of sounds to conversion.
	 */
	int32 GetCount( void ) const 
	{ 
		return Count; 
	}

	/**
	 * @return index of last converted sound.
	 */
	int32 GetIndex( void ) const 
	{ 
		return Index; 
	}

private:
	/** Number of sound to preview.	*/
	int32					Count;					
	/** Last converted sound index, also using to conversion loop. */
	int32  Index;					
	/** Pointer to sound wave to perform preview. */
	USoundWave*			SoundWave;
	/** Pointer to preview info table. */
	FPreviewInfo*		PreviewInfo; 
	/** True if job is finished. */
	bool				TaskFinished;
	/** True if cancel calculations and stop thread. */
	bool				CancelCalculations;	
};
	
#endif
