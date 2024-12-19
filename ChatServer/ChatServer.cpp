// ChatServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "ChatGrpcServiceImpl.h"

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
	// 启动Server时将登录数量设置为0
	auto serverName = ConfigMgr::getInst()["SelfServer"]["Name"];
	RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, "0");


	try {
		auto& cfg = ConfigMgr::getInst();								// 读取配置
		auto pool = AsioIOServicePool::getInstance();					// 获取连接池

		// 建立一个GrpcServer，用于和其他服务器的通信
		std::string serverAddress(cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		ChatGrpcServiceImpl service;
		grpc::ServerBuilder builder;
		// 绑定ip和端口，添加服务
		builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);
		// 构建并启动grpc服务器
		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		std::cout << "RPC Server listening on:" << serverAddress << std::endl;
		// 用一个单独的线程处理grpc服务
		std::thread grpc_server_thread([&server]() {
			server->Wait();
			});

		boost::asio::io_context  io_context;							// 独立的io_context，跟GateServer里一样
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);	// 关注SIGINT和SIGTERM信号，接收到信号时触发下面的回调函数
		signals.async_wait([&io_context, pool, &server](auto, auto) {
			io_context.stop();											// 停止接收新连接
			pool->stop();												// 关闭连接池，断开所有连接
			server->Shutdown();											// 关闭Grpc服务器
			});
		auto port_str = cfg["SelfServer"]["Port"];						// 取出本服务器端口号
		CServer s(io_context, atoi(port_str.c_str()));					// 将服务器绑定到该端口
		io_context.run();

		// 服务器关闭后要将Redis中客户端连接数量的键值对删除
		RedisMgr::getInstance()->hDel(LOGIN_COUNT, serverName);
		//RedisMgr::getInstance()->close();
		grpc_server_thread.join();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
}


