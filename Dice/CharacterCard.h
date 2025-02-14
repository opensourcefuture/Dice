#pragma once

/*
 * 玩家人物卡
 * Copyright (C) 2019-2022 String.Empty
 */

#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <mutex>
#include "CQTools.h"
#include "Unpack.h"
#include "RDConstant.h"
#include "RD.h"
#include "DiceXMLTree.h"
#include "DiceFile.hpp"
#include "ManagerSystem.h"
#include "MsgFormat.h"
#include "CardDeck.h"
#include "DiceEvent.h"
#include "DiceAttrVar.h"

using std::string;
using std::to_string;
using std::vector;
using std::map;

constexpr short NOT_FOUND = -32767;

inline map<string, short> mTempletTag = {
	{"name", 1},
	{"type", 2},
	{"alias", 20},
	{"basic", 31},
	{"info", 102},
	{"autofill", 22},
	{"variable", 23},
	{"diceexp", 21},
	{"default", 12},
	{"build", 41},
	{"generate", 24},
	{"note", 101},
};
inline map<string, short> mCardTag = {
	{"Name", 1},
	{"Type", 2},
	{"Attrs", 3},
	{"Attr", 11},	//older
	{"DiceExp", 21},
	{"Note", 101},	//older
	{"Info", 102},	//older
	{"End", 255}
};

//生成模板
class CardBuild
{
public:
	CardBuild() = default;

	CardBuild(const vector<std::pair<string, string>>& attr, const vector<string>& name, const vector<string>& note):
		vBuildList(attr), vNameList(name), vNoteList(note)
	{
	}

	//属性生成
	vector<std::pair<string, string>> vBuildList = {};
	//随机姓名
	vector<string> vNameList = {};
	//Note生成
	vector<string> vNoteList = {};

	CardBuild(const DDOM& d)
	{
		for (const auto& sub : d.vChild)
		{
			switch (mTempletTag[sub.tag])
			{
			case 1:
				vNameList = getLines(sub.strValue);
				break;
			case 24:
				readini(sub.strValue, vBuildList);
				break;
			case 101:
				vNoteList = getLines(sub.strValue);
				break;
			default: break;
			}
		}
	}
};

class CardTemp
{
public:
	string type;
	map<string, string> replaceName = {};
	//作成时生成
	vector<vector<string>> vBasicList = {};
	set<string> sInfoList = {};
	//调用时生成
	map<string, string> mAutoFill = {};
	//动态引用
	map<string, string> mVariable = {};
	//表达式
	map<string, string> mExpression = {};
	//默认值
	map<string, short> defaultSkill = {};
	//生成参数
	map<string, CardBuild> mBuildOption = {};
	CardTemp() = default;

	CardTemp(string type, map<string, string> replace, vector<vector<string>> basic, set<string> info,
	         map<string, string> autofill, map<string, string> dynamic, map<string, string> exp,
	         map<string, short> skill, map<string, CardBuild> option) : type(std::move(type)), 
			                                                            replaceName(std::move(replace)), 
		                                                                vBasicList(std::move(basic)), 
		                                                                sInfoList(std::move(info)), 
		                                                                mAutoFill(std::move(autofill)), 
		                                                                mVariable(std::move(dynamic)), 
		                                                                mExpression(std::move(exp)), 
		                                                                defaultSkill(std::move(skill)), 
		                                                                mBuildOption(std::move(option))
	{
	}

	CardTemp(const DDOM& d)
	{
		readt(d);
	}

	void readt(const DDOM& d);

	string getName()
	{
		return type;
	}

	string showItem()
	{
		const string strItem = listKey(mBuildOption);
		if (strItem.empty())return type;
		return type + "[" + strItem + "]";
	}
	string show();
};

// 由于依赖于其他全局变量，为了避免全局变量初始化顺序冲突，这个变量会在eventEnable中被初始化
inline map<string, CardTemp> mCardTemplet;
CardTemp& getCardTemplet(const string& type);

struct lua_State;
class CharaCard
{
private:
	string Name = "角色卡";
	std::mutex cardMutex;
public:
	const string& getName()const { return Name; }
	void setName(const string&);
	void setType(const string&);
	void update();
	//string Type = "COC7";
	AttrObject Attr{ {
		{"__Type",AttrVar("COC7")},
		{"__Update",AttrVar((long long)time(nullptr))},
	} };
	//map<string, string, less_ci> Info{  };
	//map<string, string, less_ci> DiceExp{};
	string Note;
	CardTemp* pTemplet{ nullptr };

