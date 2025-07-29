#pragma once
#include "mprpcconfig.h"
/*
 * mprpc框架的基础类
 * 负责rpc框架的初始化
 * 单例模式
 */

class MprpcApplication
{
public:
    static void Init(int argc, char **argv);
    static MprpcApplication &getInstance();
    static MprpcConfig & getConfig();

private:
    static MprpcConfig m_config;
    MprpcApplication();
    MprpcApplication(const MprpcApplication &) = delete;
    // MprpcApplication(const MprpcApplication &&) = delete;
    MprpcApplication &operator=(const MprpcApplication &) = delete;
};