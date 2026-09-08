/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "update-check.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QWidget>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>

#ifndef HUD_MASK_GH_REPO
#define HUD_MASK_GH_REPO "Milzstream/OBS-Vertical-Mask-Generator"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace {

constexpr int64_t k_check_interval_sec = 24 * 60 * 60;

std::atomic<bool> g_cancel{false};
std::thread g_worker;
bool g_callback_added = false;

struct Ver {
	int maj = 0;
	int min = 0;
	int pat = 0;
};

struct ReleaseInfo {
	std::string tag;
	std::string page_url;
	std::string download_url;
	bool prerelease = false;
};

bool parse_ver(const char *s, Ver &v)
{
	if (!s || !*s)
		return false;
	while (*s == 'v' || *s == 'V')
		s++;
	v = {};
	const int n = sscanf(s, "%d.%d.%d", &v.maj, &v.min, &v.pat);
	return n >= 2;
}

int cmp_ver(Ver a, Ver b)
{
	if (a.maj != b.maj)
		return a.maj < b.maj ? -1 : 1;
	if (a.min != b.min)
		return a.min < b.min ? -1 : 1;
	if (a.pat != b.pat)
		return a.pat < b.pat ? -1 : 1;
	return 0;
}

bool json_skip_ws(const std::string &json, size_t &pos)
{
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
		pos++;
	return pos < json.size();
}

bool json_string_field(const std::string &json, const char *key, std::string &out)
{
	const std::string pat = std::string("\"") + key + "\":";
	size_t pos = json.find(pat);
	if (pos == std::string::npos)
		return false;
	pos += pat.size();
	if (!json_skip_ws(json, pos) || json[pos] != '"')
		return false;
	pos++;
	const size_t end = json.find('"', pos);
	if (end == std::string::npos)
		return false;
	out = json.substr(pos, end - pos);
	return !out.empty();
}

bool json_bool_field(const std::string &json, const char *key, bool &out)
{
	const std::string pat = std::string("\"") + key + "\":";
	size_t pos = json.find(pat);
	if (pos == std::string::npos)
		return false;
	pos += pat.size();
	if (!json_skip_ws(json, pos))
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

void find_windows_asset(const std::string &json, std::string &url)
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
		if (!json_skip_ws(json, pos) || json[pos] != '"')
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

char *config_file_path(void)
{
	return obs_module_get_config_path(obs_current_module(), "update.json");
}

obs_data_t *load_state(void)
{
	char *path = config_file_path();
	obs_data_t *data = path ? obs_data_create_from_json_file(path) : nullptr;
	bfree(path);
	if (!data)
		data = obs_data_create();
	return data;
}

void save_state(obs_data_t *data)
{
	char *dir = obs_module_get_config_path(obs_current_module(), "");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	char *path = config_file_path();
	if (path) {
		obs_data_save_json(data, path);
		bfree(path);
	}
}

#ifdef _WIN32

std::wstring ascii_wide(const char *s)
{
	std::wstring out;
	if (!s)
		return out;
	out.reserve(std::strlen(s));
	for (const char *p = s; *p; p++)
		out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
	return out;
}

bool http_get_https(const wchar_t *host, const wchar_t *path, std::string &body, DWORD &status)
{
	status = 0;
	body.clear();

	std::wstring agent = L"vertical-hud-mask/";
	agent += ascii_wide(PLUGIN_VERSION);

	HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
					WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		return false;

	WinHttpSetTimeouts(session, 4000, 4000, 4000, 8000);

	HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!connect) {
		WinHttpCloseHandle(session);
		return false;
	}

	HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
					       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!request) {
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	DWORD decomp = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
	WinHttpSetOption(request, WINHTTP_OPTION_DECOMPRESSION, &decomp, sizeof(decomp));

	WinHttpAddRequestHeaders(request,
				 L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28", (ULONG)-1L,
				 WINHTTP_ADDREQ_FLAG_ADD);

	bool ok = false;
	if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
	    WinHttpReceiveResponse(request, nullptr)) {
		DWORD statusSize = sizeof(status);
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

		char buf[4096];
		DWORD got = 0;
		while (WinHttpReadData(request, buf, sizeof(buf), &got) && got > 0) {
			body.append(buf, got);
			if (body.size() > 1024 * 1024)
				break;
			if (g_cancel.load())
				break;
		}
		ok = status == 200 && !body.empty() && !g_cancel.load();
	}

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);
	return ok;
}

