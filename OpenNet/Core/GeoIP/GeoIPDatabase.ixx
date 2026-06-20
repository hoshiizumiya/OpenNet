/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */
export module OpenNet.Core.GeoIP.GeoIPDatabase;

import std;

export namespace OpenNet::Core
{
    class GeoIPDatabase
    {
    public:
        struct CountryResult
        {
            std::string isoCode;
            std::string englishName;
        };

        GeoIPDatabase() = default;
        ~GeoIPDatabase() = default;

        GeoIPDatabase(GeoIPDatabase const&) = delete;
        GeoIPDatabase& operator=(GeoIPDatabase const&) = delete;

        bool Load(std::wstring const& filePath);
        bool IsLoaded() const;

        std::string LookupCountryCode(std::string const& ipAddress) const;
        std::string LookupCountryName(std::string const& ipAddress) const;

    private:
        enum class DataType : std::uint8_t
        {
            Unknown = 0,
            Pointer = 1,
            String = 2,
            Double = 3,
            Bytes = 4,
            Integer16 = 5,
            Integer32 = 6,
            Map = 7,
            SignedInteger32 = 8,
            Integer64 = 9,
            Integer128 = 10,
            Array = 11,
            DataCacheContainer = 12,
            EndMarker = 13,
            Boolean = 14,
            Float = 15
        };

        struct DataFieldDescriptor
        {
            DataType type{ DataType::Unknown };
            bool isPointer{ false };
            std::uint32_t pointerOffset{ 0 };
            std::uint32_t fieldSize{ 0 };
        };

        struct DataValue
        {
            enum class Kind
            {
                None,
                String,
                Unsigned,
                Signed,
                Boolean,
                Bytes,
                Map,
                Array
            };

            Kind kind{ Kind::None };
            std::string stringValue;
            std::uint64_t unsignedValue{ 0 };
            std::int64_t signedValue{ 0 };
            bool boolValue{ false };
            std::vector<std::uint8_t> bytesValue;
            std::unordered_map<std::string, DataValue> mapValue;
            std::vector<DataValue> arrayValue;
        };

        bool ParseMetadata();
        bool ValidateDatabase() const;
        bool LookupCountry(std::string const& ipAddress, CountryResult& out) const;

        bool ReadDataField(std::uint32_t& offset, DataValue& out, int depth = 0) const;
        bool ReadDataFieldDescriptor(std::uint32_t& offset, DataFieldDescriptor& out) const;
        bool ReadUnsignedInteger(std::uint32_t& offset, std::uint32_t len, std::uint64_t& out) const;
        bool ReadSignedInteger(std::uint32_t& offset, std::uint32_t len, std::int64_t& out) const;
        bool ReadString(std::uint32_t& offset, std::uint32_t len, std::string& out) const;
        bool ExtractCountryFromDataValue(DataValue const& value, CountryResult& out) const;

        static bool ParseIPAddressToIPv6(std::string const& ipAddress, std::array<std::uint8_t, 16>& out);
        static bool ParseIPv4(std::string const& ip, std::array<std::uint8_t, 4>& out);
        static bool ParseIPv6(std::string ip, std::array<std::uint8_t, 16>& out);

        static constexpr std::uint32_t MAX_FILE_SIZE = 67108864;
        static constexpr std::uint32_t MAX_METADATA_SIZE = 131072;

        std::uint16_t m_ipVersion{ 0 };
        std::uint16_t m_recordSize{ 0 };
        std::uint32_t m_nodeCount{ 0 };
        std::uint32_t m_nodeSize{ 0 };
        std::uint32_t m_indexSize{ 0 };
        std::uint32_t m_recordBytes{ 0 };
        std::uint64_t m_buildEpoch{ 0 };
        std::string m_dbType;

        std::vector<std::uint8_t> m_data;
        mutable std::unordered_map<std::uint32_t, CountryResult> m_countryCache;
    };
}