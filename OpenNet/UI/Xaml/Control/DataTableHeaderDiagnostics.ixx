module;
#include "WindowsPlatform.h"

export module OpenNet.UI.Xaml.Control.DataTableHeaderDiagnostics;

import std;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Numerics;
import winrt.XamlToolkit.Labs.WinUI;

export namespace OpenNet::UI::Xaml::Control
{
    // Read-only instrumentation. No Measure/Arrange, width writes, layout
    // invalidation, rounding changes, or Tag/DataContext changes are allowed.
    struct DataTableHeaderDiagnostics
    {
        static void Attach(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column, winrt::hstring const& key);

    private:
        struct Probe : std::enable_shared_from_this<Probe>
        {
            winrt::weak_ref<winrt::XamlToolkit::Labs::WinUI::DataColumn> Column;
            winrt::hstring Key;
            std::vector<winrt::weak_ref<winrt::Microsoft::UI::Xaml::FrameworkElement>> Elements;
            bool SnapshotQueued{};

            void Write(std::wstring_view message) const noexcept
            {
                try
                {
                    const auto column = Column.get();
                    const auto id = column ? reinterpret_cast<std::uintptr_t>(winrt::get_abi(column)) : 0;
                    const auto line = std::format(L"[ON-DTH] t={} id={:x} header={} {}\n", GetTickCount64(), id, std::wstring_view{ Key }, message);
                    OutputDebugStringW(line.c_str());
                }
                catch (...) {}
            }

            void Snapshot(std::wstring_view reason) noexcept
            {
                try
                {
                    using namespace winrt::Microsoft::UI::Xaml;
                    using namespace winrt::Microsoft::UI::Xaml::Controls;
                    const auto column = Column.get();
                    if (!column) return;
                    const auto width = column.DesiredWidth();
                    Write(std::format(L"snapshot={} DesiredWidth={:.17g} unit={} ActualColumnWidth={:.17g}", reason, width.Value, static_cast<int>(width.GridUnitType), column.ActualColumnWidth()));
                    for (const auto& weakElement : Elements)
                    {
                        const auto element = weakElement.get();
                        if (!element) continue;
                        const auto slot = Primitives::LayoutInformation::GetLayoutSlot(element);
                        const auto desired = element.DesiredSize();
                        const auto actual = element.ActualSize();
                        const auto root = element.XamlRoot();
                        Write(std::format(L"  type={} name={} Width={:.17g} ActualWidth={:.17g} ActualSize.X={:.9g} DesiredSize.W={:.9g} slot=(x={:.9g},w={:.9g}) margin=({:.17g},{:.17g}) rounding={} scale={:.17g}", std::wstring_view{ winrt::get_class_name(element) }, std::wstring_view{ element.Name() }, element.Width(), element.ActualWidth(), actual.x, desired.Width, slot.X, slot.Width, element.Margin().Left, element.Margin().Right, element.UseLayoutRounding(), root ? root.RasterizationScale() : 0.0));
                        if (const auto text = element.try_as<TextBlock>())
                        {
                            Write(std::format(L"    text={} trimmed={} padding=({:.17g},{:.17g}) fontSize={:.17g} fontWeight={} alignment={}", std::wstring_view{ text.Text() }, text.IsTextTrimmed(), text.Padding().Left, text.Padding().Right, text.FontSize(), text.FontWeight().Weight, static_cast<int>(text.TextAlignment())));
                        }
                        if (const auto control = element.try_as<Controls::Control>())
                        {
                            Write(std::format(L"    contentAlignment={} padding=({:.17g},{:.17g}) border=({:.17g},{:.17g})", static_cast<int>(control.HorizontalContentAlignment()), control.Padding().Left, control.Padding().Right, control.BorderThickness().Left, control.BorderThickness().Right));
                        }
                        if (const auto grid = element.try_as<Grid>())
                        {
                            for (uint32_t index = 0; index < grid.ColumnDefinitions().Size(); ++index)
                            {
                                const auto definition = grid.ColumnDefinitions().GetAt(index);
                                Write(std::format(L"    gridColumn={} ActualWidth={:.17g} configured={:.17g} unit={}", index, definition.ActualWidth(), definition.Width().Value, static_cast<int>(definition.Width().GridUnitType)));
                            }
                        }
                    }
                }
                catch (...) { Write(L"snapshot failed; diagnostics did not modify layout"); }
            }

