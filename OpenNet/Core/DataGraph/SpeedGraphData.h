#pragma once
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation;
import std;
using namespace std;

class SpeedGraphData
{
	winrt::Microsoft::UI::Xaml::Media::PointCollection m_points{ nullptr };
	winrt::Windows::Foundation::Size m_graphSize{};
	uint64_t m_total{};
	uint64_t m_currentMax = 1024 * 1024;
	float m_ratio = 1.0f;
	double m_currentPercent{};

	constexpr float getX(uint64_t progressBytes)
	{
		if (m_total == 0) return 0.0f;
		return m_graphSize.Width * (static_cast<float>(progressBytes) / m_total);
	}

	constexpr float getY(uint64_t speed)
	{
		return m_graphSize.Height * (1.0f - static_cast<float>(speed) / m_currentMax / m_ratio);
	}

	void addInitialPointIfNeeded(uint32_t& count);
public:
	struct SetSpeedResult
	{
		float newScaleRatio = 1.0f;
		bool needAnimation = false;
	};

	SetSpeedResult SetSpeed(double percent, uint64_t speed);

	constexpr void SetRatio(float ratio)
	{
		m_ratio *= ratio;
	}

	[[nodiscard]] float GetRatio() const
	{
		return m_ratio;
	}

	[[nodiscard]] winrt::Microsoft::UI::Xaml::Media::PointCollection& Points();

	[[nodiscard]] constexpr float Height() const
	{
		return m_graphSize.Height;
	}

	[[nodiscard]] constexpr float Width() const
	{
		return m_graphSize.Width;
	}

	constexpr void NewSize(winrt::Windows::Foundation::Size size)
	{
		m_graphSize = size;
	}

	void Resize(winrt::Windows::Foundation::Size size)
	{
		if (size.Width <= 0.0f || size.Height <= 0.0f)
		{
			return;
		}
		if (m_graphSize.Width > 0.0f && m_graphSize.Height > 0.0f
			&& m_points)
		{
			auto const scaleX = size.Width / m_graphSize.Width;
			auto const scaleY = size.Height / m_graphSize.Height;
			for (std::uint32_t index = 0; index < m_points.Size(); ++index)
			{
				auto point = m_points.GetAt(index);
				point.X *= scaleX;
				point.Y *= scaleY;
				m_points.SetAt(index, point);
			}
		}
		m_graphSize = size;
	}

	[[nodiscard]] winrt::Windows::Foundation::Point GetLastPoint() const
	{
		return m_points.GetAt(m_points.Size() - 2);
	}

	// Reset the graph data to initial state
	void Reset()
	{
		if (m_points)
		{
			m_points.Clear();
		}
		m_total = 0;
		m_currentMax = 1024 * 1024;
		m_ratio = 1.0f;
		m_currentPercent = 0.0;
	}
};
