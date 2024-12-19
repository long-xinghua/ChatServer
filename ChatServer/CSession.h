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
	tcp::socket& getSocket();								// 获取session的socket
	std::string& getSessionId();							// 获取session的uuid
	void setUserId(int uid);								// 绑定用户uid
	int getUserId();
	void start();											// 开始读
	void send(char* msg, short max_length, short msgid);	// send函数发多条数据不能用for循环一个个发，因为asio中发送是异步的，第一个循环中发送时直接返回，
	void send(std::string msg, short msgid);				// 并不能确保消息全部发送出去了，此时又发送第二个消息可能导致对方收到的顺序被打乱。用消息队列解决这个问题
	void close();
	std::shared_ptr<CSession> sharedSelf();
	void asyncReadBody(int length);							// 读取完消息头后来读取消息体，读取完后继续读取下一个消息头
	void asyncReadHead(int total_len);						// 读取total_len字节的消息头
private:
	void asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler);	// 读完maxLength后才会触发后面的回调函数（或者出现错误）
	void asyncReadLen(std::size_t  read_len, std::size_t total_len,
		std::function<void(const boost::system::error_code&, std::size_t)> handler);
	void handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);		// 发送完消息队列中队首消息后触发这个回调函数

	tcp::socket _socket;
	std::string _session_id;
	char _data[MAX_LENGTH];
	CServer* _server;
	bool _b_close;										// 表示会话是否关闭
	std::queue<shared_ptr<SendNode> > _send_que;
	std::mutex _send_lock;
	std::shared_ptr<RecvNode> _recv_msg_node;			// 储存收到的消息体数据
	bool _b_head_parse;									// 表示是不是解析了消息头部（即头部4个字节，包含请求id和数据包长度信息）
	std::shared_ptr<MsgNode> _recv_head_node;			// 储存收到的消息头数据
	int _user_uid;										// 表示当前会话绑定的用户uid
};

class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(shared_ptr<CSession>, shared_ptr<RecvNode>);
private:
	shared_ptr<CSession> _session;
	shared_ptr<RecvNode> _recvnode;
};
