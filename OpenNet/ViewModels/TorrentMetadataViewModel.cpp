#include "pch.h"
#include "TorrentMetadataViewModel.h"
#if __has_include("ViewModels/TorrentFileInfoViewModel.g.cpp")
#include "ViewModels/TorrentFileInfoViewModel.g.cpp"
#endif
#if __has_include("ViewModels/TorrentMetadataViewModel.g.cpp")
#include "ViewModels/TorrentMetadataViewModel.g.cpp"
#endif

#include <chrono>
#include <ctime>
#include <algorithm>
#include <cctype>

namespace winrt::OpenNet::ViewModels::implementation
{
	TorrentFileInfoViewModel::TorrentFileInfoViewModel()
	{
	}

	TorrentFileInfoViewModel::TorrentFileInfoViewModel(::OpenNet::Core::Torrent::TorrentFileInfo const& fileInfo)
	{
		m_filePath = winrt::to_hstring(fileInfo.path);
		m_fileName = ExtractFileName(fileInfo.path);
		m_fileSizeBytes = fileInfo.size;
		m_fileSize = FormatFileSize(fileInfo.size);
		m_isSelected = fileInfo.selected;
		m_priority = fileInfo.priority;
		m_fileIndex = fileInfo.fileIndex;
	}

	void TorrentFileInfoViewModel::IsSelected(bool v)
	{
		if (m_isSelected != v)
		{
			m_isSelected = v;
			RaisePropertyChanged(L"IsSelected");
		}
	}

	winrt::hstring TorrentFileInfoViewModel::PriorityText() const
	{
		switch (m_priority)
		{
			case 0: return L"Skip";
			case 1: return L"Lowest";
			case 2: return L"Low";
			case 3: return L"Below Normal";
			case 4: return L"Normal";
			case 5: return L"Above Normal";
			case 6: return L"High";
			case 7: return L"Highest";
			default: return L"Normal";
		}
	}

	winrt::hstring TorrentFileInfoViewModel::FormatFileSize(int64_t bytes)
	{
		const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
		int unitIndex = 0;
		double size = static_cast<double>(bytes);

		while (size >= 1024.0 && unitIndex < 4)
		{
			size /= 1024.0;
			unitIndex++;
		}

		wchar_t buffer[64];
		if (unitIndex == 0)
			swprintf_s(buffer, L"%.0f %s", size, units[unitIndex]);
		else
			swprintf_s(buffer, L"%.2f %s", size, units[unitIndex]);

		return winrt::hstring(buffer);
	}

	winrt::hstring TorrentFileInfoViewModel::ExtractFileName(std::string const& path)
	{
		auto pos = path.find_last_of("/\\");
		if (pos != std::string::npos)
			return winrt::to_hstring(path.substr(pos + 1));
		return winrt::to_hstring(path);
	}

	// ========== TorrentMetadataViewModel ==========

	TorrentMetadataViewModel::TorrentMetadataViewModel()
		: m_files(winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel>())
		, m_trackers(winrt::single_threaded_observable_vector<winrt::hstring>())
	{
	}

	TorrentMetadataViewModel::TorrentMetadataViewModel(::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata)
		: m_files(winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel>())
		, m_trackers(winrt::single_threaded_observable_vector<winrt::hstring>())
		, m_rawMetadata(metadata)
	{
		m_torrentName = winrt::to_hstring(metadata.name);
		m_infoHash = winrt::to_hstring(metadata.infoHash);
		m_totalSize = FormatSize(metadata.totalSize);
		m_comment = winrt::to_hstring(metadata.comment);
		m_creator = winrt::to_hstring(metadata.creator);
		m_creationDate = FormatTimestamp(metadata.creationDate);
		m_pieceLength = metadata.pieceLength;
		m_numPieces = metadata.numPieces;
		m_isPrivate = metadata.isPrivate;

		// Populate files
		for (const auto& fileInfo : metadata.files)
		{
			auto fileVM = winrt::make<TorrentFileInfoViewModel>(fileInfo);
			m_files.Append(fileVM);
		}

		// Populate trackers
		for (const auto& tracker : metadata.trackers)
		{
			m_trackers.Append(winrt::to_hstring(tracker));
		}

		RefreshSelectedSize();
	}

	int32_t TorrentMetadataViewModel::SelectedFileCount() const
	{
		int32_t count = 0;
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			if (m_files.GetAt(i).IsSelected())
				++count;
		}
		return count;
	}

	void TorrentMetadataViewModel::SelectAll()
	{
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			m_files.GetAt(i).IsSelected(true);
		}
		RefreshSelectedSize();
		RaisePropertyChanged(L"SelectedFileCount");
	}

	void TorrentMetadataViewModel::DeselectAll()
	{
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			m_files.GetAt(i).IsSelected(false);
		}
		RefreshSelectedSize();
		RaisePropertyChanged(L"SelectedFileCount");
	}

	void TorrentMetadataViewModel::SelectByExtension(winrt::hstring const& extension)
	{
		std::wstring ext = extension.c_str();
		// Normalize extension to lowercase with leading dot
		if (!ext.empty() && ext[0] != L'.')
			ext = L"." + ext;
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			auto file = m_files.GetAt(i);
			std::wstring filePath = file.FilePath().c_str();
			std::transform(filePath.begin(), filePath.end(), filePath.begin(), ::towlower);

			bool matches = false;
			if (filePath.length() >= ext.length())
			{
				matches = (filePath.substr(filePath.length() - ext.length()) == ext);
			}
			file.IsSelected(matches);
		}
		RefreshSelectedSize();
		RaisePropertyChanged(L"SelectedFileCount");
	}

	void TorrentMetadataViewModel::ToggleSelection(int32_t fileIndex)
	{
		if (fileIndex >= 0 && static_cast<uint32_t>(fileIndex) < m_files.Size())
		{
			auto file = m_files.GetAt(fileIndex);
			file.IsSelected(!file.IsSelected());
			RefreshSelectedSize();
			RaisePropertyChanged(L"SelectedFileCount");
		}
	}

	void TorrentMetadataViewModel::RefreshSelectedSize()
	{
		int64_t selectedBytes = 0;
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			auto file = m_files.GetAt(i);
			if (file.IsSelected())
				selectedBytes += file.FileSizeBytes();
		}
		SelectedSize(FormatSize(selectedBytes));
	}

	winrt::hstring TorrentMetadataViewModel::FormatSize(int64_t bytes)
	{
		const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
		int unitIndex = 0;
		double size = static_cast<double>(bytes);

		while (size >= 1024.0 && unitIndex < 4)
		{
			size /= 1024.0;
			unitIndex++;
		}

		wchar_t buffer[64];
		if (unitIndex == 0)
			swprintf_s(buffer, L"%.0f %s", size, units[unitIndex]);
		else
			swprintf_s(buffer, L"%.2f %s", size, units[unitIndex]);

		return winrt::hstring(buffer);
	}

	winrt::hstring TorrentMetadataViewModel::FormatTimestamp(int64_t timestamp)
	{
		if (timestamp == 0)
			return L"Unknown";

		std::time_t t = static_cast<std::time_t>(timestamp);
		std::tm tm;
		localtime_s(&tm, &t);

		wchar_t buffer[64];
		wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"%Y-%m-%d %H:%M", &tm);
		return winrt::hstring(buffer);
	}
}
