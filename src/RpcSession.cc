#include "RpcSession.h"
#include "RpcProviderBoost.h" // 需要包含RpcProvider的完整定义
#include "logger.h"           // 你的日志库
#include <google/protobuf/message.h>

RpcSession::RpcSession(tcp::socket socket, RpcProviderBoost *provider)
    : m_socket(std::move(socket)), m_provider(provider)
{
}

void RpcSession::start()
{
    // 启动异步操作链的第一步：读取头部长度
    do_read_header_len();
}

void RpcSession::do_read_header_len()
{
    // 获取shared_ptr以保证在异步回调中当前对象仍然存活
    auto self = shared_from_this();
    m_buffer.assign(4, 0); // 分配4字节空间

    boost::asio::async_read(m_socket, boost::asio::buffer(m_buffer),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec && length == 4)
                                {
                                    uint32_t header_len = 0;
                                    memcpy(&header_len, m_buffer.data(), 4); // 读取前四字节
                                    do_read_header(header_len);
                                }
                                else
                                {
                                    LOG_ERR("RpcSession: read header_len error or client closed. %s", ec.message().c_str());
                                    // ec不为0表示出错（如连接断开），shared_ptr self将在此处析构，自动关闭socket
                                }
                            });
}

void RpcSession::do_read_header(uint32_t header_len)
{
    auto self = shared_from_this();
    m_buffer.resize(header_len); // 调整缓冲区大小以读取RPC头部

    boost::asio::async_read(m_socket, boost::asio::buffer(m_buffer),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    std::string rpc_header_str(m_buffer.begin(), m_buffer.end());
                                    if (m_rpc_header.ParseFromString(rpc_header_str))
                                    {
                                        // 头部反序列化成功
                                        uint32_t args_len = m_rpc_header.args_size();
                                        do_read_args(args_len);
                                    }
                                    else
                                    {
                                        LOG_ERR("RpcSession: rpc_header parse error!");
                                    }
                                }
                                else
                                {
                                    LOG_ERR("RpcSession: read header error. %s", ec.message().c_str());
                                }
                            });
}

void RpcSession::do_read_args(uint32_t args_len)
{
    auto self = shared_from_this();
    m_buffer.resize(args_len); // 调整缓冲区大小以读取参数

    boost::asio::async_read(m_socket, boost::asio::buffer(m_buffer),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    std::string args_str(m_buffer.begin(), m_buffer.end());

                                    // 请求已完整接收，现在派发给RpcProvider处理
                                    m_provider->dispatch_request(m_rpc_header, args_str, self);

                                    do_read_header_len(); // 继续读缓冲区（因为是长连接，可能发送多个消息）
                                }
                                else
                                {
                                    LOG_ERR("RpcSession: read args error. %s", ec.message().c_str());
                                }
                            });
}

// 完成调用后向调用方发送数据的回调函数
void RpcSession::send_rpc_response(google::protobuf::Message *response)
{
    auto self = shared_from_this();
    std::string response_str;

    if (response->SerializeToString(&response_str))
    {
        LOG_INFO("Send response to client success. size=%zu", response_str.size());
        boost::asio::async_write(m_socket, boost::asio::buffer(response_str),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         // 发送成功，模拟短连接，主动断开
                                         //  boost::system::error_code shutdown_ec;
                                         //  m_socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
                                         //  std::cout << "服务端关闭连接！" << std::endl;
                                     }
                                     else
                                     {
                                         LOG_ERR("RpcSession: write response error. %s", ec.message().c_str());
                                     }
                                 });
    }
    else
    {
        LOG_ERR("Serialize response_str error!");
    }

    // NewCallback创建的Message需要手动删除
    delete response;
}