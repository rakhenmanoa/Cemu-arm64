// Emulated USB devices part of the bridge (ported from the Android port, NativeEmulatedUSBDevices.cpp,
// without the JNI layer)
#include "MobileBackend.h"

#include "Cafe/OS/libs/nsyshid/Dimensions.h"
#include "Cafe/OS/libs/nsyshid/Infinity.h"
#include "Cafe/OS/libs/nsyshid/Skylander.h"
#include "Cafe/OS/libs/nsyshid/nsyshid.h"
#include "Common/FileStream.h"
#include "config/ActiveSettings.h"

#include <array>
#include <optional>

namespace MobileBackend
{
	namespace
	{
		constexpr const char* kSkylandersPath = "emulatedUSBDevices/skylanders";
		constexpr const char* kInfinityPath = "emulatedUSBDevices/infinity";
		constexpr const char* kDimensionsPath = "emulatedUSBDevices/dimensions";
		// pad of the toypad (1 = center, 2 = left, 3 = right) of each of the 7 dimensions slots, as in the desktop interface
		constexpr std::array<uint8, kDimensionsSlots> kDimensionsPads = {2, 1, 3, 2, 2, 3, 3};

		struct SkylanderSlot
		{
			uint8 portalSlot;
			std::string name;
		};
		std::array<std::optional<SkylanderSlot>, nsyshid::MAX_SKYLANDERS> s_skylanderSlots;
		std::array<std::optional<std::string>, nsyshid::MAX_FIGURES> s_infinitySlots;
		std::array<std::optional<std::string>, kDimensionsSlots> s_dimensionSlots;

		const char* FolderOf(FigureKind kind)
		{
			switch (kind)
			{
			case FigureKind::kSkylander: return kSkylandersPath;
			case FigureKind::kInfinity: return kInfinityPath;
			default: return kDimensionsPath;
			}
		}

		fs::path UniqueFigurePath(const char* relativeDir, const std::string& fileName)
		{
			const fs::path baseDir = ActiveSettings::GetUserDataPath(relativeDir);
			fs::path filePath = baseDir / fileName;
			std::error_code ec;
			fs::create_directories(baseDir, ec);
			if (!fs::exists(filePath, ec))
				return filePath;
			const std::string stem = filePath.stem().string();
			const std::string ext = filePath.extension().string();
			for (int counter = 1;; counter++)
			{
				fs::path candidate = baseDir / fmt::format("{} ({}){}", stem, counter, ext);
				if (!fs::exists(candidate, ec))
					return candidate;
			}
		}

		// reads exactly N bytes of the figure file, keeps the file open (the emulated device writes back to it)
		template<size_t N>
		bool ReadFigure(const std::string& path, std::array<uint8, N>& data, std::unique_ptr<FileStream>& file)
		{
			file.reset(FileStream::openFile2(fs::path(path), true));
			if (!file)
				return false;
			return file->readData(data.data(), data.size()) == data.size();
		}

		void ClearDimensions(int slot)
		{
			s_dimensionSlots.at(slot) = std::nullopt;
			nsyshid::g_dimensionstoypad.RemoveFigure(kDimensionsPads.at(slot), (uint8)slot, true);
		}

		void ClearSkylander(int slot)
		{
			if (auto& info = s_skylanderSlots.at(slot))
			{
				nsyshid::g_skyportal.RemoveSkylander(info->portalSlot);
				info = std::nullopt;
			}
		}

		void ClearInfinity(int slot)
		{
			s_infinitySlots.at(slot) = std::nullopt;
			nsyshid::g_infinitybase.RemoveFigure((uint8)slot);
		}

		bool ValidSlot(FigureKind kind, int slot)
		{
			return slot >= 0 && slot < GetFigureSlotCount(kind);
		}
	} // namespace

	int GetFigureSlotCount(FigureKind kind)
	{
		switch (kind)
		{
		case FigureKind::kSkylander: return kSkylanderSlots;
		case FigureKind::kInfinity: return kInfinitySlots;
		default: return kDimensionsSlots;
		}
	}

	std::vector<FigureCatalogEntry> GetFigureCatalog(FigureKind kind)
	{
		std::vector<FigureCatalogEntry> result;
		switch (kind)
		{
		case FigureKind::kSkylander:
			for (const auto& [key, name] : nsyshid::SkylanderUSB::GetListSkylanders())
				result.push_back({key.first, key.second, name});
			break;
		case FigureKind::kInfinity:
			for (const auto& [number, data] : nsyshid::InfinityUSB::GetFigureList())
				result.push_back({(int64_t)number, 0, data.second});
			break;
		case FigureKind::kDimensions:
			for (const auto& [number, name] : nsyshid::DimensionsUSB::GetListMinifigs())
				result.push_back({(int64_t)number, 0, name});
			break;
		}
		return result;
	}

