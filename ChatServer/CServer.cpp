#include "CServer.h"
#include "UserMgr.h"
#include "RedisMgr.h"

CServer::CServer(boost::asio::io_context& io_context, short port): _io_context(io_context), _port(port), 
_acceptor(_io_context, tcp::endpoint(tcp::v4(), _port)) // tcp::v4()可理解成本地的ipv4地址,0.0.0.0
{
	std::cout << "CServer starting..., listening on port: " << _port << std::endl;
	startAccept();	// 开始监听连接

}

CServer::~CServer() {
	std::cout << "CServer destructor" << std::endl;
}

void CServer::startAccept() {
	auto& io_context = AsioIOServicePool::getInstance()->getIOcontext();					// 从上下文池中取一个io_context
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(io_context, this);	// 把一个新会话绑到这个io_context上
	// std::bind指定了一个回调函数，当有客户端连接成功时调用该函数
	// 详细解释bind：async_accept接受两个参数，socket和回调函数handler，调用回调函数时往里面传入error_code参数。
	// 但自定义回调函数handleAccept还要this和当前会话的参数，参数不匹配，所以用bind包装一下handleAccept，将bind中的东西视为一个新的可调用对象，
	// 先将this和new_session绑定到handleAccept参数列表中，async_accept触发回调时传入的参数会放入std::placeholders::_1的占位符里，然后触发handleAccept函数
	_acceptor.async_accept(new_session->getSocket(), std::bind(&CServer::handleAccept, this, new_session, std::placeholders::_1));

}

void  CServer::handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error) {
	std::cout << "a new connection" << std::endl;
	if (!error) {	// 说明没有错误
		new_session->start();
		std::lock_guard<std::mutex> guard(_mutex);
		_sessions.insert(std::make_pair(new_session->getSessionId(), new_session));
	}
	else {	// 有错误
		std::cout << "session accept failed, error code: " << error.what() << std::endl;
	}

	startAccept();	// 处理完这个session后继续监听新连接
}

void CServer::clearSession(std::string sessionId) {
	// 在UserMgr中的_uidToSessions容器中也要删除相应的session
	if (_sessions.find(sessionId) != _sessions.end()) {
		UserMgr::getInstance()->removeSession(_sessions[sessionId]->getUserId());
	}
	{
		std::lock_guard<std::mutex> guard(_mutex);
		_sessions.erase(sessionId);
	}
	
}