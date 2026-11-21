// Title manager part of the bridge. Ported from the Android port (NativeGameTitles.cpp, WuaConverter.cpp,
// InstallTitleUseCase.kt, DeleteTitleUseCase.kt) without JNI / Android storage.
#include "MobileBackend.h"

#include "Cafe/Filesystem/fsc.h"
#include "Cafe/TitleList/TitleId.h"
#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"

#include <zarchive/zarchivereader.h>
#include <zarchive/zarchivewriter.h>
#include <boost/algorithm/string.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <thread>

namespace MobileBackend
{
	namespace
	{
		int ToEntryType(TitleId titleId)
		{
			switch (TitleIdParser(titleId).GetType())
			{
			case TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE: return 1;
			case TitleIdParser::TITLE_TYPE::AOC: return 2;
			case TitleIdParser::TITLE_TYPE::SYSTEM_TITLE:
			case TitleIdParser::TITLE_TYPE::SYSTEM_DATA:
			case TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE: return 3;
			default: return 0;
			}
		}

		std::string RegionName(CafeConsoleRegion region)
		{
			switch (region)
			{
			case CafeConsoleRegion::JPN: return "JPN";
			case CafeConsoleRegion::USA: return "USA";
			case CafeConsoleRegion::EUR: return "EUR";
			case CafeConsoleRegion::CHN: return "CHN";
			case CafeConsoleRegion::KOR: return "KOR";
			case CafeConsoleRegion::TWN: return "TWN";
			default: return {};
			}
		}

		// all discovered copies of a title id
		std::vector<TitleInfo> FindTitles(TitleId titleId)
		{
			std::vector<TitleInfo> result;
			auto list = CafeTitleList::AcquireInternalList();
			for (TitleInfo* info : list)
			{
				if (info->IsValid() && info->GetAppTitleId() == titleId)
					result.push_back(*info);
			}
			CafeTitleList::ReleaseInternalList();
			return result;
		}

		// the copy the user picked, otherwise the first one (the newest one for updates)
		TitleInfo PickTitle(TitleId titleId, uint64_t uid, bool preferNewest)
		{
			TitleInfo picked;
			for (TitleInfo& info : FindTitles(titleId))
			{
				if (uid != 0 && info.GetUID() == uid)
					return info;
				if (!picked.IsValid() || (preferNewest && picked.GetAppTitleVersion() < info.GetAppTitleVersion()))
					picked = info;
			}
			return picked;
		}
	} // namespace

	std::vector<TitleEntry> GetTitleEntries()
	{
		std::vector<TitleEntry> result;
		const fs::path mlc = ActiveSettings::GetMlcPath();
		const auto language = GetConfig().console_language.GetValue();
		auto list = CafeTitleList::AcquireInternalList();
		for (TitleInfo* info : list)
		{
			if (!info->IsValid() || !info->HasValidXmlInfo())
				continue;
			TitleEntry e;
			e.titleId = info->GetAppTitleId();
			e.uid = info->GetUID();
			e.name = info->GetMetaInfo()->GetShortName(language);
			e.path = info->GetPath().string();
			e.version = info->GetAppTitleVersion();
			e.region = RegionName(info->GetMetaInfo()->GetRegion());
			e.type = ToEntryType(e.titleId);
			switch (info->GetFormat())
			{
			case TitleInfo::TitleDataFormat::WUD: e.format = 1; break;
			case TitleInfo::TitleDataFormat::WIIU_ARCHIVE: e.format = 2; break;
			case TitleInfo::TitleDataFormat::NUS: e.format = 3; break;
			case TitleInfo::TitleDataFormat::WUHB: e.format = 4; break;
			default: e.format = 0; break;
			}
			const std::string mlcStr = mlc.string();
			e.inMlc = e.path.compare(0, mlcStr.size(), mlcStr) == 0;
			result.push_back(std::move(e));
		}
		CafeTitleList::ReleaseInternalList();
		return result;
	}

