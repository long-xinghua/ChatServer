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
	AsioIOServicePool(const AsioIOServicePool&) = delete;				// ���ÿ�������
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;	// ���ø�ֵ�����
	io_context& getIOcontext();											// �����ӳ�ȡ��һ��������
	void stop();
private:
	AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());	// std::thread::hardware_concurrency()�᷵�ص�ǰϵͳӲ������֧�ֵ���󲢷��߳���
	std::vector<io_context> _io_contexts;			// ���������
	std::vector<WorkPtr> _works;				// ��������Ķ�Ӧ��work����ֹδ�����첽�¼�ʱio_contextִ��run()��ֱ�ӷ���
	std::vector<std::thread> _threads;			// ��ÿ��io_context������һ���������߳���
	std::size_t _nextIndex;						// ��ѯ_ioServices��_nextIndex��ʾ��һ��Ҫ�ó�����io_context�������е��±�
};

