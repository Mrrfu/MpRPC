#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <boost/asio.hpp>
#include "rpcheader.pb.h"

// 前向声明会话类
class RpcSession;

// 服务信息结构体
struct ServiceInfo
{
    google::protobuf::Service *m_service;
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap;
};

/**
 * @brief RpcProvider类
 * 框架的RPC提供者，负责发布服务和处理RPC调用。
 * 网络部分由Boost.Asio处理。
 */
class RpcProviderBoost
{
public:
    // 构造函数，初始化Asio acceptor
    RpcProviderBoost();

    // 注册服务，此部分不变
    void NotifyService(google::protobuf::Service *service);

    // 启动RPC服务节点
    void Run();

    // 由RpcSession调用，用于派发RPC请求
    void dispatch_request(const mprpc::RpcHeader &header,
                          const std::string &args_str,
                          std::shared_ptr<RpcSession> session);

private:
    // 开始异步接受新连接
    void do_accept();

private:
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // Asio核心组件
    boost::asio::io_context m_io_context;
    boost::asio::ip::tcp::acceptor m_acceptor;
};