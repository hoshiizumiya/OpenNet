#include "XamlWorkaround.h"
#include "MaterialControlBridge.h"
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Automation.Peers;
import winrt.Microsoft.UI.Xaml.Automation.Provider;
import winrt.WinUI3Package;
import winrt.OpenNet.UI.Xaml.Control.Theme;
namespace OpenNet::UI
{
	using namespace winrt;
	using namespace Microsoft::UI::Xaml;
	namespace C = Microsoft::UI::Xaml::Controls;
	namespace W = WinUI::LiquidGlass;
	namespace
	{
		struct Subscription
		{
			DependencyProperty Property; std::int64_t Token;
		};
		struct TemplateLink
		{
			weak_ref<C::Control> Native, Glass;
			std::vector<Subscription> Subscriptions;
			event_token FocusToken{};
			event_token SelectionToken{};
			std::shared_ptr<bool> Active{ std::make_shared<bool>(true) };
			Windows::Foundation::IInspectable OriginalTabNavigation{ nullptr };
			Data::Binding OriginalTabNavigationBinding{ nullptr };
			bool OwnsTabNavigation{};
		};
		std::vector<TemplateLink> s_templateLinks;

		void Mirror(DependencyObject const& native, DependencyProperty const& nativeProperty,
					DependencyObject const& glass, DependencyProperty const& glassProperty, bool /*glassActive*/, bool twoWay = true,
					std::vector<Subscription>* subscriptions = nullptr, std::shared_ptr<bool> updating = std::make_shared<bool>(false),
					std::shared_ptr<bool> active = std::make_shared<bool>(true))
		{
			auto copy = [updating, active](DependencyObject const& from, DependencyProperty const& source, DependencyObject const& to, DependencyProperty const& target)
			{
				if (!*active || *updating) return;
				*updating = true;
				try
				{
					to.SetValue(target, from.GetValue(source));
				}
				catch (...)
				{
					*updating = false; throw;
				}
				*updating = false;
			};
			auto token = native.RegisterPropertyChangedCallback(nativeProperty, [weak = make_weak(glass), glassProperty, copy](auto const& from, auto const& property)
			{
				if (auto to = weak.get()) copy(from, property, to, glassProperty);
			});
			if (subscriptions) subscriptions->push_back({ nativeProperty, token });
			if (twoWay) glass.RegisterPropertyChangedCallback(glassProperty, [weak = make_weak(native), nativeProperty, copy](auto const& from, auto const& property)
			{
				// No user gesture chooses -1. Item regeneration may briefly clear
				// the visual's selection; only the business source may clear it.
				if (nativeProperty == C::Primitives::Selector::SelectedIndexProperty()
					&& unbox_value<std::int32_t>(from.GetValue(property)) < 0) return;
				if (auto to = weak.get()) copy(from, property, to, nativeProperty);
			});
			// The business control is authoritative at initialization, including
			// when glass is the first visible branch. Never persist glass defaults.
			copy(native, nativeProperty, glass, glassProperty);
		}

		C::Control FindMaterialVisual(DependencyObject const& root)
		{
			for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(root); ++i)
			{
				auto child = Media::VisualTreeHelper::GetChild(root, i);
				if (auto control = child.try_as<C::Control>(); control && control.Name() == L"MaterialVisual") return control;
				if (auto found = FindMaterialVisual(child)) return found;
			}
			return nullptr;
		}

