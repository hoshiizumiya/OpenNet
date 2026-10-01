# NATMap integration

Reference: `hoshiizumiya/natmap` at `31d46801a4d868c84d6bee5660ee34ba83ddc889` (MIT license). This branch implements the UDP and TCP bind, STUN keepalive, and local forwarding primitives as native Windows code. It does not copy the upstream implementation or require its MSYS2 runtime and `hev-task-system` submodule.

## Current UDP data path

1. OpenNet binds an **exclusive, dedicated UDP port** on `0.0.0.0`. A value of zero asks Windows to allocate an available port. The target is a **different** local port. A target value of zero follows libtorrent's current IPv4 uTP listener for the lifetime of the mapping; it pauses forwarding when that listener is unavailable.
2. From the mapping socket, a STUN binding request is sent every 25 seconds to the configured UDP server (default: `turn.cloudflare.com:3478`). The reply must come from the selected STUN endpoint, match the 96-bit transaction ID, and contain a valid mapped IPv4 endpoint. This is an observation of the mapping toward that STUN server, **not** a guarantee that another peer can reach it; endpoint dependent NATs may map other destinations differently.
3. In automatic torrent-target mode, only datagrams with a valid uTP v1 base header and packet type are relayed to `127.0.0.1:<target>`. DHT and UDP tracker traffic is deliberately excluded because a normal UDP relay cannot preserve the external source endpoint that those protocols consume as identity. A manually configured target remains a generic UDP relay. Each external endpoint has a separate connected local socket, so uTP sees a stable loopback endpoint for that relay entry and replies return from the mapping port to the correct external endpoint. Up to 32 peers are retained, with a two-minute inactivity timeout.
4. The existing NAT tools port check can probe the actual mapped UDP port through OpenNet's traversal servers. A positive external probe confirms reachability separately from the STUN observation; a negative probe can be inconclusive if the local service does not answer the probe payload.
5. The Windows Firewall action explicitly creates an inbound **UDP only** rule for the current executable and bound port, scoped to active network profiles. A fixed mapping port is required so this durable rule does not silently point to a different port on the next launch. The removal action checks that the rule still names this executable. Neither action runs automatically. Router rules and upstream NAT filtering remain outside Windows Firewall's control.

The mapping service is process scoped, so changing pages does not stop the session. Stop the session with **Stop mapping**, or exit OpenNet. It does not alter the libtorrent listening port, existing UPnP/NAT-PMP settings, downloads, or WebUI configuration.

## Current TCP worker

The native TCP worker follows the Windows socket sequence proven by the prerequisite test: it opens an HTTP keepalive connection from one specific local IPv4 address and port, performs TCP STUN from that same source endpoint, then binds an inbound listener to the shared port. Accepted connections are forwarded bidirectionally to a loopback target such as WebUI. Pending data is bounded to 64 KiB per direction, idle relays expire, and the relay count is capped at 30 so the listener, keeper and two sockets per relay remain below Winsock's 64-entry `select` set.

The worker is exposed through an explicit manual section on the existing NAT tools page. The MVVM/`x:Bind` surface requires a specific local IPv4 address, target, keeper and TCP STUN endpoints before it calls `Start`; it never starts merely because WebUI is running. After a validated manual start, the page remembers only those input parameters. Loading the page never writes defaults, and a later process launch restores neither the worker's running state nor firewall authorization. The page shows worker status, active relay count and the observed endpoint, and keeps a persistent warning beside the controls: shared-port `SO_REUSEADDR` cannot provide strong exclusivity, the target should require authentication, no TCP firewall rule is created automatically, and STUN does not prove public reachability. A separate explicit action can create or remove a TCP-only Windows Firewall rule after the worker is running on the same configured fixed port. The rule name, protocol, port, direction, action and executable path are verified before an existing rule is enabled or removed; UDP and TCP rules have separate identities. Network-change notifications rebuild the keepalive/STUN/listener group and clear old relays; application shutdown stops and joins it before WebUI and the torrent core.

## Why this is code-level integration

The upstream command-line program uses POSIX networking, the `hev-task-system` submodule, and an MSYS2 Windows build. Linking it as an OpenNet MSVC submodule would not expose a stable in-process API, process lifecycle, or WinRT state. The Windows-native module here implements the UDP behavior with Winsock, while the page uses an observable C++/WinRT ViewModel and `x:Bind` commands.

## Remaining integration work

