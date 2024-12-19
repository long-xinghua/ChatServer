#include "ChatGrpcClient.h"

ChatConPool::ChatConPool(size_t poolSize, std::string host, std::string port) :
	_poolSize(poolSize), _host(host), _port(port), _b_stop(false) {
	for (int i = 0; i < _poolSize; i++) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port, grpc::InsecureChannelCredentials());
		_connections.push(ChatService::NewStub(channel));
	}
}

ChatConPool::~ChatConPool() {
	std::lock_guard<std::mutex> lock(_mutex);
	close();
	while (!_connections.empty()) {
		_connections.pop();
	}
}

std::unique_ptr<ChatService::Stub> ChatConPool::getConnection() {
	std::unique_lock<std::mutex> lock(_mutex);
	_cond.wait(lock, [this]() {
		if (!_b_stop) {
			if (_connections.empty()) {
				return false;
			}
		}
		return true;
		});
	if (_b_stop) {
		return nullptr;
	}
	auto stub = std::move(_connections.front());
	_connections.pop();
	return stub;

}

bool ChatConPool::returnConnection(std::unique_ptr<ChatService::Stub> stub) {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_b_stop) {
		return true;
	}
	_connections.push(std::move(stub));
	_cond.notify_one();
	return true;
}

void ChatConPool::close() {
	_b_stop = true;
	_cond.notify_all();
}



ChatGrpcClient::ChatGrpcClient() {
	auto& cfg = ConfigMgr::getInst();
	auto serverList = cfg["PeerServer"]["Servers"];

	std::vector<std::string> words;

	std::stringstream ss(serverList);
	std::string word;

	while (std::getline(ss, word, ',')) {	// std::getline读取输入流中的数据，遇到分隔符时停止，将读到的内容存入word
		words.push_back(word);
	}
	// 得到服务器名称后寻找各服务器详细信息
	for (auto& server : words) {
		if (cfg[server]["Name"].empty()) {
			continue;
		}
		// 对每个ChatServer都建立连接池进行连接
		_pools[cfg[server]["Name"]] = std::make_unique<ChatConPool>(5, cfg[server]["Host"], cfg[server]["RPCPort"]);
	}
}

ChatGrpcClient::~ChatGrpcClient() {
	
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req) {
	AddFriendRsp rsp;
	return rsp;
	//todo...
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req) {
	AuthFriendRsp rsp;
	return rsp;
	//todo...
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo) {
	return true;
	//todo...
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue) {
	TextChatMsgRsp rsp;
	return rsp;
	//todo...
}