	CharaCard(){
		pTemplet = &getCardTemplet("BRP");
	}
	CharaCard(const CharaCard& pc)
	{
		Name = pc.Name;
		Attr = pc.Attr;
		Note = pc.Note;
		pTemplet = pc.pTemplet;
	}
	CharaCard& operator=(const CharaCard& pc)
	{
		Name = pc.Name;
		Attr = pc.Attr;
		Note = pc.Note;
		pTemplet = pc.pTemplet;
		return *this;
	}

	CharaCard(const string& name, const string& type = "COC7") : Name(name)
	{
		Attr["__Name"] = name;
		setType(type);
	}

	int call(string key)
	{
		if (Attr.has(key))return Attr.get_int(key);
		key = standard(key);
		if (Attr.has(key))return Attr.get_int(key);
		if (pTemplet->mAutoFill.count(key))
		{
			Attr[key] = cal(pTemplet->mAutoFill.find(key)->second);
			return Attr[key].to_int();
		}
		if (pTemplet->mVariable.count(key))
		{
			return cal(pTemplet->mVariable.find(key)->second);
		}
		if (pTemplet->defaultSkill.count(key))return pTemplet->defaultSkill.find(key)->second;
		return 0;
	}

	//表达式转义
	string escape(string exp, const set<string>& sRef)
	{
		if (exp[0] == '&')
		{
			string key = exp.substr(1);
			if (sRef.count(key))return "";
			return getExp(key);
		}
		int intCnt = 0, lp, rp;
		while ((lp = exp.find('[', intCnt)) != std::string::npos && (rp = exp.find(']', lp)) != std::string::npos)
		{
			string strProp = exp.substr(lp + 1, rp - lp - 1);
			if (sRef.count(strProp))return "";
			string val = getExp(strProp);
			exp.replace(exp.begin() + lp, exp.begin() + rp + 1, val);
			intCnt = lp + val.length();
		}
		return exp;
	}

	//求key对应掷骰表达式
	string getExp(string& key, set<string> sRef = {});

	bool countExp(const string& key)
	{
		return (Attr.has(key) && Attr[key].type == AttrVar::AttrType::Text)
			|| (Attr.has("&" + key))
			|| pTemplet->mExpression.count(key);
	}

	//计算表达式
	int cal(string exp)
	{
		if (exp[0] == '&')
		{
			string key = exp.substr(1);
			return call(key);
		}
		int intCnt = 0, lp, rp;
		while ((lp = exp.find('[', intCnt)) != std::string::npos && (rp = exp.find(']', lp)) != std::string::npos)
		{
			string strProp = exp.substr(lp + 1, rp - lp - 1);
			const short val = call(strProp);
			exp.replace(exp.begin() + lp, exp.begin() + rp + 1, std::to_string(val));
			intCnt = lp + std::to_string(val).length();
		}
		const RD Res(exp);
		Res.Roll();
		return Res.intTotal;
	}

	void build(const string& para = "")
	{
		const auto it = pTemplet->mBuildOption.find(para);
		if (it == pTemplet->mBuildOption.end())return;
		CardBuild build = it->second;
		for (auto it2 : build.vBuildList)
		{
			//exp
			if (it2.first[0] == '&')
			{
				if (Attr.has(it2.first))continue;
				Attr[it2.first] = it2.second;
			}
				//info
			else if (pTemplet->sInfoList.count(it2.first))
			{
				if (Attr.has(it2.first))continue;
				Attr[it2.first] = CardDeck::draw(it2.second);
			}
				//attr
			else
			{
				if (Attr.has(it2.first))continue;
				Attr[it2.first] = cal(it2.second);
			}
		}
		while (Note.empty() && !build.vNoteList.empty())
		{
			Note = CardDeck::drawCard(build.vNoteList);
		}
	}

	//解析生成参数
	void buildv(string para = "");

	[[nodiscard]] string standard(const string& key) const
	{
		if (pTemplet->replaceName.count(key))return pTemplet->replaceName.find(key)->second;
		return key;
	}

	int set(string key, const AttrVar& val);

	int set(const string& key, const string& s);

	int setNote(const string& note)
	{
		if (note.length() > 255)return -11;
		Note = note;
		scanImage(note, sReferencedImage);
		return 0;
	}

	bool erase(string& key, bool isExp = false);
	void clear();

	int show(string key, string& val) const;

