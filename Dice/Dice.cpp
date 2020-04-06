/*
 *  _______     ________    ________    ________    __
 * |   __  \   |__    __|  |   _____|  |   _____|  |  |
 * |  |  |  |     |  |     |  |        |  |_____   |  |
 * |  |  |  |     |  |     |  |        |   _____|  |__|
 * |  |__|  |   __|  |__   |  |_____   |  |_____    __
 * |_______/   |________|  |________|  |________|  |__|
 *
 * Dice! QQ Dice Robot for TRPG
 * Copyright (C) 2018-2020 w4123溯洄
 * Copyright (C) 2019-2020 String.Empty
 *
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this
 * program. If not, see <http://www.gnu.org/licenses/>.
 */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <iostream>
#include <map>
#include <set>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <mutex>

#include "APPINFO.h"
#include "DiceFile.hpp"
#include "Jsonio.h"
#include "RandomGenerator.h"
#include "RD.h"
#include "CQEVE_ALL.h"
#include "ManagerSystem.h"
#include "InitList.h"
#include "GlobalVar.h"
#include "DiceMsgSend.h"
#include "MsgFormat.h"
#include "DiceCloud.h"
#include "CardDeck.h"
#include "DiceConsole.h"
#include "EncodingConvert.h"
#include "CharacterCard.h"
#include "DiceEvent.h"

#pragma warning(disable:4996)
#pragma warning(disable:6031)

using namespace std;
using namespace CQ;

map<long long, User>UserList{};
map<long long, Chat>ChatList{};
unique_ptr<Initlist> ilInitList;
map<chatType, chatType> mLinkedList;
multimap<chatType, chatType> mFwdList;
multimap<long long, long long> ObserveGroup;
multimap<long long, long long> ObserveDiscuss;
ThreadFactory threads;
string strFileLoc;

