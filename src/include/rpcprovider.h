#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <google/protobuf/descriptor.h>
#include <unordered_map>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include "google/protobuf/service.h"
// 框架提供的专门服务发布rpc服务的网络对象类

class RpcProvider
{
public:
    // 框架提供给外部使用的，可以发布rpc方法的函数接口
    void NotifyService(google::protobuf::Service *service);
    // 启动rpc服务节点，开始提供rpc远程过程调用服务
    void Run();

private:
    // 组合EventLoop
    muduo::net::EventLoop m_eventLoop;
    // service服务类型信息
    struct ServiceInfo
    {
        google::protobuf::Service *m_service;                                                    // 保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap; // 保存服务方法
    };
    // 存储注册成功的服务对象和其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // 新的socket连接回调
    void onConnection(const muduo::net::TcpConnectionPtr &);
    void onMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);

    // Closure的回调操作，用于徐磊话rpc的响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);
};