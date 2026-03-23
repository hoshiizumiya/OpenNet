#include "pch.h"
#include "GeoIPDatabase.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace OpenNet::Core
{
	namespace
	{
		constexpr std::array<uint8_t, 14> METADATA_BEGIN_MARK = {
			0xAB, 0xCD, 0xEF, 'M', 'a', 'x', 'M', 'i', 'n', 'd', '.', 'c', 'o', 'm'
		};

		constexpr std::array<uint8_t, 16> DATA_SECTION_SEPARATOR = { 0 };
	}

	bool GeoIPDatabase::Load(std::wstring const& filePath)
	{
		m_data.clear();
		m_countryCache.clear();
		m_ipVersion = 0;
		m_recordSize = 0;
		m_nodeCount = 0;
		m_nodeSize = 0;
		m_indexSize = 0;
		m_recordBytes = 0;
		m_buildEpoch = 0;
		m_dbType.clear();

		if (!std::filesystem::exists(filePath))
			return false;

		std::ifstream file(filePath, std::ios::binary);
		if (!file.is_open())
			return false;

		file.seekg(0, std::ios::end);
		auto const size = static_cast<uint64_t>(file.tellg());
		if ((size == 0) || (size > MAX_FILE_SIZE))
			return false;

		file.seekg(0, std::ios::beg);
		m_data.resize(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(m_data.data()), static_cast<std::streamsize>(m_data.size())))
		{
			m_data.clear();
			return false;
		}

		if (!ParseMetadata())
		{
			m_data.clear();
			return false;
		}

		if (!ValidateDatabase())
		{
			m_data.clear();
			return false;
		}

		return true;
	}

	bool GeoIPDatabase::IsLoaded() const
	{
		return !m_data.empty() && (m_nodeCount > 0) && (m_recordBytes > 0);
	}

	std::string GeoIPDatabase::LookupCountryCode(std::string const& ipAddress) const
	{
		CountryResult result;
		if (!LookupCountry(ipAddress, result))
			return {};

		if (result.isoCode == "-")
			return {};

		return result.isoCode;
	}

	std::string GeoIPDatabase::LookupCountryName(std::string const& ipAddress) const
	{
		CountryResult result;
		if (!LookupCountry(ipAddress, result))
			return {};

		if (!result.englishName.empty())
			return result.englishName;

		return result.isoCode;
	}

	bool GeoIPDatabase::ParseMetadata()
	{
		if (m_data.size() < METADATA_BEGIN_MARK.size())
			return false;

		size_t scanStart = 0;
		if (m_data.size() > MAX_METADATA_SIZE)
			scanStart = m_data.size() - MAX_METADATA_SIZE;

		size_t markerPos = std::string::npos;
		for (size_t i = m_data.size() - METADATA_BEGIN_MARK.size(); i >= scanStart; --i)
		{
			if (std::memcmp(m_data.data() + i, METADATA_BEGIN_MARK.data(), METADATA_BEGIN_MARK.size()) == 0)
			{
				markerPos = i;
				break;
			}
			if (i == scanStart)
				break;
		}

		if (markerPos == std::string::npos)
			return false;

		uint32_t offset = static_cast<uint32_t>(markerPos + METADATA_BEGIN_MARK.size());
		DataValue metadata;
		if (!ReadDataField(offset, metadata))
			return false;

		if (metadata.kind != DataValue::Kind::Map)
			return false;

		auto getUnsigned = [&](std::string const& key, uint64_t& out) -> bool
			{
				auto it = metadata.mapValue.find(key);
				if (it == metadata.mapValue.end())
					return false;
				if (it->second.kind != DataValue::Kind::Unsigned)
					return false;
				out = it->second.unsignedValue;
				return true;
			};

		auto getString = [&](std::string const& key, std::string& out) -> bool
			{
				auto it = metadata.mapValue.find(key);
				if (it == metadata.mapValue.end())
					return false;
				if (it->second.kind != DataValue::Kind::String)
					return false;
				out = it->second.stringValue;
				return true;
			};

		uint64_t majorVersion = 0;
		uint64_t ipVersion = 0;
		uint64_t recordSize = 0;
		uint64_t nodeCount = 0;

		if (!getUnsigned("binary_format_major_version", majorVersion)) return false;
		if (!getUnsigned("ip_version", ipVersion)) return false;
		if (!getUnsigned("record_size", recordSize)) return false;
		if (!getUnsigned("node_count", nodeCount)) return false;
		getUnsigned("build_epoch", m_buildEpoch);
		getString("database_type", m_dbType);

		if (majorVersion != 2)
			return false;
		if (ipVersion != 6)
			return false;
		if (recordSize != 24)
			return false;
		if (nodeCount == 0)
			return false;

		m_ipVersion = static_cast<uint16_t>(ipVersion);
		m_recordSize = static_cast<uint16_t>(recordSize);
		m_nodeCount = static_cast<uint32_t>(nodeCount);
		m_nodeSize = static_cast<uint32_t>(m_recordSize / 4);
		m_recordBytes = static_cast<uint32_t>(m_nodeSize / 2);
		m_indexSize = m_nodeCount * m_nodeSize;

		return true;
	}

	bool GeoIPDatabase::ValidateDatabase() const
	{
		if (m_nodeSize == 0)
			return false;

		auto const minSize = static_cast<size_t>(m_indexSize) + DATA_SECTION_SEPARATOR.size();
		if (m_data.size() < minSize)
			return false;

		return std::memcmp(m_data.data() + m_indexSize, DATA_SECTION_SEPARATOR.data(), DATA_SECTION_SEPARATOR.size()) == 0;
	}

	bool GeoIPDatabase::LookupCountry(std::string const& ipAddress, CountryResult& out) const
	{
		out = {};

		if (!IsLoaded())
			return false;

		std::array<uint8_t, 16> addr{};
		if (!ParseIPAddressToIPv6(ipAddress, addr))
			return false;

		auto const* ptr = m_data.data();
		for (int i = 0; i < 16; ++i)
		{
			for (int j = 0; j < 8; ++j)
			{
				bool const right = ((addr[static_cast<size_t>(i)] >> (7 - j)) & 1) != 0;
				auto const* recordPtr = ptr + (right ? m_recordBytes : 0);

				uint32_t id = 0;
				for (uint32_t b = 0; b < m_recordBytes; ++b)
				{
					id = static_cast<uint32_t>((id << 8) | recordPtr[b]);
				}

				if (id == m_nodeCount)
					return false;

				if (id > m_nodeCount)
				{
					auto cached = m_countryCache.find(id);
					if (cached != m_countryCache.end())
					{
						out = cached->second;
						return !out.isoCode.empty();
					}

					uint32_t const localOffset = id - m_nodeCount - static_cast<uint32_t>(DATA_SECTION_SEPARATOR.size());
					uint32_t dataOffset = localOffset + m_indexSize + static_cast<uint32_t>(DATA_SECTION_SEPARATOR.size());

					DataValue value;
					if (!ReadDataField(dataOffset, value))
						return false;

					CountryResult parsed;
					if (!ExtractCountryFromDataValue(value, parsed) || parsed.isoCode.empty())
						return false;

					m_countryCache[id] = parsed;
					out = std::move(parsed);
					return true;
				}

				auto const nextOffset = static_cast<size_t>(id) * m_nodeSize;
				if ((nextOffset + m_nodeSize) > m_data.size())
					return false;

				ptr = m_data.data() + nextOffset;
			}
		}

		return false;
	}

	bool GeoIPDatabase::ReadDataField(uint32_t& offset, DataValue& out, int depth) const
	{
		out = {};
		if (depth > 24)
			return false;

		DataFieldDescriptor descriptor;
		if (!ReadDataFieldDescriptor(offset, descriptor))
			return false;

		if (descriptor.isPointer)
		{
			uint32_t pointerOffset = descriptor.pointerOffset + m_indexSize + static_cast<uint32_t>(DATA_SECTION_SEPARATOR.size());
			return ReadDataField(pointerOffset, out, depth + 1);
		}

		switch (descriptor.type)
		{
			case DataType::String:
				out.kind = DataValue::Kind::String;
				return ReadString(offset, descriptor.fieldSize, out.stringValue);

			case DataType::Bytes:
				if ((static_cast<size_t>(offset) + descriptor.fieldSize) > m_data.size())
					return false;
				out.kind = DataValue::Kind::Bytes;
				out.bytesValue.assign(m_data.begin() + offset, m_data.begin() + offset + descriptor.fieldSize);
				offset += descriptor.fieldSize;
				return true;

			case DataType::Map:
				out.kind = DataValue::Kind::Map;
				for (uint32_t i = 0; i < descriptor.fieldSize; ++i)
				{
					DataValue key;
					if (!ReadDataField(offset, key, depth + 1) || (key.kind != DataValue::Kind::String))
						return false;

					DataValue value;
					if (!ReadDataField(offset, value, depth + 1))
						return false;

					out.mapValue.emplace(key.stringValue, std::move(value));
				}
				return true;

			case DataType::Array:
				out.kind = DataValue::Kind::Array;
				out.arrayValue.reserve(descriptor.fieldSize);
				for (uint32_t i = 0; i < descriptor.fieldSize; ++i)
				{
					DataValue value;
					if (!ReadDataField(offset, value, depth + 1))
						return false;
					out.arrayValue.push_back(std::move(value));
				}
				return true;

			case DataType::Integer16:
			case DataType::Integer32:
			case DataType::Integer64:
			case DataType::Integer128:
				out.kind = DataValue::Kind::Unsigned;
				return ReadUnsignedInteger(offset, descriptor.fieldSize, out.unsignedValue);

			case DataType::SignedInteger32:
				out.kind = DataValue::Kind::Signed;
				return ReadSignedInteger(offset, descriptor.fieldSize, out.signedValue);

			case DataType::Boolean:
				out.kind = DataValue::Kind::Boolean;
				out.boolValue = (descriptor.fieldSize != 0);
				return true;

			default:
				return false;
		}
	}

	bool GeoIPDatabase::ReadDataFieldDescriptor(uint32_t& offset, DataFieldDescriptor& out) const
	{
		if (offset >= m_data.size())
			return false;

		auto const* dataPtr = m_data.data() + offset;
		auto const availSize = static_cast<uint32_t>(m_data.size() - offset);
		if (availSize < 1)
			return false;

		out = {};
		out.type = static_cast<DataType>((dataPtr[0] & 0xE0) >> 5);

		if (out.type == DataType::Pointer)
		{
			uint32_t const size = ((dataPtr[0] & 0x18) >> 3);
			if (availSize < (size + 2))
				return false;

			out.isPointer = true;
			if (size == 0)
				out.pointerOffset = ((dataPtr[0] & 0x07) << 8) + dataPtr[1];
			else if (size == 1)
				out.pointerOffset = ((dataPtr[0] & 0x07) << 16) + (dataPtr[1] << 8) + dataPtr[2] + 2048;
			else if (size == 2)
				out.pointerOffset = ((dataPtr[0] & 0x07) << 24) + (dataPtr[1] << 16) + (dataPtr[2] << 8) + dataPtr[3] + 526336;
			else if (size == 3)
				out.pointerOffset = (dataPtr[1] << 24) + (dataPtr[2] << 16) + (dataPtr[3] << 8) + dataPtr[4];
			else
				return false;

			offset += size + 2;
			return true;
		}

		uint32_t fieldSize = (dataPtr[0] & 0x1F);
		if (fieldSize <= 28)
		{
			if (out.type == DataType::Unknown)
			{
				if (availSize < 2)
					return false;
				out.type = static_cast<DataType>(dataPtr[1] + 7);
				if ((out.type <= DataType::Map) || (out.type > DataType::Float))
					return false;
				offset += 2;
			}
			else
			{
				offset += 1;
			}
		}
		else if (fieldSize == 29)
		{
			if (availSize < 2)
				return false;
			fieldSize = dataPtr[1] + 29;
			offset += 2;
		}
		else if (fieldSize == 30)
		{
			if (availSize < 3)
				return false;
			fieldSize = (dataPtr[1] << 8) + dataPtr[2] + 285;
			offset += 3;
		}
		else
		{
			if (availSize < 4)
				return false;
			fieldSize = (dataPtr[1] << 16) + (dataPtr[2] << 8) + dataPtr[3] + 65821;
			offset += 4;
		}

		out.fieldSize = fieldSize;
		return true;
	}

	bool GeoIPDatabase::ReadUnsignedInteger(uint32_t& offset, uint32_t len, uint64_t& out) const
	{
		if (len > 8)
			return false;

		if (len == 0)
		{
			out = 0;
			return true;
		}

		if ((static_cast<size_t>(offset) + len) > m_data.size())
			return false;

		uint64_t value = 0;
		for (uint32_t i = 0; i < len; ++i)
		{
			value = (value << 8) | m_data[offset + i];
		}

		offset += len;
		out = value;
		return true;
	}

	bool GeoIPDatabase::ReadSignedInteger(uint32_t& offset, uint32_t len, int64_t& out) const
	{
		if (len > 8)
			return false;

		if (len == 0)
		{
			out = 0;
			return true;
		}

		if ((static_cast<size_t>(offset) + len) > m_data.size())
			return false;

		int64_t value = (m_data[offset] & 0x80) ? -1 : 0;
		for (uint32_t i = 0; i < len; ++i)
		{
			value = (value << 8) | m_data[offset + i];
		}

		offset += len;
		out = value;
		return true;
	}

	bool GeoIPDatabase::ReadString(uint32_t& offset, uint32_t len, std::string& out) const
	{
		if ((static_cast<size_t>(offset) + len) > m_data.size())
			return false;

		out.assign(reinterpret_cast<char const*>(m_data.data() + offset), len);
		offset += len;
		return true;
	}

	bool GeoIPDatabase::ExtractCountryFromDataValue(DataValue const& value, CountryResult& out) const
	{
		out = {};

		if (value.kind != DataValue::Kind::Map)
			return false;

		auto countryIt = value.mapValue.find("country");
		if (countryIt == value.mapValue.end() || (countryIt->second.kind != DataValue::Kind::Map))
			return false;

		auto const& country = countryIt->second;

		auto isoIt = country.mapValue.find("iso_code");
		if ((isoIt != country.mapValue.end()) && (isoIt->second.kind == DataValue::Kind::String))
			out.isoCode = isoIt->second.stringValue;

		auto namesIt = country.mapValue.find("names");
		if ((namesIt != country.mapValue.end()) && (namesIt->second.kind == DataValue::Kind::Map))
		{
			auto enIt = namesIt->second.mapValue.find("en");
			if ((enIt != namesIt->second.mapValue.end()) && (enIt->second.kind == DataValue::Kind::String))
				out.englishName = enIt->second.stringValue;
		}

		return !out.isoCode.empty();
	}

	bool GeoIPDatabase::ParseIPAddressToIPv6(std::string const& ipAddress, std::array<uint8_t, 16>& out)
	{
		boost::system::error_code ec;
		auto addr = boost::asio::ip::make_address(ipAddress, ec);
		if (ec)
			return false;

		if (addr.is_v4())
		{
			auto mapped = boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped, addr.to_v4());
			out = mapped.to_bytes();
			return true;
		}

		out = addr.to_v6().to_bytes();
		return true;
	}

	bool GeoIPDatabase::ParseIPv4(std::string const& ip, std::array<uint8_t, 4>& out)
	{
		boost::system::error_code ec;
		auto addr = boost::asio::ip::make_address_v4(ip, ec);
		if (ec)
			return false;

		out = addr.to_bytes();
		return true;
	}

	bool GeoIPDatabase::ParseIPv6(std::string ip, std::array<uint8_t, 16>& out)
	{
		boost::system::error_code ec;
		auto addr = boost::asio::ip::make_address_v6(ip, ec);
		if (ec)
			return false;

		out = addr.to_bytes();
		return true;
	}
}
