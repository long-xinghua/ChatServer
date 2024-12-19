#pragma once
#include "Singleton.h"
#include <boost/asio.hpp>
#include <vector>

class AsioIOServicePool: public Singleton<AsioIOServicePool>
{
	friend Singleton<AsioIOServicePool>;
public:
	using io_context = boost::asio::io_context;
	using work = boost::asio::io_context::work;
	using WorkPtr = std::unique_ptr<work>;
	~AsioIOServicePool();
	AsioIOServicePool(const AsioIOServicePool&) = delete;				// 禁用拷贝构造
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;	// 禁用赋值运算符
	io_context& getIOcontext();											// 从连接池取出一个上下文
	void stop();
private:
	AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());	// std::thread::hardware_concurrency()会返回当前系统硬件上能支持的最大并发线程数
	std::vector<io_context> _io_contexts;			// 存放上下文
	std::vector<WorkPtr> _works;				// 存放上下文对应的work，防止未监听异步事件时io_context执行run()后直接返回
	std::vector<std::thread> _threads;			// 让每个io_context都跑在一个单独的线程上
	std::size_t _nextIndex;						// 轮询_ioServices，_nextIndex表示下一个要拿出来的io_context在容器中的下标
};

