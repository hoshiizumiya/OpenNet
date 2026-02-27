#include "pch.h"
#include "TorrentMetadataViewModel.h"
#if __has_include("ViewModels/TorrentFileNodeViewModel.g.cpp")
#include "ViewModels/TorrentFileNodeViewModel.g.cpp"
#endif
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
#include <map>
#include <sstream>

namespace winrt::OpenNet::ViewModels::implementation
{
	// ========== TorrentFileNodeViewModel ==========

	TorrentFileNodeViewModel::TorrentFileNodeViewModel()
	{
	}

	TorrentFileNodeViewModel::TorrentFileNodeViewModel(winrt::hstring const& name, winrt::hstring const& fullPath, bool isFolder)
		: m_name(name)
		, m_fullPath(fullPath)
		, m_isFolder(isFolder)
		, m_isExpanded(true)
		, m_fileIndex(-1)
	{
	}

	void TorrentFileNodeViewModel::IsSelected(bool v)
	{
		if (m_isSelected != v)
		{
			m_isSelected = v;
			RaisePropertyChanged(L"IsSelected");

			// If this is a folder, update all children recursively
			if (m_isFolder)
			{
				for (uint32_t i = 0; i < m_children.Size(); ++i)
				{
					m_children.GetAt(i).IsSelected(v);
				}
			}
		}
	}

	winrt::hstring TorrentFileNodeViewModel::PriorityText() const
	{
		if (m_isFolder) return L"";

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

	void TorrentFileNodeViewModel::AddChild(winrt::OpenNet::ViewModels::TorrentFileNodeViewModel const& child)
	{
		m_children.Append(child);
	}

	void TorrentFileNodeViewModel::UpdateSelectionRecursive(bool selected)
	{
		m_isSelected = selected;
		RaisePropertyChanged(L"IsSelected");

		for (uint32_t i = 0; i < m_children.Size(); ++i)
		{
			auto child = m_children.GetAt(i);
			child.as<TorrentFileNodeViewModel>()->UpdateSelectionRecursive(selected);
		}
	}

	winrt::hstring TorrentFileNodeViewModel::FormatFileSize(int64_t bytes)
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

	// ========== TorrentFileInfoViewModel ==========

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
		m_totalSizeBytes = metadata.totalSize;
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

		// Build hierarchical file tree
		BuildFileTree();

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

	// Helper to recursively select/deselect all nodes in tree
	static void SelectAllInTree(winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> const& nodes, bool select)
	{
		for (uint32_t i = 0; i < nodes.Size(); ++i)
		{
			auto node = nodes.GetAt(i);
			auto impl = node.as<TorrentFileNodeViewModel>();
			impl->UpdateSelectionRecursive(select);
		}
	}

	void TorrentMetadataViewModel::SelectAll()
	{
		// Update flat file list
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			m_files.GetAt(i).IsSelected(true);
		}
		// Update tree
		SelectAllInTree(m_fileTree, true);
		RefreshSelectedSize();
		RaisePropertyChanged(L"SelectedFileCount");
	}

