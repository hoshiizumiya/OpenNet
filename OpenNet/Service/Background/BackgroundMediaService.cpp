#include "pch.h"
#include "BackgroundMediaService.h"

import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.Windows.Storage.Pickers;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Media.Imaging;
import winrt.Windows.Media.Core;
import winrt.Windows.Media.Playback;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
	using ::OpenNet::Service::Background::BackgroundSourceKind;
	using ::OpenNet::Service::Background::BackgroundSourceMode;

	bool HasExtension(
		std::filesystem::path const& path,
		std::unordered_set<std::wstring> const& allowed)
	{
		auto extension = path.extension().wstring();
		std::ranges::transform(extension, extension.begin(), [](wchar_t value)
		{
			return static_cast<wchar_t>(::towlower(value));
		});
		return allowed.contains(extension);
	}

	std::wstring ResolveBackgroundPath(
		BackgroundSourceMode const mode,
		std::wstring const& configuredPath,
		std::wstring const& currentPath,
		bool const advance,
		std::unordered_set<std::wstring> const& allowed)
	{
		if (mode == BackgroundSourceMode::None || configuredPath.empty()) return {};
		std::filesystem::path const configured{ configuredPath };
		if (mode == BackgroundSourceMode::SingleFile)
		{
			std::error_code error;
			return std::filesystem::is_regular_file(configured, error)
				&& HasExtension(configured, allowed)
				? configured.lexically_normal().wstring()
				: std::wstring{};
		}

		// Reuse the current file for settings-only refreshes. Folder traversal
		// is reserved for a source change or the periodic advance operation.
		if (!advance && !currentPath.empty())
		{
			std::error_code error;
			std::filesystem::path const current{ currentPath };
			auto const relative = std::filesystem::relative(current, configured, error);
			if (!error && !relative.empty()
				&& *relative.begin() != L".."
				&& std::filesystem::is_regular_file(current, error)
				&& !error && HasExtension(current, allowed))
				return current.lexically_normal().wstring();
		}

		std::vector<std::wstring> candidates;
		try
		{
			for (auto const& entry : std::filesystem::recursive_directory_iterator(
				configured,
				std::filesystem::directory_options::skip_permission_denied))
			{
				std::error_code error;
				if (entry.is_regular_file(error) && HasExtension(entry.path(), allowed))
					candidates.push_back(entry.path().lexically_normal().wstring());
			}
		}
		catch (...)
		{
			return {};
		}

		if (candidates.empty()) return {};
		if (advance && candidates.size() > 1 && !currentPath.empty())
			std::erase(candidates, currentPath);
		static thread_local std::mt19937 generator{ std::random_device{}() };
		std::uniform_int_distribution<std::size_t> distribution(0, candidates.size() - 1);
		return candidates[distribution(generator)];
	}

	winrt::Windows::Foundation::Uri FileUri(std::wstring const& path)
	{
		auto generic = std::filesystem::path{ path }.lexically_normal().generic_wstring();
		return winrt::Windows::Foundation::Uri{
			generic.starts_with(L"//") ? L"file:" + generic : L"file:///" + generic };
	}

	winrt::Microsoft::UI::Xaml::Media::Stretch StretchFromIndex(std::int32_t const index)
	{
		using winrt::Microsoft::UI::Xaml::Media::Stretch;
		switch (index)
		{
			case 1: return Stretch::Fill;
			case 2: return Stretch::Uniform;
			case 3: return Stretch::UniformToFill;
			default: return Stretch::None;
		}
	}

	class BackgroundMediaServiceImpl final : public ::OpenNet::Service::Background::IBackgroundMediaService
	{
	public:
		::OpenNet::Service::Background::BackgroundMediaOptions LoadOptions() const override
		{
			using ::OpenNet::Service::Background::BackgroundMediaOptions;
			if (m_cachedOptions) return *m_cachedOptions;
			auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
			database.Initialize();
			BackgroundMediaOptions options;
			options.ImagePath = database.GetStringW(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_image").value_or(L"");
			options.VideoPath = database.GetStringW(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_media_path").value_or(L"");
			options.ImageMode = static_cast<BackgroundSourceMode>(std::clamp(
				static_cast<int>(database.GetInt(
					::OpenNet::Core::AppSettingsDatabase::CAT_UI,
					"background_image_mode").value_or(options.ImagePath.empty() ? 0 : 1)), 0, 2));
			options.VideoMode = static_cast<BackgroundSourceMode>(std::clamp(
				static_cast<int>(database.GetInt(
					::OpenNet::Core::AppSettingsDatabase::CAT_UI,
					"background_media_mode").value_or(0)), 0, 2));
			options.ImageStretch = std::clamp(static_cast<int>(database.GetInt(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"image_stretch", 3)), 0, 3);
			options.VideoStretch = std::clamp(static_cast<int>(database.GetInt(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"video_stretch", 3)), 0, 3);
			options.ImageOpacity = std::clamp(database.GetDouble(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"image_opacity").value_or(20.0), 0.0, 100.0);
			options.VideoOpacity = std::clamp(database.GetDouble(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"video_opacity").value_or(20.0), 0.0, 100.0);
			options.VideoMuted = database.GetBool(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_media_muted").value_or(true);
			options.VideoLooping = database.GetBool(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_media_looping").value_or(true);
			options.RotationMinutes = std::clamp<std::int64_t>(database.GetInt(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_rotation_minutes").value_or(5), 1, 1440);
			m_cachedOptions = options;
			return options;
		}

		void SaveOptions(
			::OpenNet::Service::Background::BackgroundMediaOptions const& options) override
		{
			using ::OpenNet::Service::Background::BackgroundMediaOptions;
			auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
			database.Initialize();
			auto const category = ::OpenNet::Core::AppSettingsDatabase::CAT_UI;
			auto const previous = LoadOptions();
			BackgroundMediaOptions normalized = options;
			normalized.ImageMode = static_cast<BackgroundSourceMode>(std::clamp(
				static_cast<int>(options.ImageMode), 0, 2));
			normalized.VideoMode = static_cast<BackgroundSourceMode>(std::clamp(
				static_cast<int>(options.VideoMode), 0, 2));
			normalized.ImageStretch = std::clamp(options.ImageStretch, 0, 3);
			normalized.VideoStretch = std::clamp(options.VideoStretch, 0, 3);
			normalized.ImageOpacity = std::clamp(options.ImageOpacity, 0.0, 100.0);
			normalized.VideoOpacity = std::clamp(options.VideoOpacity, 0.0, 100.0);
			normalized.RotationMinutes = std::clamp<std::int64_t>(
				options.RotationMinutes, 1, 1440);

			if (previous.ImageMode != normalized.ImageMode)
				database.SetInt(category, "background_image_mode", static_cast<int>(normalized.ImageMode));
			if (previous.VideoMode != normalized.VideoMode)
				database.SetInt(category, "background_media_mode", static_cast<int>(normalized.VideoMode));
			if (previous.ImagePath != normalized.ImagePath)
				database.SetStringW(category, "background_image", normalized.ImagePath);
			if (previous.VideoPath != normalized.VideoPath)
				database.SetStringW(category, "background_media_path", normalized.VideoPath);
			if (previous.ImageStretch != normalized.ImageStretch)
				database.SetInt(category, "image_stretch", normalized.ImageStretch);
			if (previous.VideoStretch != normalized.VideoStretch)
				database.SetInt(category, "video_stretch", normalized.VideoStretch);
			if (previous.ImageOpacity != normalized.ImageOpacity)
				database.SetDouble(category, "image_opacity", normalized.ImageOpacity);
			if (previous.VideoOpacity != normalized.VideoOpacity)
				database.SetDouble(category, "video_opacity", normalized.VideoOpacity);
			if (previous.VideoMuted != normalized.VideoMuted)
				database.SetBool(category, "background_media_muted", normalized.VideoMuted);
			if (previous.VideoLooping != normalized.VideoLooping)
				database.SetBool(category, "background_media_looping", normalized.VideoLooping);
			if (previous.RotationMinutes != normalized.RotationMinutes)
				database.SetInt(category, "background_rotation_minutes", normalized.RotationMinutes);
			m_cachedOptions = std::move(normalized);
		}

		winrt::event_token OptionsChanged(
			winrt::Windows::Foundation::EventHandler<
			winrt::Windows::Foundation::IInspectable> const& handler) override
		{
			return m_optionsChanged.add(handler);
		}

		void OptionsChanged(winrt::event_token const& token) noexcept override
		{
			m_optionsChanged.remove(token);
		}

		void NotifyOptionsChanged() override
		{
			m_optionsChanged(nullptr, nullptr);
		}

		winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
			PickSourceAsync(
				winrt::Microsoft::UI::WindowId const windowId,
				BackgroundSourceKind const kind,
				BackgroundSourceMode const mode) override
		{
			using namespace winrt::Microsoft::Windows::Storage::Pickers;
			if (mode == BackgroundSourceMode::None) co_return winrt::hstring{};

			if (mode == BackgroundSourceMode::SingleFile)
			{
				FileOpenPicker picker{ windowId };
				picker.SuggestedStartLocation(kind == BackgroundSourceKind::Image
											  ? PickerLocationId::PicturesLibrary
											  : PickerLocationId::VideosLibrary);
				if (kind == BackgroundSourceKind::Image)
				{
					for (auto const extension : { L".bmp", L".gif", L".ico", L".jpg",
						 L".jpeg", L".png", L".tif", L".tiff", L".webp" })
						picker.FileTypeFilter().Append(extension);
				}
				else
				{
					for (auto const extension : { L".avi", L".m4v", L".mkv", L".mov",
						 L".mp4", L".webm", L".wmv" })
						picker.FileTypeFilter().Append(extension);
				}
				if (auto file = co_await picker.PickSingleFileAsync()) co_return file.Path();
				co_return winrt::hstring{};
			}

			FolderPicker picker{ windowId };
			picker.SuggestedStartLocation(kind == BackgroundSourceKind::Image
										  ? PickerLocationId::PicturesLibrary
										  : PickerLocationId::VideosLibrary);
			if (auto folder = co_await picker.PickSingleFolderAsync()) co_return folder.Path();
			co_return winrt::hstring{};
		}

		std::chrono::minutes RotationInterval() const override
		{
			return std::chrono::minutes{ LoadOptions().RotationMinutes };
		}

		winrt::Windows::Foundation::IAsyncAction RefreshAsync(
			Image imagePresenter,
			MediaPlayerElement videoPresenter,
			bool const advance) override
		{
			if (!imagePresenter || !videoPresenter) co_return;
			auto const dispatcher = imagePresenter.DispatcherQueue();
			if (!dispatcher) co_return;
			auto const presenterKey = reinterpret_cast<void*>(
				winrt::get_abi(videoPresenter));
			for (auto state = m_presenterStates.begin();
				 state != m_presenterStates.end();)
			{
				if (!state->second.VideoPresenter.get())
					state = m_presenterStates.erase(state);
				else
					++state;
			}

			auto const options = LoadOptions();
			std::wstring const imagePath{ options.ImagePath };
			std::wstring const videoPath{ options.VideoPath };

			auto& initialState = m_presenterStates[presenterKey];
			initialState.VideoPresenter = winrt::make_weak(videoPresenter);
			auto const version = ++initialState.RefreshVersion;
			auto const currentImage = initialState.CurrentImagePath;
			auto const currentVideo = initialState.CurrentVideoPath;
			co_await winrt::resume_background();
			static std::unordered_set<std::wstring> const imageExtensions{
				L".bmp", L".gif", L".ico", L".jpg", L".jpeg",
				L".png", L".tif", L".tiff", L".webp" };
			static std::unordered_set<std::wstring> const videoExtensions{
				L".avi", L".m4v", L".mkv", L".mov", L".mp4", L".webm", L".wmv" };
			auto resolvedImage = ResolveBackgroundPath(
				options.ImageMode, imagePath, currentImage, advance, imageExtensions);
			auto resolvedVideo = ResolveBackgroundPath(
				options.VideoMode, videoPath, currentVideo, advance, videoExtensions);
			co_await winrtplus::resume_foreground(dispatcher);
			auto stateIterator = m_presenterStates.find(presenterKey);
			if (stateIterator == m_presenterStates.end()
				|| version != stateIterator->second.RefreshVersion) co_return;
			auto* state = &stateIterator->second;

			auto const imageChanged = resolvedImage != state->CurrentImagePath
				|| (!resolvedImage.empty() && !imagePresenter.Source());
			auto const videoChanged = resolvedVideo != state->CurrentVideoPath
				|| (!resolvedVideo.empty() && !videoPresenter.Source());
			if (imageChanged || videoChanged)
			{
				auto const fadeImage = imageChanged && imagePresenter.Source();
				auto const fadeVideo = videoChanged && videoPresenter.Source();
				if (fadeImage) imagePresenter.Opacity(0);
				if (fadeVideo) videoPresenter.Opacity(0);
				if (fadeImage || fadeVideo)
				{
					co_await winrt::resume_after(std::chrono::milliseconds(350));
					co_await winrtplus::resume_foreground(dispatcher);
					stateIterator = m_presenterStates.find(presenterKey);
					if (stateIterator == m_presenterStates.end()
						|| version != stateIterator->second.RefreshVersion) co_return;
					state = &stateIterator->second;
				}
			}

			imagePresenter.Stretch(StretchFromIndex(options.ImageStretch));
			if (resolvedImage.empty())
			{
				imagePresenter.Opacity(0);
				imagePresenter.Source(nullptr);
				state->CurrentImagePath.clear();
			}
			else
			{
				try
				{
					if (resolvedImage != state->CurrentImagePath
						|| !imagePresenter.Source())
					{
						auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage{};
						bitmap.UriSource(FileUri(resolvedImage));
						imagePresenter.Source(bitmap);
						state->CurrentImagePath = std::move(resolvedImage);
					}
					imagePresenter.Opacity(options.ImageOpacity / 100.0);
				}
				catch (...)
				{
					imagePresenter.Opacity(0);
					imagePresenter.Source(nullptr);
					state->CurrentImagePath.clear();
				}
			}

			videoPresenter.Stretch(StretchFromIndex(options.VideoStretch));
			if (resolvedVideo.empty())
			{
				StopVideo(videoPresenter);
				state->CurrentVideoPath.clear();
			}
			else
			{
				try
				{
					if (resolvedVideo != state->CurrentVideoPath
						|| !videoPresenter.Source())
					{
						if (auto player = videoPresenter.MediaPlayer()) player.Pause();
						videoPresenter.Source(winrt::Windows::Media::Core::MediaSource::
											  CreateFromUri(FileUri(resolvedVideo)));
						state->CurrentVideoPath = std::move(resolvedVideo);
					}
					if (auto player = videoPresenter.MediaPlayer())
					{
						player.IsMuted(options.VideoMuted);
						player.IsLoopingEnabled(options.VideoLooping);
						player.Play();
					}
					videoPresenter.Opacity(options.VideoOpacity / 100.0);
				}
				catch (...)
				{
					StopVideo(videoPresenter);
					state->CurrentVideoPath.clear();
				}
			}
		}

		void Suspend(MediaPlayerElement const& videoPresenter) noexcept override
		{
			auto const presenterKey = reinterpret_cast<void*>(
				winrt::get_abi(videoPresenter));
			if (auto state = m_presenterStates.find(presenterKey);
				state != m_presenterStates.end())
			{
				++state->second.RefreshVersion;
				state->second.CurrentVideoPath.clear();
			}
			StopVideo(videoPresenter);
		}

		void Reset(
			Image const& imagePresenter,
			MediaPlayerElement const& videoPresenter) noexcept override
		{
			auto const presenterKey = reinterpret_cast<void*>(
				winrt::get_abi(videoPresenter));
			m_presenterStates.erase(presenterKey);
			try
			{
				if (imagePresenter)
				{
					imagePresenter.Opacity(0);
					imagePresenter.Source(nullptr);
				}
			}
			catch (...)
			{
			}
			StopVideo(videoPresenter);
		}

	private:
		struct PresenterState
		{
			std::wstring CurrentImagePath;
			std::wstring CurrentVideoPath;
			std::uint64_t RefreshVersion{};
			winrt::weak_ref<MediaPlayerElement> VideoPresenter;
		};

		void StopVideo(MediaPlayerElement const& presenter) noexcept
		{
			try
			{
				if (presenter)
				{
					if (auto player = presenter.MediaPlayer())
					{
						player.Pause();
						player.Source(nullptr);
					}
					presenter.Source(nullptr);
					presenter.SetMediaPlayer(nullptr);
					presenter.Opacity(0);
				}
			}
			catch (...)
			{
			}
		}

		std::unordered_map<void*, PresenterState> m_presenterStates;
		mutable std::optional<::OpenNet::Service::Background::BackgroundMediaOptions>
			m_cachedOptions;
		winrt::event<winrt::Windows::Foundation::EventHandler<
			winrt::Windows::Foundation::IInspectable>> m_optionsChanged;
	};
}

namespace OpenNet::Service::Background
{
	IBackgroundMediaService& GetBackgroundMediaService()
	{
		static ::BackgroundMediaServiceImpl service;
		return service;
	}
}
