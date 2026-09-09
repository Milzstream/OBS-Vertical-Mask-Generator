/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "update-check.hpp"
#include "update-parse.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QObject>
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

struct ReleaseInfo {
	std::string tag;
	std::string page_url;
	std::string download_url;
	bool prerelease = false;
};

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
		obs_log(LOG_WARNING, "update check failed (HTTP %lu)", (unsigned long)status);
		return false;
	}

	if (!mask_json_string_field(body, "tag_name", info.tag)) {
		obs_log(LOG_WARNING, "update check: no tag_name in GitHub response");
		return false;
	}
	mask_json_string_field(body, "html_url", info.page_url);
	mask_json_bool_field(body, "prerelease", info.prerelease);
	mask_find_windows_asset(body, info.download_url);
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
	} else {
		obs_data_t *state = load_state();
		obs_data_set_string(state, "later_tag", info.tag.c_str());
		obs_data_set_int(state, "later_until", static_cast<int64_t>(time(nullptr)) + k_check_interval_sec);
		save_state(state);
		obs_data_release(state);
	}
}

void update_check_worker()
{
	if (g_cancel.load())
		return;

	obs_log(LOG_INFO, "checking for updates (installed %s)", PLUGIN_VERSION);

	ReleaseInfo info;
	if (!fetch_latest(info) || g_cancel.load())
		return;

	obs_data_t *state = load_state();
	obs_data_set_int(state, "last_check", static_cast<int64_t>(time(nullptr)));
	obs_data_set_string(state, "last_tag", info.tag.c_str());
	save_state(state);

	const char *skipped = obs_data_get_string(state, "skip_version");
	const char *later_tag = obs_data_get_string(state, "later_tag");
	const int64_t later_until = obs_data_get_int(state, "later_until");
	const int64_t now = static_cast<int64_t>(time(nullptr));
	obs_data_release(state);

	if (info.prerelease) {
		obs_log(LOG_INFO, "update check: latest %s is a prerelease, skipping", info.tag.c_str());
		return;
	}
	if (skipped && info.tag == skipped) {
		obs_log(LOG_INFO, "update check: skipped %s", info.tag.c_str());
		return;
	}
	if (later_tag && info.tag == later_tag && later_until > now) {
		obs_log(LOG_INFO, "update check: snoozed %s", info.tag.c_str());
		return;
	}

	MaskVer current{};
	MaskVer latest{};
	if (!mask_parse_ver(PLUGIN_VERSION, current) || !mask_parse_ver(info.tag.c_str(), latest)) {
		obs_log(LOG_WARNING, "update check: could not parse versions (%s vs %s)", PLUGIN_VERSION,
			info.tag.c_str());
		return;
	}
	if (mask_cmp_ver(latest, current) <= 0) {
		obs_log(LOG_INFO, "update check: up to date (%s)", PLUGIN_VERSION);
		return;
	}

	obs_log(LOG_INFO, "update available: %s (installed %s)", info.tag.c_str(), PLUGIN_VERSION);

	QWidget *main = static_cast<QWidget *>(obs_frontend_get_main_window());
	QObject *ctx = main ? static_cast<QObject *>(main) : static_cast<QObject *>(QCoreApplication::instance());
	if (!ctx || g_cancel.load())
		return;

	const bool queued = QMetaObject::invokeMethod(
		ctx,
		[info]() {
			show_update_dialog(info);
		},
		Qt::QueuedConnection);
	if (!queued)
		obs_log(LOG_WARNING, "update check: could not show the update dialog");
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
