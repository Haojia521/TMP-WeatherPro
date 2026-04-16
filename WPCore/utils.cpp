#include <windows.h>

#include "utils.h"
#include "AppLocale.h"
#include "Logger.h"

#include <vector>
#include <format>
#include <chrono>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <httplib.h>

namespace utils
{
    std::wstring multiByte2WideChar(const char *str, bool from_utf8 /* = true */) {
        if (str == nullptr) {
            return std::wstring();
        }

        auto code_page = from_utf8 ? CP_UTF8 : CP_ACP;
        auto len = MultiByteToWideChar(code_page, 0, str, -1, nullptr, 0);
        if (len <= 0) {
            Logger::instance().error("STR_TRANS_MB2WC_FAILED");
            return std::wstring();
        }

        std::vector<wchar_t> buffer(len, 0);
        MultiByteToWideChar(code_page, 0, str, -1, buffer.data(), len);

        return std::wstring(buffer.data());
    }

    std::string wideChar2MultiByte(const wchar_t *str, bool to_utf8 /* = true */) {
        if (str == nullptr) {
            return std::string();
        }

        auto code_page = to_utf8 ? CP_UTF8 : CP_ACP;
        auto len = WideCharToMultiByte(code_page, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            Logger::instance().error("STR_TRANS_WC2MB_FAILED");
            return std::string();
        }

        std::vector<char> buffer(len, 0);
        WideCharToMultiByte(code_page, 0, str, -1, buffer.data(), len, nullptr, nullptr);

        return std::string(buffer.data());
    }

    /******************************************************************************************************************/

    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    std::unordered_map<std::string, std::string> headers /* = {} */) {
        content.clear();

        httplib::Client clt(host);

        httplib::Headers lib_headers;
        for (const auto &h : headers) {
            lib_headers.insert(h);
        }
        auto res = lib_headers.empty() ? clt.Get(path) : clt.Get(path, lib_headers);
        if (res) {
            if (res->status == 404) return res->status;

            content = std::move(res->body);
            return res->status;
        } else {
            Logger::instance().error(
                std::format("INTERNET_RESPONSE_ERROR: {}", httplib::to_string(res.error()))
            );
            return 0;
        }
    }

    /******************************************************************************************************************/

    // RAII 包装器
    struct EvpPkeyDeleter { void operator()(EVP_PKEY* p) noexcept { EVP_PKEY_free(p); } };
    using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

    struct BioDeleter { void operator()(BIO* p) noexcept { BIO_free(p); } };
    using BioPtr = std::unique_ptr<BIO, BioDeleter>;

    struct PkeyCtxDeleter { void operator()(EVP_PKEY_CTX* p) noexcept { EVP_PKEY_CTX_free(p); } };
    using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, PkeyCtxDeleter>;

    std::tuple<std::string, std::string> generateEd25519Keypair() {
        // 初始化 OpenSSL 上下文
        PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
        if (!ctx) {
            //Logger::instance().error(L"Failed to create EVP_PKEY_CTX");
            Logger::instance().error(std::format("{}(01)", tr::txt(tr::TextID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }

        // 密钥生成初始化
        if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
            //Logger::instance().error(L"Failed to initialize key generation");
            Logger::instance().error(std::format("{}(02)", tr::txt(tr::TextID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }

        // 生成密钥
        EVP_PKEY* raw_pkey = nullptr;
        if (EVP_PKEY_keygen(ctx.get(), &raw_pkey) <= 0) {
            //Logger::instance().error(L"Failed to generate key pair");
            Logger::instance().error(std::format("{}(03)", tr::txt(tr::TextID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }
        EvpPkeyPtr pkey(raw_pkey);

        // 将私钥写入内存 BIO
        auto export_to_pem = [](const EVP_PKEY* key, std::function<int(BIO*, const EVP_PKEY*)> write_func) -> std::string {
            BioPtr bio(BIO_new(BIO_s_mem()));
            if (!bio) {
                //Logger::instance().error(L"Failed to create BIO");
                Logger::instance().error(std::format("{}(11)", tr::txt(tr::TextID::ERR_GEN_KEYPAIR_FAILED)));
                return "";
            }

            if (write_func(bio.get(), key) <= 0) {
                //Logger::instance().error(L"Failed to write key to BIO");
                Logger::instance().error(std::format("{}(12)", tr::txt(tr::TextID::ERR_GEN_KEYPAIR_FAILED)));
                return "";
            }

            char* data = nullptr;
            long len = BIO_get_mem_data(bio.get(), &data);
            return std::string(data, static_cast<size_t>(len));
        };

        // 导出公私钥
        std::string priv_pem = export_to_pem(pkey.get(),
                                             [](BIO *bio, const EVP_PKEY *k) -> int {
            return PEM_write_bio_PrivateKey(bio, k, nullptr, nullptr, 0, nullptr, nullptr);
        }
        );
        std::string pub_pem = export_to_pem(pkey.get(), PEM_write_bio_PUBKEY);

        return { std::move(priv_pem), std::move(pub_pem) };
    }

    /******************************************************************************************************************/

    std::string timestamp_string(std::int64_t sec) {
        using namespace std::chrono;

        return std::format("{:%F %T}", sys_seconds{ seconds{sec} });
    }

    std::string timestamp_string_date(std::int64_t sec) {
        using namespace std::chrono;

        const auto dp = floor<days>(sys_seconds{ seconds{sec} });
        return std::format("{:%F}", year_month_day{ dp });
    }

    std::string timestamp_string_time(std::int64_t sec) {
        using namespace std::chrono;

        const auto tp = sys_seconds{ seconds{sec} };
        const auto day_time = tp - floor<days>(tp);
        return std::format("{:%T}", hh_mm_ss{ day_time });
    }
}
