#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include "const.h"
#include "MsgNode.h"
using namespace std;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


class CServer;
class LogicSystem;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession();
	tcp::socket& getSocket();								// ��ȡsession��socket
	std::string& getSessionId();							// ��ȡsession��uuid
	void setUserId(int uid);								// ���û�uid
	int getUserId();
	void start();											// ��ʼ��
	void send(char* msg, short max_length, short msgid);	// send�������������ݲ�����forѭ��һ����������Ϊasio�з������첽�ģ���һ��ѭ���з���ʱֱ�ӷ��أ�
	void send(std::string msg, short msgid);				// ������ȷ����Ϣȫ�����ͳ�ȥ�ˣ���ʱ�ַ��͵ڶ�����Ϣ���ܵ��¶Է��յ���˳�򱻴��ҡ�����Ϣ���н���������
	void close();
	std::shared_ptr<CSession> sharedSelf();
	void asyncReadBody(int length);							// ��ȡ����Ϣͷ������ȡ��Ϣ�壬��ȡ��������ȡ��һ����Ϣͷ
	void asyncReadHead(int total_len);						// ��ȡtotal_len�ֽڵ���Ϣͷ
private:
	void asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler);	// ����maxLength��Żᴥ������Ļص����������߳��ִ���
	void asyncReadLen(std::size_t  read_len, std::size_t total_len,
		std::function<void(const boost::system::error_code&, std::size_t)> handler);
	void handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);		// ��������Ϣ�����ж�����Ϣ�󴥷�����ص�����

	tcp::socket _socket;
	std::string _session_id;
	char _data[MAX_LENGTH];
	CServer* _server;
	bool _b_close;										// ��ʾ�Ự�Ƿ�ر�
	std::queue<shared_ptr<SendNode> > _send_que;
	std::mutex _send_lock;
	std::shared_ptr<RecvNode> _recv_msg_node;			// �����յ�����Ϣ������
	bool _b_head_parse;									// ��ʾ�ǲ��ǽ�������Ϣͷ������ͷ��4���ֽڣ���������id�����ݰ�������Ϣ��
	std::shared_ptr<MsgNode> _recv_head_node;			// �����յ�����Ϣͷ����
	int _user_uid;										// ��ʾ��ǰ�Ự�󶨵��û�uid
};

class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(shared_ptr<CSession>, shared_ptr<RecvNode>);
private:
	shared_ptr<CSession> _session;
	shared_ptr<RecvNode> _recvnode;
};
