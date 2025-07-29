#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>

MprpcConfig MprpcApplication::m_config;
void ShowArgsHelp()
{
    std::cout << "format: command -i <configfile>" << std::endl;
}

MprpcApplication::MprpcApplication()
{
}

MprpcApplication &MprpcApplication::getInstance()
{
    static MprpcApplication app;
    return app;
}

void MprpcApplication::Init(int argc, char **argv)
{
    if (argc < 2)
    {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
    int c = 0;
    std::string config_file;
    while ((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
        {
        case 'i':
            config_file = optarg;
            break;
        case '?':
            ShowArgsHelp();
            exit(EXIT_FAILURE);
        case ':':
            std::cout << "need <config_file>" << std::endl;
            exit(EXIT_FAILURE);
        default:
            break;
        }
    }

    // 加载配置文件  rpcserver_ip  rpcserver_port zookeeper_ip zookeeper_port
    m_config.LoadConfigFile(config_file.c_str());
}

MprpcConfig &MprpcApplication::getConfig()
{
    return m_config;
}