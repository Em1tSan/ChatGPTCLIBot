//
// Created by v2ray on 2023/3/13.
//

#include "Network.h"

namespace api {

    //class TimeoutCountDown start:
    TimeoutChecker::TimeoutChecker(const unsigned int& timeout_ms) : creation_time(util::currentTimeMillis()), timeout_ms_(timeout_ms) {}

    TimeoutChecker::~TimeoutChecker() = default;

    long long TimeoutChecker::calc_next() const {
        return util::currentTimeMillis() - creation_time + timeout_ms_;
    }

    void TimeoutChecker::reset_creation_time() {
        creation_time = util::currentTimeMillis();
    }
    //class TimeoutCountDown end.

    size_t write_callback(char* char_ptr, size_t size, size_t mem, std::function<size_t(char*, size_t, size_t)>* callback_function) {
        return (*callback_function)(char_ptr, size, mem);
    }

    /**
     * Call the OpenAI API with a custom lambda function as callback.
     * @return True if the call was successful, false otherwise.
     */
    bool call_api(const std::string& initial_prompt, const std::vector<std::shared_ptr<chat::Exchange>>& chat_exchanges,
                  const std::string& api_key, const std::string& model, const float& temperature, const int& max_tokens,
                  const float& top_p, const float& frequency_penalty, const float& presence_penalty,
                  const std::unordered_map<std::string, float>& logit_bias,
                  const unsigned int& max_short_memory_length, const unsigned int& max_reference_length,
                  const std::string& me_id, const std::string& bot_id, std::function<void(const std::string& streamed_response)> callback,
                  const bool& debug_reference, const bool& pause_when_showing_reference) {
        CURL* curl;
        CURLcode res;
        curl = curl_easy_init();
        bool error = false;
        if (curl) {
            bool is_new_api_ = is_new_api(model);
            std::string url = is_new_api_ ? "https://api.openai.com/v1/chat/completions" : "https://api.openai.com/v1/completions";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            std::string constructed_initial = prompt::construct_reference(
                    initial_prompt, chat_exchanges.back()->getInputEmbeddings(),
                    chat_exchanges, max_reference_length, max_short_memory_length, me_id, bot_id);
            std::string suffix = ": ";
            if (debug_reference) {
                std::string dr_prefix = Term::color_fg(255, 200, 0) + "<Debug Reference> " + Term::color_fg(Term::Color::Name::Default);
                util::print_cs("\n" + dr_prefix + Term::color_fg(255, 225, 0)
                + "Constructed initial prompt:\n----------\n" + constructed_initial + "\n----------", true);
                if (pause_when_showing_reference) {
                    util::print_cs(dr_prefix + "Press " + Term::color_fg(70, 200, 255) + "Enter"
                    + Term::color_fg(Term::Color::Name::Default) + " to continue: ");
                    util::ignore_line();
                }
                util::print_cs(Term::color_fg(175, 255, 225) + bot_id + (is_new_api_ ? suffix : ":"), false, false);
            }
            TimeoutChecker timeout_checker(10000);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            std::function<size_t(char*, size_t, size_t)> callback_lambda = [&](char* char_ptr, size_t size, size_t mem){
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_checker.calc_next());
                size_t length = size * mem;
                std::string s(char_ptr, length);
                std::vector<std::string> split_str;
                split_regex(split_str, s, boost::regex("[\n][\n][d][a][t][a][:][ ]"));
                for (auto& str: split_str) {
                    if (boost::starts_with(str, "data: ")) {
                        str.erase(0, 6);
                    }
                    if (boost::ends_with(str, "\n\n")) {
                        str.erase(str.size() - 2);
                    }
                    if (str != "[DONE]") {
                        try {
                            nlohmann::json j = nlohmann::json::parse(str);
                            std::string response;
                            if (j.count("choices") > 0 && j["choices"].is_array()) {
                                auto choices = j["choices"];
                                if (!is_new_api_) {
                                    for (const auto& choice: choices) {
                                        if (choice.count("text") > 0 && choice["text"].is_string()) {
                                            response = choice["text"].get<std::string>();
                                        }
                                    }
                                } else {
                                    for (const auto& choice: choices) {
                                        if (choice.count("delta") > 0 && choice["delta"].is_object()) {
                                            auto delta = choice["delta"];
                                            if (delta.count("content") > 0 && delta["content"].is_string()) {
                                                response = delta["content"].get<std::string>();
                                            }
                                        }
                                    }
                                }
                            } else {
                                response = j.dump();
                            }
                            callback(response);
                        } catch (const nlohmann::json::parse_error& e) {
                            util::println_err("Error parsing JSON: " + std::string(e.what()));
                        }
                    }
                }
                return length;
            };
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callback_lambda);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 20000);
            util::set_curl_proxy(curl, util::system_proxy());
            util::set_curl_ssl_cert(curl);
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            std::string auth = "Authorization: Bearer ";
            headers = curl_slist_append(headers, auth.append(api_key).c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            nlohmann::json payload = {{"model", model},
                                      {"temperature", temperature},
                                      {"top_p", top_p},
                                      {"frequency_penalty", frequency_penalty},
                                      {"presence_penalty", presence_penalty},
                                      {"stream", true}};
            unsigned int model_max_tokens = util::get_max_tokens(model);
            unsigned int token_count;
            if (!is_new_api_) {
                std::string prompt = GPT::to_payload(constructed_initial, chat_exchanges, me_id, bot_id, max_short_memory_length);
                if ((token_count = util::get_token_count(prompt, model)) >= model_max_tokens) {
                    util::curl_cleanup(curl, headers);
                    throw util::max_tokens_exceeded(
                            "Max tokens exceeded in prompt: " + std::to_string(token_count) + " >= " + std::to_string(model_max_tokens));
                }
                payload["prompt"] = prompt;
                payload["stop"] = {me_id + suffix, bot_id + suffix};
            } else {
                nlohmann::json messages = ChatGPT::to_payload(constructed_initial, chat_exchanges, me_id, bot_id, max_short_memory_length);
                if ((token_count = util::get_token_count(messages, model)) >= model_max_tokens) {
                    util::curl_cleanup(curl, headers);
                    throw util::max_tokens_exceeded(
                            "Max tokens exceeded in messages: " + std::to_string(token_count) + " >= " + std::to_string(model_max_tokens));
                }
                payload["messages"] = messages;
            }
            unsigned int max_tokens_p = model_max_tokens - token_count;
            payload["max_tokens"] = max_tokens_p < max_tokens ? max_tokens_p : max_tokens;
            if (!logit_bias.empty()) {
                nlohmann::json logit_bias_json = nlohmann::json::object();
                std::shared_ptr<GptEncoding> tokenizer = util::get_enc_cache(util::get_tokenizer(model));
                std::unordered_map<uint32_t, float> converted_logit_bias;
                for (const auto& [key, value] : logit_bias) {
                    if (key.empty()) {
                        continue;
                    }
                    std::vector<int> token_ids = tokenizer->encode(key);
                    for (const auto& token_id : token_ids) {
                        converted_logit_bias[token_id] = value;
                    }
                }
                for (const auto& [key, value] : converted_logit_bias) {
                    logit_bias_json[std::to_string(key)] = value;
                }
                payload["logit_bias"] = logit_bias_json;
            }
            std::string payload_str = payload.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
            timeout_checker.reset_creation_time();
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                error = true;
                util::println_err("\nAPI request failed: " + std::string(curl_easy_strerror(res)));
            }
            util::curl_cleanup(curl, headers);
        }
        return !error;
    }

    bool is_new_api(const std::string& model_name) {
        return boost::starts_with(model_name, "gpt-3.5") || boost::starts_with(model_name, "gpt-4");
    }
} // api