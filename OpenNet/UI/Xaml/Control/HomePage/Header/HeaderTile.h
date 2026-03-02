#pragma once

#include "UI/Xaml/Control/HomePage/Header/HeaderTile.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	struct HeaderTile : HeaderTileT<HeaderTile>
	{
		HeaderTile();
		void OnApplyTemplate();

		// DependencyProperty accessors
		winrt::hstring ImageUrl() const;
		void ImageUrl(winrt::hstring const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty ImageUrlProperty();

		winrt::hstring Header() const;
		void Header(winrt::hstring const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty HeaderProperty();

		winrt::hstring Description() const;
		void Description(winrt::hstring const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty DescriptionProperty();

		winrt::hstring FeatureID() const;
		void FeatureID(winrt::hstring const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty FeatureIDProperty();

		bool IsSelected() const;
		void IsSelected(bool value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty IsSelectedProperty();

	private:
		void OnIsSelectedChanged(bool oldValue, bool newValue);
		void SetAccessibleName();
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::factory_implementation
{
	struct HeaderTile : HeaderTileT<HeaderTile, implementation::HeaderTile>
	{
	};
}
