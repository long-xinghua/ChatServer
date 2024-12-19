#pragma once
#include <boost/asio.hpp>
#include <memory.h>
#include <map>
#include <mutex>
#include "const.h"
#include "CSession.h"												// session用于维护与客户端的连接
#include "AsioIOServicePool.h"
using boost::asio::ip::tcp;

class CServer
{
public:
	CServer(boost::asio::io_context& io_context, short port);
	~CServer();
	void clearSession(std::string);									// 发现session无效时从map中清除掉这个session
private:
	void handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error);					// 处理接收到的连接
	void startAccept();						// 开始接收连接

	boost::asio::io_context& _io_context;	// 这个io_context只用于接收连接，接收到的客户端连接交给AsioIOServicePool中的io_context管理
	short _port;
	tcp::acceptor _acceptor;	// 用接收器来接收连接
	std::map < std::string, std::shared_ptr<CSession>> _sessions;	// 储存连接信息，用shared_ptr，这样在其他地方存储session（如UserMgr里）就能减少开销
	std::mutex _mutex;

};