	std::vector<InstalledFigure> GetInstalledFigures(FigureKind kind)
	{
		std::vector<InstalledFigure> result;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(ActiveSettings::GetUserDataPath(FolderOf(kind)), ec))
		{
			if (entry.is_regular_file(ec))
				result.push_back({entry.path().stem().string(), entry.path().string()});
		}
		return result;
	}

	bool CreateFigure(FigureKind kind, int64_t number, int variant)
	{
		switch (kind)
		{
		case FigureKind::kSkylander:
		{
			const std::string fileName = nsyshid::g_skyportal.FindSkylander((uint16)number, (uint16)variant) + ".sky";
			return nsyshid::g_skyportal.CreateSkylander(UniqueFigurePath(kSkylandersPath, fileName), (uint16)number, (uint16)variant);
		}
		case FigureKind::kInfinity:
		{
			const auto [series, name] = nsyshid::g_infinitybase.FindFigure((uint32)number);
			return nsyshid::g_infinitybase.CreateFigure(UniqueFigurePath(kInfinityPath, name + ".bin"), (uint32)number, series);
		}
		default:
		{
			const std::string name = nsyshid::g_dimensionstoypad.FindFigure((uint32)number);
			return nsyshid::g_dimensionstoypad.CreateFigure(UniqueFigurePath(kDimensionsPath, name + ".bin"), (uint32)number);
		}
		}
	}

	bool DeleteFigureFile(const std::string& path)
	{
		std::error_code ec;
		if (!fs::remove(fs::path(path), ec))
		{
			cemuLog_log(LogType::Force, "Failed to delete figure {}: {}", path, ec.message());
			return false;
		}
		return true;
	}

	bool LoadFigure(FigureKind kind, const std::string& path, int slot)
	{
		if (!ValidSlot(kind, slot))
			return false;
		std::unique_ptr<FileStream> file;
		switch (kind)
		{
		case FigureKind::kSkylander:
		{
			std::array<uint8, nsyshid::SKY_FIGURE_SIZE> data{};
			if (!ReadFigure(path, data, file))
				return false;
			ClearSkylander(slot);
			const uint16 skyId = uint16(data[0x11]) << 8 | uint16(data[0x10]);
			const uint16 skyVar = uint16(data[0x1D]) << 8 | uint16(data[0x1C]);
			const uint8 portalSlot = nsyshid::g_skyportal.LoadSkylander(data.data(), std::move(file));
			s_skylanderSlots.at(slot) = SkylanderSlot{portalSlot, nsyshid::g_skyportal.FindSkylander(skyId, skyVar)};
			return true;
		}
		case FigureKind::kInfinity:
		{
			std::array<uint8, nsyshid::INF_FIGURE_SIZE> data{};
			if (!ReadFigure(path, data, file))
				return false;
			ClearInfinity(slot);
			const uint32 number = nsyshid::g_infinitybase.LoadFigure(data, std::move(file), (uint8)slot);
			s_infinitySlots.at(slot) = nsyshid::g_infinitybase.FindFigure(number).second;
			return true;
		}
		default:
		{
			std::array<uint8, 0x2D * 0x04> data{};
			if (!ReadFigure(path, data, file))
				return false;
			ClearDimensions(slot);
			const uint32 number = nsyshid::g_dimensionstoypad.LoadFigure(data, std::move(file), kDimensionsPads.at(slot), (uint8)slot);
			s_dimensionSlots.at(slot) = nsyshid::g_dimensionstoypad.FindFigure(number);
			return true;
		}
		}
	}

	void ClearFigure(FigureKind kind, int slot)
	{
		if (!ValidSlot(kind, slot))
			return;
		switch (kind)
		{
		case FigureKind::kSkylander: ClearSkylander(slot); break;
		case FigureKind::kInfinity: ClearInfinity(slot); break;
		default: ClearDimensions(slot); break;
		}
	}

	std::string GetFigureInSlot(FigureKind kind, int slot)
	{
		if (!ValidSlot(kind, slot))
			return {};
		switch (kind)
		{
		case FigureKind::kSkylander:
			return s_skylanderSlots.at(slot) ? s_skylanderSlots.at(slot)->name : std::string();
		case FigureKind::kInfinity:
			return s_infinitySlots.at(slot).value_or(std::string());
		default:
			return s_dimensionSlots.at(slot).value_or(std::string());
		}
	}
} // namespace MobileBackend
