#include "CServer.h"
#include "UserMgr.h"
#include "RedisMgr.h"

CServer::CServer(boost::asio::io_context& io_context, short port): _io_context(io_context), _port(port), 
_acceptor(_io_context, tcp::endpoint(tcp::v4(), _port)) // tcp::v4()�����ɱ��ص�ipv4��ַ,0.0.0.0
{
	std::cout << "CServer starting..., listening on port: " << _port << std::endl;
	startAccept();	// ��ʼ��������

}

CServer::~CServer() {
	std::cout << "CServer destructor" << std::endl;
}

void CServer::startAccept() {
	auto& io_context = AsioIOServicePool::getInstance()->getIOcontext();					// �������ĳ���ȡһ��io_context
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(io_context, this);	// ��һ���»Ự�����io_context��
	// std::bindָ����һ���ص����������пͻ������ӳɹ�ʱ���øú���
	// ��ϸ����bind��async_accept��������������socket�ͻص�����handler�����ûص�����ʱ�����洫��error_code������
	// ���Զ���ص�����handleAccept��Ҫthis�͵�ǰ�Ự�Ĳ�����������ƥ�䣬������bind��װһ��handleAccept����bind�еĶ�����Ϊһ���µĿɵ��ö���
	// �Ƚ�this��new_session�󶨵�handleAccept�����б��У�async_accept�����ص�ʱ����Ĳ��������std::placeholders::_1��ռλ���Ȼ�󴥷�handleAccept����
	_acceptor.async_accept(new_session->getSocket(), std::bind(&CServer::handleAccept, this, new_session, std::placeholders::_1));

}

void  CServer::handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error) {
	std::cout << "a new connection" << std::endl;
	if (!error) {	// ˵��û�д���
		new_session->start();
		std::lock_guard<std::mutex> guard(_mutex);
		_sessions.insert(std::make_pair(new_session->getSessionId(), new_session));
	}
	else {	// �д���
		std::cout << "session accept failed, error code: " << error.what() << std::endl;
	}

	startAccept();	// ���������session���������������
}

void CServer::clearSession(std::string sessionId) {
	// ��UserMgr�е�_uidToSessions������ҲҪɾ����Ӧ��session
	if (_sessions.find(sessionId) != _sessions.end()) {
		UserMgr::getInstance()->removeSession(_sessions[sessionId]->getUserId());
	}
	{
		std::lock_guard<std::mutex> guard(_mutex);
		_sessions.erase(sessionId);
	}
	
}