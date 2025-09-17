#pragma once

#include "ViewModels/ObservableMixin.h"
#include "ViewModels/TransferViewModel.g.h"
#include "../Models/TransferInfo.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace winrt::OpenNet::ViewModels::implementation
{
	// 文件传输视图模型 / File Transfer View Model
	struct TransferViewModel : TransferViewModelT<TransferViewModel>, ::OpenNet::ViewModels::ObservableMixin<TransferViewModel>
	{
	public:
		TransferViewModel();

		// 传输列表属性 / Transfer List Properties
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> ActiveTransfers() const
		{
			return m_activeTransfers;
		}

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> CompletedTransfers() const
		{
			return m_completedTransfers;
		}

		// 选中的文件列表 / Selected File List
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> SelectedFiles() const
		{
			return m_selectedFiles;
		}

		// 连接的对等节点列表 / Connected Peer List
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> ConnectedPeers() const
		{
			return m_connectedPeers;
		}

		// 传输状态 / Transfer Status
		enum class TransferStatus
		{
			Idle,                          // 空闲 / Idle
			Preparing,                     // 准备中 / Preparing
			Transferring,                  // 传输中 / Transferring
			Paused,                        // 已暂停 / Paused
			Completed,                     // 已完成 / Completed
			Failed                         // 失败 / Failed
		};

		TransferStatus Status() const { return m_status; }
		void Status(TransferStatus value)
		{
			if (SetProperty(m_status, value, L"Status"))
			{
				RaisePropertyChanged(L"StatusText");
				RaisePropertyChanged(L"CanStartTransfer");
				RaisePropertyChanged(L"CanPauseTransfer");
				RaisePropertyChanged(L"CanCancelTransfer");
				UpdateCommands();
			}
		}

		winrt::hstring StatusText() const
		{
			switch (m_status)
			{
			case TransferStatus::Idle: return L"空闲 / Idle";
			case TransferStatus::Preparing: return L"准备中 / Preparing";
			case TransferStatus::Transferring: return L"传输中 / Transferring";
			case TransferStatus::Paused: return L"已暂停 / Paused";
			case TransferStatus::Completed: return L"已完成 / Completed";
			case TransferStatus::Failed: return L"失败 / Failed";
			default: return L"未知 / Unknown";
			}
		}

		// 状态检查属性 / Status Check Properties
		bool CanStartTransfer() const
		{
			return m_status == TransferStatus::Idle || m_status == TransferStatus::Paused;
		}

		bool CanPauseTransfer() const
		{
			return m_status == TransferStatus::Transferring;
		}

		bool CanCancelTransfer() const
		{
			return m_status == TransferStatus::Preparing ||
				m_status == TransferStatus::Transferring ||
				m_status == TransferStatus::Paused;
		}

		// 传输统计属性 / Transfer Statistics Properties
		uint32_t TotalFiles() const { return m_totalFiles; }
		void TotalFiles(uint32_t value) { SetProperty(m_totalFiles, value, L"TotalFiles"); }

		uint32_t CompletedFiles() const { return m_completedFiles; }
		void CompletedFiles(uint32_t value)
		{
			if (SetProperty(m_completedFiles, value, L"CompletedFiles"))
			{
				RaisePropertyChanged(L"FileProgress");
			}
		}

		double FileProgress() const
		{
			return m_totalFiles > 0 ? (static_cast<double>(m_completedFiles) / m_totalFiles * 100.0) : 0.0;
		}

		uint64_t TotalBytes() const { return m_totalBytes; }
		void TotalBytes(uint64_t value)
		{
			if (SetProperty(m_totalBytes, value, L"TotalBytes"))
			{
				RaisePropertyChanged(L"TotalBytesText");
				RaisePropertyChanged(L"ByteProgress");
			}
		}
		winrt::hstring TotalBytesText() const { return winrt::OpenNet::Models::OpenNet::FormatBytes(m_totalBytes); }

		uint64_t TransferredBytes() const { return m_transferredBytes; }
		void TransferredBytes(uint64_t value)
		{
			if (SetProperty(m_transferredBytes, value, L"TransferredBytes"))
			{
				RaisePropertyChanged(L"TransferredBytesText");
				RaisePropertyChanged(L"ByteProgress");
			}
		}

	private:
		// backing fields
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_activeTransfers{ nullptr };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_completedTransfers{ nullptr };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_selectedFiles{ nullptr };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_connectedPeers{ nullptr };
		TransferStatus m_status{ TransferStatus::Idle };
		uint32_t m_totalFiles{};
		uint32_t m_completedFiles{};
		uint64_t m_totalBytes{};
		uint64_t m_transferredBytes{};

		void UpdateCommands() {}
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TransferViewModel : TransferViewModelT<TransferViewModel, implementation::TransferViewModel>
	{
	};
}