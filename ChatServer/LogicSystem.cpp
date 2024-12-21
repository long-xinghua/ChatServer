#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"

using namespace std;

LogicSystem::LogicSystem() :_b_stop(false) {
	registerCallBacks();
	_worker_thread = std::thread(&LogicSystem::dealMsg, this);		// ���������̣߳�ר�Ŵ���Ϣ������ȡ�����ݽ��д������ݻỰ������ָ�����Ϣ���ݾ��ܽ�����Ӧ����
}																	// ����ֻ������һ�������̣߳���������ٶȽ������Զഴ������

LogicSystem::~LogicSystem() {
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::postMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);		// ����Ҫ����_msg_que���ȼ���
	// ����ŵĺܿ촦����ֺ�����_msg_que�Ĵ�С�᳤�úܿ죬Ӧ���жϵ�_msg_que���˵�ʱ��Ͳ�����������ݽڵ���
	if (_msg_que.size() >= 100) {
		std::cout << " _msg_que is full, can not push more msgs" << std::endl;
		return;
	}
	_msg_que.push(msg);					// ��msg�ӵ�_msg_que��Ϣ����
	// ������_msg_que.size() == 1����_msg_que.size() >= 1������
	if (_msg_que.size() >= 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}

void LogicSystem::dealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//�ж϶���Ϊ�������������������ȴ������ͷ���
		while (_msg_que.empty() && !_b_stop) {		// Ӧ�ÿ���_consume.wait(unique_lk, [this](){return !_msg_que.empty() || _b_stop});
			_consume.wait(unique_lk);
		}

		//�ж��Ƿ�Ϊ�ر�״̬���������߼�ִ��������˳�ѭ��
		if (_b_stop) {
			while (!_msg_que.empty()) {		// ���ε���������������Ϣ�������������˳�
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);	// �ҵ�msg_id��Ӧ�Ļص�����
				if (call_back_iter == _fun_callbacks.end()) {	// �Ҳ��������˲�������
					_msg_que.pop();
					std::cout << "can not find corresponding handler" << std::endl;
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));	// ִ�лص�����FunCallBack
				_msg_que.pop();
			}
			break;
		}

		//���û��ͣ������˵�������������ݣ���������е�һ����Ϣ�ڵ�
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::registerCallBacks() {		
	// �����Ϣid����Ϊ��¼�������loginHandler
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::loginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	// ��Ϣ����Ϊ�����û�
	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::searchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::addFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	/*_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);*/

}

void LogicSystem::loginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is  " << uid << " user token  is "<< token << endl;

	Json::Value  rtvalue;	// �ظ����ͻ��˵���Ϣ
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->send(return_str, MSG_CHAT_LOGIN_RSP);
		std::cout << "sended login response" << std::endl;
		});
	// ��redis��ȡuid��token�Ķ�Ӧ��Ϣ
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::getInstance()->get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	std::cout << "token given by user: " << token << ", token in redis: " << token_value << std::endl;
	//std::cout << "length of token: " << token.size() << ", length of token_value: " << token_value.size() << std::endl;
	if (token != token_value) {							// ֮ǰ����token_valueд����token_key�������˰���������������������������� 
		std::cout << "token invalid!" << std::endl;
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}
	rtvalue["error"] = ErrorCodes::Success;

	// ��redis�в����û���Ϣ
	std::string base_key = USER_BASE_INFO + uid_str;	
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = getBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	rtvalue["uid"] = uid;
	rtvalue["name"] = user_info->name;
	rtvalue["passwd"] = user_info->passwd;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	// defer���Զ����͸��ͻ��˵Ļذ������������ֶ�����

	// �����ݿ��ȡ���������б�

	// �����ݿ��ȡ�����б�

	auto serverName = ConfigMgr::getInst()["SelfServer"]["Name"];
	auto res = RedisMgr::getInstance()->hGet(LOGIN_COUNT, serverName);	// ��redis�в�ѯ��ǰ���������ӵĿͻ�������
	int count = 0;
	if (!res.empty()) {
		count = stoi(res);
	}
	else {
		std::cout << "Can't find Login Count" << std::endl;
	}
	count++;	// ����������һ
	std::string count_str = std::to_string(count);
	bool suc = RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, count_str);

	// ��session���û�uid
	session->setUserId(uid);

	//Ϊ�û����õ�¼ip server������
	std::string ipkey = USERIPPREFIX + uid_str;
	RedisMgr::getInstance()->set(ipkey, serverName);

	// �û�uid��session�󶨣���������
	UserMgr::getInstance()->setUserSession(uid, session);

	std::cout << "Login success!" << std::endl;
}