            void QueueSnapshot() noexcept
            {
                try
                {
                    const auto column = Column.get();
                    if (!column || SnapshotQueued) return;
                    SnapshotQueued = true;
                    const auto self = shared_from_this();
                    if (!column.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [self]()
                    {
                        self->SnapshotQueued = false;
                        self->Snapshot(L"after-events");
                    })) SnapshotQueued = false;
                }
                catch (...) { SnapshotQueued = false; }
            }

            void ObserveTree(winrt::Microsoft::UI::Xaml::DependencyObject const& node)
            {
                using namespace winrt::Microsoft::UI::Xaml;
                using namespace winrt::Microsoft::UI::Xaml::Controls;
                if (const auto element = node.try_as<FrameworkElement>())
                {
                    const bool exists = std::ranges::any_of(Elements, [&](const auto& weak) { return weak.get() == element; });
                    const bool relevant = element == Column.get() || element.try_as<Button>() || element.try_as<Grid>() || element.try_as<ContentPresenter>() || element.try_as<TextBlock>();
                    if (!exists && relevant)
                    {
                        Elements.push_back(winrt::make_weak(element));
                        const auto self = shared_from_this();
                        element.SizeChanged([self](auto const& sender, auto const& args)
                        {
                            try
                            {
                                self->Write(std::format(L"event=SizeChanged type={} old=({:.9g},{:.9g}) new=({:.9g},{:.9g})", std::wstring_view{ winrt::get_class_name(sender) }, args.PreviousSize().Width, args.PreviousSize().Height, args.NewSize().Width, args.NewSize().Height));
                                self->QueueSnapshot();
                            }
                            catch (...) {}
                        });
                        element.RegisterPropertyChangedCallback(FrameworkElement::WidthProperty(), [self](auto const& sender, auto const&)
                        {
                            try
                            {
                                const auto current = sender.template as<FrameworkElement>();
                                self->Write(std::format(L"event=Width-property-changed type={} name={} Width={:.17g}", std::wstring_view{ winrt::get_class_name(current) }, std::wstring_view{ current.Name() }, current.Width()));
                                self->QueueSnapshot();
                            }
                            catch (...) {}
                        });
                        if (const auto text = element.try_as<TextBlock>())
                        {
                            text.IsTextTrimmedChanged([self](auto const& sender, auto const&)
                            {
                                try
                                {
                                    const auto current = sender.template as<TextBlock>();
                                    self->Write(std::format(L"event=IsTextTrimmedChanged text={} trimmed={}", std::wstring_view{ current.Text() }, current.IsTextTrimmed()));
                                    self->QueueSnapshot();
                                }
                                catch (...) {}
                            });
                        }
                    }
                }
                const auto count = Media::VisualTreeHelper::GetChildrenCount(node);
                for (int index = 0; index < count; ++index) ObserveTree(Media::VisualTreeHelper::GetChild(node, index));
            }
        };
    };

    void DataTableHeaderDiagnostics::Attach(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column, winrt::hstring const& key)
    {
        if (!column) return;
        const auto probe = std::make_shared<Probe>();
        probe->Column = winrt::make_weak(column);
        probe->Key = key;
        column.Loaded([probe](auto const&, auto const&)
        {
            try
            {
                if (const auto current = probe->Column.get()) probe->ObserveTree(current);
                probe->Snapshot(L"Loaded");
                probe->QueueSnapshot();
            }
            catch (...) { probe->Write(L"Loaded diagnostic failed"); }
        });
        column.Unloaded([probe](auto const&, auto const&) { probe->Snapshot(L"Unloaded"); });
        column.RegisterPropertyChangedCallback(winrt::XamlToolkit::Labs::WinUI::DataColumn::DesiredWidthProperty(), [probe](auto const&, auto const&)
        {
            probe->Snapshot(L"DesiredWidth-property-changed");
            probe->QueueSnapshot();
        });
    }
}
