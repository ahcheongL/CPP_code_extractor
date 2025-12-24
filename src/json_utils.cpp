#include "json_utils.hpp"

void ensure_file_key(Json::Value &root, const std::string &file_path) {
  if (root.isMember(file_path)) { return; }

  root[file_path] = Json::Value(Json::objectValue);
  Json::Value &file_entry = root[file_path];

  file_entry["functions"] = Json::Value(Json::objectValue);
  file_entry["macros"] = Json::Value(Json::objectValue);
  file_entry["enums"] = Json::Value(Json::objectValue);
  file_entry["types"] = Json::Value(Json::objectValue);
  file_entry["global_variables"] = Json::Value(Json::objectValue);
  file_entry["disabled_macros"] = Json::Value(Json::objectValue);
}

void ensure_key(Json::Value &root, const std::string &key) {
  if (!root.isMember(key)) { root[key] = Json::Value(Json::objectValue); }
}

void add_object(Json::Value &root, const std::vector<std::string> &keys,
                const Json::Value &value) {
  Json::Value *current = &root;

  for (const std::string &key : keys) {
    if (!current->isMember(key)) {
      (*current)[key] = Json::Value(Json::objectValue);
    }
    current = &((*current)[key]);
  }

  const std::string &last_key = keys.back();
  (*current)[last_key] = Json::Value(value);
}

void add_list(Json::Value &root, const std::vector<std::string> &keys,
              const Json::Value &value) {
  Json::Value *current = &root;

  for (const std::string &key : keys) {
    if (!current->isMember(key)) {
      (*current)[key] = Json::Value(Json::objectValue);
    }
    current = &((*current)[key]);
  }

  const std::string &last_key = keys.back();
  if (!current->isMember(last_key)) {
    (*current)[last_key] = Json::Value(Json::arrayValue);
  }
  (*current)[last_key].append(Json::Value(value));
}

bool contains_string(const Json::Value &array, const std::string &value) {
  for (const Json::Value &item : array) {
    if (item.asString() == value) { return true; }
  }
  return false;
}