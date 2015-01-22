// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __BUSYCURSOR_H__
#define __BUSYCURSOR_H__

/**
 * While in scope, sets the cursor to the busy (hourglass) cursor for all windows.
 */
class UNREALED_API FScopedBusyCursor
{
public:
	FScopedBusyCursor();
	~FScopedBusyCursor();
};

#endif // __BUSYCURSOR_H__
