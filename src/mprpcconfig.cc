#include "mprpcconfig.h"
#include <iostream>

// 解析配置文件
void MprpcConfig::LoadConfigFile(const char *config_file)
{
    FILE *pf = fopen(config_file, "r");
    if (pf == nullptr)
    {
        std::cout << config_file << " is not exists!" << std::endl;
        exit(EXIT_FAILURE);
    }
    // 1.注释 2.正确的配置项 3.去掉开头的多余的空格
    while (!feof(pf))
    {
        char buf[512] = {0};
        fgets(buf, 512, pf);
        std::string read_buf(buf);
        Trim(read_buf);
        // 判断注释或空行
        if (read_buf[0] == '#' || read_buf.empty())
        {
            continue;
        }
        // 解析配置项
        int idx = read_buf.find('=');
        if (idx == -1)
        {
            // 配置项不合法
            continue;
        }
        std::string key;
        std::string value;

        key = read_buf.substr(0, idx);
        // 去掉前后的空格
        Trim(key);

        // 查找回车
        int endIdx = read_buf.find('\n', idx);
        value = read_buf.substr(idx + 1, endIdx - idx - 1);

        // 去掉前后空格
        Trim(value);
        m_configMap.insert({key, value});
    }
    fclose(pf);
}
// 查询配置项信息
std::string MprpcConfig::Load(std::string key)
{
    if (m_configMap.count(key) == 0)
    {
        return "";
    }
    return m_configMap[key];
}

void MprpcConfig::Trim(std::string &src_buf)
{
    // 去掉字符串前面多余的空格
    int idx = src_buf.find_first_not_of(' ');
    if (idx != -1)
    {
        src_buf = src_buf.substr(idx, src_buf.size() - idx);
    }
    // 去掉字符串后面多余的空格
    idx = src_buf.find_last_not_of(' ');
    if (idx != -1)
    {
        src_buf = src_buf.substr(0, idx + 1);
    }
}