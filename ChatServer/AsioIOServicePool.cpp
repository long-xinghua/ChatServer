#include "AsioIOServicePool.h"

AsioIOServicePool::AsioIOServicePool(std::size_t size): _io_contexts(size), _works(size), _threads(size), _nextIndex(0){
	for (int i = 0; i < size; i++) {
		_works[i] = std::unique_ptr<work>(new work(_io_contexts[i]));	// ��work�󶨵�ioService��
	}
	for (std::size_t i = 0; i < size; i++) {
		_threads.emplace_back([this,i]() {								// д��_threads.emplace_back(thread([this, i](){...}))������һ��
			_io_contexts[i].run();										// ��ÿ��io_context�����ڵ������߳���
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
		work->get_io_context().stop();	// �ر�work�󶨵�io_context
		work.reset();					// �ͷŵ�ǰ����ָ����е���Դ�����ͷ�work
	}

	for (auto& thread : _threads) {
		thread.join();					// �ȴ������߳���ɣ�����_io_contexts[i].run()����
	}
}
