export module OpenNet.Helpers.EnsureDependencyProperties;

import winrt.Microsoft.UI.Xaml;

export template <typename Derived>
class EnsureDependencyProperty
{
public:
	EnsureDependencyProperty()
	{
		Derived::EnsureDependencyProperties();
	}

	static Derived* GetSelf(winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject)
	{
		return winrt::get_self<Derived>(dependencyObject.as<typename Derived::class_type>());
	}
};
