#include <iostream>
#include "mprpcapplication.h"
#include "user.pb.h"

/*
 * 整体调用链路（以 Login 为例）：
 * 1. 创建服务代理对象 stub，传入 MprpcChannel 作为通信通道。
 * 2. 设置请求参数，通过 stub.Login 发起远程调用。
 * 3. MprpcChannel::CallMethod 序列化请求并通过网络发送到 RPC 服务器，客户端阻塞等待响应。
 * 4. 服务器端接收请求，反序列化参数，调用本地业务方法，执行后将结果序列化。
 * 5. 服务器将序列化后的结果通过网络返回给客户端。
 * 6. 客户端接收响应，反序列化结果并写入 response 对象，用户获取最终调用结果。
 */
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
    return 0;
}
