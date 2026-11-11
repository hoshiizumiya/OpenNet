# Sentry Native backend and deployment

OpenNet selects `SENTRY_BACKEND=native` in both custom Windows vcpkg triplets.
The dependency keeps the `backend` and `transport` features but disables default
features: the registry port's `wer` feature explicitly selects **Crashpad**, so
enabling it would override the intended backend. Native builds its own WER
module without that Crashpad feature.

The Native backend is also an out-of-process backend. Its runtime consists of
`sentry.dll`, `sentry-crash.exe` and, for Windows fast-fail reporting,
`sentry-wer.dll`. See the [Sentry Native backend documentation](https://docs.sentry.io/platforms/native/configuration/backends/native/).

## vcpkg integration

The registry port currently relocates only `crashpad_handler`. The custom
triplets add a post-portfile hook which uses `vcpkg_copy_tools` to move the
Native helper into `tools/sentry-native`, collect its runtime dependencies and
remove the duplicate Debug executable from `debug/bin`. This uses the existing
registry port rather than maintaining an overlay copy. vcpkg includes the
triplet and post-portfile contents in its ABI calculation, so restoring
dependencies rebuilds or retrieves a matching Native package.

MSBuild automatically imports `OpenNet/Directory.Build.targets`, which loads
`SentryNative.targets` only for OpenNet. vcpkg's normal app-local dependency
scan deploys `sentry.dll`; the additional target deploys the helper and WER
module that cannot be discovered through the import table. It also includes
them as MSIX content and records them as build outputs so incremental cleanup
does not remove them. There is no Sentry file list in `OpenNet.vcxproj`.

Run the normal dependency restore with `x64-windows-custom` or
`arm64-windows-custom` before the next build. An older installed Crashpad
package is insufficient: deployment fails clearly if Native runtime files
are missing. No change to the configured registry baseline or SDK minimum
version is required.

## Initialization and the supplied crashes

Sentry initializes on the main thread before notification registration and
activation redirection. The handler path explicitly names the app-local
`sentry-crash.exe`, independent of the working directory.

The supplied `OpenNet.exe.2628.dmp` records `0xC0000409`, fast-fail reason 7,
with HRESULT `0x8000FFFF`, during notification activation in
`AppInstance.GetActivatedEventArgs`. The Sentry initialization thread was
still in `ApplicationData.LocalCachePath`. Notification COM registration now
precedes activation-argument deserialization. Registration failure remains
nonfatal for ordinary launches; notification launches preserve the original
failure instead of continuing into invalid activation state.

The separate TreeView access violation in `OpenNet.exe.24612.dmp` has null
`sentry!g_options`, meaning the SDK was not enabled. An isolated initialization
check of the installed Crashpad SDK reproduced failure when its handler was
missing. XAML's UnhandledException event does not replace a native crash backend.

## Validation scope

No application or dependency compilation was performed for this change.
Build-free checks exercised both triplets and the vcpkg tool-relocation helper,
then the actual project's runtime collection/deployment targets for x64 and
ARM64, Debug and Release. They checked output copies, package metadata,
incremental preservation and rejection of missing Native runtime files using
inert fixtures. Live crash capture, WER callbacks and delivery to Sentry still
require running a freshly built application with the restored Native SDK.