	[[nodiscard]] string show(bool isWhole) const;

	bool count(const string& key) const;

	bool stored(string& key) const
	{
		key = standard(key);
		return Attr.has(key) || pTemplet->mAutoFill.count(key) || pTemplet->defaultSkill.count(key);
	}

	void cntRollStat(int die, int face);

	void cntRcStat(int die, int rate);

	AttrVar& operator[](const string& key);

	void operator<<(const CharaCard& card)
	{
		const string name = Name;
		*this = card;
		Attr["__Name"] = Name = name;
	}

	void writeb(std::ofstream& fout) const;

	void readb(std::ifstream& fin);

	void toCard(lua_State*);
};

class Player
{
private:
	short indexMax = 0;
	map<unsigned short, CharaCard> mCardList;
	map<string, unsigned short> mNameIndex;
	map<unsigned long long, unsigned short> mGroupIndex{{0, 0}};
	// 人物卡互斥
	std::mutex cardMutex;
public:
	Player() {
		mCardList[0] = { "角色卡" };
	}

	Player(const Player& pl)
	{
		*this = pl;
	}

	Player& operator=(const Player& pl)
	{
		indexMax = pl.indexMax;
		mCardList = pl.mCardList;
		mNameIndex = pl.mNameIndex;
		mGroupIndex = pl.mGroupIndex;
		return *this;
	}

	[[nodiscard]] size_t size() const
	{
		return mCardList.size();
	}

	[[nodiscard]] bool count(long long group) const
	{
		return mGroupIndex.count(group);
	}

	[[nodiscard]] bool count(const string& name) const
	{
		return mNameIndex.count(name);
	}

	int newCard(string& s, long long group = 0)
	{
		std::lock_guard<std::mutex> lock_queue(cardMutex);
		//人物卡数量上限
		if (mCardList.size() > 16)return -1;
		string type = "COC7";
		s = strip(s);
		std::stack<string> vOption;
		int Cnt = s.rfind(':');
		if (Cnt != string::npos)
		{
			type = s.substr(0, Cnt);
			s.erase(s.begin(), s.begin() + Cnt + 1);
			if (type == "COC")type = "COC7";
		}
		else if (mCardTemplet.count(s))
		{
			type = s;
			s.clear();
		}
		while ((Cnt = type.rfind(':')) != string::npos)
		{
			vOption.push(type.substr(Cnt + 1));
			type.erase(type.begin() + Cnt, type.end());
		}
		//无效模板不再报错
		//if (!getmCardTemplet().count(type))return -2;
		if (mNameIndex.count(s))return -4;
		if (s.find("=") != string::npos)return -6;
		mCardList.emplace(++indexMax, CharaCard{ s, type });
		CharaCard& card = mCardList[indexMax];
		// CardTemp& temp = mCardTemplet[type];
		while (!vOption.empty())
		{
			string para = vOption.top();
			vOption.pop();
			card.build(para);
			if (card.getName().empty())
			{
				std::vector<string> list = getCardTemplet(type).mBuildOption[para].vNameList;
				while (!list.empty())
				{
					s = CardDeck::draw(list[0]);
					if (mNameIndex.count(s))list.erase(list.begin());
					else
					{
						card.setName(s);
						break;
					}
				}
			}
		}
		if (card.getName().empty())
		{
			std::vector<string> list = getCardTemplet(type).mBuildOption["_default"].vNameList;
			while (!list.empty())
			{
				s = CardDeck::draw(list[0]);
				if (mNameIndex.count(s))list.erase(list.begin());
				else
				{
					card.setName(s);
					break;
				}
			}
			if (card.getName().empty())card.setName(to_string(indexMax + 1));
		}
		s = card.getName();
		mNameIndex[s] = indexMax;
		mGroupIndex[group] = indexMax;
		return 0;
	}

	int buildCard(string& name, bool isClear, long long group = 0)
	{
		string strName = name;
		string strType;
		if (name.find(":") != string::npos)
		{
			strName = strip(name.substr(name.rfind(":") + 1));
			strType = name.substr(0, name.rfind(":"));
		}
		//不存在则新建人物卡
		if (!strName.empty() && !mNameIndex.count(strName))
		{
			if (const int res = newCard(name, group))return res;
			name = getCard(strName, group).getName();
			(*this)[name].buildv();
		}
		else
		{
			name = getCard(strName, group).getName();
			if (isClear)(*this)[name].clear();
			(*this)[name].buildv(strType);
		}
		return 0;
	}

