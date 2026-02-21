#pragma once
#include <string>

namespace winrt::OpenNet::Core
{
	/// <summary>
	/// 应用程序运行环境检测
	/// 用于区分打包应用(MSIX/AppContainer)和不打包应用(Desktop)
	/// </summary>
	class AppEnvironment
	{
	public:
		/// <summary>
		/// 检测应用是否运行在AppContainer中(打包应用)
		/// </summary>
		static bool IsRunningInAppContainer();

		/// <summary>
		/// 获取当前应用是否具有包标识
		/// </summary>
		static bool HasPackageIdentity();

		/// <summary>
		/// 获取当前包族名称(仅在打包应用中有效)
		/// </summary>
		static std::string GetPackageFamilyName();

		/// <summary>
		/// 获取应用类型名称(调试用)
		/// </summary>
		static std::string GetAppTypeName();

	private:
		// 缓存包标识检测结果
		static bool s_isAppContainer;
		static bool s_initialized;
		static void Initialize();
	};

	// 全局变量：应用是否运行在AppContainer中
	extern bool g_isAppContainer;
}
