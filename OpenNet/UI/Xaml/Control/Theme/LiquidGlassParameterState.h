#pragma once

namespace OpenNet::UI
{
	// Edits stay in memory until all native setters succeed. Independent of XAML/storage.
	class LiquidGlassParameterState
	{
	public:
		constexpr LiquidGlassParameterState(double minimum = 0, double maximum = 32, double preset = 0) noexcept
			: m_minimum(minimum), m_maximum(maximum), m_preset(preset), m_value(preset), m_committed(preset)
		{
		}
		constexpr bool Accepts(double value) const noexcept
		{
			return value >= m_minimum && value <= m_maximum;
		}
		constexpr double Value() const noexcept
		{
			return m_value;
		}
		constexpr double Committed() const noexcept
		{
			return m_committed;
		}
		constexpr double DefaultValue() const noexcept
		{
			return m_preset;
		}
		constexpr bool Overridden() const noexcept
		{
			return m_value != m_preset;
		}
		constexpr bool Pending() const noexcept
		{
			return m_value != m_committed;
		}
		constexpr unsigned long long Revision() const noexcept
		{
			return m_revision;
		}
		bool Load(double value) noexcept
		{
			if (!Accepts(value)) return false; // Finite bounds also reject NaN/infinities.
			m_value = m_committed = value;
			return true;
		}
		bool Stage(double value) noexcept
		{
			if (!Accepts(value)) return false;
			Assign(value);
			return true;
		}
		void Commit() noexcept
		{
			m_committed = m_value;
		}
		void Rollback() noexcept
		{
			Assign(m_committed);
		}
		void Reset() noexcept
		{
			Assign(m_preset);
		}
		bool RestoreRejected() noexcept
		{
			if (Pending())
			{
				Rollback(); return true;
			}
			if (m_committed == m_preset) return false;
			m_committed = m_preset;
			Assign(m_preset);
			return true;
		}
	private:
		void Assign(double value) noexcept
		{
			if (m_value == value) return;
			m_value = value;
			++m_revision;
		}
		double m_minimum, m_maximum, m_preset, m_value, m_committed;
		unsigned long long m_revision{};
	};

	enum class LiquidGlassParameterApplyResult
	{
		Applied, Recovered, Unsupported
	};

	// The native adapter returns false only for a rejected argument. Unexpected
	// failures propagate to the dispatcher boundary; they are not treated as bad input.
	template<typename Parameter, typename Write>
	LiquidGlassParameterApplyResult ApplyLiquidGlassParameter(Parameter& parameter, double cachedDefault, Write&& write)
	{
		double effective = parameter.Overridden() ? parameter.Value() : cachedDefault;
		bool recovered = false;
		for (int attempt = 0; attempt < 3; ++attempt)
		{
			if (write(effective))
				return recovered ? LiquidGlassParameterApplyResult::Recovered : LiquidGlassParameterApplyResult::Applied;
			recovered = true;
			if (parameter.Value() == parameter.DefaultValue() && effective == parameter.DefaultValue()) break;
			parameter.RestoreRejected();
			effective = parameter.Value();
		}
		return LiquidGlassParameterApplyResult::Unsupported;
	}
}
