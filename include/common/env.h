#pragma once
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Env {
public:
    static Env* Get();

    static std::shared_ptr<Env> _GetSharedRef();

    static Env* Init(const std::unordered_map<std::string, std::string>& envs);

    /**
     * @brief: get all envrioment variables related to this process
     *
     * @return all enviroment variables as a vector
     */
    std::vector<const char*> ListEnvs();

    /**
     * @brief: search a environment variable value related to key
     *
     * @param key key of environment variable to search
     *
     * @return founded value
     */
    const char* Find(const char* key);

    static std::shared_ptr<Env> _GetSharedRef(
        const std::unordered_map<std::string, std::string>* envs);

    /**
     * @brief: searcch a int enviroment varialbe with int value related to key
     *
     * @param key key of environment variable to search
     *
     * @return founded value
     */
    int32_t GetIntEnv(const char* key);

    template <typename V>
    V GetEnv(const char* key, V default_val) {
        const char* val = Find(key);
        if (val == nullptr) {
            return default_val;
        } else {
            return std::atoi(val);
        }
    }

private:
    explicit Env(const std::unordered_map<std::string, std::string>* envs);

    std::unordered_map<std::string, std::string> kvs;
};