bool fetch_latest(ReleaseInfo &info)
{
	const std::wstring path = L"/repos/" + ascii_wide(HUD_MASK_GH_REPO) + L"/releases/latest";
	std::string body;
	DWORD status = 0;
	if (!http_get_https(L"api.github.com", path.c_str(), body, status)) {
		obs_log(LOG_DEBUG, "update check failed (HTTP %lu)", (unsigned long)status);
		return false;
	}

	if (!json_string_field(body, "tag_name", info.tag))
		return false;
	json_string_field(body, "html_url", info.page_url);
	json_bool_field(body, "prerelease", info.prerelease);
	find_windows_asset(body, info.download_url);
	return true;
}

#else

bool fetch_latest(ReleaseInfo &)
{
	return false;
}

#endif

void show_update_dialog(const ReleaseInfo info)
{
	if (g_cancel.load())
		return;

	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	QMessageBox box(parent);
	box.setIcon(QMessageBox::Information);
	box.setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.Update.Title")));
	box.setText(QString::fromUtf8(obs_module_text("HUDMask.Update.Text"))
			    .arg(QString::fromStdString(info.tag))
			    .arg(QString::fromUtf8(PLUGIN_VERSION)));
	box.setInformativeText(QString::fromUtf8(obs_module_text("HUDMask.Update.Info")));

	QPushButton *download =
		box.addButton(QString::fromUtf8(obs_module_text("HUDMask.Update.Download")), QMessageBox::AcceptRole);
	QPushButton *skip =
		box.addButton(QString::fromUtf8(obs_module_text("HUDMask.Update.Skip")), QMessageBox::DestructiveRole);
	box.addButton(QString::fromUtf8(obs_module_text("HUDMask.Update.Later")), QMessageBox::RejectRole);
	box.setDefaultButton(download);
	box.exec();

	if (box.clickedButton() == download) {
		const std::string &url = !info.download_url.empty() ? info.download_url : info.page_url;
		if (!url.empty())
			QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
	} else if (box.clickedButton() == skip) {
		obs_data_t *state = load_state();
		obs_data_set_string(state, "skip_version", info.tag.c_str());
		save_state(state);
		obs_data_release(state);
	}
}

void update_check_worker()
{
	if (g_cancel.load())
		return;

	obs_data_t *state = load_state();
	const int64_t now = static_cast<int64_t>(time(nullptr));
	const int64_t last = obs_data_get_int(state, "last_check");
	if (last > 0 && now - last < k_check_interval_sec) {
		obs_data_release(state);
		return;
	}

	ReleaseInfo info;
	if (!fetch_latest(info) || g_cancel.load()) {
		obs_data_release(state);
		return;
	}

	obs_data_set_int(state, "last_check", now);
	save_state(state);

	const char *skipped = obs_data_get_string(state, "skip_version");
	obs_data_release(state);

	if (info.prerelease)
		return;
	if (skipped && info.tag == skipped)
		return;

	Ver current{};
	Ver latest{};
	if (!parse_ver(PLUGIN_VERSION, current) || !parse_ver(info.tag.c_str(), latest))
		return;
	if (cmp_ver(latest, current) <= 0)
		return;

	obs_log(LOG_INFO, "update available: %s (installed %s)", info.tag.c_str(), PLUGIN_VERSION);

	QCoreApplication *app = QCoreApplication::instance();
	if (!app || g_cancel.load())
		return;

	QMetaObject::invokeMethod(
		app,
		[info]() {
			show_update_dialog(info);
		},
		Qt::QueuedConnection);
}

void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		if (g_worker.joinable())
			return;
		g_worker = std::thread(update_check_worker);
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		g_cancel.store(true);
	}
}

} // namespace

void hud_mask_update_check_start(void)
{
	g_cancel.store(false);
	if (getenv("HUD_MASK_SKIP_UPDATE"))
		return;
	if (g_callback_added)
		return;
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	g_callback_added = true;
}

void hud_mask_update_check_stop(void)
{
	g_cancel.store(true);
	if (g_callback_added) {
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
		g_callback_added = false;
	}
	if (g_worker.joinable())
		g_worker.join();
}
