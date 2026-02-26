#pragma once

#include "ViewModels/TorrentFileInfoViewModel.g.h"
#include "ViewModels/TorrentMetadataViewModel.g.h"
#include "ViewModels/ObservableMixin.h"
#include "Core/torrentCore/TorrentMetadataInfo.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <shlobj.h>  // For SHGetFolderPath

namespace winrt::OpenNet::ViewModels::implementation
{
	// ViewModel for a single file in the torrent
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
		static winrt::hstring FormatFileSize(int64_t bytes);
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

		winrt::hstring SelectedSize() const { return m_selectedSize; }
		void SelectedSize(winrt::hstring const& v) { SetProperty(m_selectedSize, v, L"SelectedSize"); }

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

		// Get metadata for adding torrent
		::OpenNet::Core::Torrent::TorrentMetadataInfo GetMetadataInfo() const { return m_rawMetadata; }

	private:
		static winrt::hstring FormatSize(int64_t bytes);
		static winrt::hstring FormatTimestamp(int64_t timestamp);

		winrt::hstring m_torrentName;
		winrt::hstring m_infoHash;
		winrt::hstring m_totalSize;
		winrt::hstring m_selectedSize;
		winrt::hstring m_comment;
		winrt::hstring m_creator;
		winrt::hstring m_creationDate;
		int32_t m_pieceLength{};
		int32_t m_numPieces{};
		bool m_isPrivate{};

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel> m_files;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_trackers;

		winrt::hstring m_savePath;
		bool m_createSubfolder{ true };
		bool m_startImmediately{ true };

		::OpenNet::Core::Torrent::TorrentMetadataInfo m_rawMetadata;
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TorrentFileInfoViewModel : TorrentFileInfoViewModelT<TorrentFileInfoViewModel, implementation::TorrentFileInfoViewModel>
	{
	};

	struct TorrentMetadataViewModel : TorrentMetadataViewModelT<TorrentMetadataViewModel, implementation::TorrentMetadataViewModel>
	{
	};
}
