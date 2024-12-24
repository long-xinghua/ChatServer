#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::regUser(const std::string& name, const std::string& email, const std::string& pwd)
{
    return _dao.regUser(name, email, pwd);
}

bool MysqlMgr::checkEmail(const std::string& name, const std::string& email) {
    return _dao.checkEmail(name, email);
}
bool MysqlMgr::updatePasswd(const std::string& email, const std::string& passwd) {
    return _dao.updatePasswd(email, passwd);
}

bool MysqlMgr::checkPasswd(const std::string& email, const std::string& passwd, UserInfo& userInfo) {
    return _dao.checkPasswd(email, passwd,  userInfo);
}

std::shared_ptr<UserInfo> MysqlMgr::getUser(const int& uid) {
    return _dao.getUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::getUser(const std::string& name) {
    return _dao.getUser(name);
}

bool MysqlMgr::addFriend(const int& from_uid, const int& to_uid)
{
    return _dao.addFriend(from_uid, to_uid);
}

bool MysqlMgr::getApplyList(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit)
{
    return _dao.getApplyList(to_uid, applyList, begin, limit);
}

MysqlMgr::MysqlMgr() {
}