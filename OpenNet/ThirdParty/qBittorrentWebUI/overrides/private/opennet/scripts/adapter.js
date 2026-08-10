/* OpenNet compatibility adapter. This file intentionally stays outside upstream. */
"use strict";

(() => {
    if (window.__openNetWebUiAdapterLoaded) {
        window.dispatchEvent(new Event("opennet-webui-refresh"));
        return;
    }
    window.__openNetWebUiAdapterLoaded = true;

    const state = {
        capabilities: null,
        supported: new Set(),
    };

    document.documentElement.dataset.opennetWebui = "true";

    const markLabel = (control) => {
        if (!control.id) return;
        const label = document.querySelector(`label[for="${CSS.escape(control.id)}"]`);
        label?.classList.add("opennet-unsupported-label");
    };

    const applyPreferencesCapabilities = () => {
        const tabs = document.querySelectorAll(".PrefTab");
        if (!tabs.length || !state.capabilities) return;

        const noticeHost = document.getElementById("BehaviorTab") ?? tabs[0];
        if (noticeHost && !document.getElementById("opennet-preferences-notice")) {
            const notice = document.createElement("div");
            notice.id = "opennet-preferences-notice";
            notice.setAttribute("role", "status");
            notice.textContent = "OpenNet synchronizes enabled options with the native settings store. Disabled controls are qBittorrent-only. Address, port and language changes take effect after the WebUI host restarts.";
            noticeHost.prepend(notice);
        }

        for (const tab of tabs) {
            for (const control of tab.querySelectorAll("input, select, textarea, button")) {
                // qBittorrent has a number of id-less dialog buttons whose
                // operation is independent of server preferences. Never disable
                // those simply because they cannot be listed as a capability.
                if (!control.id) continue;
                if (state.supported.has(control.id)) {
                    control.dataset.opennetManaged = "true";
                    continue;
                }
                if (control.dataset.opennetManaged === "true") continue;
                if (!control.disabled) control.disabled = true;
                control.classList.add("opennet-unsupported");
                control.title = "This qBittorrent option is not managed by OpenNet.";
                markLabel(control);
            }
        }
    };

    const observer = new MutationObserver(() => {
        applyPreferencesCapabilities();
    });
    observer.observe(document.documentElement, {
        childList: true,
        subtree: true,
        attributes: true,
        attributeFilter: ["disabled"],
    });
    window.addEventListener("opennet-webui-refresh", () => {
        applyPreferencesCapabilities();
    });

    fetch("/api/v2/opennet/capabilities", {
        credentials: "same-origin",
        cache: "no-store",
    })
        .then((response) => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then((capabilities) => {
            state.capabilities = capabilities;
            state.supported = new Set([
                ...(capabilities.supportedControlIds ?? []),
                ...(capabilities.clientSideControlIds ?? []),
            ]);
            applyPreferencesCapabilities();
        })
        .catch((error) => console.warn("OpenNet WebUI adapter unavailable", error));
})();
