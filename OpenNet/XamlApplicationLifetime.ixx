export module OpenNet.XamlApplicationLifetime;

import std;

export namespace OpenNet::XamlApplicationLifetime
{
	inline std::atomic_bool DispatcherQueueInitialized{ false };
	inline std::atomic_bool CultureInfoInitialized{ false };
	inline std::atomic_bool NotifyIconCreated{ false };
	inline std::atomic_bool ActivationAndInitializationCompleted{ false };
	inline std::atomic_bool IsFirstRunAfterUpdate{ false };
	inline std::atomic_bool Exiting{ false };
	inline std::atomic_bool Exited{ false };
}