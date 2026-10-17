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
                std::format("INTERNET_RESPONSE_ERROR: {}, ssl_error={}, ssl_backend_error=0x{:X}",
                            httplib::to_string(res.error()),
                            res.ssl_error(),
                            res.ssl_backend_error())
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

    namespace
    {
        // RAII 包装器
        struct EvpPkeyDeleter { void operator()(EVP_PKEY* p) const noexcept { EVP_PKEY_free(p); } };
        using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

        struct BioDeleter { void operator()(BIO* p) const noexcept { BIO_free(p); } };
        using BioPtr = std::unique_ptr<BIO, BioDeleter>;

        struct PkeyCtxDeleter { void operator()(EVP_PKEY_CTX* p) const noexcept { EVP_PKEY_CTX_free(p); } };
        using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, PkeyCtxDeleter>;

        void log_openssl_error(const std::string_view msg) {
            const unsigned long err = ERR_get_error();
            char err_buf[256];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            Logger::instance().error(std::format("{}: {}", msg, err_buf));
        }
    }

    std::tuple<std::string, std::string> generateEd25519Keypair() {
        std::tuple<std::string, std::string> key_pair;

        auto log_error = [](const std::string_view msg) {
            log_openssl_error(msg);
            Logger::instance().error(tr::txt(tr::TID::ERR_GEN_KEYPAIR_FAILED));
        };

        // 初始化 OpenSSL 上下文
        const PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
        if (!ctx) {
            log_error("Failed to create contex");
            return key_pair;
        }

        // 密钥生成初始化
        if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
            log_error("Failed to init keygen");
            return key_pair;
        }

        // 生成密钥
        EVP_PKEY* raw_pkey = nullptr;
        if (EVP_PKEY_keygen(ctx.get(), &raw_pkey) <= 0) {
            log_error("Failed to create raw key");
            return key_pair;
        }
        const EvpPkeyPtr pkey(raw_pkey);

        // 将私钥写入内存 BIO
        auto export_to_pem = [&log_error](const EVP_PKEY* key, const std::function<int(BIO*, const EVP_PKEY*)> &write_func) -> std::optional<std::string> {
            const BioPtr bio(BIO_new(BIO_s_mem()));
            if (!bio) {
                log_error("Failed to create BIO");
                return std::nullopt;
            }

            if (write_func(bio.get(), key) <= 0) {
                log_error("Failed to write BIO");
                return std::nullopt;
            }

            std::string result;
            char buffer[4096];
            int bytes;
            while ((bytes = BIO_read(bio.get(), buffer, sizeof(buffer))) > 0) {
                result.append(buffer, bytes);
            }
            return result;
        };

        // 导出公私钥
        const auto prv_key = export_to_pem(
            pkey.get(),
            [](BIO *bio, const EVP_PKEY *k) -> int {
                return PEM_write_bio_PrivateKey(bio, k, nullptr, nullptr, 0, nullptr, nullptr);
            }
        );
        if (prv_key) {
            std::get<0>(key_pair) = *prv_key;
        }

        if (const auto pub_key = export_to_pem(pkey.get(), PEM_write_bio_PUBKEY);
            pub_key) {
            std::get<1>(key_pair) = *pub_key;
        }

        return key_pair;
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

        const auto utc_seconds = parse_iso_datetime_to_utc_seconds(iso_datatime);
        return current_zone()->to_local(sys_seconds{ seconds{ utc_seconds } }).time_since_epoch().count();
    }

    std::string string_join(const std::vector<std::string> &parts, std::string_view delim) {
        std::string result;

        for (size_t i = 0; i < parts.size(); ++i) {
            if (i != 0) result.append(delim);
            result.append(parts[i]);
        }
        return result;
    }
}