bool LogicSystem::getBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo) {
	std::string info_str = "";	// ����redis�в�ѯ�����û���Ϣ���ַ���
	bool b_base = RedisMgr::getInstance()->get(base_key, info_str);
	if (b_base) {	// �����redis�в鵽��
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);

		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "searching user uid is  " << uid << ", name  is "
			<< name << ", passwd is " << passwd << ", email is " << email << endl;
		
		userInfo->uid = uid;
		userInfo->name = name;
		userInfo->passwd = passwd;
		userInfo->email = email;
		userInfo->nick = nick;
		userInfo->desc = desc;
		userInfo->sex = sex;
		userInfo->icon = icon;
		return true;
	}
	// redis��û�в鵽����mysql���
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::getInstance()->getUser(uid);
	if (user_info == nullptr) {
		return false;
	}
	userInfo = user_info;	// ֱ�Ӹ�ֵ

	// ����ѯ��������д��redis����
	Json::Value redis_root;
	redis_root["uid"] = uid;
	redis_root["name"] = user_info->name;
	redis_root["passwd"] = user_info->passwd;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::getInstance()->set(base_key, redis_root.toStyledString());	// ���ַ�������ʽ����

}


void LogicSystem::searchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();	// �ͻ��˴������Ŀ�����uid��Ҳ�������ǳ�
	std::cout << "user SearchInfo uid is  " << uid_str << endl;

	Json::Value  rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->send(return_str, ID_SEARCH_USER_RSP);
		});

	// �ж�uid_str�Ƿ�Ϊ�����֣��ǵĻ�˵��Ϊuid�����ǵĻ�˵��Ϊ�ǳ�
	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		getUserByUid(uid_str, rtvalue);
	}
	else {
		getUserByName(uid_str, rtvalue);
	}
	return;
}

