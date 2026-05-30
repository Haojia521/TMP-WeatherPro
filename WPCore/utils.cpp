#include "utils.h"
#include "AppLocale.h"
#include "Logger.h"

#include <format>
#include <chrono>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <httplib.h>

namespace utils
{
    int internetGet(const std::string &host, const std::string &path, std::string &content,
                    const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/)
    {
        using namespace std::chrono_literals;

        content.clear();

        httplib::Client clt(host);
        clt.set_connection_timeout(5s);
        clt.set_read_timeout(5s);

        httplib::Params lib_params;
        lib_params.insert(params.data.begin(), params.data.end());

        httplib::Headers lib_headers;
        lib_headers.insert(headers.data.begin(), headers.data.end());

        auto res = clt.Get(path, lib_params, lib_headers);

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

    int internetGetWithRetry(const std::string &host, const std::string &path, std::string &content,
                             const HttpParams &params/* = {}*/, const HttpHeaders &headers/* = {}*/,
                             int retry_times/* = 3*/) {
        // retry getting data if internet connection issues occurred
        auto status_code{ 0 };
        do {
            status_code = utils::internetGet(host, path, content, params, headers);
            if (status_code != 0) {
                break;
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5s);
            --retry_times;
        } while (retry_times > 0);

        return status_code;
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
            Logger::instance().error(std::format("{}(01)", tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }

        // 密钥生成初始化
        if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
            //Logger::instance().error(L"Failed to initialize key generation");
            Logger::instance().error(std::format("{}(02)", tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }

        // 生成密钥
        EVP_PKEY* raw_pkey = nullptr;
        if (EVP_PKEY_keygen(ctx.get(), &raw_pkey) <= 0) {
            //Logger::instance().error(L"Failed to generate key pair");
            Logger::instance().error(std::format("{}(03)", tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED)));
            return { "", "" };
        }
        EvpPkeyPtr pkey(raw_pkey);

        // 将私钥写入内存 BIO
        auto export_to_pem = [](const EVP_PKEY* key, std::function<int(BIO*, const EVP_PKEY*)> write_func) -> std::string {
            BioPtr bio(BIO_new(BIO_s_mem()));
            if (!bio) {
                //Logger::instance().error(L"Failed to create BIO");
                Logger::instance().error(std::format("{}(11)", tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED)));
                return "";
            }

            if (write_func(bio.get(), key) <= 0) {
                //Logger::instance().error(L"Failed to write key to BIO");
                Logger::instance().error(std::format("{}(12)", tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED)));
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

    std::int64_t parse_iso_datetime_to_utc_seconds(std::string_view iso_datatime) {
        using namespace std::chrono;

        std::string s(iso_datatime);

        auto parse_z = [](const std::string& text)
        {
            local_time<seconds> lt{};

            {
                std::istringstream iss(text);
                iss >> parse("%FT%T", lt);   // YYYY-MM-DDTHH:MM:SS
                if (!iss.fail()) {
                    return lt.time_since_epoch().count();
                }
            }

            {
                std::istringstream iss(text);
                iss >> parse("%FT%R", lt);   // YYYY-MM-DDTHH:MM
                if (!iss.fail()) {
                    return lt.time_since_epoch().count();
                }
            }

            throw std::runtime_error("failed to parse UTC datetime: " + text);
        };

        auto parse_with_offset = [](const std::string& text)
        {
            local_time<seconds> lt{};
            minutes offset{};

            {
                std::istringstream iss(text);
                iss >> parse("%FT%T%Ez", lt, offset); // YYYY-MM-DDTHH:MM:SS+08:00
                if (!iss.fail()) {
                    return (lt.time_since_epoch() - offset).count();
                }
            }

            {
                std::istringstream iss(text);
                iss >> parse("%FT%R%Ez", lt, offset); // YYYY-MM-DDTHH:MM+08:00
                if (!iss.fail()) {
                    return (lt.time_since_epoch() - offset).count();
                }
            }

            throw std::runtime_error("failed to parse offset datetime: " + text);
        };

        std::int64_t utc_seconds{};

        try {
            if (!s.empty() && s.back() == 'Z') {
                s.pop_back();
                utc_seconds = parse_z(s);
            } else {
                utc_seconds = parse_with_offset(s);
            }
        }
        catch (const std::exception &e) {
            Logger::instance().error(e.what());
        }

        return utc_seconds;
    }

    std::int64_t parse_iso_datetime_to_local_seconds(std::string_view iso_datatime) {
        using namespace std::chrono;

        auto utc_seconds = parse_iso_datetime_to_utc_seconds(iso_datatime);
        return current_zone()->to_local(sys_seconds{ seconds{ utc_seconds } }).time_since_epoch().count();
    }
}
