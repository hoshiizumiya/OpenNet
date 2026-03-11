#pragma once

#include "ViewModels/TorrentFileNodeViewModel.g.h"
#include "ViewModels/TorrentFileInfoViewModel.g.h"
#include "ViewModels/TorrentMetadataViewModel.g.h"
#include "ViewModels/ObservableMixin.h"
#include "Core/torrentCore/TorrentMetadataInfo.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <shlobj.h>  // For SHGetFolderPath

namespace winrt::OpenNet::ViewModels::implementation
{
	// ViewModel for a node in the torrent file tree (folder or file)
	struct TorrentFileNodeViewModel : TorrentFileNodeViewModelT<TorrentFileNodeViewModel>,
		::OpenNet::ViewModels::ObservableMixin<TorrentFileNodeViewModel>
	{
		TorrentFileNodeViewModel();
		TorrentFileNodeViewModel(winrt::hstring const& name, winrt::hstring const& fullPath, bool isFolder);

		using ::OpenNet::ViewModels::ObservableMixin<TorrentFileNodeViewModel>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<TorrentFileNodeViewModel>::RaisePropertyChanged;

		// Properties
		winrt::hstring Name() const { return m_name; }
		void Name(winrt::hstring const& v)
		{
			if (SetProperty(m_name, v, L"Name"))
			{
				RaisePropertyChanged(L"NodeGlyph");
				RaisePropertyChanged(L"NodeFontFamily");
			}
		}

		winrt::hstring FullPath() const { return m_fullPath; }
		void FullPath(winrt::hstring const& v) { SetProperty(m_fullPath, v, L"FullPath"); }

		winrt::hstring SizeText() const { return m_sizeText; }
		void SizeText(winrt::hstring const& v) { SetProperty(m_sizeText, v, L"SizeText"); }

		int64_t SizeBytes() const { return m_sizeBytes; }
		void SizeBytes(int64_t v) { SetProperty(m_sizeBytes, v, L"SizeBytes"); RaisePropertyChanged(L"SizeText"); }

		bool IsSelected() const { return m_isSelected; }
		void IsSelected(bool v);

		bool IsFolder() const { return m_isFolder; }
		void IsFolder(bool v)
		{
			if (SetProperty(m_isFolder, v, L"IsFolder"))
			{
				RaisePropertyChanged(L"NodeGlyph");
				RaisePropertyChanged(L"NodeFontFamily");
			}
		}

		bool IsExpanded() const { return m_isExpanded; }
		void IsExpanded(bool v) { SetProperty(m_isExpanded, v, L"IsExpanded"); }

		int32_t FileIndex() const { return m_fileIndex; }
		void FileIndex(int32_t v) { SetProperty(m_fileIndex, v, L"FileIndex"); }

		int32_t Priority() const { return m_priority; }
		void Priority(int32_t v) { SetProperty(m_priority, v, L"Priority"); RaisePropertyChanged(L"PriorityText"); }

		winrt::hstring PriorityText() const;

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> Children() { return m_children; }
		bool HasChildren() const { return m_children.Size() > 0; }

		// Helper to add child
		void AddChild(winrt::OpenNet::ViewModels::TorrentFileNodeViewModel const& child);

		// Helper to update selection recursively
		void UpdateSelectionRecursive(bool selected);

		// Allow access to m_isSelected for tree operations
		bool m_isSelected{ true };

		winrt::hstring NodeGlyph() const;
		winrt::hstring NodeFontFamily() const;

	private:
		winrt::hstring m_name;
		winrt::hstring m_fullPath;
		winrt::hstring m_sizeText;
		int64_t m_sizeBytes{};
		bool m_isFolder{ false };
		bool m_isExpanded{ true };
		int32_t m_fileIndex{ -1 };
		int32_t m_priority{ 4 };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> m_children{
			winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel>()
		};
	};

	// ViewModel for a single file in the torrent (kept for backward compatibility)
	struct TorrentFileInfoViewModel : TorrentFileInfoViewModelT<TorrentFileInfoViewModel>,
		::OpenNet::ViewModels::ObservableMixin<TorrentFileInfoViewModel>
	{
		TorrentFileInfoViewModel();
		TorrentFileInfoViewModel(::OpenNet::Core::Torrent::TorrentFileInfo const& fileInfo);

		using ::OpenNet::ViewModels::ObservableMixin<TorrentFileInfoViewModel>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<TorrentFileInfoViewModel>::RaisePropertyChanged;

		// Properties
		winrt::hstring FileName() const { return m_fileName; }
		void FileName(winrt::hstring const& v) { SetProperty(m_fileName, v, L"FileName"); }

		winrt::hstring FilePath() const { return m_filePath; }
		void FilePath(winrt::hstring const& v) { SetProperty(m_filePath, v, L"FilePath"); }

		winrt::hstring FileSize() const { return m_fileSize; }
		void FileSize(winrt::hstring const& v) { SetProperty(m_fileSize, v, L"FileSize"); }

		int64_t FileSizeBytes() const { return m_fileSizeBytes; }
		void FileSizeBytes(int64_t v) { SetProperty(m_fileSizeBytes, v, L"FileSizeBytes"); }

		bool IsSelected() const { return m_isSelected; }
		void IsSelected(bool v);

		int32_t Priority() const { return m_priority; }
		void Priority(int32_t v) { SetProperty(m_priority, v, L"Priority"); RaisePropertyChanged(L"PriorityText"); }

		int32_t FileIndex() const { return m_fileIndex; }
		void FileIndex(int32_t v) { SetProperty(m_fileIndex, v, L"FileIndex"); }

		winrt::hstring PriorityText() const;

	private:
		static winrt::hstring ExtractFileName(std::string const& path);

		winrt::hstring m_fileName;
		winrt::hstring m_filePath;
		winrt::hstring m_fileSize;
		int64_t m_fileSizeBytes{};
		bool m_isSelected{ true };
		int32_t m_priority{ 4 }; // Normal priority
		int32_t m_fileIndex{};
	};

