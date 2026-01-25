/*
 * NavItemIconHelper.cpp
 *
 * NavigationViewItem 图标扩展（Attached Properties）。
 *
 *  * 功能概述：
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
 * 关键实现约束：
 * - Attached DependencyProperty 必须“静态字段一次性注册”（RegisterAttached 只执行一次）。
 *   该模式与 WinUI/C# 的实现一致，可以避免 XAML 运行时/编译时对静态成员解析不一致而导致的
 *   启动期 fail-fast（看起来像“内部错误”）。
 * - 在 ControlTemplate 内访问 attached property，请使用：
 *   {Binding (helpers:NavItemIconHelper.SelectedIcon), RelativeSource={RelativeSource Mode=TemplatedParent}}
 *   而不是 TemplateBinding。
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
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_selectedIconProperty =
		Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"SelectedIcon",
			winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
			winrt::xaml_typename<class_type>(),
			Microsoft::UI::Xaml::PropertyMetadata(nullptr));

	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_showNotificationDotProperty =
		Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"ShowNotificationDot",
			winrt::xaml_typename<bool>(),
			winrt::xaml_typename<class_type>(),
			Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(false)));

	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_unselectedIconProperty =
		Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"UnselectedIcon",
			winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
			winrt::xaml_typename<class_type>(),
			Microsoft::UI::Xaml::PropertyMetadata(nullptr));

	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_staticIconVisibilityProperty =
		Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"StaticIconVisibility",
			winrt::xaml_typename<winrt::Microsoft::UI::Xaml::Visibility>(),
			winrt::xaml_typename<class_type>(),
			Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(Microsoft::UI::Xaml::Visibility::Collapsed)));

	/// <summary>
	/// 获取 SelectedIcon 附加属性的依赖属性对象
	/// 用于在导航项选中状态下显示的图标
	/// </summary>
	/// <returns>SelectedIcon 依赖属性的静态实例</returns>
	/// 
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
	{
		return s_selectedIconProperty;
	}

	/// <summary>
	/// 获取 ShowNotificationDot 附加属性的依赖属性对象
	/// 用于控制是否在导航项上显示通知圆点
	/// </summary>
	/// <returns>ShowNotificationDot 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::ShowNotificationDotProperty()
	{
		return s_showNotificationDotProperty;
	}

	/// <summary>
	/// 获取 UnselectedIcon 附加属性的依赖属性对象
	/// 用于在导航项未选中状态下显示的图标
	/// </summary>
	/// <returns>UnselectedIcon 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::UnselectedIconProperty()
	{
		return s_unselectedIconProperty;
	}

	/// <summary>
	/// 获取 StaticIconVisibility 附加属性的依赖属性对象
	/// 用于控制静态图标（自定义图标）的可见性
	/// </summary>
	/// <returns>StaticIconVisibility 依赖属性的静态实例</returns>
	Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::StaticIconVisibilityProperty()
	{
		return s_staticIconVisibilityProperty;
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
 * 1. 这些附加属性通过附加属性绑定（Binding + TemplatedParent）传递到模板内部
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