- **UDP source identity**: the forwarded uTP datagram still appears to libtorrent as originating from `127.0.0.1` and an ephemeral port. The stable per-peer relay endpoint satisfies uTP's connection lookup, but peer accounting and any address-based policy see loopback rather than the real peer. DHT is not relayed. If true identity preservation is required, integrate at libtorrent's socket layer instead of extending the relay.
- **Session lifecycle**: adapter changes, torrent listener stop/restart and Windows resume notifications are handled without rebinding the requested mapping port. Real sleep/resume still needs Windows-device validation. TCP input parameters are remembered only after an explicit start; mapping state and firewall authorization are intentionally never restored.
- **Discoverability**: decide how to advertise a working mapped address and port to trackers/DHT. A successful STUN exchange alone must never overwrite the BitTorrent listen endpoint.
- **Other natmap options**: IPv6, multiple STUN endpoints with fallback, external target forwarding, DNS/script notification, and configurable keepalive interval are not implemented here. An in-process notification API should replace scripts if these are added.

## Windows verification checklist

1. Build Debug and Release x64 and ARM64 in Visual Studio; check XAML compilation and generated WinRT metadata.
2. Start a torrent core, set mapping port to `0` and target to `0`, and start mapping. Confirm distinct listening and mapping ports, STUN public endpoint, and no impact to existing UPnP/NAT-PMP.
3. Run the external port probe, then test a real UDP peer through a reachable network. Compare results on endpoint independent and symmetric NATs; do not treat STUN success as reachability.
4. Start with a deliberately occupied mapping port. Confirm a visible bind error and no interference with the existing listener.
5. Create/remove the UDP and TCP firewall rules as an administrator; repeat without permission and confirm the failure is shown. Check their distinct names, exact executable, protocol, local port, and profile in Windows Defender Firewall. Confirm an automatic-port TCP mapping cannot create a durable rule.
6. Navigate away and back, stop the torrent core, restart OpenNet, then sleep and resume Windows before shutting it down. Confirm a running UDP/TCP worker clears its stale observation and rebuilds, while a stopped worker remains stopped. Inspect the mapping socket and any user-created firewall rule.

## Automated verification and continuation

The feature branch was updated from `master` at `0f6d402` on 2026-10-01 (Hong Kong time). The resource merge retains both master and mapping strings. The native mapping service now serializes Start/Stop, cancels asynchronous DNS lookup on shutdown, and moves joining to a ViewModel background coroutine. The UI disables duplicate lifecycle commands while busy. A manually configured UDP target does not require the torrent engine.

`tests/natmap` is a standalone CMake harness without WinUI, vcpkg or public STUN dependencies:

- `stun_protocol_tests`: real packet parser, IPv4/IPv6 XOR masks, legacy MAPPED address, transaction/cookie checks, truncation, padding, malformed trailing attributes, zero ports and 20,000 generated malformed messages. Linux CI runs ASan and UBSan.
- `udp_mapping_tests`: compiles the production Winsock service and firewall implementation. A local fake STUN server and targets verify separate UDP/TCP firewall rule identities, two separate uTP peers, return traffic through the mapping port, rejection of DHT/empty packets in automatic mode, generic and empty datagrams in manual mode, automatic target loss/replacement, expiry under sustained invalid STUN traffic, occupied ports, stop/rebind/restart and DNS cancellation. It never modifies Windows Firewall.
- `tcp_port_reuse_tests`: executes the exact Windows TCP prerequisite used by upstream NATMap. One specific loopback source port simultaneously owns an HTTP keepalive connection, TCP STUN connection, and inbound listener; the test parses the mapped endpoint and carries a WebUI-shaped request and response through a separate target connection. It also verifies that an exclusive bind is rejected while the shared mapping port exists.
- `tcp_mapping_tests`: compiles the production TCP worker. Local HTTP keeper, TCP STUN and echo targets verify the published mapped endpoint, bounded bidirectional relay, application-style network recovery with a fresh generation, and synchronous stop/state cleanup without any public service dependency.
- `.github/workflows/natmap-tests.yml`: Linux parser tests; Windows Debug/Release x64 execution and ARM64 compilation. ARM64 binaries cannot run on the x64 hosted runner.
- Existing Canary remains the full XAML/WinRT/application package build for Release x64 and ARM64. Dependency, application and unit-test build logs are saved as separate artifacts, with at most 200 trailing lines printed per step. Lightweight test logs and JUnit reports are also downloadable artifacts.

Local parser verification in the Linux workspace passed with GCC C++20, AddressSanitizer and UndefinedBehaviorSanitizer. Leak detection alone is disabled locally because the workspace restricts `/proc` access; CI retains the standard sanitizer settings. Windows socket tests and full app builds require their Actions runners.

Commands:

