#pragma once

// Stable Win32 preprocessing boundary. Include this before Windows SDK headers
// in non-XAML translation units; generated XAML sources receive windows.h from
// the project target before XamlWorkaround.h.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOHELP
#define NOHELP
#endif
#ifndef NOCOMM
#define NOCOMM
#endif

#include <windows.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
