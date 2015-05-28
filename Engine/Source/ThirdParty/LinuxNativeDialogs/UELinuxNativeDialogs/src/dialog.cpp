#include "UNativeDialogs.h"

#include <dlfcn.h>


#define __lnd_xstr(s) __lnd_str(s)
#define __lnd_str(s) #s

#include <stdio.h>
#include <string.h>

class DialogBackend {
public:
    DialogBackend() {
        handle = nullptr;
        const char* chosenBackend = getenv("LND_BACKEND");
        
        auto loadBackend = [&]() {
            char backend_name[64];
            snprintf(backend_name, sizeof(backend_name), "libLND-%s.so", chosenBackend);
            printf("Trying to load: %s\n", backend_name);
            handle = dlopen(backend_name, RTLD_LAZY);
            if(!handle) {
                printf("LND could not load backend: %s\n", chosenBackend);
                return false;
            }

    #define FuncPointer(name) name = (decltype(&(::name)))dlsym(handle, __lnd_xstr(name));
            FuncPointer(ULinuxNativeDialogs_Initialize);
            FuncPointer(ULinuxNativeDialogs_Shutdown);
            FuncPointer(UFileDialog_Create);
            FuncPointer(UFileDialog_ProcessEvents);
            FuncPointer(UFileDialog_Destroy);
            FuncPointer(UFileDialog_Result);
            FuncPointer(UFontDialog_Create);
            FuncPointer(UFontDialog_ProcessEvents);
            FuncPointer(UFontDialog_Destroy);
            FuncPointer(UFontDialog_Result);

            if (!this->ULinuxNativeDialogs_Initialize()) {
                printf("LND loaded, but could not initialize backend: %s\n", chosenBackend);
                dlclose(handle);
                handle = nullptr;
            }
            
            printf("LND loaded backend: %s\n", chosenBackend);
            return true;
        };
        if(chosenBackend) {
            loadBackend();
        } else {

            const char* desktopEnvironment = getenv("XDG_CURRENT_DESKTOP");
            printf("LinuxNativeDialogs running on %s\n", (desktopEnvironment && strlen(desktopEnvironment) > 0) ? 
                desktopEnvironment : "some environment that doesn't set $XDG_CURRENT_DESKTOP");

            const char* gtkFirst[] = {"gtk3", "gtk2", "qt5", "qt4", nullptr};
            // try Qt4 first, as Qt5 for some reason crashes upon shutdown
            const char* qtFirst[] = {"qt4", "qt5", "gtk3", "gtk2", nullptr};

            const char** backends_iter = gtkFirst;
            // the list is incomplete, but should cover the major Qt-based desktop environments
            if (desktopEnvironment && (strcmp(desktopEnvironment, "KDE") == 0 || strcmp(desktopEnvironment, "Razor") == 0 ||
                strcmp(desktopEnvironment, "LXQt") == 0 || strcmp(desktopEnvironment, "Hawaii") == 0)) {
                backends_iter = qtFirst;
            }

            for(; *backends_iter ; backends_iter++) {
                chosenBackend = *backends_iter;
                if(loadBackend())
                    break;
            }
            if(!handle) {
                printf("Couldn't load any LND backend, functions will act like mock ones\n");
            }
        }
    }

    ~DialogBackend() {
        if (handle) {
            printf("Tearing down LND backend\n");
            this->ULinuxNativeDialogs_Shutdown();
            dlclose(handle);
            handle = nullptr;
        }
    }

    void* handle;

#undef FuncPointer
#define FuncPointer(name) decltype(&(::name)) name
    FuncPointer(ULinuxNativeDialogs_Initialize);
    FuncPointer(ULinuxNativeDialogs_Shutdown);
    FuncPointer(UFileDialog_Create);
    FuncPointer(UFileDialog_ProcessEvents);
    FuncPointer(UFileDialog_Destroy);
    FuncPointer(UFileDialog_Result);
    FuncPointer(UFontDialog_Create);
    FuncPointer(UFontDialog_ProcessEvents);
    FuncPointer(UFontDialog_Destroy);
    FuncPointer(UFontDialog_Result);
};

DialogBackend * backend = nullptr;

bool ULinuxNativeDialogs_Initialize() {
    backend = new DialogBackend;
    return backend != nullptr && backend->handle != nullptr;
}

void ULinuxNativeDialogs_Shutdown() {
    delete backend;	// safe to pass nullptr
}

UFileDialog* UFileDialog_Create(UFileDialogHints *hints)
{
    if(backend && backend->UFileDialog_Create) return backend->UFileDialog_Create(hints);
    return nullptr;
}

bool UFileDialog_ProcessEvents(UFileDialog* handle)
{
    if(backend && backend->UFileDialog_ProcessEvents) return backend->UFileDialog_ProcessEvents(handle);
    return false;
}

void UFileDialog_Destroy(UFileDialog* handle)
{
    if(backend && backend->UFileDialog_Destroy) backend->UFileDialog_Destroy(handle);
}

const UFileDialogResult* UFileDialog_Result(UFileDialog* handle)
{
    if(backend && backend->UFileDialog_Result) return backend->UFileDialog_Result(handle);
    return nullptr;
}

UFontDialog* UFontDialog_Create(UFontDialogHints *hints)
{
    if(backend && backend->UFontDialog_Create) return backend->UFontDialog_Create(hints);
    return nullptr;
}

bool UFontDialog_ProcessEvents(UFontDialog* handle)
{
    if(backend && backend->UFontDialog_ProcessEvents) return backend->UFontDialog_ProcessEvents(handle);
    return false;
}

void UFontDialog_Destroy(UFontDialog* handle)
{
    if(backend && backend->UFontDialog_Destroy) backend->UFontDialog_Destroy(handle);
}

const UFontDialogResult* UFontDialog_Result(UFontDialog* handle)
{
    if(backend && backend->UFontDialog_Result) return backend->UFontDialog_Result(handle);
    return nullptr;
}