```sh
cmake -S tests/natmap -B out/natmap -DCMAKE_BUILD_TYPE=Debug -DNATMAP_SANITIZERS=ON
cmake --build out/natmap
ctest --test-dir out/natmap --output-on-failure
```

An enabled task runs approximately every 80 minutes. It checks the exact branch HEAD and latest job/step status, repairs concrete failures, and advances the remaining implementation once required checks pass. It does not repeatedly retrigger pending runs or merge to master. For large failed logs, download the artifact and use `tail -n 300` or `rg -n -i -C 4 'error|fatal|failed|exception'` on the file. If only decoded connector logs are available, filter within the tool call before returning output. Update this checkpoint after meaningful progress.

## Checkpoint — 2026-10-01

Reviewed exact remote HEAD `cf8933f9fd92396409d3960f21d89051e2389c04`:

- [NATMap tests run 36762287283](https://github.com/hoshiizumiya/OpenNet/actions/runs/36762287283): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=2`, `failures=0`, `skipped=0` for `stun_protocol` and `udp_mapping`; these were executed, not merely compiled.
- [Canary run 36762287138](https://github.com/hoshiizumiya/OpenNet/actions/runs/36762287138): both Release x64 and ARM64 failed in `Build MSIX`. Downloaded both log artifacts and inspected only error contexts. The first application error is `WMC1110: Property 'ViewModel' not found on type 'NatToolsPage'`; all 13 binding errors share this cause.

`NatToolsPage.xaml.h` already implemented `ViewModel()`, but `NatToolsPage.idl` omitted the property. The XAML compiler resolves these `x:Bind` paths using the WinRT metadata produced from IDL, so a C++ getter alone is insufficient. This revision imports `ViewModels/NatMappingViewModel.idl` and declares the read-only `OpenNet.ViewModels.NatMappingViewModel ViewModel` property, matching the other MVVM pages. The ViewModel, commands and binding paths remain unchanged.

Local review confirms that both IDL files are included in the project's MIDL inputs and that the property type matches the existing getter. The Linux workspace cannot run the Windows XAML compiler.

Corrective remote HEAD `36bd36d254d345217a3b94e709fc6023f95fa5db` is fully green:

- [NATMap tests run 36777785478](https://github.com/hoshiizumiya/OpenNet/actions/runs/36777785478): Linux ASan/UBSan, Windows x64 Debug/Release execution, and ARM64 Debug/Release compilation all succeeded.
- [Canary run 36777785584](https://github.com/hoshiizumiya/OpenNet/actions/runs/36777785584): complete application/MSIX Release builds succeeded for both x64 and ARM64. This verifies the IDL metadata correction through the real MIDL, C++/WinRT, XAML compiler, native linker, and packaging pipeline.

The following lifecycle revision adds application-wide network recovery. `App` subscribes once to `NetworkInformation.NetworkStatusChanged`; its callback only posts a non-blocking atomic recovery request. The mapping worker then clears peer relay sockets selected on the old route, clears the stale STUN observation, re-resolves the configured STUN endpoint, immediately sends a new binding request, and retries resolution after a transient offline period. It retains the exclusive mapping socket and its local port, so an explicit fixed-port firewall rule does not silently become stale. Incoming packets are dropped while the new STUN route is unresolved, preventing delayed STUN responses from being forwarded into the torrent target as peer traffic. Application shutdown unregisters the network callback and explicitly stops/joins the mapping worker before shutting down the torrent core.

The Windows socket regression test now simulates recovery after an expired STUN observation and verifies that the mapping port is preserved, a new observation is published, and both relay directions resume.

The first network-recovery test run at `a0f498f1de6d6174b73f053de0b876d7a2d68b15` exposed a fixture ordering error in x64 Release: several pre-recovery keepalive requests were still queued at the fake STUN server, while the test replied to only one request and assumed it carried the new transaction ID. Production correctly rejected that old transaction, so the assertion timed out. The corrected fixture replies to a bounded backlog, proving that obsolete responses remain rejected and the post-recovery transaction is eventually accepted.

Corrective HEAD `ca3b6bcec40c1fb083353f9913eba569eddcbe60` is fully green:

- [NATMap tests run 36785715924](https://github.com/hoshiizumiya/OpenNet/actions/runs/36785715924): Linux ASan/UBSan, Windows x64 Debug/Release execution, and ARM64 Debug/Release compilation all succeeded.
- [Canary run 36785715883](https://github.com/hoshiizumiya/OpenNet/actions/runs/36785715883): complete application/MSIX Release x64 and ARM64 builds both succeeded.

The next lifecycle revision makes a target value of zero a persistent automatic mode rather than a one-time lookup. An application-lifetime background monitor reads the synchronized IPv4 uTP listen status (not the generic TCP-preferred port). When the core stops or has no listener, it sends target port zero and the mapping worker pauses peer forwarding while retaining its bound mapping socket and STUN observation. When the core restarts or chooses a different port, the worker atomically switches the loopback destination and clears every per-peer socket created for the old listener. Manual target mappings ignore these updates. The monitor is stopped and joined before the mapping service and P2P core during application shutdown.

The Windows socket test exercises target loss and replacement: it pauses the automatic target, resumes on a second UDP listener, confirms the previous peer route is rebuilt, and verifies the reply still leaves through the preserved mapping port. This does not change libtorrent `announce_port`; an observed STUN endpoint is still not treated as proof of public reachability.

Automatic-target HEAD `cfa3a9d603779b037f4408128b080e02c721c156` produced these exact checks:

- [NATMap tests run 36806504145](https://github.com/hoshiizumiya/OpenNet/actions/runs/36806504145): all five jobs succeeded. Linux ASan/UBSan passed; Windows x64 Debug/Release executed the native socket tests; ARM64 Debug/Release compiled them.
- [Canary run 36806504169](https://github.com/hoshiizumiya/OpenNet/actions/runs/36806504169): both full application Release builds failed at the same compile line. `NatMappingViewModel.cpp:151` used `winrt::resume_foreground(DispatcherQueue)`, but this project does not provide that C++/WinRT overload (`C2039`/`C3861`). The repository's established DispatcherQueue coroutine adapter is `winrtplus::resume_foreground` from `winrtplus_coroutine`; this correction uses that adapter and does not return to `apartment_context`.

Coroutine-adapter correction HEAD `d3d6cc572054a1c63bec549d37e0addd2fa93000` is fully green:

- [NATMap tests run 36811819218](https://github.com/hoshiizumiya/OpenNet/actions/runs/36811819218): Linux ASan/UBSan, Windows x64 Debug/Release execution, and ARM64 Debug/Release compilation all succeeded.
- [Canary run 36811819107](https://github.com/hoshiizumiya/OpenNet/actions/runs/36811819107): complete application/MSIX Release x64 and ARM64 builds both succeeded, including MIDL, C++/WinRT, XAML, native link and packaging.

The source-endpoint audit used the exact declared dependency baseline, libtorrent `v2.1.2` at `6da363d2994f17c0b3c0450d124cf73a31a73847`. Its UDP dispatcher gives [uTP first access, then DHT and UDP trackers](https://github.com/arvidn/libtorrent/blob/6da363d2994f17c0b3c0450d124cf73a31a73847/src/session_impl.cpp#L2758-L2779). uTP requires a 20-byte version-1 header, creates incoming connections from SYN packets, and [matches later packets by connection ID plus the exact source address and port](https://github.com/arvidn/libtorrent/blob/6da363d2994f17c0b3c0450d124cf73a31a73847/src/utp_stream.cpp#L215-L224). The relay's one connected local socket per external endpoint therefore gives uTP a stable identity for that entry, although it is a loopback identity.

DHT cannot safely cross this relay. Incoming requests [publish the observed source endpoint as `ip`, add it to the routing table, and mirror its port](https://github.com/arvidn/libtorrent/blob/6da363d2994f17c0b3c0450d124cf73a31a73847/src/kademlia/node.cpp#L807-L829); the source would be `127.0.0.1:<ephemeral>`, not the remote node. Libtorrent also [skips BEP42 node-ID verification for local addresses](https://github.com/arvidn/libtorrent/blob/6da363d2994f17c0b3c0450d124cf73a31a73847/src/kademlia/node_id.cpp#L157-L166), and classifies `127/8` as local. The automatic mode now accepts only uTP v1 candidate packets before allocating a peer relay. The visible status says uTP rather than UDP. Manual targets retain the general UDP behavior explicitly selected by the user.

uTP-boundary HEAD `b141dd3a18fe1ce4889407051591db395ba53c1d` is fully green:

- [NATMap tests run 36818507587](https://github.com/hoshiizumiya/OpenNet/actions/runs/36818507587): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=2`, `failures=0`, `skipped=0`; the `udp_mapping` output explicitly confirms the uTP-only automatic and generic manual relay paths executed.
- [Canary run 36818507710](https://github.com/hoshiizumiya/OpenNet/actions/runs/36818507710): complete application/MSIX Release x64 and ARM64 builds both succeeded, including MIDL, C++/WinRT, XAML, native link and packaging.

The next TCP/WebUI prerequisite follows upstream NATMap's actual order: establish an HTTP keepalive connection from a selected specific local port, connect TCP STUN from that same port, then listen for inbound connections on it. Microsoft's [Winsock port-sharing documentation](https://learn.microsoft.com/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse) confirms that `SO_REUSEADDR` permits the repeated bind but warns that shared-port behavior and protection from port hijacking are not guaranteed; `SO_EXCLUSIVEADDRUSE` is intentionally incompatible with the scheme. The Windows test makes both the functional path and this security limit executable instead of assuming POSIX reuse semantics. It does not expose the WebUI or add a firewall rule.

TCP prerequisite HEAD `6eec1266b2bea54633f1136ffb706ddf44fce872` is fully green:

- [NATMap tests run 36832287297](https://github.com/hoshiizumiya/OpenNet/actions/runs/36832287297): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=3`, `failures=0`, `skipped=0`; `tcp_port_reuse` actually executed alongside the parser and UDP production tests.
- [Canary run 36832287271](https://github.com/hoshiizumiya/OpenNet/actions/runs/36832287271): the complete application/MSIX Release x64 and ARM64 builds both succeeded.

Production TCP-worker HEAD `31799016bc05e77c01e512358710be2ab04c12fa` is fully green:

- [NATMap tests run 36841271629](https://github.com/hoshiizumiya/OpenNet/actions/runs/36841271629): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=4`, `failures=0`, `skipped=0`; `tcp_mapping` actually executed with the parser, UDP production and TCP port-reuse tests.
- [Canary run 36841271562](https://github.com/hoshiizumiya/OpenNet/actions/runs/36841271562): complete application/MSIX Release x64 and ARM64 builds both succeeded, including MIDL, C++/WinRT, XAML, native link and packaging.

Explicit TCP/WebUI UI HEAD `7d884421afde5cecdec5c59fe0521f07b0ecddc0` is fully green:

- [NATMap tests run 36867514970](https://github.com/hoshiizumiya/OpenNet/actions/runs/36867514970): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=4`, `failures=0`, `skipped=0`, including the production TCP worker.
- [Canary run 36867515166](https://github.com/hoshiizumiya/OpenNet/actions/runs/36867515166): complete application/MSIX Release x64 and ARM64 builds both succeeded, verifying every new generated WinRT property and `x:Bind` path.

Explicit TCP firewall HEAD `a4028f666b38d9bdf702d61bb40fb3040aa16475` is fully green:

- [NATMap tests run 36882538857](https://github.com/hoshiizumiya/OpenNet/actions/runs/36882538857): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=4`, `failures=0`, `skipped=0`; `udp_mapping` executed the production firewall descriptor checks and confirmed separate UDP/TCP rule identities.
- [Canary run 36882538841](https://github.com/hoshiizumiya/OpenNet/actions/runs/36882538841): complete application/MSIX Release x64 and ARM64 builds both succeeded.

This revision implements the TCP persistence policy without turning remembered input into remembered exposure. A new non-mutating `LocalSetting::TryGet` avoids writing defaults when the page is merely opened. A validated manual start writes the local address, mapping/target ports, keepalive endpoint and STUN endpoint behind a version marker committed last; malformed, partial or mistyped storage is ignored. No running-state or firewall-authorization key exists, so every new process requires a fresh explicit Start and any firewall change remains a separate user action. Real external uTP validation, sleep/resume device validation and IPv6 remain separate. Never advertise or overwrite libtorrent endpoints from a STUN observation alone.

Manual TCP-parameter persistence HEAD `979c07f7f7320ffe99276d2abaf2221bcb914ee0` is fully green:

- [NATMap tests run 36896733368](https://github.com/hoshiizumiya/OpenNet/actions/runs/36896733368): all five jobs succeeded. The downloaded x64 Debug JUnit report records `tests=4`, `failures=0`, `skipped=0`; parser, UDP mapping, TCP port reuse and production TCP mapping all executed.
- [Canary run 36896733730](https://github.com/hoshiizumiya/OpenNet/actions/runs/36896733730): complete application/MSIX Release x64 and ARM64 builds both succeeded. The x64 native-unit build also succeeded.

This revision closes the missing process-lifetime resume signal. `App` subscribes to Windows App SDK `PowerManager.SystemSuspendStatusChanged` and requests recovery only for `AutoResume` or `ManualResume`; `Entering` and `Uninitialized` do nothing. The request is the same atomic generation rebuild used for adapter changes. It cannot start a stopped service because each later explicit `Start` clears pending recovery, and it does not touch saved parameters or firewall rules. Shutdown and destruction unregister both network and power callbacks before stopping the workers. Real device sleep/resume validation, external uTP reachability and IPv6 remain separate.
