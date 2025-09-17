#include "pch.h"

// This file provides the required WinRT DLL export functions for the OpenNet project.
// These functions are needed when the project defines runtime classes in IDL files.

// Global module instance for WinRT factory caching
static winrt::com_ptr<winrt::impl::module_reference> s_module;

// Initialize the global module reference
void InitializeModule()
{
    if (!s_module)
    {
        s_module = winrt::make_self<winrt::impl::module_reference>();
    }
}

// DLL export: Determines whether the DLL can be safely unloaded
// This is called by the system to check if any objects are still in use
STDAPI_(BOOL) WINRT_CanUnloadNow()
{
    // Initialize module if not already done
    InitializeModule();
    
    // The module can be unloaded if no objects are being used
    return s_module->can_unload_now();
}

// DLL export: Gets the activation factory for a given runtime class
// This is called by the system when creating instances of runtime classes
STDAPI WINRT_GetActivationFactory(HSTRING classId, IActivationFactory** factory)
{
    // Initialize module if not already done
    InitializeModule();
    
    try
    {
        // Let WinRT handle the activation factory resolution
        return s_module->get_activation_factory(classId, factory);
    }
    catch (...)
    {
        // Return the current exception as HRESULT
        return winrt::to_hresult();
    }
}