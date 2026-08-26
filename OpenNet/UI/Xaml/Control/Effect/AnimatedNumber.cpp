#include "XamlWorkaround.h"
#include "AnimatedNumber.h"
#include "AnimatedDigit.h"
#if __has_include("UI/Xaml/Control/Effect/AnimatedNumber.g.cpp")
#include "UI/Xaml/Control/Effect/AnimatedNumber.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	hstring AnimatedNumber::Value() const
	{
		return m_value;
	}

	void AnimatedNumber::Value(hstring const& value)
	{
		if (m_value == value) return;
		m_value = value;
		UpdateCharacters();
	}

	void AnimatedNumber::OnApplyTemplate()
	{
		base_type::OnApplyTemplate();
		m_rootPanel = GetTemplateChild(L"RootPanel").try_as<Panel>();
		m_staticText = GetTemplateChild(L"StaticText").try_as<TextBlock>();
		UpdateCharacters();
	}

	void AnimatedNumber::UpdateCharacters()
	{
		if (!m_rootPanel) return;
		auto children = m_rootPanel.Children();
		if (!AnimatedDigit::AnimationsEnabled())
		{
			children.Clear();
			m_rootPanel.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
			if (m_staticText)
			{
				m_staticText.Text(m_value);
				m_staticText.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
			}
			return;
		}
		m_rootPanel.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
		if (m_staticText)
			m_staticText.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
		auto count = children.Size();
		auto const required = static_cast<std::uint32_t>(m_value.size());
		while (count < required)
		{
			children.InsertAt(0, OpenNet::UI::Xaml::Control::Effect::AnimatedDigit{});
			++count;
		}
		while (count > required)
		{
			children.RemoveAt(0);
			--count;
		}
		for (std::uint32_t index = 0; index < required; ++index)
		{
			auto digit = children.GetAt(index).as<OpenNet::UI::Xaml::Control::Effect::AnimatedDigit>();
			digit.FontFamily(FontFamily());
			digit.FontSize(FontSize());
			digit.FontStyle(FontStyle());
			digit.FontWeight(FontWeight());
			digit.Foreground(Foreground());
			if (digit.Value() != m_value[index]) digit.Value(m_value[index]);
		}
	}
}
