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
#include <QEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QThread>
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
#include <shellapi.h>
#include <winhttp.h>
#endif

namespace {

constexpr int64_t k_check_interval_sec = 24 * 60 * 60;

std::atomic<bool> g_cancel{false};
std::thread g_worker;
std::thread g_download;
bool g_callback_added = false;
QPointer<QMessageBox> g_download_box;

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
	if (!mask_normalize_https_url(info.page_url))
		info.page_url.clear();
	if (!mask_normalize_https_url(info.download_url))
		info.download_url.clear();
	if (info.page_url.empty() && !info.tag.empty()) {
		info.page_url = std::string("https://github.com/") + HUD_MASK_GH_REPO + "/releases/tag/" + info.tag;
		if (!mask_normalize_https_url(info.page_url))
			info.page_url.clear();
	}
	return true;
}

#else

bool fetch_latest(ReleaseInfo &)
{
	return false;
}

#endif

void open_https(std::string url)
{
	if (!mask_normalize_https_url(url)) {
		obs_log(LOG_WARNING, "update check: refused non-HTTPS download url");
		return;
	}
	const QUrl qurl = QUrl::fromEncoded(QByteArray(url.data(), static_cast<int>(url.size())));
	if (!qurl.isValid() || qurl.scheme() != "https") {
		obs_log(LOG_WARNING, "update check: invalid HTTPS url");
		return;
	}
	QDesktopServices::openUrl(qurl);
}

void close_obs()
{
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (parent)
		QMetaObject::invokeMethod(parent, "close", Qt::QueuedConnection);
}

void show_update_failed(const std::string &page_url)
{
	if (g_download_box) {
		g_download_box->close();
		g_download_box = nullptr;
	}
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	QMessageBox fail(parent);
	fail.setIcon(QMessageBox::Warning);
	fail.setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.Update.Title")));
	fail.setText(QString::fromUtf8(obs_module_text("HUDMask.Update.Failed")));
	QPushButton *open =
		fail.addButton(QString::fromUtf8(obs_module_text("HUDMask.Update.Download")), QMessageBox::AcceptRole);
	fail.addButton(QMessageBox::Close);
	fail.exec();
	if (fail.clickedButton() == open)
		open_https(page_url);
}

#ifdef _WIN32

bool url_is_exe(const std::string &url)
{
	const size_t cut = url.find_first_of("?#");
	const size_t end = cut == std::string::npos ? url.size() : cut;
	if (end < 4)
		return false;
	const char *ext = url.c_str() + (end - 4);
	return ext[0] == '.' && (ext[1] == 'e' || ext[1] == 'E') && (ext[2] == 'x' || ext[2] == 'X') &&
	       (ext[3] == 'e' || ext[3] == 'E');
}

std::string narrow_ascii(const std::wstring &wide)
{
	std::string out;
	out.reserve(wide.size());
	for (wchar_t c : wide) {
		if (c < 32 || c > 126)
			return {};
		out.push_back(static_cast<char>(c));
	}
	return out;
}

bool crack_https(const std::wstring &url, std::wstring &host, std::wstring &object, INTERNET_PORT &port)
{
	URL_COMPONENTS parts{};
	parts.dwStructSize = sizeof(parts);
	wchar_t host_buf[256];
	wchar_t path_buf[4096];
	wchar_t extra_buf[4096];
	parts.lpszHostName = host_buf;
	parts.dwHostNameLength = 255;
	parts.lpszUrlPath = path_buf;
	parts.dwUrlPathLength = 4095;
	parts.lpszExtraInfo = extra_buf;
	parts.dwExtraInfoLength = 4095;
	if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts))
		return false;
	if (parts.nScheme != INTERNET_SCHEME_HTTPS)
		return false;
	host.assign(parts.lpszHostName, parts.dwHostNameLength);
	object.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
	if (parts.dwExtraInfoLength > 0)
		object.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
	port = parts.nPort;
	return !host.empty() && !object.empty();
}

bool next_redirect(const std::string &current, const std::wstring &location, std::string &out)
{
	const std::string loc = narrow_ascii(location);
	if (loc.empty())
		return false;
	if (loc.compare(0, 8, "https://") == 0 || loc.compare(0, 7, "http://") == 0)
		out = loc;
	else if (loc.compare(0, 2, "//") == 0)
		out = "https:" + loc;
	else if (loc[0] == '/') {
		const size_t scheme = current.find("://");
		if (scheme == std::string::npos)
			return false;
		const size_t host_end = current.find('/', scheme + 3);
		if (host_end == std::string::npos)
			return false;
		out = current.substr(0, host_end) + loc;
	} else {
		return false;
	}
	return mask_normalize_https_url(out);
}

std::wstring installer_dest_path()
{
	wchar_t tmp[MAX_PATH];
	const DWORD n = GetTempPathW(MAX_PATH, tmp);
	if (n == 0 || n >= MAX_PATH)
		return {};
	std::wstring path(tmp, n);
	if (path.back() != L'\\')
		path.push_back(L'\\');
	path += L"vertical-hud-mask-update.exe";
	if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(path.c_str()))
		path += L"." + std::to_wstring(GetTickCount());
	return path;
}

void close_http(HINTERNET request, HINTERNET connect, HINTERNET session)
{
	if (request)
		WinHttpCloseHandle(request);
	if (connect)
		WinHttpCloseHandle(connect);
	if (session)
		WinHttpCloseHandle(session);
}

