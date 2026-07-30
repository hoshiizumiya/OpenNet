
#include "XamlWorkaround.h"
#include "CheckUpdateControl.h"
#if __has_include("UI/Xaml/Control/CheckUpdateControl.g.cpp")
#include "UI/Xaml/Control/CheckUpdateControl.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	CheckUpdateControl::~CheckUpdateControl()
	{
		DetachButtonHandler();
	}

	void CheckUpdateControl::EnsureDependencyProperties()
	{
		if (s_isUpdateAvailableProperty)
		{
			return;
		}

		auto const ownerType =
			xaml_typename<OpenNet::UI::Xaml::Control::CheckUpdateControl>();

		s_isUpdateAvailableProperty = DependencyProperty::Register(
			L"IsUpdateAvailable", xaml_typename<bool>(), ownerType,
			PropertyMetadata{ box_value(false) });
		s_updateAvailableTitleProperty = DependencyProperty::Register(
			L"UpdateAvailableTitle", xaml_typename<IInspectable>(), ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &CheckUpdateControl::OnUpdateAvailableTitleChanged } });
		s_updateAvailableVersionTitleProperty = DependencyProperty::Register(
			L"UpdateAvailableVersionTitle", xaml_typename<hstring>(), ownerType,
			PropertyMetadata{ nullptr });
		s_updateAvailableVersionProperty = DependencyProperty::Register(
			L"UpdateAvailableVersion", xaml_typename<hstring>(), ownerType,
			PropertyMetadata{ nullptr });
		s_updateAvailableIconProperty = DependencyProperty::Register(
			L"UpdateAvailableIcon", xaml_typename<IInspectable>(), ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &CheckUpdateControl::OnUpdateAvailableIconChanged } });
		s_updateNotAvailableTitleProperty = DependencyProperty::Register(
			L"UpdateNotAvailableTitle", xaml_typename<IInspectable>(), ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &CheckUpdateControl::OnUpdateNotAvailableTitleChanged } });
		s_lastUpdateCheckTitleProperty = DependencyProperty::Register(
			L"LastUpdateCheckTitle", xaml_typename<hstring>(), ownerType,
			PropertyMetadata{ nullptr });
		s_lastUpdateCheckDateProperty = DependencyProperty::Register(
			L"LastUpdateCheckDate", xaml_typename<hstring>(), ownerType,
			PropertyMetadata{ nullptr });
		s_updateNotAvailableIconProperty = DependencyProperty::Register(
			L"UpdateNotAvailableIcon", xaml_typename<IInspectable>(), ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &CheckUpdateControl::OnUpdateNotAvailableIconChanged } });
	}

	DependencyProperty CheckUpdateControl::IsUpdateAvailableProperty() { return s_isUpdateAvailableProperty; }
	DependencyProperty CheckUpdateControl::UpdateAvailableTitleProperty() { return s_updateAvailableTitleProperty; }
	DependencyProperty CheckUpdateControl::UpdateAvailableVersionTitleProperty() { return s_updateAvailableVersionTitleProperty; }
	DependencyProperty CheckUpdateControl::UpdateAvailableVersionProperty() { return s_updateAvailableVersionProperty; }
	DependencyProperty CheckUpdateControl::UpdateAvailableIconProperty() { return s_updateAvailableIconProperty; }
	DependencyProperty CheckUpdateControl::UpdateNotAvailableTitleProperty() { return s_updateNotAvailableTitleProperty; }
	DependencyProperty CheckUpdateControl::LastUpdateCheckTitleProperty() { return s_lastUpdateCheckTitleProperty; }
	DependencyProperty CheckUpdateControl::LastUpdateCheckDateProperty() { return s_lastUpdateCheckDateProperty; }
	DependencyProperty CheckUpdateControl::UpdateNotAvailableIconProperty() { return s_updateNotAvailableIconProperty; }

	bool CheckUpdateControl::IsUpdateAvailable()
	{
		return unbox_value<bool>(GetValue(s_isUpdateAvailableProperty));
	}

	void CheckUpdateControl::IsUpdateAvailable(bool value)
	{
		SetValue(s_isUpdateAvailableProperty, box_value(value));
	}

	IInspectable CheckUpdateControl::UpdateAvailableTitle() { return GetValue(s_updateAvailableTitleProperty); }
	void CheckUpdateControl::UpdateAvailableTitle(IInspectable const& value) { SetValue(s_updateAvailableTitleProperty, value); }
	hstring CheckUpdateControl::UpdateAvailableVersionTitle() { return unbox_value_or<hstring>(GetValue(s_updateAvailableVersionTitleProperty), {}); }
	void CheckUpdateControl::UpdateAvailableVersionTitle(hstring const& value) { SetValue(s_updateAvailableVersionTitleProperty, box_value(value)); }
	hstring CheckUpdateControl::UpdateAvailableVersion() { return unbox_value_or<hstring>(GetValue(s_updateAvailableVersionProperty), {}); }
	void CheckUpdateControl::UpdateAvailableVersion(hstring const& value) { SetValue(s_updateAvailableVersionProperty, box_value(value)); }
	IInspectable CheckUpdateControl::UpdateAvailableIcon() { return GetValue(s_updateAvailableIconProperty); }
	void CheckUpdateControl::UpdateAvailableIcon(IInspectable const& value) { SetValue(s_updateAvailableIconProperty, value); }
	IInspectable CheckUpdateControl::UpdateNotAvailableTitle() { return GetValue(s_updateNotAvailableTitleProperty); }
	void CheckUpdateControl::UpdateNotAvailableTitle(IInspectable const& value) { SetValue(s_updateNotAvailableTitleProperty, value); }
	hstring CheckUpdateControl::LastUpdateCheckTitle() { return unbox_value_or<hstring>(GetValue(s_lastUpdateCheckTitleProperty), {}); }
	void CheckUpdateControl::LastUpdateCheckTitle(hstring const& value) { SetValue(s_lastUpdateCheckTitleProperty, box_value(value)); }
	hstring CheckUpdateControl::LastUpdateCheckDate() { return unbox_value_or<hstring>(GetValue(s_lastUpdateCheckDateProperty), {}); }
	void CheckUpdateControl::LastUpdateCheckDate(hstring const& value) { SetValue(s_lastUpdateCheckDateProperty, box_value(value)); }
	IInspectable CheckUpdateControl::UpdateNotAvailableIcon() { return GetValue(s_updateNotAvailableIconProperty); }
	void CheckUpdateControl::UpdateNotAvailableIcon(IInspectable const& value) { SetValue(s_updateNotAvailableIconProperty, value); }

	event_token CheckUpdateControl::Click(EventHandler<RoutedEventArgs> const& handler)
	{
		return m_click.add(handler);
	}

	void CheckUpdateControl::Click(event_token const& token) noexcept
	{
		m_click.remove(token);
	}

	void CheckUpdateControl::OnApplyTemplate()
	{
		DetachButtonHandler();
		base_type::OnApplyTemplate();

		m_updateAvailableTitlePresenter =
			GetTemplateChild(L"PART_UpdateAvailableTitlePresenter").try_as<ContentPresenter>();
		m_updateAvailableTitleStackPanel =
			GetTemplateChild(L"PART_UpdateAvailableTitleStackPanel").try_as<StackPanel>();
		m_updateAvailableIconPresenter =
			GetTemplateChild(L"PART_UpdateAvailableIconPresenter").try_as<ContentPresenter>();
		m_updateAvailableIconBorder =
			GetTemplateChild(L"PART_UpdateAvailableIconBorder").try_as<Border>();
		m_updateNotAvailableTitlePresenter =
			GetTemplateChild(L"PART_UpdateNotAvailableTitlePresenter").try_as<ContentPresenter>();
		m_updateNotAvailableTitleStackPanel =
			GetTemplateChild(L"PART_UpdateNotAvailableTitleStackPanel").try_as<StackPanel>();
		m_updateNotAvailableIconPresenter =
			GetTemplateChild(L"PART_UpdateNotAvailableIconPresenter").try_as<ContentPresenter>();
		m_updateNotAvailableIconBorder =
			GetTemplateChild(L"PART_UpdateNotAvailableIconBorder").try_as<Border>();
		m_updateAvailableButton =
			GetTemplateChild(L"PART_UpdateAvailableButton").try_as<Button>();

		if (m_updateAvailableButton)
		{
			m_updateAvailableButtonClickToken =
				m_updateAvailableButton.Click({ this, &CheckUpdateControl::OnUpdateAvailableButton });
		}

		UpdateAvailableTitlePresentation();
		UpdateAvailableIconPresentation();
		UpdateNotAvailableTitlePresentation();
		UpdateNotAvailableIconPresentation();
	}

	void CheckUpdateControl::OnUpdateAvailableTitleChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const&)
	{
		GetSelf(dependencyObject)->UpdateAvailableTitlePresentation();
	}

	void CheckUpdateControl::OnUpdateAvailableIconChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const&)
	{
		GetSelf(dependencyObject)->UpdateAvailableIconPresentation();
	}

	void CheckUpdateControl::OnUpdateNotAvailableTitleChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const&)
	{
		GetSelf(dependencyObject)->UpdateNotAvailableTitlePresentation();
	}

	void CheckUpdateControl::OnUpdateNotAvailableIconChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const&)
	{
		GetSelf(dependencyObject)->UpdateNotAvailableIconPresentation();
	}

	void CheckUpdateControl::UpdateAvailableTitlePresentation()
	{
		if (!m_updateAvailableTitlePresenter || !m_updateAvailableTitleStackPanel)
		{
			return;
		}

		auto const title = UpdateAvailableTitle();
		bool const useDefaultTitle = title && IsStringContent(title);
		m_updateAvailableTitlePresenter.Visibility(
			title && !useDefaultTitle ? Visibility::Visible : Visibility::Collapsed);
		m_updateAvailableTitleStackPanel.Visibility(
			useDefaultTitle ? Visibility::Visible : Visibility::Collapsed);
	}

	void CheckUpdateControl::UpdateAvailableIconPresentation()
	{
		if (!m_updateAvailableIconPresenter || !m_updateAvailableIconBorder)
		{
			return;
		}

		bool const hasCustomIcon = static_cast<bool>(UpdateAvailableIcon());
		m_updateAvailableIconPresenter.Visibility(
			hasCustomIcon ? Visibility::Visible : Visibility::Collapsed);
		m_updateAvailableIconBorder.Visibility(
			hasCustomIcon ? Visibility::Collapsed : Visibility::Visible);
	}

	void CheckUpdateControl::UpdateNotAvailableTitlePresentation()
	{
		if (!m_updateNotAvailableTitlePresenter || !m_updateNotAvailableTitleStackPanel)
		{
			return;
		}

		auto const title = UpdateNotAvailableTitle();
		bool const useDefaultTitle = title && IsStringContent(title);
		m_updateNotAvailableTitlePresenter.Visibility(
			title && !useDefaultTitle ? Visibility::Visible : Visibility::Collapsed);
		m_updateNotAvailableTitleStackPanel.Visibility(
			useDefaultTitle ? Visibility::Visible : Visibility::Collapsed);
	}

	void CheckUpdateControl::UpdateNotAvailableIconPresentation()
	{
		if (!m_updateNotAvailableIconPresenter || !m_updateNotAvailableIconBorder)
		{
			return;
		}

		bool const hasCustomIcon = static_cast<bool>(UpdateNotAvailableIcon());
		m_updateNotAvailableIconPresenter.Visibility(
			hasCustomIcon ? Visibility::Visible : Visibility::Collapsed);
		m_updateNotAvailableIconBorder.Visibility(
			hasCustomIcon ? Visibility::Collapsed : Visibility::Visible);
	}

	void CheckUpdateControl::OnUpdateAvailableButton(
		IInspectable const&,
		RoutedEventArgs const& args)
	{
		m_click(*this, args);
	}

	void CheckUpdateControl::DetachButtonHandler() noexcept
	{
		try
		{
			if (m_updateAvailableButton && m_updateAvailableButtonClickToken.value)
			{
				m_updateAvailableButton.Click(m_updateAvailableButtonClickToken);
			}
		}
		catch (...)
		{
		}

		m_updateAvailableButtonClickToken = {};
		m_updateAvailableButton = nullptr;
	}

	bool CheckUpdateControl::IsStringContent(IInspectable const& value)
	{
		auto const propertyValue = value.try_as<IPropertyValue>();
		return propertyValue && propertyValue.Type() == PropertyType::String;
	}

}
