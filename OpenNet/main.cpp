#include "pch.h"
#include "App.xaml.h"
//#include "Core/IO/FileOperation.h"
#include "Core/ApplicationModel/PackageIdentityAdapter.h"
#include <sentry.h>

#include <winrt/Microsoft.Windows.ApplicationModel.WindowsAppRuntime.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.Storage.h>

#include <WindowsAppSDK-VersionInfo.h>
#include <mddbootstrap.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::Windows::AppLifecycle;

// 重定向激活到主实例的辅助函数
// 这是一个非阻塞的实现，使用线程和事件来等待重定向完成
void RedirectActivationTo(AppActivationArguments const& args, AppInstance const& keyInstance)
{
	// 创建事件句柄用于同步
	HANDLE redirectEventHandle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!redirectEventHandle)
	{
		return;
	}

	// 在另一个线程中执行异步重定向
	// 这样可以避免阻塞 STA 线程
	std::thread redirectThread([keyInstance, args, redirectEventHandle]()
	{
		try
		{
			// 执行重定向
			keyInstance.RedirectActivationToAsync(args).get();
		}
		catch (...)
		{
			// 如果重定向失败，记录错误但不崩溃
			OutputDebugStringW(L"RedirectActivationToAsync failed\n");
		}
		// 通知等待线程重定向已完成
		SetEvent(redirectEventHandle);
	});

	// 使用 COM 兼容的等待方式等待重定向完成
	// CoWaitForMultipleHandles 允许在 STA 线程上等待而不阻塞消息泵
	DWORD waitResult = CoWaitForMultipleHandles(
		COWAIT_DEFAULT,
		INFINITE,
		1,
		&redirectEventHandle,
		nullptr
	);

	// 等待线程完成
	redirectThread.join();
	CloseHandle(redirectEventHandle);

	// 重定向完成后，将主实例窗口置于前台
	DWORD processId = keyInstance.ProcessId();
	if (processId != 0)
	{
		// 枚举主实例的窗口并激活
		struct EnumData
		{
			DWORD targetProcessId;
			HWND foundHwnd;
		} enumData = { processId, nullptr };

		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
		{
			auto* data = reinterpret_cast<EnumData*>(lParam);
			DWORD windowProcessId = 0;
			GetWindowThreadProcessId(hwnd, &windowProcessId);

			if (windowProcessId == data->targetProcessId && IsWindowVisible(hwnd))
			{
				data->foundHwnd = hwnd;
				return FALSE; // 停止枚举
			}
			return TRUE; // 继续枚举
		}, reinterpret_cast<LPARAM>(&enumData));

		if (enumData.foundHwnd)
		{
			// 恢复窗口（如果最小化）
			if (IsIconic(enumData.foundHwnd))
			{
				ShowWindow(enumData.foundHwnd, SW_RESTORE);
			}

			// 激活窗口
			SetForegroundWindow(enumData.foundHwnd);

			// 闪烁提示用户
			FLASHWINFO fw = {};
			fw.cbSize = sizeof(FLASHWINFO);
			fw.hwnd = enumData.foundHwnd;
			fw.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
			fw.uCount = 3;
			fw.dwTimeout = 0;
			FlashWindowEx(&fw);
		}
	}
}

// 决定是否需要重定向的辅助函数
bool DecideRedirection()
{
	bool isRedirect = false;

	// 获取当前激活参数
	AppActivationArguments args = AppInstance::GetCurrent().GetActivatedEventArgs();

	// 注册或查找单实例密钥
	AppInstance keyInstance = AppInstance::FindOrRegisterForKey(L"opennetmain");

	if (keyInstance.IsCurrent())
	{
		// 这是主实例，注册 Activated 事件处理器
		// 当其他实例重定向激活到这里时会触发此事件
		keyInstance.Activated(
			[](auto&& sender, AppActivationArguments const& args)
		{
			// 在 UI 线程中处理激活
			auto dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
			if (dispatcher)
			{
				dispatcher.TryEnqueue([args]()
				{
					winrt::OpenNet::implementation::App::HandleActivation(args);
				});
			}
		}
		);
	}
	else
	{
		// 这不是主实例，需要重定向
		isRedirect = true;
		RedirectActivationTo(args, keyInstance);
	}

	return isRedirect;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// 初始化 WinRT
	winrt::init_apartment(winrt::apartment_type::single_threaded);

	std::thread sentryInitThread([]()
	{
		sentry_options_t* options = sentry_options_new();
		sentry_options_set_dsn(options, "https://8030af3a7ff2e854f827e44c62f50880@o4510805000454144.ingest.de.sentry.io/4510939441397840");
		// This is also the default-path. For further information and recommendations:
		// https://docs.sentry.io/platforms/native/configuration/options/#database_path
		sentry_options_set_database_path(options, winrt::to_string(winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().TemporaryPath()).c_str());
		sentry_options_set_release(options, (winrt::to_string(::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetFamilyName()) + "@" + ::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetAppVersion().ToString()).c_str());
		sentry_options_set_debug(options, 1);
		sentry_init(options);
	});

	// 决定是否需要重定向
	if (DecideRedirection())
	{
		// 需要重定向，退出当前实例
		return 0;
	}

	// 这是主实例，启动 WinUI 3 应用
	winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
	{
		winrt::make<winrt::OpenNet::implementation::App>();
	});

	return 0;
}