bool download_installer(const std::string &start_url, std::wstring &dest)
{
	dest = installer_dest_path();
	if (dest.empty())
		return false;
	const std::wstring part = dest + L".part";
	std::string url = start_url;
	if (!mask_normalize_https_url(url) || !url_is_exe(url))
		return false;

	for (int hop = 0; hop < 5; hop++) {
		if (g_cancel.load())
			return false;
		std::wstring host;
		std::wstring object;
		INTERNET_PORT port = 0;
		if (!crack_https(ascii_wide(url.c_str()), host, object, port))
			return false;

		std::wstring agent = L"vertical-hud-mask/";
		agent += ascii_wide(PLUGIN_VERSION);
		HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
						WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			return false;
		WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);
		DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
		protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
		WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

		HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
		if (!connect) {
			close_http(nullptr, nullptr, session);
			return false;
		}
		HINTERNET request = WinHttpOpenRequest(connect, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
						       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!request) {
			close_http(nullptr, connect, session);
			return false;
		}
		DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
		WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
		if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		    !WinHttpReceiveResponse(request, nullptr)) {
			close_http(request, connect, session);
			return false;
		}

		DWORD status = 0;
		DWORD status_size = sizeof(status);
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				    WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
		if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
			DWORD loc_bytes = 0;
			WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
					    WINHTTP_NO_OUTPUT_BUFFER, &loc_bytes, WINHTTP_NO_HEADER_INDEX);
			std::wstring location;
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && loc_bytes >= sizeof(wchar_t)) {
				location.resize(loc_bytes / sizeof(wchar_t));
				if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
							 location.data(), &loc_bytes, WINHTTP_NO_HEADER_INDEX))
					location.clear();
				else if (!location.empty() && location.back() == L'\0')
					location.pop_back();
			}
			close_http(request, connect, session);
			std::string next;
			if (!next_redirect(url, location, next)) {
				obs_log(LOG_WARNING, "update check: refused installer redirect");
				return false;
			}
			url = next;
			continue;
		}
		if (status != 200) {
			obs_log(LOG_WARNING, "update check: installer download HTTP %lu", (unsigned long)status);
			close_http(request, connect, session);
			return false;
		}

		HANDLE file = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
					  nullptr);
		if (file == INVALID_HANDLE_VALUE) {
			close_http(request, connect, session);
			return false;
		}
		char buf[16384];
		DWORD got = 0;
		size_t total = 0;
		bool ok = true;
		const size_t k_max = 128u * 1024u * 1024u;
		const size_t k_min = 64u * 1024u;
		while (WinHttpReadData(request, buf, sizeof(buf), &got) && got > 0) {
			if (g_cancel.load() || total + got > k_max) {
				ok = false;
				break;
			}
			DWORD wrote = 0;
			if (!WriteFile(file, buf, got, &wrote, nullptr) || wrote != got) {
				ok = false;
				break;
			}
			total += got;
		}
		CloseHandle(file);
		close_http(request, connect, session);
		if (!ok || total < k_min || g_cancel.load()) {
			DeleteFileW(part.c_str());
			obs_log(LOG_WARNING, "update check: installer download incomplete");
			return false;
		}
		if (!MoveFileExW(part.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING)) {
			DeleteFileW(part.c_str());
			return false;
		}
		return true;
	}
	obs_log(LOG_WARNING, "update check: too many installer redirects");
	return false;
}

bool launch_installer(const std::wstring &path)
{
	wchar_t params[] = L"/SILENT /SUPPRESSMSGBOXES /NORESTART";
	SHELLEXECUTEINFOW sei{};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"open";
	sei.lpFile = path.c_str();
	sei.lpParameters = params;
	sei.nShow = SW_SHOWNORMAL;
	if (!ShellExecuteExW(&sei)) {
		obs_log(LOG_WARNING, "update check: could not start installer (%lu)", (unsigned long)GetLastError());
		return false;
	}
	if (sei.hProcess)
		CloseHandle(sei.hProcess);
	return true;
}

void start_installer_update(const ReleaseInfo info)
{
	if (g_download.joinable())
		return;
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	auto *box = new QMessageBox(parent);
	box->setIcon(QMessageBox::Information);
	box->setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.Update.Title")));
	box->setText(QString::fromUtf8(obs_module_text("HUDMask.Update.Downloading")));
	box->setStandardButtons(QMessageBox::NoButton);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setModal(true);
	box->show();
	g_download_box = box;
	g_download = std::thread([info]() {
		std::wstring path;
		const bool ok = download_installer(info.download_url, path) && !g_cancel.load();
		QObject *ctx = QCoreApplication::instance();
		if (!ctx)
			return;
		QMetaObject::invokeMethod(
			ctx,
			[ok, path, info]() {
				if (g_download_box) {
					g_download_box->close();
					g_download_box = nullptr;
				}
				if (g_cancel.load())
					return;
				if (!ok || !launch_installer(path)) {
					show_update_failed(info.page_url.empty() ? info.download_url : info.page_url);
					return;
				}
				close_obs();
			},
			Qt::QueuedConnection);
	});
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
#ifdef _WIN32
		std::string exe = info.download_url;
		if (mask_normalize_https_url(exe) && url_is_exe(exe)) {
			start_installer_update(info);
			return;
		}
#endif
		open_https(!info.download_url.empty() ? info.download_url : info.page_url);
		close_obs();
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

	const int64_t now = static_cast<int64_t>(time(nullptr));

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
	if (g_download.joinable())
		g_download.join();
	if (QCoreApplication *app = QCoreApplication::instance()) {
		if (QThread::currentThread() == app->thread())
			QCoreApplication::sendPostedEvents(app, QEvent::MetaCall);
	}
	if (g_download_box) {
		g_download_box->close();
		g_download_box = nullptr;
	}
}
