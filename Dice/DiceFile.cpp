/*
 * 文件读写
 * Copyright (C) 2019-2020 String.Empty
 */
#include <fstream>
#include "filesystem.hpp"
#include "DiceMsgSend.h"
#include "ManagerSystem.h"
#include "StrExtern.hpp"
#include "DiceFile.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

int mkDir(const std::string& dir)
{
	std::error_code err;
	std::filesystem::create_directories(dir, err);
	return err.value();
	/*if (_access(dir.c_str(), 0))	return _mkdir(dir.c_str());
	return -2;*/
}

int clrDir(const std::string& dir, const std::unordered_set<std::string>& exceptList)
{
	int nCnt = 0;
	std::error_code err;
	for (const auto& p : std::filesystem::directory_iterator(dir, err))
	{
		if (std::filesystem::is_regular_file(p.status()))
		{
			std::string path = convert_w2a(p.path().filename().u16string().c_str());
			if (path.length() >= 36 && !exceptList.count(path))
			{
				std::error_code err2;
				std::filesystem::remove(p.path(), err2);
				if (!err2) nCnt++;
			}
		}
	}
	return (err ? err.value() : nCnt);

	/* int nCnt = 0;
	_finddata_t file;
	long lf = _findfirst((dir + "*").c_str(), &file);
	//输入文件夹路径
	if (lf < 0) {
		return -1;
	}
	else {
		do {
			if (file.attrib == _A_SUBDIR)continue;
			if (!strcmp(file.name, ".") || !strcmp(file.name, ".."))continue;
			if (strlen(file.name) < 36)continue;
			if (exceptList.count(file.name))continue;
			if (!remove((dir + file.name).c_str()))nCnt++;
		} while (!_findnext(lf, &file));
	}
	_findclose(lf);
	return nCnt;*/
}

string printLine(string s)
{
	while (s.find('\t') != std::string::npos)s.replace(s.find('\t'), 1, "\\t");
	while (s.find("\r\n") != std::string::npos)s.replace(s.find("\r\n"), 2, "\\n");
	while (s.find('\r') != std::string::npos)s.replace(s.find('\r'), 1, "\\n");
	while (s.find('\n') != std::string::npos)s.replace(s.find('\n'), 1, "\\n");
	return s;
}

bool fscan(std::ifstream& fin, std::string& t)
{
	std::string val;
	if (fin >> val)
	{
		while (val.find("{space}") != std::string::npos)val.replace(val.find("{space}"), 7, " ");
		while (val.find("{tab}") != std::string::npos)val.replace(val.find("{tab}"), 5, "\t");
		while (val.find("{endl}") != std::string::npos)val.replace(val.find("{endl}"), 6, "\n");
		while (val.find("{enter}") != std::string::npos)val.replace(val.find("{enter}"), 7, "\r");
		while (val.find("\\n") != std::string::npos)val.replace(val.find("\\n"), 2, "\n");
		while (val.find("\\s") != std::string::npos)val.replace(val.find("\\s"), 2, " ");
		while (val.find("\\t") != std::string::npos)val.replace(val.find("\\t"), 2, "	");
		t = val;
		return true;
	}
	return false;
}

[[deprecated]] bool rdbuf(const string& strPath, string& s)
{
	const std::ifstream fin(strPath);
	if (!fin)return false;
	stringstream ss;
	ss << fin.rdbuf();
	s = ss.str();
	return true;
}

bool rdbuf(const std::filesystem::path& fpPath, string& s)
{
	const std::ifstream fin(fpPath);
	if (!fin)return false;
	stringstream ss;
	ss << fin.rdbuf();
	s = ss.str();
	return true;
}

void fprint(std::ofstream& fout, std::string s)
{
	while (s.find(' ') != std::string::npos)s.replace(s.find(' '), 1, "{space}");
	while (s.find('\t') != std::string::npos)s.replace(s.find('\t'), 1, "{tab}");
	while (s.find('\n') != std::string::npos)s.replace(s.find('\n'), 1, "{endl}");
	while (s.find('\r') != std::string::npos)s.replace(s.find('\r'), 1, "{enter}");
	fout << s;
}

void fwrite(ofstream& fout, const std::string& s)
{
	auto len = static_cast<short>(s.length());
	fout.write(reinterpret_cast<char*>(&len), sizeof(short));
	fout.write(s.c_str(), sizeof(char) * s.length());
}

