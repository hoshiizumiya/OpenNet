# NATMap integration

Reference: `hoshiizumiya/natmap` at `31d46801a4d868c84d6bee5660ee34ba83ddc889` (MIT license). This branch implements the UDP bind, STUN keepalive, and local forwarding behavior as native Windows code. It does not copy the upstream implementation or require its MSYS2 runtime and `hev-task-system` submodule.

## Current UDP data path

1. OpenNet binds an **exclusive, dedicated UDP port** on `0.0.0.0`. A value of zero asks Windows to allocate an available port. The target is a **different** local port, defaulting to libtorrent's current listening port.
2. From the mapping socket, a STUN binding request is sent every 25 seconds to the configured UDP server (default: `turn.cloudflare.com:3478`). The reply must come from the selected STUN endpoint, match the 96-bit transaction ID, and contain a valid mapped IPv4 endpoint. This is an observation of the mapping toward that STUN server, **not** a guarantee that another peer can reach it; endpoint dependent NATs may map other destinations differently.
3. Other UDP packets arriving at the mapping port are relayed to `127.0.0.1:<target>` through a separate local socket for each external endpoint. Replies on that local socket are sent back from the same mapping port to the original external endpoint. Up to 32 peers are retained, with a two-minute inactivity timeout.
4. The existing NAT tools port check can probe the actual mapped UDP port through OpenNet's traversal servers. A positive external probe confirms reachability separately from the STUN observation; a negative probe can be inconclusive if the local service does not answer the probe payload.
5. The Windows Firewall action explicitly creates an inbound **UDP only** rule for the current executable and bound port, scoped to active network profiles. A fixed mapping port is required so this durable rule does not silently point to a different port on the next launch. The removal action checks that the rule still names this executable. Neither action runs automatically. Router rules and upstream NAT filtering remain outside Windows Firewall's control.

The mapping service is process scoped, so changing pages does not stop the session. Stop the session with **Stop mapping**, or exit OpenNet. It does not alter the libtorrent listening port, existing UPnP/NAT-PMP settings, downloads, or WebUI configuration.

## Why this is code-level integration

The upstream command-line program uses POSIX networking, the `hev-task-system` submodule, and an MSYS2 Windows build. Linking it as an OpenNet MSVC submodule would not expose a stable in-process API, process lifecycle, or WinRT state. The Windows-native module here implements the UDP behavior with Winsock, while the page uses an observable C++/WinRT ViewModel and `x:Bind` commands.

## Remaining integration work

- **TCP**: implement a separate mapping/forward service for the HTTP/WebUI or BT TCP listener. Upstream TCP mode relies on binding a keepalive connection, TCP STUN connection, and inbound listener to the same local port. Windows port reuse, the target service's bind address, and firewall behavior must be tested with actual simultaneous sockets before enabling it.
- **UDP source identity**: the forwarded local datagram appears to libtorrent as originating from `127.0.0.1` and an ephemeral port. Verify its effect on uTP, DHT, peer accounting, and peer exchange with real peers. If identity preservation is required, integrate at libtorrent's socket layer instead of extending the relay.
- **Session lifecycle**: tie the mapping to torrent core stop/restart, adapter changes, sleep/resume, and saved user opt-in. Avoid automatically restoring a public firewall rule or a stale target port.
- **Discoverability**: decide how to advertise a working mapped address and port to trackers/DHT. A successful STUN exchange alone must never overwrite the BitTorrent listen endpoint.
- **Other natmap options**: IPv6, multiple STUN endpoints with fallback, TCP forwarding, external target forwarding, DNS/script notification, and configurable keepalive interval are not implemented here. An in-process notification API should replace scripts if these are added.

## Windows verification checklist

1. Build Debug and Release x64 and ARM64 in Visual Studio; check XAML compilation and generated WinRT metadata.
2. Start a torrent core, set mapping port to `0` and target to `0`, and start mapping. Confirm distinct listening and mapping ports, STUN public endpoint, and no impact to existing UPnP/NAT-PMP.
3. Run the external port probe, then test a real UDP peer through a reachable network. Compare results on endpoint independent and symmetric NATs; do not treat STUN success as reachability.
4. Start with a deliberately occupied mapping port. Confirm a visible bind error and no interference with the existing listener.
5. Create/remove a firewall rule as an administrator; repeat without permission and confirm the failure is shown. Check exact executable, protocol, local port, and profile in Windows Defender Firewall.
6. Navigate away and back, stop the torrent core, restart OpenNet, then shut it down; inspect the mapping socket and any user-created firewall rule.

## Automated verification and continuation

The feature branch was updated from `master` at `0f6d402` on 2026-10-01 (Hong Kong time). The resource merge retains both master and mapping strings. The native mapping service now serializes Start/Stop, cancels asynchronous DNS lookup on shutdown, and moves joining to a ViewModel background coroutine. The UI disables duplicate lifecycle commands while busy. A manually configured UDP target does not require the torrent engine.

`tests/natmap` is a standalone CMake harness without WinUI, vcpkg or public STUN dependencies:

- `stun_protocol_tests`: real packet parser, IPv4/IPv6 XOR masks, legacy MAPPED address, transaction/cookie checks, truncation, padding, malformed trailing attributes, zero ports and 20,000 generated malformed messages. Linux CI runs ASan and UBSan.
- `udp_mapping_tests`: compiles the production Winsock service and firewall implementation. A local fake STUN server and target verify two separate peers, return traffic through the mapping port, empty datagrams, expiry under sustained invalid STUN traffic, occupied ports, stop/rebind/restart and DNS cancellation. It never modifies Windows Firewall.
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

Local review confirms that both IDL files are included in the project's MIDL inputs and that the property type matches the existing getter. The Linux workspace cannot run the Windows XAML compiler. The corrective revision still needs its own full Canary run; the previous native test success does not establish application build success.

Next gate: check the corrective revision's exact HEAD and Release x64/ARM64 Canary results, repairing any newly exposed compiler/linker errors. Once the full app passes, investigate real libtorrent UDP identity semantics and torrent-core lifecycle before adding TCP/WebUI mapping or advertising mapped endpoints.
