module;
#include "XamlWorkaround.h"

module OpenNet.Factory.OperationProgressDialog;

import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml;
import OpenNet.Core.Utils.Message;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace OpenNet::Factory::ContentDialog
{
	OperationProgressDialog::OperationProgressDialog(const winrt::hstring& title, const winrt::hstring& description, const winrt::Microsoft::UI::Xaml::XamlRoot& xamlRoot)
		: m_title(title), m_description(description)
	{
		// Container
		m_dialog = winrt::Microsoft::UI::Xaml::Controls::ContentDialog();
		m_dialog.XamlRoot(xamlRoot);
		m_dialog.Style(winrt::Microsoft::UI::Xaml::Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).try_as<winrt::Microsoft::UI::Xaml::Style>());
		m_dialog.Title(winrt::box_value(title));
		m_dialog.CloseButtonText(ResourceGetString(L"CommonCancel"));
		m_dialog.DefaultButton(winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary);

		// Progress bar
		m_progressBar = winrt::Microsoft::UI::Xaml::Controls::ProgressBar();
		m_progressBar.IsIndeterminate(true);
		m_progressBar.Height(20);
		m_progressBar.Margin({ 0, 10, 0, 10 });

		// Status text
		m_statusText = winrt::Microsoft::UI::Xaml::Controls::TextBlock();
		m_statusText.Text(hstring(description));
		m_statusText.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
		m_statusText.TextTrimming(winrt::Microsoft::UI::Xaml::TextTrimming::CharacterEllipsis);

		auto stackPanel = winrt::Microsoft::UI::Xaml::Controls::StackPanel();
		stackPanel.Orientation(winrt::Microsoft::UI::Xaml::Controls::Orientation::Vertical);
		stackPanel.Children().Append(m_statusText);
		stackPanel.Children().Append(m_progressBar);

		m_dialog.Content(stackPanel);
	}

	winrt::Windows::Foundation::IAsyncAction OperationProgressDialog::ShowAsync()
	{
		try
		{
			m_isOpen = true;
			auto result = co_await m_dialog.ShowAsync();
			m_isOpen = false;
		}
		catch (...)
		{
			m_isOpen = false;
		}

		co_return;
	}

	void OperationProgressDialog::UpdateProgress(size_t current, size_t total)
	{
		if (!m_isOpen)
			return;

		try
		{
			// 更新进度条
			if (total > 0)
			{
				m_progressBar.IsIndeterminate(false);
				m_progressBar.Maximum(static_cast<double>(total));
				m_progressBar.Value(static_cast<double>(current));
			}

			// 更新状态文本
			auto percentComplete = total > 0 ? (current * 100) / total : 0;
			auto statusMessage = std::format(
				L"{}\nProgress: {}/{} ({:.1f}%)",
				m_description.c_str(),
				current,
				total,
				static_cast<double>(percentComplete)
			);

			m_statusText.Text(hstring(statusMessage));
		}
		catch (...)
		{
		}
	}

	void OperationProgressDialog::CompleteOperation(bool success, const winrt::hstring& message)
	{
		if (!m_isOpen)
			return;

		try
		{
			m_progressBar.IsIndeterminate(false);
			m_progressBar.Maximum(100);
			m_progressBar.Value(100);

			auto statusMessage = success
				? ResourceGetString(L"OperationProgressDialogCompleted")
				: ResourceGetString(L"OperationProgressDialogFailed");
			if (!message.empty())
			{
				statusMessage = message.c_str();
			}

			m_statusText.Text(hstring(statusMessage));
		}
		catch (...)
		{
		}
	}

	void OperationProgressDialog::Close()
	{
		if (m_isOpen && m_dialog)
		{
			m_dialog.Hide();
			m_isOpen = false;
		}
	}

	winrt::Microsoft::UI::Xaml::Controls::ContentDialog OperationProgressDialog::Dialog()
	{
		return m_dialog;
	}
}
