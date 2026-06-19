// Copyright (c) DGP Studio. All rights reserved.
// Licensed under the MIT license.
// Copyright (c) Millennium-Science-Technology-R-D-Inst. All rights reserved.
// Licensed under the MIT license.
export module OpenNet.Core.Setting.SettingKeys;

import std;

export namespace OpenNet::Core::Setting::SettingKeys
{
	// Application
	inline constexpr wchar_t const* DataDirectory                          = L"OpenNet::Application::DataFolderPath";
	inline constexpr wchar_t const* OverrideElevationRequirement           = L"OpenNet::Application::Elevation::Override";
	inline constexpr wchar_t const* AutoRestartAsAdmin                     = L"OpenNet::Application::Elevation::AutoRestartAsAdmin";
	inline constexpr wchar_t const* StartupEnabled                         = L"OpenNet::Application::Startup::Enabled";
	inline constexpr wchar_t const* StartupAsAdminEnabled                  = L"OpenNet::Application::Startup::AsAdmin::Enabled";
	inline constexpr wchar_t const* LaunchTimes                            = L"OpenNet::Application::LaunchTimes";
	inline constexpr wchar_t const* PreviousDataDirectoryToDelete          = L"OpenNet::Application::PreviousDataFolderToDelete";
	inline constexpr wchar_t const* LastVersion                            = L"OpenNet::Application::Update::LastVersion";
	inline constexpr wchar_t const* AlwaysIsFirstRunAfterUpdate            = L"OpenNet::Application::Update::LastVersion::TreatAsFirstRun";
	inline constexpr wchar_t const* PendingRefreshAutoStartTaskAfterUpdate = L"OpenNet::Application::Update::StartupTask::PendingRefresh";
	inline constexpr wchar_t const* OverrideUpdateVersionComparison        = L"OpenNet::Application::Update::VersionComparison::Override";
																			 
	// Globalization
	inline constexpr wchar_t const* FirstDayOfWeek                         = L"OpenNet::Globalization::FirstDayOfWeek";
	inline constexpr wchar_t const* PrimaryLanguage                        = L"OpenNet::Globalization::PrimaryLanguage";
	inline constexpr wchar_t const* AnnouncementRegion                     = L"OpenNet::Globalization::Region::Announcement";
																			 
	// UI
	inline constexpr wchar_t const* BackgroundImageType                    = L"OpenNet::UI::BackgroundImage::Type";
	inline constexpr wchar_t const* ElementTheme                           = L"OpenNet::UI::ElementTheme";
	inline constexpr wchar_t const* SystemBackdropType                     = L"OpenNet::UI::SystemBackdropType";
	inline constexpr wchar_t const* GuideState                             = L"OpenNet::UI::Windowing::GuideWindow::State";
	inline constexpr wchar_t const* LastWindowCloseBehavior                = L"OpenNet::UI::Windowing::LastWindowCloseBehavior";
	inline constexpr wchar_t const* IsLastWindowCloseBehaviorSet           = L"OpenNet::UI::Windowing::LastWindowCloseBehavior::Set";
	inline constexpr wchar_t const* IsNavPaneOpen                          = L"OpenNet::UI::Windowing::MainWindow::NavigationView::IsPaneOpen";

	// HomeCard

	// HotKey
	inline constexpr wchar_t const* HotKeyRepeatForeverInGameOnly            = L"OpenNet::HotKey::RepeatForever::InGameOnly";
	inline constexpr wchar_t const* HotKeyKeyPressRepeatForever              = L"OpenNet::HotKey::RepeatForever::KeyPress";
	inline constexpr wchar_t const* HotKeyMouseClickRepeatForever            = L"OpenNet::HotKey::RepeatForever::MouseClick";
	inline constexpr wchar_t const* LowLevelKeyboardWebView2VideoPlayPause   = L"OpenNet::HotKey::LowLevel::WebView2::Video::PlayPause";
	inline constexpr wchar_t const* LowLevelKeyboardWebView2VideoFastForward = L"OpenNet::HotKey::LowLevel::WebView2::Video::FastForward";
	inline constexpr wchar_t const* LowLevelKeyboardWebView2VideoRewind      = L"OpenNet::HotKey::LowLevel::WebView2::Video::Rewind";
	inline constexpr wchar_t const* LowLevelKeyboardWebView2Hide             = L"OpenNet::HotKey::LowLevel::WebView2::Hide";
	inline constexpr wchar_t const* LowLevelKeyboardOverlayHide              = L"OpenNet::HotKey::LowLevel::Overlay::Hide";

