#pragma once
// SurfaceLoader - C++/WinRT port of CommunityToolkit SurfaceLoader
// Loads images to CompositionBrush instances using Win2D with caching support

#include <mutex>
#include <map>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.UI.Composition.h>
#include <winrt/Microsoft.Graphics.DirectX.h>

namespace OpenNet::UI::Xaml::Media::Helpers
{
	namespace MUC = winrt::Microsoft::UI::Composition;

	/// <summary>
	/// A class that can load images and draw surfaces using Win2D.
	/// Provides both static (cached) and instance-based loading.
	/// </summary>
	class SurfaceLoader
	{
	public:
		// ---- Static image loading with cache ----

		/// <summary>
		/// Loads a CompositionSurfaceBrush from a URI with caching
		/// </summary>
		static winrt::Windows::Foundation::IAsyncOperation<MUC::CompositionBrush>
			LoadImageAsync(winrt::Windows::Foundation::Uri const& uri)
		{
			auto compositor = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::GetCompositorForCurrentThread();
			auto uriStr = uri.ToString();

			// Check cache
			{
				std::lock_guard lock(s_cacheMutex);
				auto it = s_cache.find(uriStr);
				if (it != s_cache.end())
				{
					auto brush = it->second.get();
					if (brush)
					{
						co_return brush;
					}
					s_cache.erase(it);
				}
			}

			MUC::CompositionBrush brush{ nullptr };
			try
			{
				auto sharedDevice = winrt::Microsoft::Graphics::Canvas::CanvasDevice::GetSharedDevice();
				auto bitmap = co_await winrt::Microsoft::Graphics::Canvas::CanvasBitmap::LoadAsync(sharedDevice, uri);

				auto sizeInPixels = bitmap.SizeInPixels();
				winrt::Windows::Foundation::Size pixelSize{
					static_cast<float>(sizeInPixels.Width),
					static_cast<float>(sizeInPixels.Height)
				};

				auto graphicsDevice = winrt::Microsoft::Graphics::Canvas::UI::Composition::CanvasComposition::CreateCompositionGraphicsDevice(
					compositor, sharedDevice);

				auto drawingSurface = graphicsDevice.CreateDrawingSurface(
					pixelSize,
					winrt::Microsoft::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
					winrt::Microsoft::Graphics::DirectX::DirectXAlphaMode::Premultiplied);

				auto bitmapSize = bitmap.Size();
				winrt::Windows::Foundation::Rect drawRect{ 0, 0, static_cast<float>(bitmapSize.Width), static_cast<float>(bitmapSize.Height) };

				{
					auto session = winrt::Microsoft::Graphics::Canvas::UI::Composition::CanvasComposition::CreateDrawingSession(
						drawingSurface, drawRect);
					session.Clear(winrt::Windows::UI::Color{ 0, 0, 0, 0 });
					session.DrawImage(bitmap, drawRect, drawRect);
					session.Close();
				}

				auto surfaceBrush = compositor.CreateSurfaceBrush(drawingSurface);
				surfaceBrush.Stretch(MUC::CompositionStretch::None);
				brush = surfaceBrush;
			}
			catch (...)
			{
				brush = nullptr;
			}

			// Cache the result
			if (brush)
			{
				std::lock_guard lock(s_cacheMutex);
				s_cache[uriStr] = winrt::make_weak(brush);
			}

			co_return brush;
		}

		/// <summary>
		/// Clears the static image cache
		/// </summary>
		static void ClearCache()
		{
			std::lock_guard lock(s_cacheMutex);
			s_cache.clear();
		}

		// ---- Instance-based surface loading ----

		/// <summary>
		/// Gets an instance for the current thread's compositor
		/// </summary>
		static SurfaceLoader& GetInstance()
		{
			auto compositor = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::GetCompositorForCurrentThread();
			std::lock_guard lock(s_instanceMutex);
			auto it = s_instances.find(winrt::get_abi(compositor));
			if (it != s_instances.end())
			{
				return *it->second;
			}
			auto instance = std::make_unique<SurfaceLoader>(compositor);
			auto& ref = *instance;
			s_instances[winrt::get_abi(compositor)] = std::move(instance);
			return ref;
		}

		explicit SurfaceLoader(MUC::Compositor const& compositor)
			: m_compositor(compositor)
		{
			InitializeDevices();
		}

		~SurfaceLoader()
		{
			if (m_compositionDevice)
			{
				m_compositionDevice.Close();
			}
			if (m_canvasDevice)
			{
				m_canvasDevice.Close();
			}
		}

		/// <summary>
		/// Loads an image from URI
		/// </summary>
		winrt::Windows::Foundation::IAsyncOperation<MUC::CompositionDrawingSurface>
			LoadFromUri(winrt::Windows::Foundation::Uri const& uri)
		{
			return LoadFromUri(uri, winrt::Windows::Foundation::Size{ 0, 0 });
		}

		/// <summary>
		/// Loads an image from URI with a specified size
		/// </summary>
		winrt::Windows::Foundation::IAsyncOperation<MUC::CompositionDrawingSurface>
			LoadFromUri(winrt::Windows::Foundation::Uri const& uri, winrt::Windows::Foundation::Size sizeTarget)
		{
			if (!m_compositionDevice)
			{
				co_return nullptr;
			}

			auto bitmap = co_await winrt::Microsoft::Graphics::Canvas::CanvasBitmap::LoadAsync(m_canvasDevice, uri);
			auto sizeSource = bitmap.Size();

			if (sizeTarget.Width == 0 && sizeTarget.Height == 0)
			{
				sizeTarget = winrt::Windows::Foundation::Size{
					static_cast<float>(sizeSource.Width),
					static_cast<float>(sizeSource.Height)
				};
			}

			auto surface = m_compositionDevice.CreateDrawingSurface(
				sizeTarget,
				winrt::Microsoft::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
				winrt::Microsoft::Graphics::DirectX::DirectXAlphaMode::Premultiplied);

			{
				auto session = winrt::Microsoft::Graphics::Canvas::UI::Composition::CanvasComposition::CreateDrawingSession(surface);
				session.Clear(winrt::Windows::UI::Color{ 0, 0, 0, 0 });
				session.DrawImage(bitmap,
					winrt::Windows::Foundation::Rect{ 0, 0, sizeTarget.Width, sizeTarget.Height },
					winrt::Windows::Foundation::Rect{ 0, 0, static_cast<float>(sizeSource.Width), static_cast<float>(sizeSource.Height) });
				session.Close();
			}

			co_return surface;
		}

	private:
		void InitializeDevices()
		{
			m_canvasDevice = winrt::Microsoft::Graphics::Canvas::CanvasDevice(false);
			m_compositionDevice = winrt::Microsoft::Graphics::Canvas::UI::Composition::CanvasComposition::CreateCompositionGraphicsDevice(
				m_compositor, m_canvasDevice);
		}

		MUC::Compositor m_compositor{ nullptr };
		winrt::Microsoft::Graphics::Canvas::CanvasDevice m_canvasDevice{ nullptr };
		MUC::CompositionGraphicsDevice m_compositionDevice{ nullptr };

		// Static cache
		static inline std::mutex s_cacheMutex;
		static inline std::map<winrt::hstring, winrt::weak_ref<MUC::CompositionBrush>> s_cache;

		// Instance cache
		static inline std::mutex s_instanceMutex;
		static inline std::map<void*, std::unique_ptr<SurfaceLoader>> s_instances;
	};
}
