#pragma once

// Bridge between the Qt/QML mobile interface and the Cemu core.
//
// Only standard C++ types are used here on purpose: the Qt side never includes Cemu headers and the
// Cemu side never includes Qt headers. Both pull in a lot of macros/types (X11, GTK, uint32, signals/slots, ..)
// which clash when mixed in a single translation unit.

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MobileBackend
{
	struct GameEntry
	{
		uint64_t titleId = 0;
		std::string name;
		std::string region;
		int version = 0;
		bool hasUpdate = false;
		bool hasDLC = false;
	};

	struct NativeWindow
	{
		bool isWayland = false;
		void* display = nullptr; // wl_display* or Display*
		void* surface = nullptr; // wl_surface* or X11 window id
		int width = 0, height = 0;		   // logical size
		int physWidth = 0, physHeight = 0; // size in physical pixels
		double scale = 1.0;
	};

	// must be called once from the UI thread before anything else
	void Initialize();

	// ---- game list ----
	// the callback can be invoked from any thread
	void SetGameListChangedCallback(std::function<void()> cb);
	std::vector<GameEntry> GetGames();
	void RefreshGameList();
	bool IsScanning();
	// decodes the icon of a title to 32bit RGBA. Slow (mounts the title), call from a worker thread
	bool LoadGameIcon(uint64_t titleId, std::vector<uint8_t>& rgbaOut, int& widthOut, int& heightOut);

	// ---- game folders ----
	std::vector<std::string> GetGamePaths();
	void AddGamePath(const std::string& path);
	void RemoveGamePath(const std::string& path);

	// ---- emulation ----
	// returns false and sets errorOut on failure. The window must already be mapped/exposed
	bool LaunchGame(uint64_t titleId, const NativeWindow& window, std::string& errorOut);
	bool IsGameRunning();
	std::string GetRunningGameName();
	// invoked (from an emulator thread) when the emulated title exits on its own
	void SetGameExitedCallback(std::function<void()> cb);
	void Shutdown();
	// replaces the current process with a fresh instance of the mobile UI. Does not return on success
	void RestartProcess();

	// ---- window state & input forwarded from the render window ----
	void SetWindowSize(int width, int height, int physWidth, int physHeight, double scale);
	void SetAppActive(bool active);
	void SetKeyState(uint32_t platformKeyCode, bool down);
	void ReleaseAllKeys();
	void SetMouse(int physX, int physY, bool leftDown, bool rightDown);
	// converts physical window coordinates to the (possibly rotated) game view coordinates
	void PhysicalToLogical(int& x, int& y);
	void SetTouch(int physX, int physY, bool down);
	// all the fingers on the game window (physical pixels). With the on-screen gamepad enabled, the fingers that start on
	// a control drive it, the first other finger is the GamePad touch screen
	struct TouchPoint
	{
		int id = 0;
		int x = 0, y = 0;
		bool down = false;
	};
	void SetTouchPoints(const std::vector<TouchPoint>& points);

	std::string GetVersionString();

	// ---- settings (shared with the desktop interface, saved to settings.xml) ----
	// integer/bool settings addressed by key, see the table in MobileBackend.cpp. Unknown keys return 0
	int GetSetting(const std::string& key);
	void SetSetting(const std::string& key, int value); // saves the config
	struct AudioDevice
	{
		std::string id; // empty = disabled
		std::string name;
	};
	std::vector<AudioDevice> GetAudioDevices(); // output devices of the active audio API
	std::string GetTVAudioDevice();
	void SetTVAudioDevice(const std::string& id);

	// ---- per game actions ----
	int CountShaderCaches(uint64_t titleId);
	int RemoveShaderCaches(uint64_t titleId); // returns the number of deleted files

	struct GraphicPackPresetGroup
	{
		std::string category; // empty = default category
		std::vector<std::string> presets;
		std::string active;
	};
	struct GraphicPackInfo
	{
		int index = 0; // stable handle for the lifetime of the process
		std::string name;
		std::string path; // position in the graphic pack tree, e.g. "Resolution/BotW"
		std::string description;
		bool enabled = false;
		std::vector<GraphicPackPresetGroup> presetGroups; // only categories with visible presets
	};
	std::vector<GraphicPackInfo> GetGraphicPacks(uint64_t titleId);
	void SetGraphicPackEnabled(int index, bool enabled);
	void SetGraphicPackPreset(int index, const std::string& category, const std::string& preset);

	// ---- controllers (profiles are shared with the desktop interface: controllerProfiles/controllerN.xml) ----
	struct InputDevice
	{
		std::string uuid;
		std::string name;
		std::string api;
		bool connected = false;
	};
	struct InputPlayer
	{
		bool enabled = false;	  // false = no emulated controller on this player slot
		std::string type;		  // "Wii U GamePad", "Wii U Pro Controller", ...
		std::vector<InputDevice> devices; // physical devices assigned to the slot
	};
	struct InputPressedButton
	{
		int id = 0;
		std::string emulatedName; // name of the Wii U button
		std::string deviceName;	  // name of the physical button/axis mapped to it
	};
	std::vector<std::string> GetInputControllerTypes();
	InputPlayer GetInputPlayer(int player);
	// physical devices seen by SDL right now
	std::vector<InputDevice> GetAvailableInputDevices();
	// type "" removes the emulated controller of the slot. Both save the profile
	bool SetInputPlayerType(int player, const std::string& type, std::string& errorOut);
	// replaces the devices of the slot by the given one and applies the default mapping. Saves the profile
	bool AssignInputDevice(int player, const std::string& uuid, std::string& errorOut);
	void RemoveInputDevice(int player, const std::string& uuid);
	bool ResetInputMapping(int player, std::string& errorOut);
	// Wii U buttons currently held on this slot, for the test screen
	std::vector<InputPressedButton> GetPressedInputButtons(int player);

	// ---- title manager: installed / discovered titles, install from a folder, delete, convert to .wua
	// (ported from the Android port, NativeGameTitles.cpp and WuaConverter.cpp) ----
	struct TitleEntry
	{
		uint64_t titleId = 0;
		uint64_t uid = 0; // identifies the location of this copy of the title
		std::string name;
		std::string path;
		bool inMlc = false; // installed in the mlc folder
		int version = 0;
		std::string region; // "JPN", "USA", "EUR", ... or empty
		int type = 0;	  // 0 base, 1 update, 2 DLC, 3 system
		int format = 0;	  // 0 folder, 1 WUD/WUX, 2 WUA, 3 NUS, 4 WUHB
	};
	std::vector<TitleEntry> GetTitleEntries();

	enum class InstallCheck
	{
		kInvalid,		  // not a title folder
		kOk,			  // can be installed
		kOverwrite,		  // an older version is installed and gets replaced
		kSameVersion,	  // the same version is already installed
		kNewerInstalled,  // a newer version is installed
		kDifferentType	  // something of another type is installed at the target
	};
	struct InstallInfo
	{
		InstallCheck check = InstallCheck::kInvalid;
		std::string targetPath;
		std::string name;
	};
	// sourcePath: folder of the title (the one that contains code, content and meta)
	InstallInfo CheckInstallTitle(const std::string& sourcePath);
	enum class InstallResult
	{
		kFinished,
		kError,
		kNotEnoughSpace,
		kCancelled
	};
	// blocking, run it from a worker thread. progress(bytesDone, bytesTotal) is called from that thread
	InstallResult InstallTitle(const std::string& sourcePath, const std::function<void(uint64_t, uint64_t)>& progress,
							   const std::atomic_bool* cancel);
	// deletes the files of a title (folder or file) and refreshes the list
	bool DeleteTitle(const std::string& path);

	enum class WuaStage
	{
		kIdle,
		kStarting,
		kCollectingFiles,
		kCompressing,
		kFinalizing,
		kFinished,
		kCancelled,
		kError
	};
	struct WuaProgress
	{
		WuaStage stage = WuaStage::kIdle;
		uint64_t done = 0, total = 0;
	};
	// the base title, its update and its DLC are put in one archive. uid selects the copy to use when a title is present several times
	std::string GetWuaSuggestedName(uint64_t titleId, uint64_t uid);
	bool StartWuaConversion(uint64_t titleId, uint64_t uid, const std::string& outputFile);
	WuaProgress GetWuaProgress();
	void CancelWuaConversion();

	// ---- emulated USB devices: Skylanders portal, Disney Infinity base, LEGO Dimensions toypad
	// (ported from the Android port, NativeEmulatedUSBDevices.cpp). Enable them with the settings
	// "usb_skylander", "usb_infinity" and "usb_dimensions" ----
	enum class FigureKind
	{
		kSkylander,
		kInfinity,
		kDimensions
	};
	struct FigureCatalogEntry
	{
		int64_t number = 0; // Skylander: id, Infinity / Dimensions: figure number
		int variant = 0;	// Skylander only
		std::string name;
	};
	struct InstalledFigure
	{
		std::string name;
		std::string path;
	};
	constexpr int kSkylanderSlots = 16, kInfinitySlots = 9, kDimensionsSlots = 7;
	int GetFigureSlotCount(FigureKind kind);
	std::vector<FigureCatalogEntry> GetFigureCatalog(FigureKind kind);
	std::vector<InstalledFigure> GetInstalledFigures(FigureKind kind);
	// creates a new empty figure file in emulatedUSBDevices/<kind>
	bool CreateFigure(FigureKind kind, int64_t number, int variant);
	bool DeleteFigureFile(const std::string& path);
	bool LoadFigure(FigureKind kind, const std::string& path, int slot);
	void ClearFigure(FigureKind kind, int slot);
	std::string GetFigureInSlot(FigureKind kind, int slot); // empty = nothing

	// ---- graphic packs download (ported from the Android port, GraphicPacksDownloader.kt) ----
	enum class GraphicPackDownloadStatus
	{
		kCheckingVersion,
		kNoUpdateAvailable,
		kDownloading, // progress = 0..1
		kExtracting,  // progress = 0..1
		kFinished,
		kError,
		kCancelled
	};
	// blocking, run it from a worker thread. The callback is called from that thread.
	// Returns the last status. Set *cancel to true to abort (checked between steps and while downloading)
	GraphicPackDownloadStatus DownloadGraphicPacks(const std::function<void(GraphicPackDownloadStatus, double)>& progress,
												   const std::atomic_bool* cancel);
	// reloads the graphic pack list from disk. Does nothing while a title runs
	// per game profile (same files as the desktop interface). Keys: cpu_mode (0 interpreter, 1 single core recompiler,
	// 2 multi core recompiler, 3 auto), thread_quantum, accurate_shader_mul, precompiled_shaders (0 auto, 1 on, 2 off),
	// load_shared_libraries
	int GetGameProfileSetting(uint64_t titleId, const std::string& key);
	void SetGameProfileSetting(uint64_t titleId, const std::string& key, int value);
	void ResetGameProfile(uint64_t titleId);

	void ReloadGraphicPacks();
	int GetGraphicPackCount(); // all loaded graphic packs
	constexpr uint64_t kAllGraphicPacks = ~0ull; // GetGraphicPacks(kAllGraphicPacks) lists every pack

	// ---- accounts (stored like the desktop interface: mlc01/usr/save/system/act/<id>/account.dat) ----
	struct AccountEntry
	{
		uint32_t persistentId = 0;
		std::string miiName;
		int birthYear = 0, birthMonth = 0, birthDay = 0;
		int gender = 0; // 0 = female, 1 = male
		std::string email;
		int country = 0; // index in GetAccountCountries()
		bool validOnline = false;
	};
	struct AccountCountry
	{
		int index = 0;
		std::string name; // two letter code
	};
	constexpr int kMaxAccountCount = 12;
	constexpr uint32_t kMinPersistentId = 0x80000001;
	std::vector<AccountEntry> GetAccounts();
	std::vector<AccountCountry> GetAccountCountries();
	// returns false and sets errorOut if the account can not be created (bad id, already used, ...)
	bool CreateAccount(uint32_t persistentId, const std::string& miiName, std::string& errorOut);
	bool SaveAccount(const AccountEntry& account);
	// refuses to delete the last account
	bool DeleteAccount(uint32_t persistentId);
	uint32_t GetActiveAccountId();
	void SetActiveAccountId(uint32_t persistentId);
	int GetAccountNetworkService(uint32_t persistentId); // 0 offline, 1 Nintendo, 2 Pretendo, 3 custom
	void SetAccountNetworkService(uint32_t persistentId, int service);
	bool IsOTPPresent();
	bool IsSEEPROMPresent();
	// problems that prevent the account from going online. Each entry is "missingOTP", "corruptedOTP", "missingSEEPROM",
	// "corruptedSEEPROM", "missingFile:<path>" or "accountError:<1..4>" (1 no account id, 2 no password cached,
	// 3 password cache empty, 4 no principal id). Empty = ok
	std::vector<std::string> GetAccountValidationErrors(uint32_t persistentId);

	// ---- full input mapping (ported from the Android port, InputMapper / ControllerInputSettingsScreen) ----
	struct InputMapping
	{
		int id = 0;
		std::string emulatedName; // Wii U button or axis
		std::string deviceName;	  // physical button/axis mapped to it, empty = nothing
	};
	// all the mappable buttons of the controller of this slot
	std::vector<InputMapping> GetInputMappings(int player);
	// waits for the next press on one of the assigned controllers (not the on-screen gamepad) and maps it to mappingId.
	// Call StartInputCapture, then PollInputCapture regularly (about every 50 ms). Returns true once a button was captured
	void StartInputCapture();
	bool PollInputCapture(int player, int mappingId);
	void ClearInputMapping(int player, int mappingId);

	// ---- on-screen gamepad (ported from the Android port, input overlay). It is the "Keyboard" device of the player, whose
	// virtual keys are pressed by the controls. Only for the Wii U GamePad controller type. Saved in the controller profile ----
	bool IsTouchGamepadEnabled(int player);
	bool SetTouchGamepadEnabled(int player, bool enabled, std::string& errorOut);
	// shows the gamepad when player 1 has the keyboard device (called when a game starts)
	void SyncTouchGamepad();
	// the next game starts with the layout editor of the on-screen gamepad open
	void SetTouchGamepadEditOnLaunch(bool edit);
	bool IsTouchGamepadEditOnLaunch();

	// ---- navigation of the interface with a gamepad ----
	enum NavAction
	{
		kNavUp = 1,
		kNavDown,
		kNavLeft,
		kNavRight,
		kNavAccept, // south button (A / cross)
		kNavBack,	// east button (B / circle)
		kNavMenu	// north button (Y / triangle)
	};
	// polls every connected gamepad (D-pad, left stick, A/B/Y) and returns the actions triggered since the last call,
	// with key repeat for the directions. Call regularly from the UI thread while no game runs
	std::vector<int> PollUiNavigation();
}
