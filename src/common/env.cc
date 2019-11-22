#include "common/env.h"

Env* Env::Get() {
    return _GetSharedRef(nullptr).get();
}

std::shared_ptr<Env> Env::_GetSharedRef() {
    return _GetSharedRef(nullptr);
}
Env* Env::Init(const std::unordered_map<std::string, std::string>& envs) {
    return _GetSharedRef(&envs).get();
}

std::vector<const char*> Env::ListEnvs() {
    extern char** environ;
    std::vector<const char*> all_envs;
    const char* cur = *environ;
    int idx = 1;
    for (; cur; idx++) {
        all_envs.push_back(cur);
        cur = *(environ + idx);
    }
    return all_envs;
}

const char* Env::Find(const char* k) {
    std::string key(k);
    if (!kvs.empty() && kvs.find(key) != kvs.end()) {
        return kvs[key].c_str();
    } else {
        return getenv(k);
    }
}

std::shared_ptr<Env> Env::_GetSharedRef(
    const std::unordered_map<std::string, std::string>* envs) {
    std::shared_ptr<Env> inst_ptr(new Env(envs));
    return inst_ptr;
}

int32_t Env::GetIntEnv(const char* key) {
    const char* val = Find(key);
    if (val == nullptr) {
        return 0;
    } else {
        return std::atoi(val);
    }
}

Env::Env(const std::unordered_map<std::string, std::string>* envs) {
    if (envs) {
        kvs = *envs;
    }
}