	// Passport
	inline constexpr wchar_t const* PassportRefreshToken                     = L"OpenNet::Passport::RefreshToken";
	inline constexpr wchar_t const* PassportUserName                         = L"OpenNet::Passport::UserName";

	// AvatarProperty
	inline constexpr wchar_t const* AvatarPropertySortDescriptionKind    = L"OpenNet::AvatarProperty::SortDescriptionKind";

	// Cultivation
	inline constexpr wchar_t const* CultivationAvatarLevelCurrent           = L"OpenNet::Cultivation::Avatar::Level::Current";
	inline constexpr wchar_t const* CultivationAvatarLevelTarget            = L"OpenNet::Cultivation::Avatar::Level::Target";
	inline constexpr wchar_t const* CultivationAvatarSkillACurrent          = L"OpenNet::Cultivation::Avatar::SkillA::Current";
	inline constexpr wchar_t const* CultivationAvatarSkillATarget           = L"OpenNet::Cultivation::Avatar::SkillA::Target";
	inline constexpr wchar_t const* CultivationAvatarSkillECurrent          = L"OpenNet::Cultivation::Avatar::SkillE::Current";
	inline constexpr wchar_t const* CultivationAvatarSkillETarget           = L"OpenNet::Cultivation::Avatar::SkillE::Target";
	inline constexpr wchar_t const* CultivationAvatarSkillQCurrent          = L"OpenNet::Cultivation::Avatar::SkillQ::Current";
	inline constexpr wchar_t const* CultivationAvatarSkillQTarget           = L"OpenNet::Cultivation::Avatar::SkillQ::Target";
	inline constexpr wchar_t const* CultivationWeapon70LevelCurrent         = L"OpenNet::Cultivation::Weapon70::Level::Current";
	inline constexpr wchar_t const* CultivationWeapon70LevelTarget          = L"OpenNet::Cultivation::Weapon70::Level::Target";
	inline constexpr wchar_t const* CultivationWeapon90LevelCurrent         = L"OpenNet::Cultivation::Weapon90::Level::Current";
	inline constexpr wchar_t const* CultivationWeapon90LevelTarget          = L"OpenNet::Cultivation::Weapon90::Level::Target";
	inline constexpr wchar_t const* ResinStatisticsSelectedDropDistribution = L"OpenNet::Cultivation::ResinStatistics::DropDistribution";

	// Note
	inline constexpr wchar_t const* NoteIsAutoRefreshEnabled               = L"OpenNet::Note::AutoRefresh::Enabled";
	inline constexpr wchar_t const* NoteRefreshSeconds                     = L"OpenNet::Note::RefreshSeconds";
	inline constexpr wchar_t const* NoteReminderNotify                     = L"OpenNet::Note::ReminderNotify";
	inline constexpr wchar_t const* NoteSilentWhenPlayingGame              = L"OpenNet::Note::SilentWhenPlayingGame";
	inline constexpr wchar_t const* NoteWebhookUrl                         = L"OpenNet::Note::Webhook::Url";

	// Geetest
	inline constexpr wchar_t const* GeetestCustomCompositeUrl               = L"OpenNet::Geetest::CustomCompositeUrl";

	// Web
	inline constexpr wchar_t const* ExcludedAnnouncementIds                  = L"OpenNet::Web::Homa::ExcludedAnnouncementIds";
	inline constexpr wchar_t const* StaticResourceImageQuality               = L"OpenNet::Web::StaticResource::ImageQuality";
	inline constexpr wchar_t const* StaticResourceImageArchive               = L"OpenNet::Web::StaticResource::ImageArchive";
	inline constexpr wchar_t const* BridgeShareSaveType                      = L"OpenNet::Web::WebView::BridgeShare::SaveType";
	inline constexpr wchar_t const* CompactWebView2WindowInactiveOpacity     = L"OpenNet::Web::WebView::Compact::InactiveOpacity";
	inline constexpr wchar_t const* CompactWebView2WindowPreviousSourceUrl   = L"OpenNet::Web::WebView::Compact::PreviousSourceUrl";
	inline constexpr wchar_t const* WebView2VideoFastForwardOrRewindSeconds  = L"OpenNet::Web::WebView::Video::FastForwardOrRewind::Seconds";


}