	InstallInfo CheckInstallTitle(const std::string& sourcePath)
	{
		InstallInfo info;
		const fs::path sourceDir(sourcePath);
		TitleInfo titleInfo(sourceDir);
		if (!titleInfo.IsValid() || !titleInfo.HasValidXmlInfo())
			return info;
		info.name = titleInfo.GetMetaInfo()->GetShortName(GetConfig().console_language.GetValue());
		const fs::path target = ActiveSettings::GetMlcPath(titleInfo.GetInstallPath());
		info.targetPath = target.string();
		info.check = InstallCheck::kOk;

		std::error_code ec;
		if (!fs::exists(target, ec))
			return info;
		try
		{
			const TitleInfo installed(target);
			if (!installed.IsValid())
				return info; // not a title, may be overwritten
			if (TitleIdParser(titleInfo.GetAppTitleId()).GetType() != TitleIdParser(installed.GetAppTitleId()).GetType())
				info.check = InstallCheck::kDifferentType;
			else if (installed.GetAppTitleVersion() == titleInfo.GetAppTitleVersion())
				info.check = InstallCheck::kSameVersion;
			else if (installed.GetAppTitleVersion() > titleInfo.GetAppTitleVersion())
				info.check = InstallCheck::kNewerInstalled;
			else
				info.check = InstallCheck::kOverwrite;
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "Title install check failed: {}", ex.what());
		}
		return info;
	}

	InstallResult InstallTitle(const std::string& sourcePath, const std::function<void(uint64_t, uint64_t)>& progress,
							   const std::atomic_bool* cancel)
	{
		const InstallInfo info = CheckInstallTitle(sourcePath);
		if (info.check == InstallCheck::kInvalid || info.check == InstallCheck::kDifferentType)
			return InstallResult::kError;

		const fs::path source(sourcePath);
		const fs::path target(info.targetPath);
		fs::path backup = target;
		backup += ".backup";
		std::error_code ec;

		// list what has to be copied
		struct Item
		{
			fs::path from, to;
			uint64_t size;
			bool isDir;
		};
		std::vector<Item> items;
		uint64_t totalSize = 0;
		for (auto it = fs::recursive_directory_iterator(source, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (ec)
				return InstallResult::kError;
			const fs::path relative = fs::relative(it->path(), source, ec);
			const bool isDir = it->is_directory(ec);
			const uint64_t size = isDir ? 0 : (uint64_t)it->file_size(ec);
			items.push_back({it->path(), target / relative, size, isDir});
			totalSize += size;
		}
		fs::create_directories(target.parent_path(), ec);
		if (fs::space(target.parent_path(), ec).available < totalSize)
			return InstallResult::kNotEnoughSpace;

		fs::remove_all(backup, ec);
		if (fs::exists(target, ec))
			fs::rename(target, backup, ec);
		auto restore = [&] {
			fs::remove_all(target, ec);
			if (fs::exists(backup, ec))
				fs::rename(backup, target, ec);
		};

		fs::create_directories(target, ec);
		uint64_t written = 0;
		progress(0, totalSize);
		for (const Item& item : items)
		{
			if (cancel && cancel->load())
			{
				restore();
				return InstallResult::kCancelled;
			}
			if (item.isDir)
			{
				fs::create_directories(item.to, ec);
				continue;
			}
			fs::create_directories(item.to.parent_path(), ec);
			fs::copy_file(item.from, item.to, fs::copy_options::overwrite_existing, ec);
			if (ec)
			{
				cemuLog_log(LogType::Force, "Title install: failed to copy {}: {}", item.from.string(), ec.message());
				restore();
				return InstallResult::kError;
			}
			written += item.size;
			progress(written, totalSize);
		}
		fs::remove_all(backup, ec);
		CafeTitleList::AddTitleFromPath(target);
		return InstallResult::kFinished;
	}

	bool DeleteTitle(const std::string& path)
	{
		std::error_code ec;
		const auto removed = fs::remove_all(fs::path(path), ec);
		if (ec)
		{
			cemuLog_log(LogType::Force, "Failed to delete title {}: {}", path, ec.message());
			return false;
		}
		CafeTitleList::Refresh();
		return removed > 0;
	}

	// ---- .wua conversion ----
	namespace
	{
		class WuaConverter
		{
		  public:
			WuaConverter(TitleInfo base, TitleInfo update, TitleInfo aoc) : m_base(std::move(base)), m_update(std::move(update)), m_aoc(std::move(aoc)) {}
			~WuaConverter()
			{
				m_cancelled = true;
				if (m_thread.joinable())
					m_thread.join();
			}

			void Start(const std::string& outputFile)
			{
				m_thread = std::thread([this, outputFile] { Run(outputFile); });
			}
			void Cancel() { m_cancelled = true; }
			WuaProgress GetProgress() const
			{
				return {m_stage.load(), m_transferred.load(), m_total.load()};
			}
			std::string SuggestedName() const;

		  private:
			struct WriterContext
			{
				int fd = -1;
				std::vector<uint8> buffer;
			};
			static void NewOutputFile(int32_t, void*) {}
			static void WriteOutputData(const void* data, size_t length, void* ctx)
			{
				auto* c = static_cast<WriterContext*>(ctx);
				const auto* p = static_cast<const uint8*>(data);
				while (length > 0)
				{
					const ssize_t n = ::write(c->fd, p, length);
					if (n <= 0)
						return;
					p += n;
					length -= (size_t)n;
				}
			}

			bool CountFiles(const std::string& fscPath)
			{
				sint32 status;
				std::unique_ptr<FSCVirtualFile> dir(fsc_openDirIterator(fscPath.c_str(), &status));
				if (!dir || m_cancelled)
					return false;
				FSCDirEntry entry;
				while (fsc_nextDir(dir.get(), &entry))
				{
					if (entry.isFile)
						m_total += (uint64)entry.fileSize;
					else if (entry.isDirectory && !CountFiles(fmt::format("{}{}/", fscPath, entry.path)))
						return false;
				}
				return true;
			}

			bool AddFiles(ZArchiveWriter& writer, const std::string& archivePath, const std::string& fscPath)
			{
				sint32 status;
				std::unique_ptr<FSCVirtualFile> dir(fsc_openDirIterator(fscPath.c_str(), &status));
				if (!dir || m_cancelled)
					return false;
				writer.MakeDir(archivePath.c_str(), false);
				FSCDirEntry entry;
				std::vector<uint8> buffer(32 * 1024);
				while (fsc_nextDir(dir.get(), &entry))
				{
					if (entry.isFile)
					{
						writer.StartNewFile((archivePath + entry.path).c_str());
						std::unique_ptr<FSCVirtualFile> file(fsc_open((fscPath + entry.path).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &status));
						if (!file)
							return false;
						while (true)
						{
							const uint32 n = file->fscReadData(buffer.data(), buffer.size());
							if (n == 0)
								break;
							writer.AppendData(buffer.data(), n);
							if (m_cancelled)
								return false;
							m_transferred += n;
						}
					}
					else if (entry.isDirectory &&
							 !AddFiles(writer, fmt::format("{}{}/", archivePath, entry.path), fmt::format("{}{}/", fscPath, entry.path)))
						return false;
				}
				return true;
			}

			bool WithMounted(TitleInfo& title, const std::function<bool(const std::string&)>& fn)
			{
				const std::string mountPath = TitleInfo::GetUniqueTempMountingPath();
				title.Mount(mountPath, "", FSC_PRIORITY_BASE);
				const bool ok = fn(mountPath);
				title.Unmount(mountPath);
				return ok;
			}

			void Run(const std::string& outputFile)
			{
				m_stage = WuaStage::kStarting;
				std::vector<TitleInfo*> titles;
				for (TitleInfo* t : {&m_base, &m_update, &m_aoc})
				{
					if (t->IsValid())
						titles.push_back(t);
				}
				WriterContext ctx;
				ctx.fd = ::open(outputFile.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
				if (titles.empty() || ctx.fd < 0)
				{
					if (ctx.fd >= 0)
						::close(ctx.fd);
					m_stage = WuaStage::kError;
					return;
				}
				bool ok = true;
				{
					ZArchiveWriter writer(&NewOutputFile, &WriteOutputData, &ctx);
					m_stage = WuaStage::kCollectingFiles;
					for (TitleInfo* t : titles)
						ok = ok && WithMounted(*t, [&](const std::string& mount) { return CountFiles(mount); });
					m_stage = WuaStage::kCompressing;
					for (TitleInfo* t : titles)
					{
						ok = ok && WithMounted(*t, [&](const std::string& mount) {
							return AddFiles(writer, fmt::format("{:016x}_v{}/", t->GetAppTitleId(), t->GetAppTitleVersion()), mount);
						});
					}
					if (ok)
					{
						m_stage = WuaStage::kFinalizing;
						writer.Finalize();
					}
				}
				::close(ctx.fd);
				if (m_cancelled)
				{
					std::error_code ec;
					fs::remove(outputFile, ec);
					m_stage = WuaStage::kCancelled;
					return;
				}
				if (ok)
				{
					// check that the archive can be opened
					ZArchiveReader* reader = ZArchiveReader::OpenFromFile(outputFile);
					ok = reader != nullptr;
					delete reader;
				}
				if (!ok)
				{
					std::error_code ec;
					fs::remove(outputFile, ec);
					m_stage = WuaStage::kError;
					return;
				}
				CafeTitleList::Refresh();
				m_stage = WuaStage::kFinished;
			}

			TitleInfo m_base, m_update, m_aoc;
			std::atomic_uint64_t m_transferred{0};
			std::atomic_uint64_t m_total{0};
			std::atomic_bool m_cancelled{false};
			std::atomic<WuaStage> m_stage{WuaStage::kIdle};
			std::thread m_thread;
		};

		std::string WuaConverter::SuggestedName() const
		{
			const auto language = CafeConsoleLanguage::EN;
			std::string name;
			for (const TitleInfo* t : {&m_base, &m_update, &m_aoc})
			{
				if (t->IsValid() && t->HasValidXmlInfo())
				{
					name = const_cast<TitleInfo*>(t)->GetMetaInfo()->GetShortName(language);
					break;
				}
			}
			boost::replace_all(name, ":", "");
			boost::replace_all(name, "/", "");
			boost::replace_all(name, "\\", "");
			CafeConsoleRegion region = CafeConsoleRegion::Auto;
			if (m_base.IsValid() && m_base.HasValidXmlInfo())
				region = const_cast<TitleInfo&>(m_base).GetMetaInfo()->GetRegion();
			else if (m_update.IsValid() && m_update.HasValidXmlInfo())
				region = const_cast<TitleInfo&>(m_update).GetMetaInfo()->GetRegion();
			if (region == CafeConsoleRegion::JPN)
				name += " (JP)";
			else if (region == CafeConsoleRegion::EUR)
				name += " (EU)";
			else if (region == CafeConsoleRegion::USA)
				name += " (US)";
			if (m_update.IsValid())
				name += fmt::format(" (v{})", m_update.GetAppTitleVersion());
			return name + ".wua";
		}

		std::unique_ptr<WuaConverter> s_converter;

		std::unique_ptr<WuaConverter> MakeConverter(uint64_t selectedTitleId, uint64_t uid)
		{
			// a selected update or DLC is converted back to its base title id
			const TitleId baseId = TitleIdParser::MakeBaseTitleId(selectedTitleId);
			const TitleIdParser parser(baseId);
			const bool hasBase = parser.GetType() != TitleIdParser::TITLE_TYPE::AOC;
			const TitleId updateId = parser.CanHaveSeparateUpdateTitleId() ? parser.GetSeparateUpdateTitleId() : 0;
			// AOC ids only differ from the base in the type bits (same assumption as the desktop interface)
			const TitleId aocId = hasBase ? ((baseId & (uint64)~0xFF00000000) | (uint64)0xC00000000) : baseId;
			TitleInfo base = hasBase ? PickTitle(baseId, uid, false) : TitleInfo();
			TitleInfo update = updateId ? PickTitle(updateId, uid, true) : TitleInfo();
			TitleInfo aoc = PickTitle(aocId, uid, true);
			return std::make_unique<WuaConverter>(base, update, aoc);
		}
	} // namespace

	std::string GetWuaSuggestedName(uint64_t titleId, uint64_t uid)
	{
		return MakeConverter(titleId, uid)->SuggestedName();
	}

	bool StartWuaConversion(uint64_t titleId, uint64_t uid, const std::string& outputFile)
	{
		if (s_converter && s_converter->GetProgress().stage != WuaStage::kFinished &&
			s_converter->GetProgress().stage != WuaStage::kError && s_converter->GetProgress().stage != WuaStage::kCancelled &&
			s_converter->GetProgress().stage != WuaStage::kIdle)
			return false; // already running
		s_converter = MakeConverter(titleId, uid);
		s_converter->Start(outputFile);
		return true;
	}

	WuaProgress GetWuaProgress()
	{
		return s_converter ? s_converter->GetProgress() : WuaProgress{};
	}

	void CancelWuaConversion()
	{
		if (s_converter)
			s_converter->Cancel();
	}
} // namespace MobileBackend