//加载数据
void loadData() {
	mkDir("DiceData");
	string strLog;
	loadDir(loadXML<CardTemp>, string("DiceData\\CardTemp\\"), mCardTemplet, strLog, true);
	loadJMap(strFileLoc + "ReplyDeck.json", CardDeck::mReplyDeck);
	HelpDoc["回复列表"] = "回复触发词列表:" + listKey(CardDeck::mReplyDeck);
	if (loadDir(loadJMap, string("DiceData\\PublicDeck\\"), CardDeck::mExternPublicDeck, strLog) < 1) {
		loadJMap(strFileLoc + "PublicDeck.json", CardDeck::mExternPublicDeck);
		loadJMap(strFileLoc + "ExternDeck.json", CardDeck::mExternPublicDeck);
	}
	map_merge(CardDeck::mPublicDeck, CardDeck::mExternPublicDeck);
	if (!strLog.empty()) {
		strLog += "扩展配置读取完毕√";
		console.log(strLog, 1, printSTNow());
	}
	HelpDoc["扩展牌堆"] = listKey(CardDeck::mExternPublicDeck);
	HelpDoc["全牌堆列表"] = listKey(CardDeck::mPublicDeck);
	//读取帮助文档
	HelpDoc["master"] = printQQ(console.master());
	ifstream ifstreamHelpDoc(strFileLoc + "HelpDoc.txt");
	if (ifstreamHelpDoc)
	{
		string strName, strMsg, strDebug;
		while (ifstreamHelpDoc) {
			getline(ifstreamHelpDoc, strName);
			getline(ifstreamHelpDoc, strMsg);
			while (strMsg.find("\\n") != string::npos)strMsg.replace(strMsg.find("\\n"), 2, "\n");
			while (strMsg.find("\\s") != string::npos)strMsg.replace(strMsg.find("\\s"), 2, " ");
			while (strMsg.find("\\t") != string::npos)strMsg.replace(strMsg.find("\\t"), 2, "	");
			EditedHelpDoc[strName] = strMsg;
			HelpDoc[strName] = strMsg;
		}
	}
	ifstreamHelpDoc.close();
}
//备份数据
void dataBackUp() {
	mkDir("DiceData\\conf");
	mkDir("DiceData\\user");
	mkDir("DiceData\\audit");
	//保存卡牌
	saveJMap(strFileLoc + "GroupDeck.json", CardDeck::mGroupDeck);
	saveJMap(strFileLoc + "GroupDeckTmp.json", CardDeck::mGroupDeckTmp);
	saveJMap(strFileLoc + "PrivateDeck.json", CardDeck::mPrivateDeck);
	saveJMap(strFileLoc + "PrivateDeckTmp.json", CardDeck::mPrivateDeckTmp);
	saveFile(strFileLoc + "ObserveGroup.RDconf", ObserveGroup);
	saveFile(strFileLoc + "ObserveDiscuss.RDconf", ObserveDiscuss);
	//备份列表
	saveBFile("DiceData\\user\\PlayerCards.RDconf", PList);
	saveFile("DiceData\\user\\ChatList.txt", ChatList);
	saveBFile("DiceData\\user\\ChatConf.RDconf", ChatList);
	clearUser();
	saveFile("DiceData\\user\\UserList.txt", UserList);
	saveBFile("DiceData\\user\\UserConf.RDconf", UserList);
	ilInitList->save();
}
EVE_Enable(eventEnable)
{
	llStartTime = clock();
	//Wait until the thread terminates
	strFileLoc = getAppDirectory();
	console.DiceMaid = getLoginQQ();
	GlobalMsg["strSelfName"] = getLoginNick();
	mkDir("DiceData\\audit");
	if(!console.load()){
		ifstream ifstreamMaster(strFileLoc + "Master.RDconf");
		if (ifstreamMaster)
		{
			std::pair<int, int>ClockToWork{}, ClockOffWork{};
			int iDisabledGlobal, iDisabledMe, iPrivate, iDisabledJrrp, iLeaveDiscuss;
			ifstreamMaster >> console.masterQQ >> console.isMasterMode >> iDisabledGlobal >> iDisabledMe >> iPrivate >> iDisabledJrrp >> iLeaveDiscuss
				>> ClockToWork.first >> ClockToWork.second >> ClockOffWork.first >> ClockOffWork.second;
			console.set("DisabledGlobal", iDisabledGlobal);
			console.set("DisabledMe", iDisabledMe);
			console.set("Private", iPrivate);
			console.set("DisabledJrrp", iDisabledJrrp);
			console.set("LeaveDiscuss", iLeaveDiscuss);
			console.setClock(ClockToWork, ClockEvent::on);
			console.setClock(ClockOffWork, ClockEvent::off);
		}
		ifstreamMaster.close();
		std::map<string, int>boolConsole;
		loadJMap(strFileLoc + "boolConsole.json", boolConsole);
		for (auto& [key, val] : boolConsole) {
			console.set(key, val);
		}
		console.setClock({ 4, 0 }, ClockEvent::save);
		console.setClock({ 5, 0 }, ClockEvent::clear);
		console.loadNotice();
	}
	//读取聊天列表
	if (loadBFile("DiceData\\user\\UserConf.RDconf", UserList) < 1) {
		map<long long, int>DefaultDice;
		if (loadFile(strFileLoc + "Default.RDconf", DefaultDice) > 0)
			for (auto p : DefaultDice) {
				getUser(p.first).create(NEWYEAR).intConf["默认骰"] = p.second;
			}
		map<long long, string>DefaultRule;
		if (loadFile(strFileLoc + "DefaultRule.RDconf", DefaultRule) > 0)
			for (auto p : DefaultRule) {
				if (isdigit(static_cast<unsigned char>(p.second[0])))break;
				getUser(p.first).create(NEWYEAR).strConf["默认规则"] = p.second;
			}
		ifstream ifName(strFileLoc + "Name.dicedb");
		if (ifName)
		{
			long long GroupID = 0, QQ = 0;
			string name;
			while (ifName >> GroupID >> QQ >> name){
				getUser(QQ).create(NEWYEAR).setNick(GroupID, base64_decode(name));
			}
		}
	}
	if (loadFile("DiceData\\user\\UserList.txt", UserList) < 1) {
		set<long long> WhiteQQ;
		if (loadFile(strFileLoc + "WhiteQQ.RDconf", WhiteQQ) > 0)
			for (auto qq : WhiteQQ) {
				getUser(qq).create(NEWYEAR).trust(1);
			}
		//读取管理员列表
		set<long long> AdminQQ;
		if (loadFile(strFileLoc + "AdminQQ.RDconf", AdminQQ) > 0)
		for (auto qq : AdminQQ) {
			getUser(qq).create(NEWYEAR).trust(4);
		}
		if(console.master())getUser(console.master()).create(NEWYEAR).trust(5);
	}
	if (loadBFile("DiceData\\user\\ChatConf.RDconf", ChatList) < 1) {
		set<long long>GroupList;
		if (loadFile(strFileLoc + "DisabledDiscuss.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).discuss().set("停用指令");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledJRRPDiscuss.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).discuss().set("禁用jrrp");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledMEGroup.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).discuss().set("禁用me");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledHELPDiscuss.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).discuss().set("禁用help");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledOBDiscuss.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).discuss().set("禁用ob");
			}
		GroupList.clear();
		map<chatType, int> mDefault;
		if (loadFile(strFileLoc + "DefaultCOC.MYmap", mDefault) > 0)
			for (auto it : mDefault) {
				if (it.first.second == Private)getUser(it.first.first).create(NEWYEAR).setConf("rc房规",it.second);
				else chat(it.first.first).create(NEWYEAR).setConf("rc房规", it.second);
			}
		if (loadFile(strFileLoc + "DisabledGroup.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).group().set("停用指令");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledJRRPGroup.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).group().set("禁用jrrp");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledMEDiscuss.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).group().set("禁用me");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledHELPGroup.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).group().set("禁用help");
			}
		GroupList.clear();
		if (loadFile(strFileLoc + "DisabledOBGroup.RDconf", GroupList) > 0)
			for (auto p : GroupList) {
				chat(p).group().set("禁用ob");
			}
		GroupList.clear();
		map<long long, string>WelcomeMsg;
		if (loadFile(strFileLoc + "WelcomeMsg.RDconf", WelcomeMsg) > 0) {
			for (auto p : WelcomeMsg) {
				chat(p.first).group().setText("入群欢迎",p.second);
			}
		}
		if (loadFile(strFileLoc + "WhiteGroup.RDconf", GroupList) > 0) {
			for (auto g : GroupList) {
				chat(g).group().set("许可使用").set("免清");
			}
		}
	}
	if (loadFile("DiceData\\user\\ChatList.txt", ChatList) < 1) {
		map<chatType, time_t> mLastMsgList;
		for (auto it : mLastMsgList) {
			if (it.first.second == Private)getUser(it.first.first).create(it.second);
			else chat(it.first.first).create(it.second).lastmsg(it.second).isGroup = 2 - it.first.second;
		}
		std::map<long long, long long> mGroupInviter;
		if (loadFile(strFileLoc + "GroupInviter.RDconf", mGroupInviter) < 1) {
			for (auto it : mGroupInviter) {
				chat(it.first).group().inviter = it.second;
			}
		}
	}
	for (auto &[gid,gname] : getGroupList()) {
		chat(gid).group().name(gname).reset("已退");
	}
	loadFile(strFileLoc + "ObserveGroup.RDconf", ObserveGroup);
	loadFile(strFileLoc + "ObserveDiscuss.RDconf", ObserveDiscuss);
	blacklist = make_unique<DDBlackManager>();
	if (!blacklist->loadJson("DiceData\\conf\\BlackList.json")) {
		blacklist->loadJson(strFileLoc + "BlackMarks.json");
		blacklist->loadHistory(strFileLoc);
		blacklist->saveJson("DiceData\\conf\\BlackList.json");
	}
	ilInitList = make_unique<Initlist>(strFileLoc + "INIT.DiceDB");
	if (loadJMap("DiceData\\conf\\CustomMsg.json", EditedMsg) < 0)loadJMap(strFileLoc + "CustomMsg.json", EditedMsg);
	//预修改出场回复文本
	if (EditedMsg.count("strSelfName"))GlobalMsg["strSelfName"] = EditedMsg["strSelfName"];
	for (auto it : EditedMsg) {
		while (it.second.find("本机器人") != string::npos)it.second.replace(it.second.find("本机器人"), 8, GlobalMsg["strSelfName"]);
		GlobalMsg[it.first] = it.second;
	}
	loadData();
	if (loadBFile("DiceData\\user\\PlayerCards.RDconf", PList) < 1) {
		ifstream ifstreamCharacterProp(strFileLoc + "CharacterProp.RDconf");
		if (ifstreamCharacterProp)
		{
			long long QQ, GrouporDiscussID;
			int Type, Value;
			string SkillName;
			while (ifstreamCharacterProp >> QQ >> Type >> GrouporDiscussID >> SkillName >> Value)
			{
				if (SkillName == "智力/灵感")SkillName = "智力";
				getPlayer(QQ)[0].set(SkillName, Value);
			}
		}
		ifstreamCharacterProp.close();
	}
	for (auto pl : PList) {
		if (!UserList.count(pl.first))getUser(pl.first).create(NEWYEAR);
	}
	//读取卡牌
	loadJMap(strFileLoc + "GroupDeck.json",CardDeck::mGroupDeck);
	loadJMap(strFileLoc + "GroupDeckTmp.json", CardDeck::mGroupDeckTmp);
	loadJMap(strFileLoc + "PrivateDeck.json", CardDeck::mPrivateDeck);
	loadJMap(strFileLoc + "PrivateDeckTmp.json", CardDeck::mPrivateDeckTmp);
	//骰娘网络
	getDiceList();
	Cloud::update();
	while (msgSendThreadRunning)Sleep(10);
	Enabled = true;
	threads(SendMsg);
	threads(ConsoleTimer);
	threads(warningHandler);
	threads(frqHandler);
	console.log(GlobalMsg["strSelfName"] + "初始化完成，用时" + to_string((clock() - llStartTime) / 1000) + "秒", 0b1, printSTNow());
	llStartTime = clock();
	return 0;
}

mutex GroupAddMutex;
bool eve_GroupAdd(Chat& grp) {
	{
		lock_guard<std::mutex> lock_queue(GroupAddMutex);
		if (grp.isset("未进"))grp.reset("未进");
		else if(grp.isset("已退"))grp.reset("已退");
		else if (time(NULL) - grp.tCreated > 1)return 0;
	}
	if (!console["ListenGroupAdd"] || grp.isset("忽略"))return 0;
	long long fromGroup = grp.ID;
	string strNow = printSTNow();
	string strMsg = GlobalMsg["strSelfName"] + "新加入" + GroupInfo(fromGroup).tostring();
	if (blacklist->get_group_danger(fromGroup)) {
		grp.leave("!warning" + blacklist->list_group_warning(fromGroup));
		strMsg += "为黑名单群，已退群";
		console.log(strMsg, 0b10, printSTNow());
		return 1;
	}
	if (grp.isset("使用许可"))strMsg += "（已获使用许可）";
	if (grp.inviter) {
		strMsg += ",邀请者" + printQQ(chat(fromGroup).inviter);
	}
	int max_trust = 0;
	int max_danger = 0;
	long long ownerQQ = 0;
	ResList blacks;
	std::vector<GroupMemberInfo>list = getGroupMemberList(fromGroup);
	if (list.empty()) {
		strMsg += "，群员名单未加载；";
	}
	else {
		for (auto &each : list) {
			if (each.QQID == console.DiceMaid)continue;
			if (each.permissions > 1) {
				max_trust |= (1 << trustedQQ(each.QQID));
				if (blacklist->get_qq_danger(each.QQID)) {
					strMsg += ",发现黑名单管理员" + printQQ(each.QQID);
					if (grp.isset("免黑")) {
						strMsg += "（群免黑）";
					}
					else {
						sendGroupMsg(fromGroup, "!warning" + blacklist->list_qq_warning(each.QQID));
						grp.leave("发现黑名单管理员" + printQQ(each.QQID) + "将预防性退群");
						strMsg += "，已退群";
						console.log(strMsg, 0b10, strNow);
						return 1;
					}
				}
				if (each.permissions == 3) {
					ownerQQ = each.QQID;
					strMsg += "，群主" + printQQ(each.QQID) + "；";
				}
			}
			else if (blacklist->get_qq_danger(each.QQID)) {
				//max_trust |= 1;
				blacks << printQQ(each.QQID);
				if (blacklist->get_qq_danger(each.QQID)) {
					AddMsgToQueue(blacklist->list_self_qq_warning(each.QQID), fromGroup, Group);
				}
			}
		}
		if (!chat(fromGroup).inviter && list.size() == 2 && ownerQQ) {
			chat(fromGroup).inviter = ownerQQ;
			strMsg += "邀请者" + printQQ(ownerQQ);
		}
	}
	if (!blacks.empty()) {
		string strNote = "发现黑名单群员" + blacks.show();
		strMsg += strNote;
	}
	if (console["Private"] && !grp.isset("许可使用"))
	{	//避免小群绕过邀请没加上白名单
		if (max_trust > 1) {
			grp.set("许可使用");
			strMsg += "已自动追加使用许可";
		}
		else {
			strMsg += "无白名单，已退群";
			console.log(strMsg, 1, strNow);
			grp.leave(getMsg("strPreserve"));
			return 1;
		}
	}
	if (!blacks.empty())console.log(strMsg, 0b10, strNow);
	else console.log(strMsg, 1, strNow);
	if (!GlobalMsg["strAddGroup"].empty()) {
		this_thread::sleep_for(chrono::seconds(2));
		AddMsgToQueue(getMsg("strAddGroup"), { fromGroup, Group });
	}
	if (console["CheckGroupLicense"] && !grp.isset("许可使用")) {
		grp.set("未审核");
		this_thread::sleep_for(chrono::seconds(2));
		AddMsgToQueue(getMsg("strAddGroupNoLicense"), { fromGroup, Group });
	}
	return 0;
}

//处理骰子指令

EVE_PrivateMsg_EX(eventPrivateMsg)
{
	if (!Enabled)return;
	FromMsg Msg(eve.message, eve.fromQQ);
	if (Msg.DiceFilter())eve.message_block();
	Msg.FwdMsg(eve.message);
	return;
}

EVE_GroupMsg_EX(eventGroupMsg)
{
	if (!Enabled)return;
	if (eve.isAnonymous())return;
	if (eve.isSystem())return;
	Chat& grp = chat(eve.fromGroup).group().lastmsg(time(NULL));
	if (grp.isset("未进"))eve_GroupAdd(grp);
	if (!grp.isset("忽略")) {
		FromMsg Msg(eve.message, eve.fromGroup, Group, eve.fromQQ);
		if (Msg.DiceFilter())eve.message_block();
		Msg.FwdMsg(eve.message);
	}
	if (grp.isset("拦截消息"))eve.message_block();
	return;
}

EVE_DiscussMsg_EX(eventDiscussMsg)
{
	if (!Enabled)return;
	time_t tNow = time(NULL);
	if (console["LeaveDiscuss"]) {
		sendDiscussMsg(eve.fromDiscuss, getMsg("strLeaveDiscuss"));
		Sleep(1000);
		setDiscussLeave(eve.fromDiscuss);
		return;
	}
	Chat& grp = chat(eve.fromDiscuss).discuss().lastmsg(time(NULL));
	if (blacklist->get_qq_danger(eve.fromQQ) && console["AutoClearBlack"]) {
		string strMsg = "发现黑名单用户" + printQQ(eve.fromQQ) + "，自动执行退群";
		console.log(printChat({ eve.fromDiscuss,Discuss }) + strMsg, 0b10, printSTNow());
		grp.leave(strMsg);
		return;
	}
	FromMsg Msg(eve.message, eve.fromDiscuss, Discuss, eve.fromQQ);
	if (Msg.DiceFilter() || grp.isset("拦截消息"))eve.message_block();
	Msg.FwdMsg(eve.message);
	return;
}

EVE_System_GroupMemberIncrease(eventGroupMemberIncrease)
{
	Chat& grp = chat(fromGroup);
	if (grp.isset("忽略"))return 0;
	if (beingOperateQQ != console.DiceMaid){
		if(chat(fromGroup).strConf.count("入群欢迎")){
			string strReply = chat(fromGroup).strConf["入群欢迎"];
			while (strReply.find("{at}") != string::npos) {
				strReply.replace(strReply.find("{at}"), 4, "[CQ:at,qq=" + to_string(beingOperateQQ) + "]");
			}
			while (strReply.find("{@}") != string::npos)
			{
				strReply.replace(strReply.find("{@}"), 3, "[CQ:at,qq=" + to_string(beingOperateQQ) + "]");
			}
			while (strReply.find("{nick}") != string::npos)
			{
				strReply.replace(strReply.find("{nick}"), 6, getStrangerInfo(beingOperateQQ).nick);
			}
			while (strReply.find("{age}") != string::npos)
			{
				strReply.replace(strReply.find("{age}"), 5, to_string(getStrangerInfo(beingOperateQQ).age));
			}
			while (strReply.find("{sex}") != string::npos)
			{
				strReply.replace(strReply.find("{sex}"), 5,
					getStrangerInfo(beingOperateQQ).sex == 0
					? "男"
					: getStrangerInfo(beingOperateQQ).sex == 1
					? "女"
					: "未知");
			}
			while (strReply.find("{qq}") != string::npos)
			{
				strReply.replace(strReply.find("{qq}"), 4, to_string(beingOperateQQ));
			}
			grp.update(time(NULL));
			AddMsgToQueue(strReply, fromGroup, Group); 
		}
		if(blacklist->get_qq_danger(beingOperateQQ)) {
		string strNow = printSTNow();
		string strNote = printGroup(fromGroup) + "发现" + GlobalMsg["strSelfName"] + "的黑名单用户" + printQQ(beingOperateQQ) + "入群";
		AddMsgToQueue(blacklist->list_self_qq_warning(beingOperateQQ), fromGroup, Group);
		if (grp.isset("免清"))strNote += "（群免清）";
		else if (grp.isset("免黑"))strNote += "（群免黑）";
		else if (getGroupMemberInfo(fromGroup, console.DiceMaid).permissions > 1)strNote += "（群内有权限）";
		else if (console["LeaveBlackQQ"]) {
			strNote += "（已退群）";
			grp.leave("发现黑名单用户" + printQQ(beingOperateQQ) + "入群,将预防性退群");
		}
		console.log(strNote, 0b10, strNow);
	}
	}
	else if (beingOperateQQ == console.DiceMaid){
		return eve_GroupAdd(grp);
	}
	return 0;
}

EVE_System_GroupMemberDecrease(eventGroupMemberDecrease) {
	Chat& grp = chat(fromGroup);
	if (beingOperateQQ == console.DiceMaid) {
		grp.set("已退");
		if (!console || trustedQQ(fromQQ) > 1 || grp.isset("忽略"))return 0;
		string strNow = printSTime(stNow);
		string strNote = printQQ(fromQQ) + "将" + printQQ(beingOperateQQ) + "移出了群" + to_string(fromGroup);
		console.log(strNote, 0b1000, strNow);
		if (!console["ListenGroupKick"] || trustedQQ(fromQQ) > 1 || grp.isset("免黑"))return 0;
		DDBlackMarkFactory mark{ fromQQ ,fromGroup };
		mark.sign().type("kick").time(strNow).note(strNow + " " + strNote);
		blacklist->create(mark.product());
		grp.reset("许可使用").reset("免清");
	}
	else if (mDiceList.count(beingOperateQQ) && subType == 2 && console["ListenGroupKick"]) {
		if (!console || grp.isset("忽略"))return 0;
		string strNow = printSTime(stNow);
		string strNote = printQQ(fromQQ) + "将" + printQQ(beingOperateQQ) + "移出了群" + to_string(fromGroup);
		console.log(strNote, 0b1000, strNow);
		if (trustedQQ(fromQQ) > 1 || grp.isset("免黑"))return 0;
		DDBlackMarkFactory mark{ fromQQ ,fromGroup };
		mark.type("kick").time(strNow).note(strNow + " " + strNote).DiceMaid(beingOperateQQ).masterQQ(mDiceList[beingOperateQQ]);
		if (grp.inviter && trustedQQ(grp.inviter) < 2) {
			strNote += ";入群邀请者：" + printQQ(grp.inviter);
			if (console["KickedBanInviter"])mark.inviterQQ(grp.inviter).note(strNow + " " + strNote);
		}
		grp.reset("许可使用").reset("免清");
		blacklist->create(mark.product());
	}
	return 0;
}

EVE_System_GroupBan(eventGroupBan) {
	Chat& grp = chat(fromGroup);
	if (grp.isset("忽略") || (beingOperateQQ != console.DiceMaid && !mDiceList.count(beingOperateQQ)) || !console["ListenGroupBan"])return 0;
	if (subType == 1) {
		if (beingOperateQQ == console.DiceMaid) {
			console.log(GlobalMsg["strSelfName"] + "在" + printGroup(fromGroup) + "中被解除禁言",0b10, printSTNow());
			return 1;
		}
	}
	else {
		string strNow = printSTNow();
		long long llOwner = 0;
		string strNote = "在" + printGroup(fromGroup) + "中," + printQQ(beingOperateQQ) + "被" + printQQ(fromQQ) + "禁言" + to_string(duration) + "秒";
		if (trustedQQ(fromQQ) > 1 || grp.isset("免黑")){
			console.log(strNote, 0b10, strNow);
			return 1;
		}
		DDBlackMarkFactory mark{ fromQQ ,fromGroup };
		mark.type("ban").time(strNow).note(strNow + " " + strNote);
		if (mDiceList.count(fromQQ))mark.fromQQ(0);
		if (beingOperateQQ == console.DiceMaid) {
			if (!console)return 0;
			mark.sign();
		}
		else {
			mark.DiceMaid(beingOperateQQ).masterQQ(mDiceList[beingOperateQQ]);
		}
		//统计群内管理
		int intAuthCnt = 0;
		string strAuthList;
		vector<GroupMemberInfo> list = getGroupMemberList(fromGroup);
		for (auto& member : list) {
			if (member.permissions == 3) {
				llOwner = member.QQID;
			}
			else if (member.permissions == 2) {
				strAuthList += '\n' + member.Nick + "(" + to_string(member.QQID) + ")";
				intAuthCnt++;
			}
		}
		strAuthList = "；群主" + printQQ(llOwner) + ",另有管理员" + to_string(intAuthCnt) + "名" + strAuthList;
		mark.note(strNow + " " + strNote);
		if (grp.inviter && beingOperateQQ == console.DiceMaid  && trustedQQ(grp.inviter) < 2) {
			strNote += ";入群邀请者：" + printQQ(grp.inviter);
			if (console["BannedBanInviter"])mark.inviterQQ(grp.inviter);
			mark.note(strNow + " " + strNote);
		}
		grp.reset("免清").reset("许可使用").leave();
		console.log(strNote + strAuthList, 0b1000, strNow);
		blacklist->create(mark.product());
		return 1;
	}
	return 0;
}

EVE_Request_AddGroup(eventGroupInvited) {
	if (!console["ListenGroupRequest"])return 0;
	if (subType == 2 && groupset(fromGroup,"忽略") < 1) {
		if (console) {
			string strNow = printSTNow();
			string strMsg = "群添加请求，来自：" + getStrangerInfo(fromQQ).nick +"("+ to_string(fromQQ) + "),群:" + to_string(fromGroup)+"。";
			if (blacklist->get_group_danger(fromGroup)) {
				strMsg += "\n已拒绝（群在黑名单中）";
				setGroupAddRequest(responseFlag, 2, 2, "");
				console.log(strMsg, 0b10, strNow);
				return 1;
			}
			else if (blacklist->get_qq_danger(fromQQ)) {
				strMsg += "\n已拒绝（用户在黑名单中）";
				setGroupAddRequest(responseFlag, 2, 2, "");
				console.log(strMsg, 0b10, strNow);
				return 1;
			}
			else if (Chat& grp = chat(fromGroup).group(); grp.isset("许可使用")) {
				strMsg += "\n已同意（群已许可使用）";
				grp.inviter = fromQQ;
				setGroupAddRequest(responseFlag, 2, 1, "");
				console.log(strMsg, 1, strNow);
			}
			else if (trustedQQ(fromQQ)) {
				strMsg += "\n已同意（受信任用户）";
				grp.set("许可使用").set("未进");
				grp.inviter = fromQQ;
				setGroupAddRequest(responseFlag, 2, 1, "");
				console.log(strMsg, 1, strNow);
			}
			else if (console["Private"]) {
				strMsg += "\n已拒绝（当前在私用模式）";
				setGroupAddRequest(responseFlag, 2, 2, "");
				sendPrivateMsg(fromQQ, getMsg("strPreserve"));
				console.log(strMsg, 1, strNow);
			}
			else{
				strMsg += "已同意";
				grp.set("未进");
				grp.inviter = fromQQ;
				setGroupAddRequest(responseFlag, 2, 1, "");
				console.log(strMsg, 1, strNow);
			}
		}
		else {
			setGroupAddRequest(responseFlag, 2, 1, "");
		}
		return 1;
	}
	return 0;
}

EVE_Menu(eventMasterMode) {
	if (console) {
		console.isMasterMode = false;
		console.killMaster();
		MessageBoxA(nullptr, "Master模式已关闭√\nmaster已清除", "Master模式切换", MB_OK | MB_ICONINFORMATION);
	}else {
		console.isMasterMode = true;
		MessageBoxA(nullptr, "Master模式已开启√", "Master模式切换", MB_OK | MB_ICONINFORMATION);
	}
	return 0;
}

EVE_Disable(eventDisable)
{
	Enabled = false;
	threads = {};
	dataBackUp();
	PList.clear();
	ChatList.clear();
	UserList.clear();
	console.reset();
	ilInitList.reset();
	EditedMsg.clear(); 
	ObserveGroup.clear();
	ObserveDiscuss.clear();
	blacklist.reset();
	return 0;
}

EVE_Exit(eventExit)
{
	if (!Enabled)
		return 0;
	dataBackUp();
	return 0;
}

MUST_AppInfo_RETURN(CQAPPID);
