#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <jsoncpp/json/json.h>

void ensure_file_key(Json::Value &root, const std::string &file_path);
void ensure_key(Json::Value &root, const std::string &key);
void add_object(Json::Value &root, const std::vector<std::string> &keys,
                const Json::Value &value);
void add_list(Json::Value &root, const std::vector<std::string> &keys,
              const Json::Value &value);

bool contains_string(const Json::Value &array, const std::string &value);

#endif