	int changeCard(const string& name, long long group)
	{
		if (name.empty())
		{
			mGroupIndex.erase(group);
			return 1;
		}
		if (!mNameIndex.count(name))return -5;
		mGroupIndex[group] = mNameIndex[name];
		return 0;
	}

	int removeCard(const string& name)
	{
		std::lock_guard<std::mutex> lock_queue(cardMutex);
		if (!mNameIndex.count(name))return -5;
		if (!mNameIndex[name])return -7;
		auto it = mGroupIndex.cbegin();
		while (it != mGroupIndex.cend())
		{
			if (it->second == mNameIndex[name])
			{
				it = mGroupIndex.erase(it);
			}
			else
			{
				++it;
			}
		}
		mCardList.erase(mNameIndex[name]);
		while (!mCardList.count(indexMax))indexMax--;
		mNameIndex.erase(name);
		return 0;
	}

	int renameCard(const string& name, const string& name_new);

	int copyCard(const string& name1, const string& name2, long long group = 0)
	{
		if (name1.empty() || name2.empty())return -3;
		//不存在则新建人物卡
		if (!mNameIndex.count(name1))
		{
			std::lock_guard<std::mutex> lock_queue(cardMutex);
			//人物卡数量上限
			if (mCardList.size() > 16)return -1;
			if (name1.find(":") != string::npos)return -6;
			mCardList[++indexMax].setName(name1);
			mNameIndex[name1] = indexMax;
		}
		(*this)[name1] << (*this)[name2];
		return 0;
	}

	string listCard();

	string listMap()
	{
		ResList Res;
		for (const auto& it : mGroupIndex)
		{
			if (!it.first)Res << "default:" + mCardList[it.second].getName();
			else Res << "(" + to_string(it.first) + ")" + mCardList[it.second].getName();
		}
		return Res.show();
	}

	CharaCard& getCard(const string& name, long long group = 0);

	CharaCard& operator[](long long id)
	{
		if (mGroupIndex.count(id))return mCardList[mGroupIndex[id]];
		if (mGroupIndex.count(0))return mCardList[mGroupIndex[0]];
		return mCardList[0];
	}

	CharaCard& operator[](const string& name)
	{
		if (mNameIndex.count(name))return mCardList[mNameIndex[name]];
		if (mGroupIndex.count(0))return mCardList[mGroupIndex[0]];
		return mCardList[0];
	}

	bool load(std::ifstream& fin)
	{
		string s;
		if (!(fin >> s))return false;
		Unpack pack(base64_decode(s));
		indexMax = pack.getInt();
		Unpack cards = pack.getUnpack();
		while (cards.len() > 0)
		{
			Unpack card = cards.getUnpack();
			const int nIndex = card.getInt();
			string name = card.getstring();
			const string type = card.getstring();
			mCardList[nIndex] = CharaCard(name, type);
			mNameIndex[name] = nIndex;
			Unpack skills = card.getUnpack();
			string skillname;
			while (skills.len() > 0)
			{
				skillname = skills.getstring();
				mCardList[nIndex][skillname] = skills.getInt();
			}
			mCardList[nIndex].Note = card.getstring();
		}
		Unpack groups = pack.getUnpack();
		while (groups.len() > 0)
		{
			mGroupIndex[groups.getLong()] = groups.getInt();
		}
		return true;
	}

	void save(std::ofstream& fout)
	{
		Unpack pack;
		pack.add(indexMax);
		Unpack cards;
		for (const auto& it : mCardList)
		{
			Unpack card;
			card.add(it.first);
			card.add(it.second.getName());
			Unpack skills;
			for (const auto& skill : *it.second.Attr)
			{
				skills.add(skill.first);
				skills.add(skill.second.to_str());	//need fix
			}
			card.add(skills);
			card.add(it.second.Note);
			cards.add(card);
		}
		pack.add(cards);
		Unpack groups;
		for (const auto& it : mGroupIndex)
		{
			groups.add(static_cast<long long>(it.first));
			groups.add(it.second);
		}
		pack.add(groups);
		fout << base64_encode(pack.getAll());
	}

	void writeb(std::ofstream& fout) const
	{
		fwrite(fout, indexMax);
		fwrite(fout, mCardList);
		fwrite(fout, mGroupIndex);
	}

	void readb(std::ifstream& fin);
};

inline map<long long, Player> PList;

Player& getPlayer(long long qq);

AttrVar idx_pc(AttrObject&);
