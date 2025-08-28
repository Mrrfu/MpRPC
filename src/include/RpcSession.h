#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "rpcheader.pb.h" // 假设这是你的protobuf头文件

// 前向声明 RpcProvider
class RpcProviderBoost;

using boost::asio::ip::tcp;

/**
 * @brief RpcSession类
 * 负责处理单个客户端的完整RPC请求。
 * 从socket读取数据 -> 解析协议 -> 派发给RpcProvider -> 将结果写回socket。
 * 使用enable_shared_from_this来管理异步操作中的对象生命周期。
 */
class RpcSession : public std::enable_shared_from_this<RpcSession>
{
public:
    // 构造函数，接收一个已经建立的socket和RpcProvider的指针
    RpcSession(tcp::socket socket, RpcProviderBoost *provider);

    // 开始处理会话，启动第一个异步读操作
    void start();

    // 由RpcProvider的Closure回调此函数，发送RPC响应
    void send_rpc_response(google::protobuf::Message *response);

private:
    // 异步操作链
    void do_read_header_len();                // 1. 异步读取4字节的头部长度
    void do_read_header(uint32_t header_len); // 2. 异步读取RPC头部
    void do_read_args(uint32_t args_len);     // 3. 异步读取RPC参数

private:
    tcp::socket m_socket;
    RpcProviderBoost *m_provider;
    std::vector<char> m_buffer;    // 复用的读缓冲区
    mprpc::RpcHeader m_rpc_header; // 解析后的RPC头部
};