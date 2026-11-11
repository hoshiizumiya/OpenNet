# The registry port only relocates crashpad_handler. Native also installs a helper executable in bin; package it as a vcpkg tool, including dependencies, and remove its Debug copy using vcpkg's standard packaging helper.
if(PORT STREQUAL "sentry-native")
    vcpkg_copy_tools(TOOL_NAMES sentry-crash AUTO_CLEAN)
endif()
