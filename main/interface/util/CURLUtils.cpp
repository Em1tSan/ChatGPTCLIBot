//
// Created by v2ray on 2023/3/6.
//

#include "CURLUtils.h"

namespace util {

    inline std::string search_ca_cert() {
#ifdef __linux__
        static const std::vector<std::filesystem::path> ca_cert_extensions = {".crt", ".pem"};
        static const std::vector<std::filesystem::path> possible_ca_path = {
                "/etc/ssl/certs",
                "/etc/pki/tls/certs",
                "/usr/local/share/ca-certificates",
                "/usr/local/etc/ssl/certs",
                "/usr/share/ca-certificates",
                "/usr/share/ssl/certs",
                "/usr/local/ssl/certs",
                "/etc/pki/ca-trust/extracted/pem",
                "/etc/pki/ca-trust/extracted/openssl"
        };
        for (const auto& path : possible_ca_path) {
            try {
                std::unordered_set<std::filesystem::path, file::path_hash> certs = file::list_files(path);
                for (const auto& cert : certs) {
                    for (const auto& extension : ca_cert_extensions) {
                        if (cert.extension() == extension) {
                            return cert.string();
                        }
                    }
                }
            } catch (const std::exception& e) {}
        }
#endif
        return "/etc/ssl/certs/ca-bundle.crt";
    }

    std::string ca_bundle_path = search_ca_cert();

    void set_curl_proxy(CURL* curl, const std::string& proxy) {
        if (!proxy.empty()) {
            std::string proxy_lower = proxy;
            std::transform(proxy_lower.begin(), proxy_lower.end(), proxy_lower.begin(), tolower);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
            int proxy_type = CURLPROXY_HTTP;
            if (boost::starts_with(proxy_lower, "socks")) {
                proxy_type = CURLPROXY_SOCKS5;
                if (proxy_lower.size() >= 6 && proxy_lower[5] == '4') {
                    proxy_type = CURLPROXY_SOCKS4;
                }
            } else if (boost::starts_with(proxy_lower, "https")) {
                proxy_type = CURLPROXY_HTTPS;
            }
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
        }
    }

    void set_curl_ssl_cert([[maybe_unused]] CURL* curl) {
#ifdef __linux__
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle_path.c_str());
#endif
    }

    void set_ca_bundle_path(const std::string& path) {
        ca_bundle_path = path;
    }

    std::string get_ca_bundle_path() {
        return ca_bundle_path;
    }

    void curl_cleanup(CURL* curl, curl_slist* headers) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
} // util