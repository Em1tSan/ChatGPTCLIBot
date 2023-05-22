//
// Created by v2ray on 2023/4/5.
//

#ifndef GPT3BOT_APIKEY_H
#define GPT3BOT_APIKEY_H

#include "nlohmann/json.hpp"
#include "../util/TermUtils.h"
#include "../network/Request.h"

namespace api {

    enum class APIKeyStatus {
        VALID = 0,
        EMPTY,
        INVALID_PREFIX,
        INVALID_LENGTH,
        QUOTA_EXCEEDED,
        RATE_LIMIT_REACHED,
        INVALID_KEY,
        API_REQUEST_FAILED
    };

    std::string get_key_from_console();
    size_t get_key_count();
    void remove_first_key();
    bool has_key();
    std::string get_key();
    const std::vector<std::string>& get_keys();
    void set_key(const std::string& api_key);
    void set_key(const std::vector<std::string>& api_keys);
    APIKeyStatus check_key(const std::string& api_key);
    bool check_err_obj(const nlohmann::json& json_to_check, APIKeyStatus& status_in, const bool& print_err_msg = true);
} // api

#endif //GPT3BOT_APIKEY_H
