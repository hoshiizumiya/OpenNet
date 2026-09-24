module;

#include <openssl/evp.h>

module OpenNet.Core.Content.ResourceKey;

import winrt.Windows.Foundation;
import std;

namespace OpenNet::Core::Content
{
    namespace
    {
        std::string Lower(std::string value)
        {
            std::ranges::transform(
                value,
                value.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            return value;
        }

        bool IsSensitiveQueryName(std::string name)
        {
            name = Lower(std::move(name));
            static constexpr std::string_view exact[] = {
                "token",
                "access_token",
                "auth",
                "authorization",
                "signature",
                "sig",
                "credential",
                "credentials",
                "api_key",
                "apikey",
                "key",
                "secret",
                "session",
                "sessionid",
                "jwt"
            };
            if (std::ranges::find(exact, name) != std::end(exact))
                return true;

            return name.contains("token")
                || name.contains("signature")
                || name.starts_with("x-amz-")
                || name.starts_with("x-goog-")
                || name.starts_with("x-oss-")
                || name.starts_with("x-cos-");
        }

        std::array<std::uint8_t, 32> Sha256(std::string_view value)
        {
            std::array<std::uint8_t, 32> digest{};
            EVP_MD_CTX* raw = EVP_MD_CTX_new();
            if (!raw)
                throw std::runtime_error("Unable to allocate SHA-256 context.");

            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>
                context{ raw, &EVP_MD_CTX_free };
            if (EVP_DigestInit_ex(
                context.get(), EVP_sha256(), nullptr) != 1
                || EVP_DigestUpdate(
                    context.get(), value.data(), value.size()) != 1)
                throw std::runtime_error("Unable to hash resource key.");

            unsigned int size{};
            if (EVP_DigestFinal_ex(
                context.get(), digest.data(), &size) != 1
                || size != digest.size())
                throw std::runtime_error("Unable to finalize resource key.");

            return digest;
        }
    }

    std::string ResourceKey::ToHex() const
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result(digest.size() * 2, '\0');
        for (std::size_t i = 0; i < digest.size(); ++i)
        {
            result[i * 2] = digits[digest[i] >> 4];
            result[i * 2 + 1] = digits[digest[i] & 0x0f];
        }
        return result;
    }

    namespace
    {
        std::optional<std::string> CanonicalPublicHttpUrl(
            std::string_view url)
        {
            if (url.empty()) return std::nullopt;

            winrt::Windows::Foundation::Uri uri{
                winrt::to_hstring(std::string(url))
            };

            auto scheme = Lower(winrt::to_string(uri.SchemeName()));
            if (scheme != "http" && scheme != "https")
                return std::nullopt;

            if (!uri.UserName().empty() || !uri.Password().empty())
                return std::nullopt;

            for (auto const& query : uri.QueryParsed())
            {
                if (IsSensitiveQueryName(
                    winrt::to_string(query.Name())))
                    return std::nullopt;
            }

            auto host = Lower(winrt::to_string(uri.Host()));
            if (host.empty()) return std::nullopt;

            std::string canonical;
            canonical.reserve(url.size() + 64);
            canonical += scheme;
            canonical += "://";

            if (host.contains(':')
                && !(host.starts_with('[') && host.ends_with(']')))
            {
                canonical += '[';
                canonical += host;
                canonical += ']';
            }
            else
            {
                canonical += host;
            }

            int const port = uri.Port();
            bool const defaultPort =
                (scheme == "http" && (port == 0 || port == 80))
                || (scheme == "https" && (port == 0 || port == 443));
            if (!defaultPort && port > 0)
            {
                canonical += ':';
                canonical += std::to_string(port);
            }

            auto path = winrt::to_string(uri.Path());
            canonical += path.empty() ? "/" : path;

            auto query = winrt::to_string(uri.Query());
            if (!query.empty())
            {
                if (query.front() != '?')
                    canonical += '?';
                canonical += query;
            }

            return canonical;
        }
    }

    std::optional<ResourceKey> ResourceKeyFactory::FromHttpUrl(
        std::string_view url)
    {
        try
        {
            auto canonical = CanonicalPublicHttpUrl(url);
            if (!canonical) return std::nullopt;

            constexpr std::string_view domain =
                "OpenNet.Resource.ExactUrlSha256V1\n";
            std::string input;
            input.reserve(domain.size() + canonical->size());
            input.append(domain);
            input.append(*canonical);

            ResourceKey result;
            result.algorithm = ResourceKeyAlgorithm::ExactUrlSha256V1;
            result.digest = Sha256(input);
            return result;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<ResourceKey> ResourceKeyFactory::FromHttpValidator(
        HttpResourceValidator const& validator)
    {
        if (validator.strongETag.empty()
            || validator.contentLength == 0)
            return std::nullopt;

        auto etag = validator.strongETag;
        auto trimmed = std::string_view{ etag };
        while (!trimmed.empty()
            && std::isspace(
                static_cast<unsigned char>(trimmed.front())))
            trimmed.remove_prefix(1);
        if (trimmed.starts_with("W/")
            || trimmed.starts_with("w/"))
            return std::nullopt;

        try
        {
            auto canonical = CanonicalPublicHttpUrl(
                validator.finalUrl);
            if (!canonical) return std::nullopt;

            constexpr std::string_view domain =
                "OpenNet.Resource.HttpValidatorSha256V1\n";
            std::string input;
            input.reserve(
                domain.size()
                + canonical->size()
                + validator.strongETag.size()
                + 64);
            input.append(domain);
            input.append(*canonical);
            input.append("\netag:");
            input.append(validator.strongETag);
            input.append("\nlength:");
            input.append(std::to_string(validator.contentLength));

            ResourceKey result;
            result.algorithm = ResourceKeyAlgorithm::HttpValidatorSha256V1;
            result.digest = Sha256(input);
            return result;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}
