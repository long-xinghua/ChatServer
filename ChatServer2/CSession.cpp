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
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();	// ��ÿ��session����һ��uuid
	_session_id = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);				// ����һ��MsgNode������ָ�뵱���ڵ㣬����������Ϣ��ǰ�ĸ��ֽڣ���Ϣͷ��
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
	int send_que_size = _send_que.size();		// ��ȡ��ʱ���Ͷ��еĴ�С
	if (send_que_size > MAX_SENDQUE) {			// �����������޷�����
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));	// ��Ҫ���͵����ݹ��쵽SendNode���͵Ľڵ���ٰѸýڵ����뷢�Ͷ���
	if (send_que_size > 0) {										// �������ýڵ�֮ǰ��������������ڵ��ˣ���ֱ�ӷ��أ���Ϣ�ķ��ͽ�������Ļص���������
		return;
	}
	auto& msgnode = _send_que.front();								// ȡ������Ԫ��������
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::handleWrite, this, std::placeholders::_1, sharedSelf()));	// ������֮�����handleWrite�ص�����
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
	asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {	// ��ȡ��total_len���ȵ����ݻ��߳�������ûص�����
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

			memcpy(_recv_msg_node->_data, _data, bytes_transfered);			// ��_data�е�ǰbytes_transfered�ֽ����ݿ�����_recv_msg_node��_data��
			_recv_msg_node->_cur_len += bytes_transfered;					// ����_recv_msg_node����Ϣ�峤��
			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';		// дһ��������־
			cout << "receive data is " << _recv_msg_node->_data << endl;
			//�˴�����ϢͶ�ݵ��߼�������
			LogicSystem::getInstance()->postMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node));	// ���յ�����Ϣ��͵�ǰ�Ự�����LogicNode���У���ӵ�LogicSystem����Ϣ������ȴ�����
			//��һ�����ݰ�������ϣ���������ͷ�������¼�
			asyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (std::exception& e) {
			std::cout << "Exception code is " << e.what() << endl;
		}
		});
}

void CSession::asyncReadHead(int total_len)
{
	auto self = shared_from_this();		// ��������Ϊ����ָ�빲���ȥ��Ϊ�˷�ֹ�ص�֮ǰ��session��û�˵������
	asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {	// ����HEAD_TOTAL_LEN�ֽں�����д����򴥷��ص�
		try {
			if (ec) {	// ���ִ���
				std::cout << "handle read failed, error is " << ec.what() << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			if (bytes_transfered < HEAD_TOTAL_LEN) {	// ��ʹӦ�ò����ܶ���С��HEAD_TOTAL_LEN���ȵ����ݣ�������һ���ж�
				std::cout << "read length not match, read [" << bytes_transfered << "] , total ["
					<< HEAD_TOTAL_LEN << "]" << endl;
				close();
				_server->clearSession(_session_id);
				return;
			}

			_recv_head_node->Clear();	// ��ս�����Ϣͷ�Ľڵ㣬���㽫_data�����ݿ������ڵ���
			memcpy(_recv_head_node->_data, _data, bytes_transfered);

			//��ȡͷ��MSGID���ݣ��Ȼ�ȡid
			short msg_id = 0;
			memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);	// shortΪ2�ֽڣ��Ƚ�_data��ǰ2�ֽڿ�������
			//�����ֽ���ת��Ϊ�����ֽ���
			msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);	// �Է����������Ǵ����ת�ɱ��ض���
			std::cout << "msg_id is " << msg_id << endl;
			//id�Ƿ�
			if (msg_id > MAX_LENGTH) {
				std::cout << "invalid msg_id is " << msg_id << endl;
				_server->clearSession(_session_id);
				return;
			}
			// ��ȡ���ݰ�������Ϣ
			short msg_len = 0;
			memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);		// �ӵ������ֽڿ�ʼ���������ֽڵ�msg_len
			//�����ֽ���ת��Ϊ�����ֽ���
			msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
			std::cout << "msg_len is " << msg_len << endl;

			//id�Ƿ�
			if (msg_len > MAX_LENGTH) {
				std::cout << "invalid data length is " << msg_len << endl;
				_server->clearSession(_session_id);
				return;
			}

			_recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);	// ����������Ϣ��Ľڵ㣬��Ϣ�峤�ȼ�Ϊmsg_len����Ϣ��idΪmsg_id
			asyncReadBody(msg_len);										// ��Ϣͷ�������ˣ����ŵ���asyncReadBody(msg_len)������Ϣ��
		}
		catch (std::exception& e) {
			std::cout << "Exception code is " << e.what() << endl;
		}
		});
}

void CSession::handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) {
	//�����쳣����
	try {
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			//cout << "send data " << _send_que.front()->_data+HEAD_LENGTH << endl;
			_send_que.pop();							// �Ѷ���Ԫ�ص���������Ϊ����Ԫ���Ѿ���������
			if (!_send_que.empty()) {					// ������в�Ϊ��˵������Ҫ���͵���Ϣ�����������ͺ������Ϣ
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

//��ȡ��������
void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	::memset(_data, 0, MAX_LENGTH);			// �����һ��Ҫ��������
	asyncReadLen(0, maxLength, handler);	// �ӵ�0��λ�ÿ�ʼ����������maxLength���ֽڣ�����֮�󴥷��ص�handler
}

//��ȡָ���ֽ���
void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len,
	std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	auto self = shared_from_this();
	_socket.async_read_some(boost::asio::buffer(_data + read_len, total_len - read_len),	// ����buffer����ʼ��ַΪ_data�׵�ַ+�Ѷ��ĳ��ȣ�Ҫ���ĳ���Ϊ�ܳ���-�Ѷ��ĳ��ȣ�asio���������ݴ浽buffer��
		[read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {	// bytesTransferedΪ��ζ����ĳ���,����total_len - read_len���Ȼ��߳���ʹ�����������ص�����
			if (ec) {
				// ���ִ��󣬵��ûص�����
				handler(ec, read_len + bytesTransfered);
				return;
			}

			if (read_len + bytesTransfered >= total_len) {
				//���ȹ��˾͵��ûص�����
				handler(ec, read_len + bytesTransfered);
				return;
			}

			// û�д����ҳ��Ȳ����������ȡ
			self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
		});
}

LogicNode::LogicNode(shared_ptr<CSession>  session,
	shared_ptr<RecvNode> recvnode) :_session(session), _recvnode(recvnode) {	// LogicNode���캯��

}
