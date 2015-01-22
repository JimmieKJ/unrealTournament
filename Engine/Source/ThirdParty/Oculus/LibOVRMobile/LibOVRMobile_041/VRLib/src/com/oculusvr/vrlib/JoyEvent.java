/************************************************************************************

Filename    :   JoyEvent.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

/**
 * Joypad button events generate keyCodes that overlap
 * keyboard codes, but you will usually want to treat
 * them differently, so we add a flag to them, allowing
 * code like:
 * 
 * if ( keyCode == KeyEvent.KEYCODE_SPACE || joyCode == JoyEvent.KEYCODE_A )
 *
 * The DPAD events are synthesized from hat axis changes. 
 */
public class JoyEvent {
    static public final int BUTTON_JOYPAD_FLAG = 0x10000;
    
	static public final int KEYCODE_GAME			= 0 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_A 				= 96 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_B 				= 97 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_X 				= 99 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_Y 				= 100 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_START 			= 108 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_BACK 			= 4 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_SELECT 			= 109 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_MENU 			= 82 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_RIGHT_TRIGGER	= 103 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_LEFT_TRIGGER	= 102 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_DPAD_UP 		= 19 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_DPAD_DOWN 		= 20 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_DPAD_LEFT 		= 21 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_DPAD_RIGHT 		= 22 | BUTTON_JOYPAD_FLAG;

	static public final int KEYCODE_LSTICK_UP 		= 200 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_LSTICK_DOWN 	= 201 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_LSTICK_LEFT 	= 202 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_LSTICK_RIGHT	= 203 | BUTTON_JOYPAD_FLAG;

	static public final int KEYCODE_RSTICK_UP 		= 204 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_RSTICK_DOWN 	= 205 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_RSTICK_LEFT 	= 206 | BUTTON_JOYPAD_FLAG;
	static public final int KEYCODE_RSTICK_RIGHT 	= 207 | BUTTON_JOYPAD_FLAG;
	
}
