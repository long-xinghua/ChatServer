#include "AsioIOServicePool.h"

AsioIOServicePool::AsioIOServicePool(std::size_t size): _io_contexts(size), _works(size), _threads(size), _nextIndex(0){
	for (int i = 0; i < size; i++) {
		_works[i] = std::unique_ptr<work>(new work(_io_contexts[i]));	// 把work绑定到ioService上
	}
	for (std::size_t i = 0; i < size; i++) {
		_threads.emplace_back([this,i]() {								// 写成_threads.emplace_back(thread([this, i](){...}))会清晰一点
			_io_contexts[i].run();										// 让每个io_context都跑在单独的线程上
			});
	}
}

AsioIOServicePool::~AsioIOServicePool() {
	std::cout << "AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::getIOcontext() {
	io_context& context = _io_contexts[_nextIndex++];
	if (_nextIndex >= _io_contexts.size()) {
		_nextIndex = 0;
	}
	return context;
}

void AsioIOServicePool::stop() {
	for (auto& work : _works) {
		work->get_io_context().stop();	// 关闭work绑定的io_context
		work.reset();					// 释放当前智能指针持有的资源，即释放work
	}

	for (auto& thread : _threads) {
		thread.join();					// 等待所有线程完成，即等_io_contexts[i].run()返回
	}
}
