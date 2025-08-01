#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "mprpclogger.h"
/*
 * 整体调用链路（以 Login 为例）：
 * 1. 创建服务代理对象 stub，传入 MprpcChannel 作为通信通道。
 * 2. 设置请求参数，通过 stub.Login 发起远程调用。
 * 3. MprpcChannel::CallMethod 序列化请求并通过网络发送到 RPC 服务器，客户端阻塞等待响应。
 * 4. 服务器端接收请求，反序列化参数，调用本地业务方法，执行后将结果序列化。
 * 5. 服务器将序列化后的结果通过网络返回给客户端。
 * 6. 客户端接收响应，反序列化结果并写入 response 对象，用户获取最终调用结果。
 */

void send_request(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count)
{
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");

    fixbug::LoginResponse response;
    MprpcController controller;

    stub.Login(&controller, &request, &response, nullptr);

    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
        fail_count++;
    }
    else
    {
        if (response.result().errcode() == 0)
        {
            std::cout << "rpc login success: " << response.success() << std::endl;
            success_count++;
        }
        else
        {
            std::cout << "rpc login error: " << response.result().errmsg() << std::endl;
            fail_count++;
        }
    }
}

int main(int argc, char **argv)
{
    // 整个程序启动后，如果想使用mprpc框架来使用rpc服务，需要先调用框架的初始化函数
    MprpcApplication::Init(argc, argv);

    // 演示调用远程发布的rpc方法login
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel()); // 这里实际通过MprpcChannel进行调用
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");
    fixbug::LoginResponse response;

    // 发起rpc方法的调用， 同步的rpc方法的调用，Login是通过MprpcChannel.callMethod调用
    MprpcController controller;
    stub.Login(&controller, &request, &response, nullptr);
    // stub.Login(); // 实际上是通过RpcChannel->RpcChannel::callMethod集中做所有Rpc方法调用的参数序列化和网络发送
    // 一次rpc调用完成，读调用的结果
    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        if (response.result().errcode() == 0)
        {
            std::cout << "rpc login success: " << response.success() << std::endl;
        }
        else
        {
            std::cout << "rpc login error: " << response.result().errmsg() << std::endl;
        }
    }

    // 调用注册方法
    fixbug::RegisterRequest reg_request;
    MprpcController reg_controller;
    reg_request.set_id(1);
    reg_request.set_name("li si");
    reg_request.set_pwd("123456");
    fixbug::RegisterResponse reg_response;
    stub.Register(&reg_controller, &reg_request, &reg_response, nullptr);

    if (reg_controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        if (reg_response.result().errcode() == 0)
        {
            std::cout << "rpc register success: " << reg_response.success() << std::endl;
        }
        else
        {
            std::cout << "rpc register error: " << reg_response.result().errmsg() << std::endl;
        }
    }
    MprpcLogger logger("MyRPC");
    const int thread_count = 1000;     // 并发线程数
    const int request_per_thread = 10; // 每个线程发送的请求数
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back([argc, argv, i, &success_count, &fail_count, request_per_thread]()
                             {
                for(int j=0;j<request_per_thread;++j)
                {
                    send_request(i,success_count,fail_count);
                } });
    }
    for (auto &t : threads)
    {
        t.join();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "total requests: " << thread_count * request_per_thread << std::endl;
    std::cout << "success count: " << success_count << std::endl;
    std::cout << "fail count: " << fail_count << std::endl;
    std::cout << "elapsed time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "QPS: " << (thread_count * request_per_thread) / elapsed.count() << std::endl;

    // 输出统计结果
    LOG(INFO) << "Total requests: " << thread_count * request_per_thread;          // 总请求数
    LOG(INFO) << "Success count: " << success_count;                               // 成功请求数
    LOG(INFO) << "Fail count: " << fail_count;                                     // 失败请求数
    LOG(INFO) << "Elapsed time: " << elapsed.count() << " seconds";                // 测试耗时
    LOG(INFO) << "QPS: " << (thread_count * request_per_thread) / elapsed.count(); // 计算 QPS（每秒请求数）

    return 0;
}
