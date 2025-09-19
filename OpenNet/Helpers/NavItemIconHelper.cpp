/*
 * NavItemIconHelper.cpp
 *
 * NavigationView 导航项图标帮助类的实现文件
 *
 * 功能概述：
 * 该类提供了一组附加属性（Attached Properties），用于扩展 NavigationViewItem 的图标功能。
 * 主要实现了导航项在选中和未选中状态下显示不同图标的功能，以及通知圆点的显示控制。
 *
 * 主要特性：
 * 1. 双态图标系统：支持选中和未选中状态的不同图标
 * 2. 通知圆点：支持在导航项上显示通知圆点
 * 3. 静态图标可见性控制：可以控制自定义图标的显示与隐藏
 * 4. 与 WinUI 3 NavigationView 完全集成
 *
 * 设计模式：
 * 使用 WinRT 附加属性模式，允许在 XAML 中直接设置额外的属性，
 * 而无需继承或修改原始控件类。
 */
#include "pch.h"
#include "NavItemIconHelper.h"
#if __has_include("Helpers/NavItemIconHelper.g.cpp")
#include "Helpers/NavItemIconHelper.g.cpp"
#endif


 // 必要的 WinRT 头文件
#include <winrt/Windows.UI.Xaml.Interop.h>

// 使用 WinRT 命名空间
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Foundation;

namespace winrt::OpenNet::Helpers::implementation
{

	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::m_SelectedIconProperty = nullptr;
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::m_ShowNotificationDotProperty = nullptr;
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::m_UnselectedIconProperty = nullptr;
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::m_StaticIconVisibilityProperty = nullptr;

