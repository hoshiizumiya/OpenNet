#pragma once
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <functional>

namespace winrt::OpenNet::UI::Xaml::View::Dialog
{
	/// <summary>
	/// 文件操作进度对话框
	/// 用于显示移动/复制/删除等长时间运行操作的进度
	/// </summary>
	class OperationProgressDialog
	{
	public:
		OperationProgressDialog(const winrt::hstring& title, const winrt::hstring& description);

		/// <summary>
		/// 显示进度对话框
		/// </summary>
		winrt::Windows::Foundation::IAsyncAction ShowAsync();

		/// <summary>
		/// 更新操作进度
		/// current: 已完成的项目数
		/// total: 总项目数
		/// </summary>
		void UpdateProgress(size_t current, size_t total);

		/// <summary>
		/// 显示操作完成
		/// </summary>
		void CompleteOperation(bool success, const winrt::hstring& message = L"");

		/// <summary>
		/// 关闭对话框
		/// </summary>
		void Close();

	private:
		winrt::Microsoft::UI::Xaml::Controls::ContentDialog m_dialog{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ProgressBar m_progressBar{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::TextBlock m_statusText{ nullptr };
		winrt::hstring m_title;
		winrt::hstring m_description;
		bool m_isOpen = false;
	};
}
