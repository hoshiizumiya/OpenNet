export module OpenNet.Service.Notification.AppNotificationLifeTime;

import winrt.Microsoft.Windows.AppNotifications;

using namespace winrt::Microsoft::Windows::AppNotifications;

export namespace OpenNet::Service::Notification
{
	class AppNotificationLifeTime
	{
		~AppNotificationLifeTime()
		{
			// 用于在程序退出时尝试清除所有的系统通知
			try
			{
				AppNotificationManager::Default().RemoveAllAsync();
				AppNotificationManager::Default().Unregister();
			}
			catch (...)
			{
				// Ignored
			}
		}
	};
}