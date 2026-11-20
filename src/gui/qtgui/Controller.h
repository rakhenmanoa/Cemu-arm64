#pragma once

#include "MobileBackend.h"

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <atomic>
#include <thread>

class GameListModel;
class GameWindow;

// Object exposed to QML as "cemu". Glue between the QML pages and MobileBackend.
class Controller : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QAbstractItemModel* games READ games CONSTANT)
	Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
	Q_PROPERTY(bool launching READ launching NOTIFY launchingChanged)
	Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY gameRunningChanged)
	Q_PROPERTY(QStringList gamePaths READ gamePaths NOTIFY gamePathsChanged)
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(bool installingTitle READ installingTitle NOTIFY titleInstallChanged)
	Q_PROPERTY(double titleInstallProgress READ titleInstallProgress NOTIFY titleInstallChanged)
	Q_PROPERTY(bool downloadingGraphicPacks READ downloadingGraphicPacks NOTIFY graphicPackDownloadChanged)
	Q_PROPERTY(QString graphicPackDownloadText READ graphicPackDownloadText NOTIFY graphicPackDownloadChanged)
	Q_PROPERTY(double graphicPackDownloadProgress READ graphicPackDownloadProgress NOTIFY graphicPackDownloadChanged)
  public:
	explicit Controller(QObject* parent = nullptr);
	~Controller() override;

	QAbstractItemModel* games() const;
	bool scanning() const { return m_scanning; }
	bool launching() const { return m_launching; }
	bool gameRunning() const { return m_gameRunning; }
	QStringList gamePaths() const;
	QString version() const;
	bool installingTitle() const { return m_installingTitle; }
	double titleInstallProgress() const { return m_titleInstallProgress; }
	bool downloadingGraphicPacks() const { return m_downloadingPacks; }
	QString graphicPackDownloadText() const { return m_downloadText; }
	double graphicPackDownloadProgress() const { return m_downloadProgress; }

	Q_INVOKABLE void refresh();
	Q_INVOKABLE void launch(const QString& titleId);
	Q_INVOKABLE void addGamePath(const QUrl& folder);
	Q_INVOKABLE void addLocalGamePath(const QString& path);
	Q_INVOKABLE void removeGamePath(const QString& path);

	// settings shared with the desktop interface. Keys: see SettingTable() in MobileBackend.cpp
	Q_INVOKABLE int setting(const QString& key) const;
	Q_INVOKABLE void setSetting(const QString& key, int value);
	Q_INVOKABLE QVariantList audioDevices() const; // [{id, name}], first entry = disabled
	Q_INVOKABLE QString tvAudioDevice() const;
	Q_INVOKABLE void setTvAudioDevice(const QString& id);

	// per game actions
	Q_INVOKABLE int shaderCacheCount(const QString& titleId) const;
	Q_INVOKABLE int removeShaderCaches(const QString& titleId);
	// [{index, name, path, description, enabled, presets: [{category, presets: [..], active}]}]
	Q_INVOKABLE QVariantList graphicPacks(const QString& titleId) const;
	Q_INVOKABLE void setGraphicPackEnabled(int index, bool enabled);
	Q_INVOKABLE void setGraphicPackPreset(int index, const QString& category, const QString& preset);

	// controllers. player 0 = player 1. Profiles are shared with the desktop interface
	Q_INVOKABLE QStringList inputTypes() const;
	Q_INVOKABLE QVariantMap inputPlayer(int player) const; // {enabled, type, devices: [{uuid, name, api, connected}]}
	Q_INVOKABLE QVariantList availableInputDevices() const; // [{uuid, name, api, connected}] seen by SDL right now
	Q_INVOKABLE bool setInputPlayerType(int player, const QString& type); // empty type = disabled
	Q_INVOKABLE bool assignInputDevice(int player, const QString& uuid);
	Q_INVOKABLE void removeInputDevice(int player, const QString& uuid);
	Q_INVOKABLE bool resetInputMapping(int player);
	Q_INVOKABLE QVariantList pressedInputButtons(int player) const; // [{id, name, device}]

	// downloads the community graphic packs (same files as the desktop interface). graphicPacks("universal") lists the
	// packs that apply to every game
	Q_INVOKABLE void downloadGraphicPacks();
	Q_INVOKABLE void cancelGraphicPackDownload();

	// title manager. type: 0 base, 1 update, 2 DLC, 3 system. format: 0 folder, 1 WUD/WUX, 2 WUA, 3 NUS, 4 WUHB
	Q_INVOKABLE QVariantList titleEntries() const; // [{titleId, uid, name, path, inMlc, version, region, type, format}]
	// {status: invalid|ok|overwrite|sameVersion|newerInstalled|differentType, name, target}
	Q_INVOKABLE QVariantMap checkInstallTitle(const QUrl& folder) const;
	Q_INVOKABLE void installTitle(const QUrl& folder);
	Q_INVOKABLE void cancelTitleInstall();
	Q_INVOKABLE bool deleteTitle(const QString& path);
	Q_INVOKABLE QString wuaSuggestedName(const QString& titleId, const QString& uid) const;
	Q_INVOKABLE bool startWuaConversion(const QString& titleId, const QString& uid, const QUrl& outputFile);
	Q_INVOKABLE QVariantMap wuaProgress() const; // {stage: idle|starting|collecting|compressing|finalizing|finished|cancelled|error, done, total}
	Q_INVOKABLE void cancelWuaConversion();

	// emulated USB devices. kind: 0 Skylanders portal, 1 Infinity base, 2 Dimensions toypad
	Q_INVOKABLE QVariantList usbFigureSlots(int kind) const;	  // [{slot, name}] name empty = nothing loaded
	Q_INVOKABLE QVariantList usbFigureCatalog(int kind) const;	  // [{number, variant, name}] figures that can be created
	Q_INVOKABLE QVariantList usbInstalledFigures(int kind) const; // [{name, path}] figure files already created
	Q_INVOKABLE bool usbCreateFigure(int kind, double number, int variant);
	Q_INVOKABLE bool usbDeleteFigureFile(const QString& path);
	Q_INVOKABLE bool usbLoadFigure(int kind, const QString& path, int slot);
	Q_INVOKABLE void usbClearFigure(int kind, int slot);

	// accounts. ids are the persistent ids (0x80000001 and up)
	// [{id, miiName, birthYear, birthMonth, birthDay, gender, email, country, valid}]
	Q_INVOKABLE QVariantList accounts() const;
	Q_INVOKABLE QVariantList accountCountries() const; // [{index, name}]
	Q_INVOKABLE double activeAccountId() const;
	Q_INVOKABLE void setActiveAccount(double id);
	Q_INVOKABLE int accountNetworkService(double id) const; // 0 offline, 1 Nintendo, 2 Pretendo, 3 custom
	Q_INVOKABLE void setAccountNetworkService(double id, int service);
	Q_INVOKABLE bool createAccount(const QString& hexId, const QString& miiName); // false: error signal emitted
	Q_INVOKABLE double suggestedAccountId() const; // first free id
	Q_INVOKABLE void saveAccount(const QVariantMap& account);
	Q_INVOKABLE bool deleteAccount(double id);
	Q_INVOKABLE bool otpPresent() const;
	Q_INVOKABLE bool seepromPresent() const;
	Q_INVOKABLE QStringList accountValidationErrors(double id) const; // translated, empty = ok
	Q_INVOKABLE int maxAccountCount() const;

	// stops the emulator. Must be called while the render window still exists
	void shutdown();

  Q_SIGNALS:
	void scanningChanged();
	void launchingChanged();
	void gameRunningChanged();
	void gamePathsChanged();
	void inputChanged();
	void accountsChanged();
	void usbDevicesChanged();
	void titleInstallChanged();
	void titleInstallFinished(const QString& result); // finished|error|notEnoughSpace|cancelled
	void graphicPackDownloadChanged();
	void errorOccurred(const QString& message);

  private:
	void pollNavigation();
	void onWindowReady();
	void onWindowClosed();
	void endSession();
	void setLaunching(bool launching);
	void setDownloadState(bool running, MobileBackend::GraphicPackDownloadStatus status, double progress);

	GameListModel* m_games = nullptr;
	GameWindow* m_window = nullptr;
	QTimer m_reloadTimer;
	QTimer m_scanPollTimer;
	QTimer m_navTimer; // gamepad navigation of the interface
	quint64 m_pendingTitleId = 0;
	bool m_scanning = false;
	bool m_launching = false;
	bool m_gameRunning = false;
	bool m_sessionEnded = false;
	bool m_downloadingPacks = false;
	QString m_downloadText;
	double m_downloadProgress = 0.0;
	std::atomic_bool m_downloadCancel = false;
	std::thread m_downloadThread;
	bool m_installingTitle = false;
	double m_titleInstallProgress = 0.0;
	std::atomic_bool m_installCancel = false;
	std::thread m_installThread;
};
