#include "pch.h"
#include "OperationProgressDialog.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog
{
	OperationProgressDialog::OperationProgressDialog(const winrt::hstring& title, const winrt::hstring& description)
		: m_title(title), m_description(description)
	{
		// 创建对话框主容器
		m_dialog = ContentDialog();
		m_dialog.Title(box_value(title));
		m_dialog.CloseButtonText(L"Cancel");
		m_dialog.DefaultButton(ContentDialogButton::Primary);

		// 创建进度条
		m_progressBar = ProgressBar();
		m_progressBar.IsIndeterminate(true);
		m_progressBar.Height(20);
		m_progressBar.Margin({ 0, 10, 0, 10 });

		// 创建状态文本
		m_statusText = TextBlock();
		m_statusText.Text(hstring(description));
		m_statusText.TextWrapping(TextWrapping::Wrap);

		// 创建主面板
		auto stackPanel = StackPanel();
		stackPanel.Orientation(Orientation::Vertical);
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

			auto statusMessage = success ? L"✓ Operation completed successfully" : L"✗ Operation failed";
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
}
