/************************************************************************************

Filename    :   IVRManager.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package android.app;

import android.graphics.Bitmap;

/** {@hide} */
public interface IVRManager
{
	/**
	 * Service identification string. Use this to get an instance to the global VR system service
	 * IVRManager vr = (IVRManager)getSystemService( IVRManager.VR_MANAGER );
	 * {@hide}
	 */
	public static String VR_MANAGER = "vr";
	
	/**
	 * Option strings used by set and get option methods. VRManager will synchronize and cache global VR user
	 * settings for any application present on the device.
	 * {@hide}
	 */
	public static String VR_OPTION_IPD = "ipd"; // IPD expressed in millimeters
	
	/**
	 * Option strings used by set and get system option methods.
	 * {@hide}
	 */
	public static String VR_COMFORT_VIEW = "comfortable_view";
	public static String VR_BRIGHTNESS = "bright";
	public static String VR_DO_NOT_DISTURB = "do_not_disturb_mode";

	/**
	 * Flag to monitor state of manager
	 * @return true if an application is presently in VR
	 * {@hide}
	 */
	abstract public boolean isActive();
	
	/**
	 * @return A service version number, expressed as major, minor, subversion, e.g. "2.5.0"
	 * {@hide}
	 */
	abstract public String vrManagerVersion();
	
	/**
	 * @return A LibOVR version number, expressed as major, minor, subversion, e.g. "2.5.0"
	 * {@hide}
	 */
	abstract public String vrOVRVersion();
	
	/**
	 * Heartbeat notification for VR activity. If the application fails to call this within
	 * a particular timeframe, VRManager will force kill the application.
	 * This is necessary to prevent SCHED_FIFO elevated threads from potentially
	 * locking up the device completely.
	 * @param pkg String identifying app package
	 * @param activeInVR Boolean used to acknowledge an application's prescence within
	 *        the VR mode. When the application is backgrounded this should be called
	 *        as false at least once
	 * {@hide} 
	 */
	abstract public void reportApplicationInVR( String pkg, boolean activeInVR );
	
	/**
	 * SCHED_FIFO thread management
	 * FIXME: Should be hidden within general setup up/tear down code. 
	 * @param pkg String identifying app package 
	 * @param pid Process id (In Java, obtain via Process.myPid())
	 * @param tid Thread id (In Java, obtain view Process.myTid())
	 * @param prio Priority. For demote, set to 0. For elevate, SCHED_FIFO levels between 1 and 3
	 * @return true if successfully changed the thread priority
	 * {@hide}
	 */
	abstract public boolean setThreadSchedFifo( String pkg, int pid, int tid, int prio );
	
	/**
	 * OLD SCHED_FIFO thread management
	 * FIXME: Should be hidden within general setup up/tear down code. 
	 * @param pkg String identifying app package 
	 * @param pid Process id (In Java, obtain via Process.myPid())
	 * @param tid Thread id (In Java, obtain view Process.myTid())
	 * @param prio Priority. For demote, nice values between -20 and 20. For elevate, SCHED_FIFO levels between 0 and 3
	 * @return true if successfully changed the thread priority
	 * {@hide}
	 */
	abstract public boolean elevateProcessThread( String pkg, int pid, int tid, int prio );
	/** {@hide} */
	abstract public boolean demoteProcessThread( String pkg, int pid, int tid, int prio );	
	
	//==========================================================
	// Fixed Clock Level API for S5 G906S (1440): 
	
	/**
	 * setFreq - lock the CPU/GPU frequency level - DEPRECATED for Note 4 (S5 - G906S only)
	 * @param pkg String identifying app package
	 * @param CPU Frequency Level
	 * @param GPU Frequency Level
	 * @return true on success
	 * {@hide}
	 */	
	abstract public boolean setFreq( String pkg, int gpuFreqLev, int cpuFreqLev );
	
	//==========================================================	
	// Fixed Clock Level API for Note4
	
	/**
	 * setVrClocks - lock the CPU/GPU frequency level
	 * @param pkg String identifying app package
	 * @param CPU Frequency Level
	 * @param GPU Frequency Level
	 * @return [CPU CLOCK, GPU CLOCK, POWERSAVE CPU CLOCK, POWERSAVE GPU CLOCK]
	 * {@hide}
	 */
	abstract public int[] SetVrClocks( String pkg, int cpuLevel, int gpuLevel );
	/**
	 * relFreq - release frequency lock
	 * @param pkg String identifying app package
	 * @return true if successfully released frequency lock
	 * {@hide}
	 */	
	abstract public boolean relFreq( String pkg );
	/**
	 * return2EnableFreqLev - return available level values
	 * @return [GPU MIN, GPU MAX, CPU MIN, CPU MAX]
	 * {@hide}
	 */		
	abstract public int[] return2EnableFreqLev();
	
	//==========================================================	
	
	/**
	 * GetPowerLevelState
	 * @return the power level state, where: 0 = Normal State 1 = Power Save State 2 = Minimum State
	 * {@hide}
	 */	
	abstract public int GetPowerLevelState();
	
	/**
	 * Generic global option accessors. See the VR_OPTION defines for a list of possible
	 * option parameters
	 * @param optionName
	 * @param value
	 * {@hide} 
	 */
	abstract public void setOption( String optionName, String value );
	/** {@hide} */
	abstract public String getOption( String optionName );
	
	/**
	 * System option accessors.See the VR_OPTION defines for a list of possible
	 * option parameters
	 * @param optionName
	 * @param value
	 * {@hide} 
	 */	
	abstract public void setSystemOption( String optionName, String value );
	
	abstract public String getSystemOption( String optionName );	
	
	/**
	 * Function to composite system and dialog layers beneath the application for proper projection within VR.
	 * This can be called at any time to obtain a renderable bitmap. If no dialogs are ready to be displayed, the bitmap will be completely clear.
	 * @return Returns a transparent bitmap containing the current displayed system notifications and dialogs underneath the VR application
	 * {@hide} 
	 */
	abstract public Bitmap compositeSystemNotifications();	
}
