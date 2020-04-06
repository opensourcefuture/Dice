/**
 * 黑名单明细
 * 更数据库式的管理
 * Copyright (C) 2019-2020 String.Empty
 */
#pragma once
#include <string>
#include "StrExtern.hpp"
#include <vector>
#include <map>
#include <set>
#include <memory>

using std::pair;
using std::string;
using std::to_string;
using std::vector; 
using std::map;
using std::multimap;
using std::set;

class FromMsg;

enum class BlackID { fromGroup, fromQQ, inviterQQ, ownerQQ };
class DDBlackMark {
    //不良记录类型:null,kick,ban,spam,other,ruler,local,extern;枚举之外一律非法
    string type = "null";
    string time;
    //fromGroup,fromQQ,inviterQQ,ownerQQ
    using item = pair<long long, bool>;
    item fromGroup{ 0,false };
    item fromQQ{ 0,false };
    item inviterQQ{ 0,false };
    item ownerQQ{ 0,false };
    //
    long long DiceMaid = 0;
    long long masterQQ = 0;
    //危险等级
    short danger = 0;
    //唯一云端编号（0表示不对应云端）
    int wid = 0;
    //说明
    string note;
    //附加备注
    string comment{};
    void erase();
    void upload();
    int check_cloud();
public:
    DDBlackMark(long long, long long);
    DDBlackMark(void*);
    DDBlackMark(string);
    friend class DDBlackManager;
    friend class DDBlackMarkFactory;
    //warning文本生成
    string printJson(int tab)const;
    string warning()const;
    const char* getData()const;
    void fill_note();
    //是否合法构造
    bool isValid = false;
    bool isClear = false;
    bool isType()const;
    bool isType(string strType)const;
    bool isSame(const DDBlackMark&)const;
    bool isSource(long long)const;
    bool is_remit()const;
    DDBlackMark& operator<<(const DDBlackMark&);
    //bool operator<(const DDBlackMark&)const;
};

class DDBlackManager {
    vector<DDBlackMark>vBlackList;
    //云端编号映射表
    map<int, unsigned int>mCloud;
    set<unsigned int> sIDEmpty;
    //可重复映射表
    multimap<string, unsigned int>mTimeIndex;
    set<unsigned int> sTimeEmpty;
    set<unsigned int> sGroupEmpty;
    set<unsigned int> sQQEmpty;
    //发现所指相同的记录
    int find(const DDBlackMark&);
    //更新记录
    void insert(DDBlackMark&);
    bool update(DDBlackMark&, unsigned int, int);
    void reset_group_danger(long long);
    void reset_qq_danger(long long);
    bool up_group_danger(long long, DDBlackMark&);
    bool up_qq_danger(long long, DDBlackMark&);
    bool isActive = false;
public:
    multimap<long long, unsigned int>mGroupIndex;
    multimap<long long, unsigned int>mQQIndex;
    //未注销黑名单的危险等级
    map<long long, short>mQQDanger;
    map<long long, short>mGroupDanger;
    short get_group_danger(long long)const;
    short get_qq_danger(long long)const;
    void isban(FromMsg*);
    string list_group_warning(long long);
    string list_qq_warning(long long);
    string list_self_group_warning(long long);
    string list_self_qq_warning(long long);
    void add_black_group(long long, FromMsg*);
    void add_black_qq(long long, FromMsg*);
    void rm_black_group(long long, FromMsg*);
    void rm_black_qq(long long, FromMsg*);
    void verify(void*, long long);
    void create(DDBlackMark&);
    //读取json格式黑名单记录
    int loadJson(string strPath);
    //读取旧版本黑名单列表
    int loadHistory(string strLoc);
    void saveJson(string strPath)const;
};

extern std::unique_ptr<DDBlackManager> blacklist;

class DDBlackMarkFactory {
    DDBlackMark mark;
    using Factory = DDBlackMarkFactory;
public:
    DDBlackMarkFactory(long long qq, long long group):mark(qq,group){
        mark.comment.clear();
    }
    Factory& type(string strType) {
        mark.type = strType;
        return *this;
    }
    Factory& time(string strTime) {
        mark.time = strTime;
        return *this;
    }
    Factory& note(string strNote) {
        mark.note = strNote;
        return *this;
    }
    Factory& inviterQQ(long long qq) {
        mark.inviterQQ = { qq,true };
        return *this;
    }
    Factory& fromQQ(long long qq) {
        mark.fromQQ = { qq,true };
        return *this;
    }
    Factory& DiceMaid(long long qq) {
        mark.DiceMaid = qq;
        return *this;
    }
    Factory& masterQQ(long long qq) {
        mark.masterQQ = qq;
        return *this;
    }
    Factory& sign();
    Factory& upload(){
        mark.upload();
        return *this;
    }
    DDBlackMark& product() {
        return mark;
    }
};

void AddWarning(const std::string& msg, long long DiceQQ, long long fromGroup);
void warningHandler();