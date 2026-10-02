#pragma once

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.XamlToolkit.WinUI.Interactivity;

namespace OpenNet::Helpers::PageMemoryDiagnostics
{
    inline std::atomic_bool& EnabledState() noexcept
    {
        static std::atomic_bool enabled{ false };
        return enabled;
    }

    inline bool Enabled() noexcept { return EnabledState().load(std::memory_order_relaxed); }

    // UI-thread-only diagnostics. Neither the records nor the delayed probes own a page/control.
    struct ObjectRecord
    {
        winrt::weak_ref<winrt::Windows::Foundation::IInspectable> object;
        std::string type;
    };

    struct Visit
    {
        std::string page;
        std::unordered_map<void*, ObjectRecord> objects;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer timer{ nullptr };
        bool retired{};
        std::chrono::steady_clock::time_point retiredAt{};
        unsigned int probe{};
        std::size_t omitted{};
        std::size_t failed{};
    };

    struct State
    {
        std::uint64_t nextId{};
        std::map<std::uint64_t, Visit> visits;
        std::ofstream log;
        std::filesystem::path logPath;

        State()
        {
            wchar_t temporaryPath[MAX_PATH]{};
            auto length = GetTempPathW(MAX_PATH, temporaryPath);
            if (!length || length >= MAX_PATH) return;
            logPath = std::filesystem::path{ temporaryPath } / std::format(L"OpenNet-memory-{}.log", GetCurrentProcessId());
        }

        void OpenLog()
        {
            if (log.is_open() || logPath.empty()) return;
            std::error_code error;
            auto const directory = logPath.parent_path();
            auto const cutoff = std::filesystem::file_time_type::clock::now() - std::chrono::days{ 14 };
            for (std::filesystem::directory_iterator it{ directory, error }, end; !error && it != end; it.increment(error))
            {
                auto const name = it->path().filename().wstring();
                if (!name.starts_with(L"OpenNet-memory-") || (it->path() == logPath)) continue;
                auto const modified = it->last_write_time(error);
                if (!error && modified < cutoff) std::filesystem::remove(it->path(), error);
                error.clear();
            }
            log.open(logPath, std::ios::out | std::ios::app);
            if (log) OutputDebugStringW((L"OpenNet memory report: " + logPath.wstring() + L"\n").c_str());
        }

        void RotateLogIfNeeded()
        {
            constexpr std::uintmax_t maximumBytes = 1024 * 1024;
            if (!log.is_open() || log.tellp() < 0 || static_cast<std::uintmax_t>(log.tellp()) < maximumBytes) return;
            log.close();
            std::error_code error;
            auto first = logPath; first += L".1";
            auto second = logPath; second += L".2";
            std::filesystem::remove(second, error); error.clear();
            if (std::filesystem::exists(first, error))
            {
                std::filesystem::rename(first, second, error); error.clear();
            }
            if (std::filesystem::exists(logPath, error))
            {
                std::filesystem::rename(logPath, first, error); error.clear();
            }
            log.open(logPath, std::ios::out | std::ios::trunc);
        }
    };

    inline State& GetState()
    {
        static State state;
        return state;
    }

    // Called once at startup and whenever the user changes the persistent setting.
    inline void SetEnabled(bool enabled) noexcept
    {
        try
        {
            auto& state = GetState();
            if (!enabled)
            {
                EnabledState().store(false, std::memory_order_relaxed);
                for (auto& [id, visit] : state.visits) if (visit.timer) visit.timer.Stop();
                state.visits.clear();
                if (state.log.is_open()) state.log.close();
                return;
            }
            state.OpenLog();
            EnabledState().store(true, std::memory_order_relaxed);
        }
        catch (...) { EnabledState().store(false, std::memory_order_relaxed); }
    }

