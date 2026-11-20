// Downloads the community graphic packs (ported from the Android port, GraphicPacksDownloader.kt,
// and from the desktop DownloadGraphicPacksWindow). Same files and folders as the desktop interface.
#include "MobileBackend.h"

#include "Cafe/CafeSystem.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Common/FileStream.h"
#include "Common/version.h"
#include "config/ActiveSettings.h"

#include <curl/curl.h>
#include <zip.h>
#include <rapidjson/document.h>
#include <boost/algorithm/string.hpp>

#include <chrono>

namespace MobileBackend
{
	namespace
	{
		struct DownloadState
		{
			std::vector<uint8_t> data;
			const std::atomic_bool* cancel = nullptr;
			const std::function<void(GraphicPackDownloadStatus, double)>* progress = nullptr;
			bool reportProgress = false;
		};

		size_t WriteData(void* ptr, size_t size, size_t nmemb, DownloadState* state)
		{
			const size_t count = size * nmemb;
			const auto* bytes = static_cast<const uint8_t*>(ptr);
			state->data.insert(state->data.end(), bytes, bytes + count);
			return count;
		}

		int OnProgress(DownloadState* state, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
		{
			if (state->cancel && state->cancel->load())
				return 1; // abort
			if (state->reportProgress && state->progress && dltotal > 0)
				(*state->progress)(GraphicPackDownloadStatus::kDownloading, (double)dlnow / (double)dltotal);
			return 0;
		}

		bool Download(const std::string& url, DownloadState& state)
		{
			CURL* curl = curl_easy_init();
			if (!curl)
				return false;
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteData);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, OnProgress);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
			curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, BUILD_VERSION_WITH_NAME_STRING);
			const CURLcode res = curl_easy_perform(curl);
			curl_easy_cleanup(curl);
			return res == CURLE_OK;
		}

		bool IsUpToDate(const std::string& version)
		{
			const auto path = ActiveSettings::GetUserDataPath("graphicPacks/downloadedGraphicPacks/version.txt");
			std::unique_ptr<FileStream> file(FileStream::openFile2(path));
			std::string versionInFile;
			return file && file->readLine(versionInFile) && boost::iequals(versionInFile, version);
		}

		void ClearDownloadedGraphicPacks()
		{
			const auto path = ActiveSettings::GetUserDataPath("graphicPacks/downloadedGraphicPacks");
			std::error_code ec;
			if (!fs::exists(path, ec))
				return;
			for (auto& entry : fs::directory_iterator(path, ec))
				fs::remove_all(entry.path(), ec);
		}

		bool Extract(const std::vector<uint8_t>& zipData, const std::string& version,
					 const std::function<void(GraphicPackDownloadStatus, double)>& progress, const std::atomic_bool* cancel)
		{
			zip_error_t error;
			zip_error_init(&error);
			zip_source_t* src = zip_source_buffer_create(zipData.data(), zipData.size(), 0, &error);
			if (!src)
			{
				zip_error_fini(&error);
				return false;
			}
			zip_t* za = zip_open_from_source(src, 0, &error);
			if (!za)
			{
				zip_source_free(src);
				zip_error_fini(&error);
				return false;
			}

			const auto root = ActiveSettings::GetUserDataPath("graphicPacks/downloadedGraphicPacks");
			std::error_code ec;
			ClearDownloadedGraphicPacks();
			fs::create_directories(root, ec);

			const zip_int64_t numEntries = zip_get_num_entries(za, 0);
			for (zip_int64_t i = 0; i < numEntries; i++)
			{
				if (cancel && cancel->load())
				{
					zip_discard(za);
					zip_error_fini(&error);
					return false;
				}
				progress(GraphicPackDownloadStatus::kExtracting, (double)i / (double)numEntries);
				zip_stat_t sb = {};
				if (zip_stat_index(za, i, 0, &sb) != 0 || !(sb.valid & ZIP_STAT_NAME))
					continue;
				const std::string name = sb.name;
				if (name.empty() || name.find("../") != std::string::npos || name.find("..\\") != std::string::npos)
					continue; // bad path
				const auto path = ActiveSettings::GetUserDataPath("graphicPacks/downloadedGraphicPacks/{}", name);
				if (name.back() == '/')
				{
					fs::create_directories(path, ec);
					continue;
				}
				if (sb.size == 0 || sb.size > (1024 * 1024 * 128))
					continue; // empty or unusually huge
				zip_file_t* zipFile = zip_fopen_index(za, i, 0);
				if (!zipFile)
					continue;
				std::vector<uint8_t> buffer(sb.size);
				if (static_cast<zip_uint64_t>(zip_fread(zipFile, buffer.data(), sb.size)) == sb.size)
				{
					fs::create_directories(path.parent_path(), ec);
					std::unique_ptr<FileStream> out(FileStream::createFile2(path));
					if (out)
						out->writeData(buffer.data(), static_cast<sint32>(buffer.size()));
				}
				zip_fclose(zipFile);
			}
			zip_discard(za);
			zip_error_fini(&error);

			std::unique_ptr<FileStream> versionFile(FileStream::createFile2(ActiveSettings::GetUserDataPath("graphicPacks/downloadedGraphicPacks/version.txt")));
			if (versionFile)
				versionFile->writeString(version.c_str());
			return true;
		}
	} // namespace

	GraphicPackDownloadStatus DownloadGraphicPacks(const std::function<void(GraphicPackDownloadStatus, double)>& progress, const std::atomic_bool* cancel)
	{
		auto finish = [&](GraphicPackDownloadStatus status) {
			progress(status, 1.0);
			return status;
		};
		auto cancelled = [&] { return cancel && cancel->load(); };

		progress(GraphicPackDownloadStatus::kCheckingVersion, 0.0);

		// the Cemu server tells which release matches this version, GitHub is the fallback
		std::string apiUrl;
		{
			DownloadState state;
			state.cancel = cancel;
			const std::string query = fmt::format("https://cemu.info/api2/query_graphicpack_url.php?version={}.{}.{}&t={}",
												  EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_MINOR, EMULATOR_VERSION_PATCH,
												  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			if (Download(query, state))
				apiUrl.assign(state.data.begin(), state.data.end());
			boost::trim(apiUrl);
			if (apiUrl.rfind("http", 0) != 0)
				apiUrl = "https://api.github.com/repos/cemu-project/cemu_graphic_packs/releases/latest";
		}
		if (cancelled())
			return finish(GraphicPackDownloadStatus::kCancelled);

		std::string version, zipUrl;
		{
			DownloadState state;
			state.cancel = cancel;
			if (!Download(apiUrl, state))
				return finish(cancelled() ? GraphicPackDownloadStatus::kCancelled : GraphicPackDownloadStatus::kError);
			rapidjson::Document d;
			d.Parse(reinterpret_cast<const char*>(state.data.data()), state.data.size());
			if (d.HasParseError() || !d.IsObject() || !d.HasMember("name") || !d["name"].IsString() ||
				!d.HasMember("assets") || !d["assets"].IsArray() || d["assets"].Empty() || !d["assets"][0].IsObject() ||
				!d["assets"][0].HasMember("browser_download_url") || !d["assets"][0]["browser_download_url"].IsString())
				return finish(GraphicPackDownloadStatus::kError);
			version = d["name"].GetString();
			zipUrl = d["assets"][0]["browser_download_url"].GetString();
		}

		if (IsUpToDate(version))
			return finish(GraphicPackDownloadStatus::kNoUpdateAvailable);

		DownloadState zipState;
		zipState.cancel = cancel;
		zipState.progress = &progress;
		zipState.reportProgress = true;
		progress(GraphicPackDownloadStatus::kDownloading, 0.0);
		if (!Download(zipUrl, zipState))
			return finish(cancelled() ? GraphicPackDownloadStatus::kCancelled : GraphicPackDownloadStatus::kError);

		if (!Extract(zipState.data, version, progress, cancel))
			return finish(cancelled() ? GraphicPackDownloadStatus::kCancelled : GraphicPackDownloadStatus::kError);

		ReloadGraphicPacks();
		return finish(GraphicPackDownloadStatus::kFinished);
	}

	void ReloadGraphicPacks()
	{
		if (CafeSystem::IsTitleRunning())
			return;
		GraphicPack2::ClearGraphicPacks();
		GraphicPack2::LoadAll();
	}
} // namespace MobileBackend
