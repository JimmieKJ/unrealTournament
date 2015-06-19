/************************************************************************************

Filename    :   OVR_Android_DeviceManager.h
Content     :   Android implementation of DeviceManager.
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Android_DeviceManager.h"

// Sensor & HMD Factories
#include "OVR_LatencyTestDeviceImpl.h"
#include "OVR_SensorDeviceImpl.h"
#include "OVR_Android_HIDDevice.h"
#include "OVR_Android_HMDDevice.h"

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_Log.h"

#include <jni.h>

jobject gRiftconnection;

namespace OVR { namespace Android {

//-------------------------------------------------------------------------------------
// **** Android::DeviceManager

DeviceManager::DeviceManager()
{
}

DeviceManager::~DeviceManager()
{    
}

bool DeviceManager::Initialize(DeviceBase*)
{
    if (!DeviceManagerImpl::Initialize(0))
        return false;

    pThread = *new DeviceManagerThread();
    if (!pThread || !pThread->Start())
        return false;

    // Wait for the thread to be fully up and running.
    pThread->StartupEvent.Wait();

    // Do this now that we know the thread's run loop.
    HidDeviceManager = *HIDDeviceManager::CreateInternal(this);
         
    pCreateDesc->pDevice = this;
    LogText("OVR::DeviceManager - initialized.\n");
    return true;
}

void DeviceManager::Shutdown()
{   
    LogText("OVR::DeviceManager - shutting down.\n");

    // Set Manager shutdown marker variable; this prevents
    // any existing DeviceHandle objects from accessing device.
    pCreateDesc->pLock->pManager = 0;

    // Push for thread shutdown *WITH NO WAIT*.
    // This will have the following effect:
    //  - Exit command will get enqueued, which will be executed later on the thread itself.
    //  - Beyond this point, this DeviceManager object may be deleted by our caller.
    //  - Other commands, such as CreateDevice, may execute before ExitCommand, but they will
    //    fail gracefully due to pLock->pManager == 0. Future commands can't be enqued
    //    after pManager is null.
    //  - Once ExitCommand executes, ThreadCommand::Run loop will exit and release the last
    //    reference to the thread object.
    pThread->PushExitCommand(false);
    pThread.Clear();

    DeviceManagerImpl::Shutdown();
}

ThreadCommandQueue* DeviceManager::GetThreadQueue()
{
    return pThread;
}

ThreadId DeviceManager::GetThreadId() const
{
    return pThread->GetThreadId();
}

int DeviceManager::GetThreadTid() const
{
    return pThread->GetThreadTid();
}

void DeviceManager::SuspendThread() const
{
    pThread->SuspendThread();
}

void DeviceManager::ResumeThread() const
{
    pThread->ResumeThread();
}

bool DeviceManager::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_Manager) &&
        (info->InfoClassType != Device_None))
        return false;
    
    info->Type    = Device_Manager;
    info->Version = 0;
    OVR_strcpy(info->ProductName, DeviceInfo::MaxNameLength, "DeviceManager");
    OVR_strcpy(info->Manufacturer,DeviceInfo::MaxNameLength, "Oculus VR, LLC");
    return true;
}

DeviceEnumerator<> DeviceManager::EnumerateDevicesEx(const DeviceEnumerationArgs& args)
{
    // TBD: Can this be avoided in the future, once proper device notification is in place?
    pThread->PushCall((DeviceManagerImpl*)this,
                      &DeviceManager::EnumerateAllFactoryDevices, true);

    return DeviceManagerImpl::EnumerateDevicesEx(args);
}


//-------------------------------------------------------------------------------------
// ***** DeviceManager Thread 

DeviceManagerThread::DeviceManagerThread()
    : Thread(ThreadStackSize),
      Suspend( false )
{
    int result = pipe(CommandFd);
	OVR_UNUSED( result );	// no warning
    OVR_ASSERT(!result);

    AddSelectFd(NULL, CommandFd[0]);
}

DeviceManagerThread::~DeviceManagerThread()
{
    if (CommandFd[0])
    {
        RemoveSelectFd(NULL, CommandFd[0]);
        close(CommandFd[0]);
        close(CommandFd[1]);
    }
}

bool DeviceManagerThread::AddSelectFd(Notifier* notify, int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN|POLLHUP|POLLERR;
    pfd.revents = 0;

    FdNotifiers.PushBack(notify);
    PollFds.PushBack(pfd);

    OVR_ASSERT(FdNotifiers.GetSize() == PollFds.GetSize());
    LogText( "DeviceManagerThread::AddSelectFd %d (Tid=%d)\n", fd, GetThreadTid() );
    return true;
}

bool DeviceManagerThread::RemoveSelectFd(Notifier* notify, int fd)
{
    // [0] is reserved for thread commands with notify of null, but we still
    // can use this function to remove it.

    LogText( "DeviceManagerThread::RemoveSelectFd %d (Tid=%d)\n", fd, GetThreadTid() );
    for (UPInt i = 0; i < FdNotifiers.GetSize(); i++)
    {
        if ((FdNotifiers[i] == notify) && (PollFds[i].fd == fd))
        {
            FdNotifiers.RemoveAt(i);
            PollFds.RemoveAt(i);
            return true;
        }
    }
    LogText( "DeviceManagerThread::RemoveSelectFd failed %d (Tid=%d)\n", fd, GetThreadTid() );
    return false;
}

static int event_count = 0;
static double event_time = 0;

int DeviceManagerThread::Run()
{
    ThreadCommand::PopBuffer command;

    SetThreadName("OVR::DeviceMngr");

    LogText( "DeviceManagerThread - running (Tid=%d).\n", GetThreadTid() );
    
    // needed to set SCHED_FIFO
    DeviceManagerTid = gettid();

    // Signal to the parent thread that initialization has finished.
    StartupEvent.SetEvent();

    while(!IsExiting())
    {
        // PopCommand will reset event on empty queue.
        if (PopCommand(&command))
        {
            command.Execute();
        }
        else
        {
            bool commands = false;
            do
            {
                int waitMs = INT_MAX;

                // If devices have time-dependent logic registered, get the longest wait
                // allowed based on current ticks.
                if (!TicksNotifiers.IsEmpty())
                {
                    double timeSeconds = Timer::GetSeconds();
                    int    waitAllowed;

                    for (UPInt j = 0; j < TicksNotifiers.GetSize(); j++)
                    {
                        waitAllowed = (int)(TicksNotifiers[j]->OnTicks(timeSeconds) * Timer::MsPerSecond);
                        if (waitAllowed < (int)waitMs)
                        {
                            waitMs = waitAllowed;
                        }
                    }
                }

                nfds_t nfds = PollFds.GetSize();
                if (Suspend)
                {
                    // only poll for commands when device polling is suspended
                    nfds = Alg::Min(nfds, (nfds_t)1);
                    // wait no more than 100 milliseconds to allow polling of the devices to resume
                    // within 100 milliseconds to avoid any noticeable loss of head tracking
                    waitMs = Alg::Min(waitMs, 100);
                }

                // wait until there is data available on one of the devices or the timeout expires
                int n = poll(&PollFds[0], nfds, waitMs);

                if (n > 0)
                {
                    // Iterate backwards through the list so the ordering will not be
                    // affected if the called object gets removed during the callback
                    // Also, the HID data streams are located toward the back of the list
                    // and servicing them first will allow a disconnect to be handled
                    // and cleaned directly at the device first instead of the general HID monitor
                    for (int i = nfds - 1; i >= 0; i--)
                    {
                    	const short revents = PollFds[i].revents;

                    	// If there was an error or hangup then we continue, the read will fail, and we'll close it.
                    	if (revents & (POLLIN | POLLERR | POLLHUP))
                        {
                            if ( revents & POLLERR )
                            {
                                LogText( "DeviceManagerThread - poll error event %d (Tid=%d)\n", PollFds[i].fd, GetThreadTid() );
                            }
                            if (FdNotifiers[i])
                            {
                                event_count++;
                                if ( event_count >= 500 )
                                {
                                    const double current_time = Timer::GetSeconds();
                                    const int eventHz = (int)( event_count / ( current_time - event_time ) + 0.5 );
                                    LogText( "DeviceManagerThread - event %d (%dHz) (Tid=%d)\n", PollFds[i].fd, eventHz, GetThreadTid() );
                                    event_count = 0;
                                    event_time = current_time;
                                }

                                FdNotifiers[i]->OnEvent(i, PollFds[i].fd);
                            }
                            else if (i == 0) // command
                            {
                                char dummy[128];
                                read(PollFds[i].fd, dummy, 128);
                                commands = true;
                            }
                        }

                        if (revents != 0)
                        {
                            n--;
                            if (n == 0)
                            {
                                break;
                            }
                        }
                    }
                }
                else
                {
                    if ( waitMs > 1 && !Suspend )
                    {
                        LogText( "DeviceManagerThread - poll(fds,%d,%d) = %d (Tid=%d)\n", nfds, waitMs, n, GetThreadTid() );
                    }
                }
            } while (PollFds.GetSize() > 0 && !commands);
        }
    }

    LogText( "DeviceManagerThread - exiting (Tid=%d).\n", GetThreadTid() );
    return 0;
}

bool DeviceManagerThread::AddTicksNotifier(Notifier* notify)
{
     TicksNotifiers.PushBack(notify);
     return true;
}

bool DeviceManagerThread::RemoveTicksNotifier(Notifier* notify)
{
    for (UPInt i = 0; i < TicksNotifiers.GetSize(); i++)
    {
        if (TicksNotifiers[i] == notify)
        {
            TicksNotifiers.RemoveAt(i);
            return true;
        }
    }
    return false;
}

void DeviceManagerThread::SuspendThread()
{
    Suspend = true;
    LogText( "DeviceManagerThread - Suspend = true\n" );
}

void DeviceManagerThread::ResumeThread()
{
    Suspend = false;
    LogText( "DeviceManagerThread - Suspend = false\n" );
}

} // namespace Android


//-------------------------------------------------------------------------------------
// ***** Creation


// Creates a new DeviceManager and initializes OVR.
DeviceManager* DeviceManager::Create()
{
    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "DeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Android::DeviceManager> manager = *new Android::DeviceManager;

    if (manager)
    {
        if (manager->Initialize(0))
        {
            manager->AddFactory(&LatencyTestDeviceFactory::GetInstance());
            manager->AddFactory(&SensorDeviceFactory::GetInstance());
            manager->AddFactory(&Android::HMDDeviceFactory::GetInstance());

            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }

    }    

    return manager.GetPtr();
}


} // namespace OVR