	/// <summary>
	/// 获取 SelectedIcon 附加属性的依赖属性对象
	/// 用于在导航项选中状态下显示的图标
	/// </summary>
	/// <returns>SelectedIcon 依赖属性的静态实例</returns>
	/// 
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
	{
		// 使用静态变量确保属性只注册一次
		// RegisterAttached 注册一个附加属性，可以应用到任何 DependencyObject
		m_SelectedIconProperty = Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"SelectedIcon",											// 属性名称
			winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),	// 属性类型：可以是任何 WinRT 对象
			winrt::xaml_typename<class_type>(),	// 属性所有者类型
			Microsoft::UI::Xaml::PropertyMetadata(nullptr));			// 属性元数据，默认值为 null
		return m_SelectedIconProperty;
	}

	/// <summary>
	/// 获取 ShowNotificationDot 附加属性的依赖属性对象
	/// 用于控制是否在导航项上显示通知圆点
	/// </summary>
	/// <returns>ShowNotificationDot 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::ShowNotificationDotProperty()
	{
		// 注册布尔类型的附加属性
		m_ShowNotificationDotProperty = Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"ShowNotificationDot",									// 属性名称
			winrt::xaml_typename<bool>(),								// 属性类型：布尔值
			winrt::xaml_typename<class_type>(),	// 属性所有者类型
			Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(false)));	// 默认值：false（不显示）
		return m_ShowNotificationDotProperty;
	}

	/// <summary>
	/// 获取 UnselectedIcon 附加属性的依赖属性对象
	/// 用于在导航项未选中状态下显示的图标
	/// </summary>
	/// <returns>UnselectedIcon 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::UnselectedIconProperty()
	{
		// 注册用于未选中状态图标的附加属性
		m_UnselectedIconProperty = Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"UnselectedIcon",										// 属性名称
			winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),	// 属性类型：可以是任何图标对象
			winrt::xaml_typename<class_type>(),	// 属性所有者类型
			Microsoft::UI::Xaml::PropertyMetadata(nullptr));		// 默认值：null
		return m_UnselectedIconProperty;
	}

	/// <summary>
	/// 获取 StaticIconVisibility 附加属性的依赖属性对象
	/// 用于控制静态图标（自定义图标）的可见性
	/// </summary>
	/// <returns>StaticIconVisibility 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::StaticIconVisibilityProperty()
	{
		// 注册可见性控制的附加属性
		m_StaticIconVisibilityProperty = Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"StaticIconVisibility",								// 属性名称
			winrt::xaml_typename<winrt::Microsoft::UI::Xaml::Visibility>(),	// 属性类型：可见性枚举
			winrt::xaml_typename<class_type>(),	// 属性所有者类型
			Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(Microsoft::UI::Xaml::Visibility::Collapsed)));	// 默认值：折叠（不可见）
		return m_StaticIconVisibilityProperty;
	}

	/// <summary>
	/// 获取指定对象的 SelectedIcon 属性值
	/// </summary>
	/// <param name="obj">要获取属性值的依赖对象（通常是 NavigationViewItem）</param>
	/// <returns>选中状态的图标对象</returns>
	IInspectable NavItemIconHelper::GetSelectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj)
	{
		// 从依赖对象获取附加属性的值
		return obj.GetValue(SelectedIconProperty());
	}

	/// <summary>
	/// 设置指定对象的 SelectedIcon 属性值
	/// </summary>
	/// <param name="obj">要设置属性值的依赖对象（通常是 NavigationViewItem）</param>
	/// <param name="value">要设置的选中状态图标对象</param>
	void NavItemIconHelper::SetSelectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj, IInspectable const& value)
	{
		// 为依赖对象设置附加属性的值
		obj.SetValue(SelectedIconProperty(), value);
	}

	/// <summary>
	/// 获取指定对象的 ShowNotificationDot 属性值
	/// </summary>
	/// <param name="obj">要获取属性值的依赖对象</param>
	/// <returns>是否显示通知圆点的布尔值</returns>
	bool NavItemIconHelper::GetShowNotificationDot(Microsoft::UI::Xaml::DependencyObject const& obj)
	{
		// 获取布尔值属性需要使用 unbox_value 进行类型转换
		return unbox_value<bool>(obj.GetValue(ShowNotificationDotProperty()));
	}

	/// <summary>
	/// 设置指定对象的 ShowNotificationDot 属性值
	/// </summary>
	/// <param name="obj">要设置属性值的依赖对象</param>
	/// <param name="value">是否显示通知圆点</param>
	void NavItemIconHelper::SetShowNotificationDot(Microsoft::UI::Xaml::DependencyObject const& obj, bool value)
	{
		// 设置布尔值属性需要使用 box_value 进行装箱
		obj.SetValue(ShowNotificationDotProperty(), box_value(value));
	}

	/// <summary>
	/// 获取指定对象的 UnselectedIcon 属性值
	/// </summary>
	/// <param name="obj">要获取属性值的依赖对象</param>
	/// <returns>未选中状态的图标对象</returns>
	IInspectable NavItemIconHelper::GetUnselectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj)
	{
		// 从依赖对象获取未选中图标属性的值
		return obj.GetValue(UnselectedIconProperty());
	}

	/// <summary>
	/// 设置指定对象的 UnselectedIcon 属性值
	/// </summary>
	/// <param name="obj">要设置属性值的依赖对象</param>
	/// <param name="value">要设置的未选中状态图标对象</param>
	void NavItemIconHelper::SetUnselectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj, IInspectable const& value)
	{
		// 为依赖对象设置未选中图标属性的值
		obj.SetValue(UnselectedIconProperty(), value);
	}

	/// <summary>
	/// 获取指定对象的 StaticIconVisibility 属性值
	/// </summary>
	/// <param name="obj">要获取属性值的依赖对象</param>
	/// <returns>静态图标的可见性状态</returns>
	Microsoft::UI::Xaml::Visibility NavItemIconHelper::GetStaticIconVisibility(Microsoft::UI::Xaml::DependencyObject const& obj)
	{
		// 获取可见性枚举值需要使用 unbox_value 进行类型转换
		return unbox_value<Microsoft::UI::Xaml::Visibility>(obj.GetValue(StaticIconVisibilityProperty()));
	}

	/// <summary>
	/// 设置指定对象的 StaticIconVisibility 属性值
	/// </summary>
	/// <param name="obj">要设置属性值的依赖对象</param>
	/// <param name="value">静态图标的可见性状态</param>
	void NavItemIconHelper::SetStaticIconVisibility(Microsoft::UI::Xaml::DependencyObject const& obj, Microsoft::UI::Xaml::Visibility const& value)
	{
		// 设置可见性枚举值需要使用 box_value 进行装箱
		obj.SetValue(StaticIconVisibilityProperty(), box_value(value));
	}
}

/*
 * 使用说明：
 *
 * 在 XAML 中的典型用法：
 *
 * <NavigationViewItem Content="主页"
 *                     helpers:NavItemIconHelper.UnselectedIcon="{StaticResource HomeOutlineIcon}"
 *                     helpers:NavItemIconHelper.SelectedIcon="{StaticResource HomeFilledIcon}"
 *                     helpers:NavItemIconHelper.StaticIconVisibility="Visible"
 *                     helpers:NavItemIconHelper.ShowNotificationDot="True">
 * </NavigationViewItem>
 *
 * 工作原理：
 * 1. 这些附加属性通过 TemplateBinding 传递到 NavigationViewItemPresenter
 * 2. 在 NavigationView.xaml 的样式中，使用 Visual States 来控制图标的显示
 * 3. 当导航项的选中状态改变时，相应的图标会自动切换
 * 4. UnselectedIconBox 和 SelectedIconBox 通过透明度变化实现平滑过渡
 *
 * 技术细节：
 * - 使用 WinRT 的依赖属性系统实现数据绑定支持
 * - 支持动画和样式系统的集成
 * - 遵循 WinUI 3 的设计模式和最佳实践
 * - 类型安全的属性访问器确保运行时稳定性
 */