		void ForwardMaterialFocus(weak_ref<C::Control> const& owner, weak_ref<C::Control> const& visual,
								  std::shared_ptr<bool> const& active)
		{
			if (!*active) return;
			auto control = owner.get();
			auto target = visual.get();
			if (!control || !target) return;
			try
			{
				if (!control.IsLoaded() || !target.IsLoaded() || !target.IsEnabled()) return;
				auto root = control.XamlRoot();
				if (!root || target.XamlRoot() != root) return;
				auto state = control.FocusState();
				// GotFocus is delivered asynchronously; the owner may already
				// have lost focus during navigation or template replacement.
				if (state == FocusState::Unfocused) return;
				if (Input::FocusManager::GetFocusedElement(root) != control) return;
				if (auto password = target.try_as<W::LiquidGlassPasswordBox>())
				{
					auto inner = password.InnerPasswordBox();
					if (!inner || !inner.IsLoaded() || inner.XamlRoot() != root) return;
					inner.Focus(state);
				}
				else target.Focus(state);
			}
			catch (hresult_error const& error)
			{
				// An optional focus handoff can race XamlRoot disconnection.
				// Do not turn a stale navigation target into an app-wide failure.
				if (error.code() != static_cast<std::int32_t>(0x80070490)
					&& error.code() != static_cast<std::int32_t>(0x80000013)) throw;
			}
		}
	}

	void ReleaseMaterialTemplateBridge(C::Control const& native)
	{
		for (auto const& link : s_templateLinks)
			if (link.Native.get() == native)
			{
				// The outgoing visual may reset properties while disconnecting.
				// Disable both directions before replacing its template tree.
				*link.Active = false;
				for (auto const& subscription : link.Subscriptions)
					native.UnregisterPropertyChangedCallback(subscription.Property, subscription.Token);
				native.GotFocus(link.FocusToken);
				if (auto text = native.try_as<C::TextBox>(); text && link.SelectionToken.value) text.SelectionChanged(link.SelectionToken);
				if (link.OwnsTabNavigation)
				{
					auto property = UIElement::TabFocusNavigationProperty();
					if (link.OriginalTabNavigationBinding)
						native.SetBinding(property, link.OriginalTabNavigationBinding);
					else if (link.OriginalTabNavigation == DependencyProperty::UnsetValue())
						native.ClearValue(property);
					else
						native.SetValue(property, link.OriginalTabNavigation);
				}
			}
		std::erase_if(s_templateLinks, [&](auto const& link)
		{
			return !link.Native.get() || link.Native.get() == native;
		});
	}

	void EnsureMaterialTemplateBridge(C::Control const& native)
	{
		auto glass = FindMaterialVisual(native);
		if (!glass) return;
		for (auto const& link : s_templateLinks) if (link.Native.get() == native && link.Glass.get() == glass) return;
		ReleaseMaterialTemplateBridge(native);
		TemplateLink link{ make_weak(native), make_weak(glass),{},{} };
		link.FocusToken = native.GotFocus([weak = make_weak(glass), owner = make_weak(native), active = link.Active](auto const&, RoutedEventArgs const& args)
		{
			auto control = owner.get();
			if (!*active || !control || !control.IsLoaded() || args.OriginalSource() != control) return;
			if (auto queue = control.DispatcherQueue())
				queue.TryEnqueue([weak, owner, active]
			{
				ForwardMaterialFocus(owner, weak, active);
			});
		});
		auto updating = std::make_shared<bool>(false);
		auto bind = [&](DependencyProperty const& source, DependencyProperty const& target, bool twoWay = true)
		{
			Mirror(native, source, glass, target, false, twoWay, &link.Subscriptions, updating, link.Active);
		};

		// The single business control owns state. Template visuals never seed it.
		bind(C::Control::IsEnabledProperty(), C::Control::IsEnabledProperty(), false);
		if (auto radio = glass.try_as<W::LiquidGlassRadioButton>())
		{
			// Keep keyboard navigation between real radio visuals, separate from
			// the authoritative outer radio group. Implicit groups stay parent-local.
			auto updateGroup = [weak = make_weak(radio)](auto const& from, auto const&)
			{
				if (auto target = weak.get())
				{
					C::RadioButton source = from.template as<C::RadioButton>();
					auto group = source.GroupName();
					if (group.empty()) group = L"Parent" + to_hstring(reinterpret_cast<std::uintptr_t>(get_abi(source.Parent())));
					target.GroupName(group + L".MaterialVisual");
				}
			};
			auto property = C::RadioButton::GroupNameProperty();
			link.Subscriptions.push_back({ property, native.RegisterPropertyChangedCallback(property, updateGroup) });
			updateGroup(native, property);
		}
		if (glass.try_as<W::LiquidGlassToggleSwitch>())
			bind(C::ToggleSwitch::IsOnProperty(), W::LiquidGlassToggleSwitch::IsOnProperty());
		else if (glass.try_as<C::Primitives::ToggleButton>())
			bind(C::Primitives::ToggleButton::IsCheckedProperty(), C::Primitives::ToggleButton::IsCheckedProperty());
		else if (glass.try_as<W::LiquidGlassSlider>())
		{
			bind(C::Primitives::RangeBase::MinimumProperty(), C::Primitives::RangeBase::MinimumProperty(), false);
			bind(C::Primitives::RangeBase::MaximumProperty(), C::Primitives::RangeBase::MaximumProperty(), false);
			bind(C::Primitives::RangeBase::ValueProperty(), C::Primitives::RangeBase::ValueProperty());
		}
		else if (auto combo = glass.try_as<W::LiquidGlassComboBox>())
		{
			auto updateItems = [weak = make_weak(combo), updating](auto const& from, auto const&)
			{
				if (auto target = weak.get())
				{
					*updating = true;
					try
					{
						C::ComboBox source = from.template as<C::ComboBox>();
						target.ItemsSource(source.ItemsSource() ? source.ItemsSource() : source.Items().as<Windows::Foundation::IInspectable>());
						target.SelectedIndex(source.SelectedIndex());
					}
					catch (...)
					{
						*updating = false; throw;
					}
					*updating = false;
				}
			};
			auto token = native.RegisterPropertyChangedCallback(C::ItemsControl::ItemsSourceProperty(), updateItems);
			link.Subscriptions.push_back({ C::ItemsControl::ItemsSourceProperty(), token });
			updateItems(native, C::ItemsControl::ItemsSourceProperty());
			bind(C::Primitives::Selector::SelectedIndexProperty(), C::Primitives::Selector::SelectedIndexProperty());
			if (native.as<C::ComboBox>().IsEditable()) bind(C::ComboBox::TextProperty(), C::ComboBox::TextProperty());
			auto property = WinUI3Package::ComboBoxHelper::AcrylicWorkaroundProperty();
			if (unbox_value_or<bool>(native.GetValue(property), false)) WinUI3Package::ComboBoxHelper::SetAcrylicWorkaround(combo, true);
		}
		else if (glass.try_as<W::LiquidGlassTextBox>())
		{
			// The visible editor must be focusable (IsTabStop is template-bound).
			// Treat it and the business TextBox as one tab stop; otherwise
			// Shift+Tab can return to the owner and immediately focus it again.
			auto property = UIElement::TabFocusNavigationProperty();
			link.OriginalTabNavigation = native.ReadLocalValue(property);
			if (auto expression = native.GetBindingExpression(property))
				link.OriginalTabNavigationBinding = expression.ParentBinding();
			link.OwnsTabNavigation = true;
			native.TabFocusNavigation(Input::KeyboardNavigationMode::Once);
			bind(C::TextBox::TextProperty(), C::TextBox::TextProperty());
			// SelectionStart/Length are ordinary WinRT properties, not public DPs.
			auto copySelection = [updating, active = link.Active](C::TextBox const& from, C::TextBox const& to)
			{
				if (!*active || *updating) return;
				*updating = true;
				try
				{
					// The editor may announce its selection before TextChanged.
					// Keep its destination text in sync before applying the range.
					if (to.Text() != from.Text()) to.Text(from.Text());
					to.Select(from.SelectionStart(), from.SelectionLength());
				}
				catch (...)
				{
					*updating = false; throw;
				}
				*updating = false;
			};
			auto source = native.as<C::TextBox>();
			auto target = glass.as<C::TextBox>();
			link.SelectionToken = source.SelectionChanged([weak = make_weak(target), copySelection](auto const& sender, auto const&)
			{
				if (auto to = weak.get()) copySelection(sender.template as<C::TextBox>(), to);
			});
			target.SelectionChanged([weak = make_weak(source), copySelection](auto const& sender, auto const&)
			{
				if (auto to = weak.get()) copySelection(sender.template as<C::TextBox>(), to);
			});
			copySelection(source, target);
		}
		else if (auto password = glass.try_as<W::LiquidGlassPasswordBox>())
		{
			bind(C::PasswordBox::PasswordProperty(), W::LiquidGlassPasswordBox::PasswordProperty());
			if (auto inner = password.InnerPasswordBox())
				for (auto const& property : { C::PasswordBox::HeaderProperty(), C::PasswordBox::HeaderTemplateProperty(),
					C::PasswordBox::PasswordRevealModeProperty(), C::PasswordBox::MaxLengthProperty(), C::PasswordBox::PasswordCharProperty() })
					Mirror(native, property, inner, property, false, false, &link.Subscriptions, updating, link.Active);
		}

		if (native.try_as<C::Button>() || native.try_as<C::HyperlinkButton>())
		{
			// Invoke the business control once; Click, Command, navigation and
			// Flyout remain owned by that control, not duplicated in the template.
			glass.as<C::Primitives::ButtonBase>().Click([weak = make_weak(native), active = link.Active](auto const&, auto const&)
			{
				if (auto target = weak.get(); *active && target && target.IsLoaded() && target.IsEnabled())
				{
					namespace P = Microsoft::UI::Xaml::Automation::Peers;
					auto peer = P::FrameworkElementAutomationPeer::CreatePeerForElement(target);
					if (peer) if (auto invoke = peer.GetPattern(P::PatternInterface::Invoke).try_as<Microsoft::UI::Xaml::Automation::Provider::IInvokeProvider>()) invoke.Invoke();
				}
			});
		}
		s_templateLinks.push_back(std::move(link));
		// Applying a material to an already-focused control does not raise a
		// new GotFocus event. Hand off after its new template has loaded too.
		if (native.try_as<C::TextBox>() && native.FocusState() != FocusState::Unfocused)
		{
			auto active = s_templateLinks.back().Active;
			if (auto queue = native.DispatcherQueue())
				queue.TryEnqueue([weak = make_weak(glass), owner = make_weak(native), active]
			{
				ForwardMaterialFocus(owner, weak, active);
			});
		}
	}
	void EnsureMaterialHostBridge(DependencyObject const& sender)
	{
		auto cursor = sender;
		WinUI3Package::SwitchPresenter host{ nullptr };
		while (cursor)
		{
			if (auto found = cursor.try_as<WinUI3Package::SwitchPresenter>(); found && found.SwitchCases().Size() == 2)
			{
				auto native = found.SwitchCases().GetAt(0).as<WinUI3Package::Case>().Content().try_as<C::Control>();
				auto glass = found.SwitchCases().GetAt(1).as<WinUI3Package::Case>().Content().try_as<C::Control>();
				if (native && glass)
				{
					host = found; break;
				}
			}
			cursor = Media::VisualTreeHelper::GetParent(cursor);
		}
		if (!host) return;
		static auto hooked = DependencyProperty::RegisterAttached(L"MaterialBridgeHooked", xaml_typename<bool>(),
																  xaml_typename<winrt::OpenNet::UI::Xaml::Control::Theme::ControlAppearance>(), PropertyMetadata{ box_value(false) });
		if (unbox_value<bool>(host.GetValue(hooked))) return;
		host.SetValue(hooked, box_value(true));
		auto native = host.SwitchCases().GetAt(0).as<WinUI3Package::Case>().Content().as<C::Control>();
		auto glass = host.SwitchCases().GetAt(1).as<WinUI3Package::Case>().Content().as<C::Control>();
		auto active = host.CurrentCase() == host.SwitchCases().GetAt(1).as<WinUI3Package::Case>();
		// An inactive Case is detached, so it cannot inherit the visible row's
		// DataContext. Keep the original contract usable by existing code-behind,
		// then remove only our inherited-context local copy when it re-enters.
		auto bindContext = [](C::Control const& from, C::Control const& to)
		{
			auto inherited = to.ReadLocalValue(FrameworkElement::DataContextProperty()) == DependencyProperty::UnsetValue();
			from.RegisterPropertyChangedCallback(FrameworkElement::DataContextProperty(), [weak = make_weak(to)](auto const& sender, auto const& property)
			{
				if (sender.template as<FrameworkElement>().IsLoaded())
					if (auto target = weak.get(); target && !target.IsLoaded()) target.SetValue(property, sender.GetValue(property));
			});
			if (inherited) to.Loaded([](auto const& sender, auto const&)
			{
				sender.template as<FrameworkElement>().ClearValue(FrameworkElement::DataContextProperty());
			});
		};
		bindContext(native, glass); bindContext(glass, native);
		if (active) native.DataContext(glass.DataContext()); else glass.DataContext(native.DataContext());
		if (host.ReadLocalValue(FrameworkElement::HorizontalAlignmentProperty()) == DependencyProperty::UnsetValue()) host.HorizontalAlignment(native.HorizontalAlignment());
		if (host.ReadLocalValue(FrameworkElement::VerticalAlignmentProperty()) == DependencyProperty::UnsetValue()) host.VerticalAlignment(native.VerticalAlignment());
		Mirror(native, C::Control::IsEnabledProperty(), glass, C::Control::IsEnabledProperty(), false, false);
		if (native.try_as<C::Button>() || native.try_as<C::HyperlinkButton>())
		{
			native.RegisterPropertyChangedCallback(C::ContentControl::ContentProperty(), [weak = make_weak(glass)](auto const& from, auto const& property)
			{
				auto content = from.GetValue(property);
				// Never reparent a UIElement between native and glass controls.
				if (!content || !content.template try_as<UIElement>())
					if (auto to = weak.get()) to.SetValue(C::ContentControl::ContentProperty(), content);
			});
		}
		if (auto toggle = glass.try_as<W::LiquidGlassToggleSwitch>())
			Mirror(native, C::ToggleSwitch::IsOnProperty(), toggle, W::LiquidGlassToggleSwitch::IsOnProperty(), active);
		else if (glass.try_as<C::Primitives::ToggleButton>())
			Mirror(native, C::Primitives::ToggleButton::IsCheckedProperty(), glass, C::Primitives::ToggleButton::IsCheckedProperty(), active);
		else if (glass.try_as<W::LiquidGlassSlider>())
		{
			Mirror(native, C::Primitives::RangeBase::MinimumProperty(), glass, C::Primitives::RangeBase::MinimumProperty(), false, false);
			Mirror(native, C::Primitives::RangeBase::MaximumProperty(), glass, C::Primitives::RangeBase::MaximumProperty(), false, false);
			Mirror(native, C::Primitives::RangeBase::ValueProperty(), glass, C::Primitives::RangeBase::ValueProperty(), active);
		}
		else if (glass.try_as<W::LiquidGlassComboBox>())
			Mirror(native, C::Primitives::Selector::SelectedIndexProperty(), glass, C::Primitives::Selector::SelectedIndexProperty(), active);
		else if (glass.try_as<W::LiquidGlassTextBox>())
			Mirror(native, C::TextBox::TextProperty(), glass, C::TextBox::TextProperty(), active);
		else if (auto password = glass.try_as<W::LiquidGlassPasswordBox>())
		{
			Mirror(native, C::PasswordBox::PasswordProperty(), password, W::LiquidGlassPasswordBox::PasswordProperty(), active);
			auto inner = password.InnerPasswordBox();
			if (inner)
			{
				for (auto const& property : { C::PasswordBox::HeaderProperty(), C::PasswordBox::HeaderTemplateProperty(),
					C::PasswordBox::PasswordRevealModeProperty(), C::PasswordBox::MaxLengthProperty(), C::PasswordBox::PasswordCharProperty() })
					Mirror(native, property, inner, property, false, false);
			}
		}
		else if (auto search = glass.try_as<W::LiquidGlassSearchBox>())
		{
			Mirror(native, C::AutoSuggestBox::TextProperty(), glass, W::LiquidGlassSearchBox::TextProperty(), active);
			// AutoSuggestBoxHelper's setter accepts only the native type. The
			// LiquidGlassSearchBox wrapper is a ContentControl, not AutoSuggestBox.
			auto property = WinUI3Package::AutoSuggestBoxHelper::AcrylicWorkaroundProperty();
			if (auto inner = search.InnerAutoSuggestBox(); inner
				&& unbox_value_or<bool>(native.GetValue(property), false)
				&& !unbox_value_or<bool>(inner.GetValue(property), false))
			{
				WinUI3Package::AutoSuggestBoxHelper::SetAcrylicWorkaround(inner, true);
			}
		}
	}
}