    inline void Report(std::uint64_t id, std::string_view stage) noexcept
    {
        try
        {
            auto& state = GetState();
            auto found = state.visits.find(id);
            if (found == state.visits.end()) return;
            auto const& visit = found->second;
            std::map<std::string, std::pair<std::size_t, std::size_t>> counts;
            for (auto const& [identity, record] : visit.objects)
            {
                auto& [alive, observed] = counts[record.type];
                ++observed;
                if (record.object.get()) ++alive;
            }

            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            bool measured = K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE;
            auto elapsed = visit.retired ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - visit.retiredAt).count() : 0;
            std::string line = std::format("OpenNet memory: page={} visit={} stage={} elapsed_ms={} memory_valid={} private_bytes={} working_set={} omitted={} failed={} alive/observed:", visit.page, id, stage, elapsed, measured, memory.PrivateUsage, memory.WorkingSetSize, visit.omitted, visit.failed);
            for (auto const& [type, count] : counts) line += std::format(" {}={}/{}", type, count.first, count.second);
            line += '\n';
            OutputDebugStringA(line.c_str());
            if (state.log)
            {
                state.RotateLogIfNeeded();
                state.log << line;
                state.log.flush();
            }
        }
        catch (...) {}
    }

    inline std::uint64_t Begin(std::string_view page) noexcept
    {
        if (!Enabled()) return 0;
        try
        {
            auto& state = GetState();
            // Keep at most four outgoing visits. Weak references retain small control blocks too.
            std::size_t retired = std::count_if(state.visits.begin(), state.visits.end(), [](auto const& entry) { return entry.second.retired; });
            while (retired > 3)
            {
                auto oldest = std::find_if(state.visits.begin(), state.visits.end(), [](auto const& entry) { return entry.second.retired; });
                if (oldest->second.timer) oldest->second.timer.Stop();
                state.visits.erase(oldest);
                --retired;
            }
            state.OpenLog();
            auto id = ++state.nextId;
            state.visits[id].page = page;
            Report(id, "created");
            return id;
        }
        catch (...) { return 0; }
    }

    inline void Watch(std::uint64_t id, winrt::Windows::Foundation::IInspectable const& object) noexcept
    {
        if (!id || !object) return;
        try
        {
            auto& visits = GetState().visits;
            auto found = visits.find(id);
            if (found == visits.end()) return;
            auto& visit = found->second;
            auto inspectable = object.as<winrt::Windows::Foundation::IInspectable>();
            auto identity = winrt::get_abi(inspectable);
            auto old = visit.objects.find(identity);
            if (old != visit.objects.end() && old->second.object.get()) return;
            if (old == visit.objects.end() && visit.objects.size() >= 4096)
            {
                ++visit.omitted;
                return;
            }
            auto name = winrt::to_string(winrt::get_class_name(inspectable));
            auto separator = name.find_last_of('.');
            if (separator != std::string::npos) name.erase(0, separator + 1);
            visit.objects.insert_or_assign(identity, ObjectRecord{ winrt::make_weak(inspectable), std::move(name) });
        }
        catch (...)
        {
            if (auto found = GetState().visits.find(id); found != GetState().visits.end()) ++found->second.failed;
        }
    }

    inline void WatchTree(std::uint64_t id, winrt::Microsoft::UI::Xaml::DependencyObject const& root) noexcept
    {
        if (!id || !root) return;
        try
        {
            using namespace winrt::Microsoft::UI::Xaml;
            using namespace winrt::XamlToolkit::WinUI::Interactivity;
            std::vector<DependencyObject> pending{ root };
            while (!pending.empty())
            {
                auto current = std::move(pending.back());
                pending.pop_back();
                auto name = winrt::to_string(winrt::get_class_name(current));
                constexpr std::array kinds{ ".DataRow", ".DataTable", ".DataColumn", ".HeaderedTreeView", ".TreeViewList", ".TreeViewItem", ".ScrollViewer", ".RangePieceBar", ".ProgressBar", ".ComboBox", ".ToolTip", ".Image", ".TaskFilesPage", ".TaskPeersListPage" };
                if (std::ranges::any_of(kinds, [&](auto suffix) { return name.ends_with(suffix); })) Watch(id, current);

                // Read the attached property directly: GetBehaviors would create a new collection.
                if (auto behaviors = current.GetValue(Interaction::BehaviorsProperty()).try_as<BehaviorCollection>())
                {
                    Watch(id, current);
                    Watch(id, behaviors);
                    for (auto const& behavior : behaviors) Watch(id, behavior);
                }
                if (auto image = current.try_as<Controls::Image>()) Watch(id, image.Source());
                if (auto element = current.try_as<FrameworkElement>())
                {
                    auto data = element.DataContext();
                    if (data)
                    {
                        auto type = winrt::to_string(winrt::get_class_name(data));
                        if (type.ends_with(".FileDisplayItem") || type.ends_with(".PeerDisplayItem")) Watch(id, data);
                    }
                }
                int children = Media::VisualTreeHelper::GetChildrenCount(current);
                for (int index = 0; index < children; ++index) pending.push_back(Media::VisualTreeHelper::GetChild(current, index));
            }
        }
        catch (...)
        {
            if (auto found = GetState().visits.find(id); found != GetState().visits.end()) ++found->second.failed;
        }
    }

    template<typename T>
    void WatchItems(std::uint64_t id, T const& items) noexcept
    {
        if (!id || !items) return;
        try
        {
            for (auto const& item : items)
            {
                Watch(id, item);
                if (item.IsFolder()) WatchItems(id, item.Children());
            }
        }
        catch (...) {}
    }

    inline void Retire(std::uint64_t id, winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher) noexcept
    {
        if (!id) return;
        try
        {
            auto& visits = GetState().visits;
            auto found = visits.find(id);
            if (found == visits.end() || found->second.retired) return;
            auto& visit = found->second;
            visit.retired = true;
            visit.retiredAt = std::chrono::steady_clock::now();
            Report(id, "unloaded");
            if (!dispatcher) return;
            visit.timer = dispatcher.CreateTimer();
            visit.timer.Interval(std::chrono::seconds{ 2 });
            visit.timer.IsRepeating(true);
            visit.timer.Tick([id](auto const&, auto const&)
            {
                auto& visits = GetState().visits;
                auto found = visits.find(id);
                if (found == visits.end()) return;
                auto& visit = found->second;
                Report(id, visit.probe == 0 ? "unloaded+2s" : "unloaded+10s");
                if (++visit.probe == 1) visit.timer.Interval(std::chrono::seconds{ 8 });
                else
                {
                    visit.timer.Stop();
                    visit.timer = nullptr;
                }
            });
            visit.timer.Start();
        }
        catch (...) {}
    }
}