	void TorrentMetadataViewModel::DeselectAll()
	{
		// Update flat file list
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			m_files.GetAt(i).IsSelected(false);
		}
		// Update tree
		SelectAllInTree(m_fileTree, false);
		RefreshSelectedSize();
		RaisePropertyChanged(L"SelectedFileCount");
	}

	// Helper to recursively select by extension in tree
	static void SelectByExtensionInTree(
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> const& nodes,
		const std::vector<std::wstring>& extensions)
	{
		for (uint32_t i = 0; i < nodes.Size(); ++i)
		{
			auto node = nodes.GetAt(i);

			if (node.IsFolder())
			{
				// Recurse into children
				SelectByExtensionInTree(node.Children(), extensions);

				// Update folder selection based on children
				bool anySelected = false;
				auto children = node.Children();
				for (uint32_t j = 0; j < children.Size(); ++j)
				{
					if (children.GetAt(j).IsSelected())
					{
						anySelected = true;
						break;
					}
				}
				auto impl = node.as<TorrentFileNodeViewModel>();
				impl->m_isSelected = anySelected;
				impl->RaisePropertyChanged(L"IsSelected");
			}
			else
			{
				// Check if file matches any extension
				std::wstring fileName = std::wstring(node.Name().c_str());
				std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::towlower);

				bool matches = false;
				for (const auto& ext : extensions)
				{
					if (fileName.length() >= ext.length() &&
						fileName.substr(fileName.length() - ext.length()) == ext)
					{
						matches = true;
						break;
					}
				}

				auto impl = node.as<TorrentFileNodeViewModel>();
				impl->m_isSelected = matches;
				impl->RaisePropertyChanged(L"IsSelected");
			}
		}
	}

	void TorrentMetadataViewModel::SelectByExtension(winrt::hstring const& extensionsStr)
	{
		// Parse comma-separated extensions
		std::wstring extList = extensionsStr.c_str();
		std::vector<std::wstring> extensions;

		std::wistringstream iss(extList);
		std::wstring ext;
		while (std::getline(iss, ext, L','))
		{
			// Trim and normalize
			while (!ext.empty() && ext[0] == L' ') ext.erase(0, 1);
			while (!ext.empty() && ext.back() == L' ') ext.pop_back();
			if (!ext.empty())
			{
				std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
				extensions.push_back(ext);
			}
		}

		// Update flat file list
		for (uint32_t i = 0; i < m_files.Size(); ++i)
		{
			auto file = m_files.GetAt(i);
			std::wstring filePath = file.FilePath().c_str();
			std::transform(filePath.begin(), filePath.end(), filePath.begin(), ::towlower);

			bool matches = false;
			for (const auto& extension : extensions)
			{
				if (filePath.length() >= extension.length() &&
					filePath.substr(filePath.length() - extension.length()) == extension)
				{
					matches = true;
					break;
				}
			}
			file.IsSelected(matches);
		}

		// Update tree
		SelectByExtensionInTree(m_fileTree, extensions);

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
		m_selectedSizeBytes = selectedBytes;
		RaisePropertyChanged(L"SelectedSizeBytes");
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

	void TorrentMetadataViewModel::BuildFileTree()
	{
		m_fileTree.Clear();

		// Use a map to track folder nodes by path
		std::map<std::wstring, winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> folderNodes;

		for (const auto& fileInfo : m_rawMetadata.files)
		{
			std::wstring path = winrt::to_hstring(fileInfo.path).c_str();

			// Split path into parts
			std::vector<std::wstring> parts;
			std::wistringstream iss(path);
			std::wstring part;
			while (std::getline(iss, part, L'/'))
			{
				if (!part.empty())
				{
					parts.push_back(part);
				}
			}

			// Also try backslash separator
			if (parts.size() <= 1 && path.find(L'\\') != std::wstring::npos)
			{
				parts.clear();
				std::wistringstream iss2(path);
				while (std::getline(iss2, part, L'\\'))
				{
					if (!part.empty())
					{
						parts.push_back(part);
					}
				}
			}

			if (parts.empty()) continue;

			// Build folder hierarchy
			std::wstring currentPath;
			winrt::OpenNet::ViewModels::TorrentFileNodeViewModel parentNode{ nullptr };

			for (size_t i = 0; i < parts.size(); ++i)
			{
				bool isFile = (i == parts.size() - 1);
				std::wstring nodeName = parts[i];
				currentPath += (currentPath.empty() ? L"" : L"/") + nodeName;

				if (isFile)
				{
					// Create file node
					auto fileNode = winrt::make<TorrentFileNodeViewModel>(
						winrt::hstring(nodeName),
						winrt::hstring(currentPath),
						false // not a folder
					);
					fileNode.FileIndex(fileInfo.fileIndex);
					fileNode.SizeBytes(fileInfo.size);
					fileNode.SizeText(TorrentFileNodeViewModel::FormatFileSize(fileInfo.size));
					fileNode.Priority(fileInfo.priority);
					fileNode.IsSelected(fileInfo.selected);

					if (parentNode)
					{
						parentNode.as<TorrentFileNodeViewModel>()->AddChild(fileNode);
					}
					else
					{
						m_fileTree.Append(fileNode);
					}
				}
				else
				{
					// Create or get folder node
					auto it = folderNodes.find(currentPath);
					if (it == folderNodes.end())
					{
						auto folderNode = winrt::make<TorrentFileNodeViewModel>(
							winrt::hstring(nodeName),
							winrt::hstring(currentPath),
							true // is folder
						);
						folderNode.IsExpanded(true);
						folderNodes[currentPath] = folderNode;

						if (parentNode)
						{
							parentNode.as<TorrentFileNodeViewModel>()->AddChild(folderNode);
						}
						else
						{
							m_fileTree.Append(folderNode);
						}
						parentNode = folderNode;
					}
					else
					{
						parentNode = it->second;
					}
				}
			}
		}

		// Update folder sizes (sum of children)
		std::function<int64_t(winrt::OpenNet::ViewModels::TorrentFileNodeViewModel const&)> calculateFolderSize;
		calculateFolderSize = [&calculateFolderSize](winrt::OpenNet::ViewModels::TorrentFileNodeViewModel const& node) -> int64_t
		{
			if (!node.IsFolder())
			{
				return node.SizeBytes();
			}

			int64_t totalSize = 0;
			auto children = node.Children();
			for (uint32_t i = 0; i < children.Size(); ++i)
			{
				totalSize += calculateFolderSize(children.GetAt(i));
			}

			auto impl = node.as<TorrentFileNodeViewModel>();
			impl->SizeBytes(totalSize);
			impl->SizeText(TorrentFileNodeViewModel::FormatFileSize(totalSize));

			return totalSize;
		};

		for (uint32_t i = 0; i < m_fileTree.Size(); ++i)
		{
			calculateFolderSize(m_fileTree.GetAt(i));
		}
	}
}