void readini(ifstream& fin, std::string& s)
{
	std::string line;
	getline(fin, line);
	const int pos = line.find('=');
	if (pos == std::string::npos)return;
	std::istringstream sin(line.substr(pos + 1));
	sin >> s;
}


using namespace std;

std::ifstream& operator>>(std::ifstream& fin, msgtype& t)
{
	fin >> reinterpret_cast<int&>(t);
	return fin;
}

std::ifstream& operator>>(std::ifstream& fin, chatInfo& ct)
{
	long long id;
	int t;
	fin >> id >> t;
	ct.type = static_cast<msgtype>(t);
	if (ct.type == msgtype::Private || ct.type == msgtype::ChannelPrivate)ct.uid = id;
	else ct.gid = id;
	return fin;
}

ifstream& operator>>(ifstream& fin, User& user)
{
	fin >> user.ID >> user.nTrust >> user.tCreated >> user.tUpdated;
	return fin;
}

ofstream& operator<<(ofstream& fout, const User& user)
{
	fout << user.ID << '\t' << user.nTrust << '\t' << user.tCreated << '\t' << user.tUpdated;
	return fout;
}

ifstream& operator>>(ifstream& fin, Chat& grp)
{
	fin >> grp.ID >> grp.isGroup >> grp.inviter >> grp.tCreated >> grp.tUpdated >> grp.tLastMsg;
	return fin;
}

ofstream& operator<<(ofstream& fout, const Chat& grp)
{
	fout << grp.ID << '\t' << grp.isGroup << '\t' << grp.inviter << '\t' << grp.tCreated << '\t' << grp.tUpdated << '\t'
		<< grp.tLastMsg;
	return fout;
}

template <typename T>
[[deprecated]] int _listDir(const string& dir, vector<std::filesystem::path>& files)
{
	int intFile = 0;
	std::error_code err;
	for (const auto& file : T(dir, err))
	{
		if (std::filesystem::is_regular_file(file.status()))
		{
			intFile++;
			files.push_back(file.path());
		}
	}
	return err ? -1 : intFile;
}

template <typename T>
int _listDir(const std::filesystem::path& dir, vector<std::filesystem::path>& files)
{
	int intFile = 0;
	std::error_code err;
	for (const auto& file : T(dir, err))
	{
		if (std::filesystem::is_regular_file(file.status()) && file.path().filename().u8string().substr(0, 1) != ".")
		{
			intFile++;
			files.push_back(file.path());
		}
	}
	return err ? -1 : intFile;
}

[[deprecated]] int listDir(const string& dir, vector<std::filesystem::path>& files, bool isSub)
{
	if (isSub)
	{
		return _listDir<std::filesystem::recursive_directory_iterator>(dir, files);
	}
	return _listDir<std::filesystem::directory_iterator>(dir, files);
}

int listDir(const std::filesystem::path& dir, vector<std::filesystem::path>& files, bool isSub)
{
	if (!std::filesystem::exists(dir))return 0;
	if (isSub)
	{
		return _listDir<std::filesystem::recursive_directory_iterator>(dir, files);
	}
	return _listDir<std::filesystem::directory_iterator>(dir, files);
}

// 获取本地编码格式的文件名
// 在Windows上不能直接调用string(), 因为文件名可能无法转换为本地ANSI编码，导致异常抛出
// 使用8.3文件名绕过此问题
// 在Windows上，如果文件不存在则无法正常获取短路径，会尝试返回ANSI编码的长路径
std::string getNativePathString(const std::filesystem::path& fpPath)
{
#ifdef _WIN32
	auto size = GetShortPathNameW(reinterpret_cast<const wchar_t*>(fpPath.u16string().c_str()), nullptr, 0);
	if (size == 0) 
	{
		try 
		{
			return fpPath.string();
		}
		catch (...)
		{
			return "";
		}
	}
	wchar_t* buf = new wchar_t[size + 1];
	GetShortPathNameW(reinterpret_cast<const wchar_t*>(fpPath.u16string().c_str()), buf, size + 1);
	std::string ret(convert_w2a(reinterpret_cast<char16_t*>(buf)));
	delete[] buf;
	return ret;
#else
	return fpPath.string();
#endif
}