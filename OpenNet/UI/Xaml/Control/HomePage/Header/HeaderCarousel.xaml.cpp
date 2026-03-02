#include "pch.h"
#include "HeaderCarousel.xaml.h"
#if __has_include("UI/Xaml/Control/HomePage/Header/HeaderCarousel.g.cpp")
#include "UI/Xaml/Control/HomePage/Header/HeaderCarousel.g.cpp"
#endif

#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	HeaderCarousel::HeaderCarousel()
	{
		InitializeComponent();

		m_selectionTimer = DispatcherTimer{};
		m_selectionTimer.Interval(std::chrono::milliseconds(4000));
		m_deselectionTimer = DispatcherTimer{};
		m_deselectionTimer.Interval(std::chrono::milliseconds(3000));

		// Stored delay timers (replacing leaking one-shot timer patterns)
		m_scrollDelayTimer = DispatcherTimer{};
		m_scrollDelayTimer.Interval(std::chrono::milliseconds(500));
		m_selectDelayTimer = DispatcherTimer{};
		m_selectDelayTimer.Interval(std::chrono::milliseconds(360));
	}

	void HeaderCarousel::UserControl_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		ResetAndShuffle();
		SelectNextTile();
		SubscribeToEvents();
	}

	void HeaderCarousel::UserControl_Unloaded(IInspectable const&, RoutedEventArgs const&)
	{
		UnsubscribeToEvents();
	}

	void HeaderCarousel::SubscribeToEvents()
	{
		m_selectionTimerToken = m_selectionTimer.Tick({ this, &HeaderCarousel::SelectionTimer_Tick });
		m_deselectionTimerToken = m_deselectionTimer.Tick({ this, &HeaderCarousel::DeselectionTimer_Tick });
		m_selectionTimer.Start();

		// Subscribe stored delay timers
		m_scrollDelayTimerToken = m_scrollDelayTimer.Tick([this](auto&&, auto&&)
		{
			m_scrollDelayTimer.Stop();
			SetTileVisuals();
			m_deselectionTimer.Start();
		});
		m_selectDelayTimerToken = m_selectDelayTimer.Tick([this](auto&&, auto&&)
		{
			m_selectDelayTimer.Stop();
			SetTileVisuals();
		});

		auto children = TilePanel().Children();
		m_tileTokens.clear();
		m_tileTokens.reserve(static_cast<size_t>(children.Size()));

		for (uint32_t i = 0; i < children.Size(); i++)
		{
			if (auto tile = children.GetAt(i).try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
			{
				TileTokens tokens{};
				tokens.pointerEntered = tile.PointerEntered({ this, &HeaderCarousel::Tile_PointerEntered });
				tokens.pointerExited = tile.PointerExited({ this, &HeaderCarousel::Tile_PointerExited });
				tokens.gotFocus = tile.GotFocus({ this, &HeaderCarousel::Tile_GotFocus });
				tokens.lostFocus = tile.LostFocus({ this, &HeaderCarousel::Tile_LostFocus });
				tokens.click = tile.Click({ this, &HeaderCarousel::Tile_Click });
				m_tileTokens.push_back(tokens);
			}
		}
	}

	void HeaderCarousel::UnsubscribeToEvents()
	{
		m_selectionTimer.Tick(m_selectionTimerToken);
		m_deselectionTimer.Tick(m_deselectionTimerToken);
		m_selectionTimer.Stop();
		m_deselectionTimer.Stop();

		// Stop and unsubscribe stored delay timers
		m_scrollDelayTimer.Stop();
		m_scrollDelayTimer.Tick(m_scrollDelayTimerToken);
		m_selectDelayTimer.Stop();
		m_selectDelayTimer.Tick(m_selectDelayTimerToken);

		auto children = TilePanel().Children();
		for (uint32_t i = 0; i < children.Size() && i < m_tileTokens.size(); i++)
		{
			if (auto tile = children.GetAt(i).try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
			{
				tile.PointerEntered(m_tileTokens[i].pointerEntered);
				tile.PointerExited(m_tileTokens[i].pointerExited);
				tile.GotFocus(m_tileTokens[i].gotFocus);
				tile.LostFocus(m_tileTokens[i].lostFocus);
				tile.Click(m_tileTokens[i].click);
			}
		}
		m_tileTokens.clear();
	}

	void HeaderCarousel::Tile_Click(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto tile = sender.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			auto featureId = tile.FeatureID();

			// Map FeatureID → navigation tag
			hstring tag;
			if (featureId == L"file-transfer" || featureId == L"peer-chat")
				tag = L"tasks";
			else if (featureId == L"network-discovery")
				tag = L"contacts";
			else if (featureId == L"vpn-tunnel" || featureId == L"bandwidth-monitor")
				tag = L"net";
			else if (featureId == L"shared-storage")
				tag = L"files";
			else
				tag = L"home";

			// Navigate via MainWindow
			if (auto mainWindow = winrt::OpenNet::implementation::App::window.try_as<winrt::OpenNet::MainWindow>())
			{
				mainWindow.Navigate(tag);
			}
		}
	}

	void HeaderCarousel::SelectionTimer_Tick(IInspectable const&, IInspectable const&)
	{
		SelectNextTile();
	}

	void HeaderCarousel::SelectNextTile()
	{
		auto children = TilePanel().Children();
		auto index = GetNextUniqueRandom();
		if (index < 0 || static_cast<uint32_t>(index) >= children.Size()) return;

		if (auto tile = children.GetAt(static_cast<uint32_t>(index)).try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			m_selectedTile = tile;

			// Scroll to center the selected tile
			auto transform = m_selectedTile.TransformToVisual(TilePanel());
			auto point = transform.TransformPoint({ 0, 0 });
			auto targetX = point.X - (scrollViewer().ActualWidth() / 2) + (m_selectedTile.ActualSize().x / 2);
			scrollViewer().ChangeView(targetX, nullptr, nullptr);

			// Delay to let scroll complete, then set visuals (using stored timer)
			m_scrollDelayTimer.Stop();
			m_scrollDelayTimer.Start();
		}
	}

	void HeaderCarousel::DeselectionTimer_Tick(IInspectable const&, IInspectable const&)
	{
		if (m_selectedTile)
		{
			m_selectedTile.IsSelected(false);
			m_selectedTile = nullptr;
		}
		m_deselectionTimer.Stop();
	}

	void HeaderCarousel::ResetAndShuffle()
	{
		auto count = TilePanel().Children().Size();
		m_numbers.clear();
		m_numbers.reserve(count);
		for (uint32_t i = 0; i < count; i++)
		{
			m_numbers.push_back(static_cast<int>(i));
		}

		// Fisher-Yates shuffle
		for (int i = static_cast<int>(m_numbers.size()) - 1; i > 0; i--)
		{
			std::uniform_int_distribution<int> dist(0, i);
			int j = dist(m_rng);
			std::swap(m_numbers[j], m_numbers[i]);
		}
		m_currentIndex = 0;
	}

	int HeaderCarousel::GetNextUniqueRandom()
	{
		if (m_currentIndex >= static_cast<int>(m_numbers.size()))
		{
			ResetAndShuffle();
		}
		return m_numbers[m_currentIndex++];
	}

	void HeaderCarousel::SetTileVisuals()
	{
		if (!m_selectedTile) return;

		m_selectedTile.IsSelected(true);

		// Update blurred backdrop image
		auto imageUrl = m_selectedTile.ImageUrl();
		if (!imageUrl.empty())
		{
			try
			{
				BackDropImage().ImageUrl(Uri{ imageUrl });
			}
			catch (...) {}
		}

		// Animate title gradient to match tile foreground
		if (auto brush = m_selectedTile.Foreground().try_as<LinearGradientBrush>())
		{
			AnimateTitleGradient(brush);
		}
	}

	void HeaderCarousel::AnimateTitleGradient(LinearGradientBrush const& brush)
	{
		Storyboard storyboard;
		auto targetStops = AnimatedGradientBrush().GradientStops();
		auto sourceStops = brush.GradientStops();

		for (uint32_t i = 0; i < sourceStops.Size() && i < targetStops.Size(); i++)
		{
			ColorAnimation colorAnim;
			colorAnim.To(sourceStops.GetAt(i).Color());
			colorAnim.Duration(DurationHelper::FromTimeSpan(std::chrono::milliseconds(500)));
			colorAnim.EnableDependentAnimation(true);
			Storyboard::SetTarget(colorAnim, targetStops.GetAt(i));
			Storyboard::SetTargetProperty(colorAnim, L"Color");
			storyboard.Children().Append(colorAnim);
		}
		storyboard.Begin();
	}

	void HeaderCarousel::Tile_PointerExited(IInspectable const& sender, Input::PointerRoutedEventArgs const&)
	{
		if (auto tile = sender.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			tile.IsSelected(false);
		}
		m_selectionTimer.Start();
	}

	void HeaderCarousel::Tile_PointerEntered(IInspectable const& sender, Input::PointerRoutedEventArgs const&)
	{
		if (auto tile = sender.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			m_selectedTile = tile;
			SelectTile();
		}
	}

	void HeaderCarousel::SelectTile()
	{
		m_selectionTimer.Stop();
		m_deselectionTimer.Stop();

		// Deselect all tiles
		auto children = TilePanel().Children();
		for (uint32_t i = 0; i < children.Size(); i++)
		{
			if (auto tile = children.GetAt(i).try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
			{
				tile.IsSelected(false);
			}
		}

		// Delayed selection for smooth animation (using stored timer)
		m_selectDelayTimer.Stop();
		m_selectDelayTimer.Start();
	}

	void HeaderCarousel::Tile_GotFocus(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto tile = sender.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			m_selectedTile = tile;
			SelectTile();
		}
	}

	void HeaderCarousel::Tile_LostFocus(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto tile = sender.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
		{
			tile.IsSelected(false);
		}
		m_selectionTimer.Start();
	}
}