	// ViewModel for the complete torrent metadata
	struct TorrentMetadataViewModel : TorrentMetadataViewModelT<TorrentMetadataViewModel>,
		::OpenNet::ViewModels::ObservableMixin<TorrentMetadataViewModel>
	{
		TorrentMetadataViewModel();
		TorrentMetadataViewModel(::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata);

		using ::OpenNet::ViewModels::ObservableMixin<TorrentMetadataViewModel>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<TorrentMetadataViewModel>::RaisePropertyChanged;

		// Properties
		winrt::hstring TorrentName() const { return m_torrentName; }
		void TorrentName(winrt::hstring const& v) { SetProperty(m_torrentName, v, L"TorrentName"); }

		winrt::hstring InfoHash() const { return m_infoHash; }
		void InfoHash(winrt::hstring const& v) { SetProperty(m_infoHash, v, L"InfoHash"); }

		winrt::hstring TotalSize() const { return m_totalSize; }
		void TotalSize(winrt::hstring const& v) { SetProperty(m_totalSize, v, L"TotalSize"); }

		int64_t TotalSizeBytes() const { return m_totalSizeBytes; }
		void TotalSizeBytes(int64_t v) { SetProperty(m_totalSizeBytes, v, L"TotalSizeBytes"); }

		winrt::hstring SelectedSize() const { return m_selectedSize; }
		void SelectedSize(winrt::hstring const& v) { SetProperty(m_selectedSize, v, L"SelectedSize"); }

		int64_t SelectedSizeBytes() const { return m_selectedSizeBytes; }

		winrt::hstring Comment() const { return m_comment; }
		void Comment(winrt::hstring const& v) { SetProperty(m_comment, v, L"Comment"); }

		winrt::hstring Creator() const { return m_creator; }
		void Creator(winrt::hstring const& v) { SetProperty(m_creator, v, L"Creator"); }

		winrt::hstring CreationDate() const { return m_creationDate; }
		void CreationDate(winrt::hstring const& v) { SetProperty(m_creationDate, v, L"CreationDate"); }

		int32_t PieceLength() const { return m_pieceLength; }
		void PieceLength(int32_t v) { SetProperty(m_pieceLength, v, L"PieceLength"); }

		int32_t NumPieces() const { return m_numPieces; }
		void NumPieces(int32_t v) { SetProperty(m_numPieces, v, L"NumPieces"); }

		bool IsPrivate() const { return m_isPrivate; }
		void IsPrivate(bool v) { SetProperty(m_isPrivate, v, L"IsPrivate"); }

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel> Files() { return m_files; }
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> FileTree() { return m_fileTree; }
		int32_t TotalFileCount() const { return m_files.Size(); }
		int32_t SelectedFileCount() const;

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> Trackers() { return m_trackers; }

		winrt::hstring SavePath() const { return m_savePath; }
		void SavePath(winrt::hstring const& v) { SetProperty(m_savePath, v, L"SavePath"); }

		bool CreateSubfolder() const { return m_createSubfolder; }
		void CreateSubfolder(bool v) { SetProperty(m_createSubfolder, v, L"CreateSubfolder"); }

		bool StartImmediately() const { return m_startImmediately; }
		void StartImmediately(bool v) { SetProperty(m_startImmediately, v, L"StartImmediately"); }

		// Selection helpers
		void SelectAll();
		void DeselectAll();
		void SelectByExtension(winrt::hstring const& extension);
		void ToggleSelection(int32_t fileIndex);
		void RefreshSelectedSize();

		// Sync tree node IsSelected states to the flat file list
		void SyncTreeToFlatList();

		// Sync flat file list IsSelected states to tree leaf nodes + update folders
		void SyncFlatListToTree();

		// Update folder checkbox states based on children (child→parent propagation)
		void UpdateFolderSelectionStates();

		// Get metadata for adding torrent
		::OpenNet::Core::Torrent::TorrentMetadataInfo GetMetadataInfo() const;

	private:
		static winrt::hstring FormatTimestamp(int64_t timestamp);
		void BuildFileTree();

		winrt::hstring m_torrentName;
		winrt::hstring m_infoHash;
		winrt::hstring m_totalSize;
		int64_t m_totalSizeBytes{};
		winrt::hstring m_selectedSize;
		int64_t m_selectedSizeBytes{};
		winrt::hstring m_comment;
		winrt::hstring m_creator;
		winrt::hstring m_creationDate;
		int32_t m_pieceLength{};
		int32_t m_numPieces{};
		bool m_isPrivate{};

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel> m_files;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel> m_fileTree{
			winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TorrentFileNodeViewModel>()
		};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_trackers;

		winrt::hstring m_savePath;
		bool m_createSubfolder{ true };
		bool m_startImmediately{ true };

		::OpenNet::Core::Torrent::TorrentMetadataInfo m_rawMetadata;
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TorrentFileNodeViewModel : TorrentFileNodeViewModelT<TorrentFileNodeViewModel, implementation::TorrentFileNodeViewModel>
	{
	};

	struct TorrentFileInfoViewModel : TorrentFileInfoViewModelT<TorrentFileInfoViewModel, implementation::TorrentFileInfoViewModel>
	{
	};

	struct TorrentMetadataViewModel : TorrentMetadataViewModelT<TorrentMetadataViewModel, implementation::TorrentMetadataViewModel>
	{
	};
}
