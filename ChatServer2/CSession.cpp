#include "CSession.h"
#include "CServer.h"
#include <iostream>
#include <sstream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicSystem.h"

CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
	_socket(io_context), _server(server), _b_close(false), _b_head_parse(false), _user_uid(0) {
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();	// 给每个session生成一个uuid
	_session_id = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);				// 构造一个MsgNode的智能指针当作节点，用来接收消息中前四个字节（消息头）
}
CSession::~CSession() {
	std::cout << "~CSession destructor" << endl;
}

tcp::socket& CSession::getSocket() {
	return _socket;
}

std::string& CSession::getSessionId() {
	return _session_id;
}

void CSession::setUserId(int uid)
{
	_user_uid = uid;
}

int CSession::getUserId()
{
	return _user_uid;
}

void CSession::start() {
	asyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::send(std::string msg, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::handleWrite, this, std::placeholders::_1, sharedSelf()));
}

void CSession::send(char* msg, short max_length, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();		// 获取此时发送队列的大小
	if (send_que_size > MAX_SENDQUE) {			// 队列已满，无法发送
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));	// 把要发送的数据构造到SendNode类型的节点里，再把该节点推入发送队列
	if (send_que_size > 0) {										// 如果放入该节点之前队列里就有其他节点了，则直接返回，消息的发送交给后面的回调函数处理
		return;
	}
	auto& msgnode = _send_que.front();								// 取出队首元素来发送
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::handleWrite, this, std::placeholders::_1, sharedSelf()));	// 发送完之后调用handleWrite回调函数
}

void CSession::close() {
	_socket.close();
	_b_close = true;
}

std::shared_ptr<CSession>CSession::sharedSelf() {
	return shared_from_this();
}

void CSession::asyncReadBody(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {	// 读取完total_len长度的数据或者出错则调用回调函数
		try {
			if (ec) {
				std::cout << "handle read failed, error is " << ec.what() << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			if (bytes_transfered < total_len) {
				std::cout << "read length not match, read [" << bytes_transfered << "] , total ["
					<< total_len << "]" << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			memcpy(_recv_msg_node->_data, _data, bytes_transfered);			// 将_data中的前bytes_transfered字节数据拷贝到_recv_msg_node的_data中
			_recv_msg_node->_cur_len += bytes_transfered;					// 设置_recv_msg_node中消息体长度
			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';		// 写一个结束标志
			cout << "receive data is " << _recv_msg_node->_data << endl;
			//此处将消息投递到逻辑队列中
			LogicSystem::getInstance()->postMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node));	// 把收到的消息体和当前会话打包到LogicNode类中，添加到LogicSystem的消息队列里等待处理
			//上一个数据包处理完毕，继续监听头部接受事件
			asyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (std::exception& e) {
			std::cout << "Exception code is " << e.what() << endl;
		}
		});
}

void CSession::asyncReadHead(int total_len)
{
	auto self = shared_from_this();		// 把自身作为智能指针共享出去，为了防止回调之前本session就没了的情况？
	asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {	// 读完HEAD_TOTAL_LEN字节后或者有错误则触发回调
		try {
			if (ec) {	// 出现错误
				std::cout << "handle read failed, error is " << ec.what() << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			if (bytes_transfered < HEAD_TOTAL_LEN) {	// 即使应该不可能读到小于HEAD_TOTAL_LEN长度的数据，还是做一下判断
				std::cout << "read length not match, read [" << bytes_transfered << "] , total ["
					<< HEAD_TOTAL_LEN << "]" << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			_recv_head_node->Clear();	// 清空接收消息头的节点，方便将_data中数据拷贝到节点里
			memcpy(_recv_head_node->_data, _data, bytes_transfered);

			//获取头部MSGID数据，先获取id
			short msg_id = 0;
			memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);	// short为2字节，先将_data的前2字节拷贝过来
			//网络字节序转化为本地字节序
			msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);	// 对方传过来的是大端序，转成本地端序
			std::cout << "msg_id is " << msg_id << endl;
			//id非法
			if (msg_id > MAX_LENGTH) {
				std::cout << "invalid msg_id is " << msg_id << endl;
				_server->clearSession(_session_id);
				return;
			}
			// 获取数据包长度信息
			short msg_len = 0;
			memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);		// 从第三个字节开始拷贝两个字节到msg_len
			//网络字节序转化为本地字节序
			msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
			std::cout << "msg_len is " << msg_len << endl;

			//id非法
			if (msg_len > MAX_LENGTH) {
				std::cout << "invalid data length is " << msg_len << endl;
				_server->clearSession(_session_id);
				return;
			}

			_recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);	// 创建接收消息体的节点，消息体长度即为msg_len，消息体id为msg_id
			asyncReadBody(msg_len);										// 消息头接收完了，接着调用asyncReadBody(msg_len)接收消息体
		}
		catch (std::exception& e) {
			std::cout << "Exception code is " << e.what() << endl;
		}
		});
}

void CSession::handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) {
	//增加异常处理
	try {
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			//cout << "send data " << _send_que.front()->_data+HEAD_LENGTH << endl;
			_send_que.pop();							// 把队首元素弹出来，因为队首元素已经发送完了
			if (!_send_que.empty()) {					// 如果队列不为空说明还有要发送的消息，继续处理发送后面的消息
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
					std::bind(&CSession::handleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else {
			std::cout << "handle write failed, error is " << error.what() << endl;
			close();
			_server->clearSession(_session_id);
		}
	}
	catch (std::exception& e) {
		std::cerr << "Exception code : " << e.what() << endl;
	}

}

//读取完整长度
void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	::memset(_data, 0, MAX_LENGTH);			// 先清空一下要读的数组
	asyncReadLen(0, maxLength, handler);	// 从第0个位置开始读，读到第maxLength个字节，读完之后触发回调handler
}

//读取指定字节数
void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len,
	std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	auto self = shared_from_this();
	_socket.async_read_some(boost::asio::buffer(_data + read_len, total_len - read_len),	// 构建buffer，起始地址为_data首地址+已读的长度，要读的长度为总长度-已读的长度，asio读到的数据存到buffer中
		[read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {	// bytesTransfered为这次读到的长度,读完total_len - read_len长度或者出错就触发现在这个回调函数
			if (ec) {
				// 出现错误，调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			if (read_len + bytesTransfered >= total_len) {
				//长度够了就调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			// 没有错误，且长度不足则继续读取
			self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
		});
}

LogicNode::LogicNode(shared_ptr<CSession>  session,
	shared_ptr<RecvNode> recvnode) :_session(session), _recvnode(recvnode) {	// LogicNode构造函数

}
