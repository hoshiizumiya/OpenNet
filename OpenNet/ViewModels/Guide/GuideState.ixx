export module OpenNet.ViewModels.Guide.GuideState;

import std;

/// <summary>
/// 指引状态
/// </summary>
export namespace OpenNet::ViewModels::Guide
{
	enum class GuideState : std::uint32_t
	{
		/// <summary>
		/// 选择语言
		/// </summary>
		Language,

		/// <summary>
		/// 查看文档与隐私政策
		/// </summary>
		Document,

		/// <summary>
		/// 查看环境配置
		/// </summary>
		Environment,

		/// <summary>
		/// 查看数据文件夹
		/// </summary>
		DataFolder,

		/// <summary>
		/// 查看常用设置
		/// </summary>
		CommonSetting,

		/// <summary>
		/// 完成
		/// </summary>
		Completed,
	};
}