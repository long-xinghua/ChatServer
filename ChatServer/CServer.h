#pragma once
#include <boost/asio.hpp>
#include <memory.h>
#include <map>
#include <mutex>
#include "const.h"
#include "CSession.h"												// session����ά����ͻ��˵�����
#include "AsioIOServicePool.h"
using boost::asio::ip::tcp;

class CServer
{
public:
	CServer(boost::asio::io_context& io_context, short port);
	~CServer();
	void clearSession(std::string);									// ����session��Чʱ��map����������session
private:
	void handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error);					// ������յ�������
	void startAccept();						// ��ʼ��������

	boost::asio::io_context& _io_context;	// ���io_contextֻ���ڽ������ӣ����յ��Ŀͻ������ӽ���AsioIOServicePool�е�io_context����
	short _port;
	tcp::acceptor _acceptor;	// �ý���������������
	std::map < std::string, std::shared_ptr<CSession>> _sessions;	// ����������Ϣ����shared_ptr�������������ط��洢session����UserMgr����ܼ��ٿ���
	std::mutex _mutex;

};

