// Begin the libtorrent include boundary. Deliberately no #pragma once: a
// translation unit may contain more than one isolated third-party block.
//
// XAML-generated sources force-include windows.h, whose min/max macros must not
// leak into libtorrent declarations. Preserve the caller's macro state so this
// boundary does not silently change later Win32 code in the same translation
// unit. Pair every inclusion with LibtorrentIncludeRestore.h.
#pragma push_macro("min")
#pragma push_macro("max")

#undef min
#undef max
