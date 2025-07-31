#pragma once
#include <unordered_map>
#include <string>
// 框架读取配置配置文件类
// rpcserverip rpcserverport zookeeperip zookpeerport
class MprpcConfig
{
public:
    void LoadConfigFile(const char *config_file);
    std::string Load(std::string key);

private:
    std::unordered_map<std::string, std::string> m_configMap;
    void Trim(std::string &src_buf);
};