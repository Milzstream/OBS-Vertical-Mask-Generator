#pragma once

#include <cstdio>
#include <cstring>
#include <string>

struct MaskVer {
	int maj = 0;
	int min = 0;
	int pat = 0;
};

inline bool mask_parse_ver(const char *s, MaskVer &v)
{
	if (!s || !*s)
		return false;
	while (*s == 'v' || *s == 'V')
		s++;
	v = {};
	const int n = sscanf(s, "%d.%d.%d", &v.maj, &v.min, &v.pat);
	return n >= 2;
}

inline int mask_cmp_ver(MaskVer a, MaskVer b)
{
	if (a.maj != b.maj)
		return a.maj < b.maj ? -1 : 1;
	if (a.min != b.min)
		return a.min < b.min ? -1 : 1;
	if (a.pat != b.pat)
		return a.pat < b.pat ? -1 : 1;
	return 0;
}

inline bool mask_json_skip_ws(const std::string &json, size_t &pos)
{
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
		pos++;
	return pos < json.size();
}

inline bool mask_json_string_field(const std::string &json, const char *key, std::string &out)
{
	const std::string pat = std::string("\"") + key + "\":";
	size_t pos = json.find(pat);
	if (pos == std::string::npos)
		return false;
	pos += pat.size();
	if (!mask_json_skip_ws(json, pos) || json[pos] != '"')
		return false;
	pos++;
	const size_t end = json.find('"', pos);
	if (end == std::string::npos)
		return false;
	out = json.substr(pos, end - pos);
	return !out.empty();
}

inline bool mask_json_bool_field(const std::string &json, const char *key, bool &out)
{
	const std::string pat = std::string("\"") + key + "\":";
	size_t pos = json.find(pat);
	if (pos == std::string::npos)
		return false;
	pos += pat.size();
	if (!mask_json_skip_ws(json, pos))
		return false;
	if (json.compare(pos, 4, "true") == 0) {
		out = true;
		return true;
	}
	if (json.compare(pos, 5, "false") == 0) {
		out = false;
		return true;
	}
	return false;
}

inline void mask_find_windows_asset(const std::string &json, std::string &url)
{
	std::string exe;
	std::string zip;
	const char *key = "\"browser_download_url\":";
	size_t pos = 0;
	while (true) {
		pos = json.find(key, pos);
		if (pos == std::string::npos)
			break;
		pos += std::strlen(key);
		if (!mask_json_skip_ws(json, pos) || json[pos] != '"')
			break;
		pos++;
		const size_t end = json.find('"', pos);
		if (end == std::string::npos)
			break;
		const std::string u = json.substr(pos, end - pos);
		pos = end + 1;
		if (u.find("windows") == std::string::npos)
			continue;
		if (u.size() >= 4 && u.compare(u.size() - 4, 4, ".exe") == 0)
			exe = u;
		else if (zip.empty() && u.size() >= 4 && u.compare(u.size() - 4, 4, ".zip") == 0)
			zip = u;
	}
	url = !exe.empty() ? exe : zip;
}
