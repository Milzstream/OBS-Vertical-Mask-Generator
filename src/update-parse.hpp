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

inline void mask_json_unescape_solidus(std::string &s)
{
	size_t w = 0;
	for (size_t r = 0; r < s.size(); r++) {
		if (s[r] == '\\' && r + 1 < s.size() && s[r + 1] == '/') {
			s[w++] = '/';
			r++;
		} else {
			s[w++] = s[r];
		}
	}
	s.resize(w);
}

inline bool mask_https_github_host(const std::string &url)
{
	const char *pfx = "https://";
	const size_t pfx_len = 8;
	if (url.size() <= pfx_len || url.compare(0, pfx_len, pfx) != 0)
		return false;

	size_t start = pfx_len;
	const size_t slash = url.find('/', start);
	const size_t at = url.find('@', start);
	if (at != std::string::npos && (slash == std::string::npos || at < slash))
		start = at + 1;

	size_t end = url.find_first_of("/?#:", start);
	if (end == std::string::npos)
		end = url.size();
	if (end <= start)
		return false;

	std::string host = url.substr(start, end - start);
	for (char &c : host) {
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	}
	if (host == "github.com" || host == "www.github.com")
		return true;
	const char *suffix = ".githubusercontent.com";
	const size_t suffix_len = std::strlen(suffix);
	return host.size() > suffix_len && host.compare(host.size() - suffix_len, suffix_len, suffix) == 0;
}

inline bool mask_normalize_https_url(std::string &url)
{
	if (url.empty())
		return false;

	mask_json_unescape_solidus(url);

	size_t a = 0;
	size_t b = url.size();
	while (a < b && (url[a] == ' ' || url[a] == '\t' || url[a] == '\r' || url[a] == '\n'))
		a++;
	while (b > a && (url[b - 1] == ' ' || url[b - 1] == '\t' || url[b - 1] == '\r' || url[b - 1] == '\n'))
		b--;
	url = url.substr(a, b - a);
	if (url.empty())
		return false;

	if (url.size() >= 7 && (url.compare(0, 7, "http://") == 0 || url.compare(0, 7, "HTTP://") == 0))
		url.replace(0, 7, "https://");
	else if (url.compare(0, 2, "//") == 0)
		url.insert(0, "https:");
	else if (url.compare(0, 8, "https://") != 0 && url.compare(0, 8, "HTTPS://") != 0)
		url.insert(0, "https://");

	if (url.compare(0, 8, "HTTPS://") == 0)
		url.replace(0, 8, "https://");

	if (!mask_https_github_host(url)) {
		url.clear();
		return false;
	}
	return true;
}