// �ж�һ���ַ����Ƿ�ȫ���������
bool LogicSystem::isPureDigit(const std::string& s) {
	for (char c : s) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::getUserByUid(std::string uid_str, Json::Value& rtvalue) {
	rtvalue["error"] = ErrorCodes::Success;
	// �ȴ�redis������
	std::string base_key = USER_BASE_INFO + uid_str;
	std::string info_str = "";
	bool success = RedisMgr::getInstance()->get(base_key, info_str);
	if (success) {	// ���ҳɹ�
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);

		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " passwd is " << passwd << " email is " << email << " icon is " << icon << endl;
		rtvalue["uid"] = uid;
		rtvalue["passwd"] = passwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;

	}
	// redis�в���ʧ����ȥmysql����
	std::shared_ptr<UserInfo> userInfo = MysqlMgr::getInstance()->getUser(std::stoi(uid_str));
	if (userInfo == nullptr) {	// ��ѯ�����δ�ҵ��û�
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	rtvalue["uid"] = userInfo->uid;
	rtvalue["passwd"] = userInfo->passwd;
	rtvalue["name"] = userInfo->name;
	rtvalue["email"] = userInfo->email;
	rtvalue["nick"] = userInfo->nick;
	rtvalue["desc"] = userInfo->desc;
	rtvalue["sex"] = userInfo->sex;
	rtvalue["icon"] = userInfo->icon;

	// ��redis��д���û���Ϣ
	RedisMgr::getInstance()->set(base_key, rtvalue.toStyledString());
}

// ��getUserByUid����
void LogicSystem::getUserByName(std::string name, Json::Value& rtvalue) {
	rtvalue["error"] = ErrorCodes::Success;
	std::string base_key = NAME_INFO + name;
	std::string userInfo_str = "";
	bool success = RedisMgr::getInstance()->get(base_key, userInfo_str);
	if (success) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(userInfo_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " passwd is " << passwd << " email is " << email << endl;

		rtvalue["uid"] = uid;
		rtvalue["passwd"] = passwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		return;
	}

	std::shared_ptr<UserInfo> userInfo = MysqlMgr::getInstance()->getUser(name);
	if (userInfo == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;	// ��ʵӦ�����ǳƷǷ�
		return;
	}
	rtvalue["uid"] = userInfo->uid;
	rtvalue["passwd"] = userInfo->passwd;
	rtvalue["name"] = userInfo->name;
	rtvalue["email"] = userInfo->email;
	rtvalue["nick"] = userInfo->nick;
	rtvalue["desc"] = userInfo->desc;
	rtvalue["sex"] = userInfo->sex;
	rtvalue["icon"] = userInfo->icon;

	RedisMgr::getInstance()->set(base_key, rtvalue.toStyledString());
}

void  LogicSystem::addFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Value rtvalue;
	Json::Value root;
	Json::Reader reader;
	
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session](){
		std::string rsp = rtvalue.toStyledString();
		session->send(rsp, MSG_IDS::ID_ADD_FRIEND_RSP);
		});

	reader.parse(msg_data, root);
	auto applyName = root["applyName"].asString();
	auto from_uid = root["uid"].asInt();
	auto to_uid = root["to_uid"].asInt();
	auto remark = root["remark"].asString();

	std::cout<<"user add friend, uid is " << from_uid << ", apply name is " << applyName 
			 << ", to_uid is " << to_uid << ", remark is " << remark << std::endl;

	bool success = MysqlMgr::getInstance()->addFriend(from_uid, to_uid);
	if (!success) {
		std::cout << "failed to add firend in mysql" << std::endl;
		rtvalue["error"] = ErrorCodes::MysqlErr;
	}

	// ͨ��redis����Ŀ���û�����server�ĵ�ַ����������ߵĻ���
	std::string to_str = std::to_string(to_uid);
	std::string to_ip_key = USERIPPREFIX + to_str;
	std::string ip_value = "";											// ���Ŀ���û����ڷ���������
	// ����Ŀ���û����ڷ�����
	bool b_ip = RedisMgr::getInstance()->get(to_ip_key, ip_value);
	// û�в鵽˵���û�������
	if(!b_ip) {
		std::cout << "target user offline" << std::endl;
		return;
	}

	// �鵽��
	if (ip_value == ConfigMgr::getInst()["SelfServer"]["Name"]) {	// ������ڱ�������
		// ֪ͨ�Է��� �������� ����
		std::cout << "target user in the same server" << std::endl;
		Json::Value notice;
		notice["error"] = ErrorCodes::Success;
		notice["applyUid"] = from_uid;
		notice["applyName"] = applyName;
		notice["desc"] = "";
		std::string return_str = notice.toStyledString();
		// ���ظ��ͻ��ˣ�
		session->send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
	}
	else {															// ���ڱ�������
		std::cout << "target user in server:" << ip_value << std::endl;
		std::string uid_str = std::to_string(from_uid);
		std::string base_key = USER_BASE_INFO + uid_str;
		auto fromUserInfo = std::make_shared<UserInfo>();
		bool b_info = this->getBaseInfo(base_key, from_uid, fromUserInfo);	// ���ұ��û���Ϣ

		// ��Ŀ���û����ڵķ���������grpc����
		AddFriendReq add_req;
		add_req.set_applyuid(from_uid);
		add_req.set_touid(to_uid);
		add_req.set_name(applyName);
		add_req.set_desc("");
		if (b_info) {	// ������ҵ����û���Ϣ�Ļ��ͰѲ鵽����ϢҲ����
			add_req.set_icon(fromUserInfo->icon);
			add_req.set_nick(fromUserInfo->nick);
			add_req.set_sex(fromUserInfo->sex);
		}
		ChatGrpcClient::getInstance()->NotifyAddFriend(ip_value, add_req);

	}
	return;
}

