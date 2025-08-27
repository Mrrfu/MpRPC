#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "zookeeperuitl.h"

class MprpcChannel : public google::protobuf::RpcChannel
{
public:
    // 所有通过stu代理对象调用的rpc方法，都使用此方法统一做rpc方法调用的数据序列化和网络发送
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done);
    MprpcChannel();
    ~MprpcChannel();

private:
    // ZkClientPool g_zkpool;
    int m_clientFd; // 存放客户端套接字
    // 保存的ip和端口，用于重连
    std::string service_name;
    std::string m_ip;
    uint16_t m_port;
    std::string method_name;
    bool newConnect(const char *ip, uint16_t port, std::string *errMsg);
    std::string queryServiceHost(std::shared_ptr<ZkClient> &zkclient, const std::string &service_name, const std::string &methdo_name, int &idx);
};
