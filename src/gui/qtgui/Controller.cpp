#include "Controller.h"
#include "GameListModel.h"
#include "GameWindow.h"
#include "MobileBackend.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>
#include <QDir>
#include <QVariantMap>

#include <unistd.h>

Controller::Controller(QObject* parent) : QObject(parent)
{
	MobileBackend::Initialize();

	m_games = new GameListModel(this);

	// title list events arrive in bursts (one per discovered title) and from other threads: coalesce them
	m_reloadTimer.setSingleShot(true);
	m_reloadTimer.setInterval(150);
	connect(&m_reloadTimer, &QTimer::timeout, this, [this] { m_games->reload(); });

	MobileBackend::SetGameListChangedCallback([this] {
		QMetaObject::invokeMethod(this, [this] { m_reloadTimer.start(); }, Qt::QueuedConnection);
	});
	MobileBackend::SetGameExitedCallback([this] {
		QMetaObject::invokeMethod(this, [this] { endSession(); }, Qt::QueuedConnection);
	});

	m_scanPollTimer.setInterval(400);
	connect(&m_scanPollTimer, &QTimer::timeout, this, [this] {
		const bool scanning = MobileBackend::IsScanning();
		if (scanning == m_scanning)
			return;
		m_scanning = scanning;
		Q_EMIT scanningChanged();
		if (!scanning)
			m_games->reload();
	});
	m_scanPollTimer.start();

	m_scanning = MobileBackend::IsScanning();
	m_games->reload();

	// gamepad navigation: the actions are turned into key events for the focused window
	m_navTimer.setInterval(16);
	connect(&m_navTimer, &QTimer::timeout, this, &Controller::pollNavigation);
	m_navTimer.start();
}

namespace
{
	// returns whether an item of the window handled the key press
	bool sendKey(QWindow* window, int key)
	{
		QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
		QCoreApplication::sendEvent(window, &press);
		QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
		QCoreApplication::sendEvent(window, &release);
		return press.isAccepted();
	}
}

void Controller::pollNavigation()
{
	if (m_gameRunning || m_launching)
		return;
	const std::vector<int> actions = MobileBackend::PollUiNavigation();
	QWindow* window = QGuiApplication::focusWindow();
	if (!window)
		return;
	for (const int action : actions)
	{
		switch (action)
		{
		case MobileBackend::kNavUp: sendKey(window, Qt::Key_Up); break;
		case MobileBackend::kNavDown: sendKey(window, Qt::Key_Down); break;
		case MobileBackend::kNavLeft: sendKey(window, Qt::Key_Left); break;
		case MobileBackend::kNavRight: sendKey(window, Qt::Key_Right); break;
		case MobileBackend::kNavAccept:
			// Return first (lists, combo box popups), Space for buttons and switches
			if (!sendKey(window, Qt::Key_Return))
				sendKey(window, Qt::Key_Space);
			break;
		case MobileBackend::kNavBack: sendKey(window, Qt::Key_Escape); break;
		case MobileBackend::kNavMenu: sendKey(window, Qt::Key_Menu); break;
		default: break;
		}
	}
}

Controller::~Controller()
{
	MobileBackend::SetGameListChangedCallback(nullptr);
	MobileBackend::SetGameExitedCallback(nullptr);
	m_downloadCancel = true;
	if (m_downloadThread.joinable())
		m_downloadThread.join();
}

QAbstractItemModel* Controller::games() const
{
	return m_games;
}

QStringList Controller::gamePaths() const
{
	QStringList result;
	for (const std::string& p : MobileBackend::GetGamePaths())
		result.push_back(QString::fromStdString(p));
	return result;
}

QString Controller::version() const
{
	return QString::fromStdString(MobileBackend::GetVersionString());
}

void Controller::refresh()
{
	MobileBackend::RefreshGameList();
}

void Controller::addGamePath(const QUrl& folder)
{
	addLocalGamePath(folder.isLocalFile() ? folder.toLocalFile() : folder.toString());
}

void Controller::addLocalGamePath(const QString& rawPath)
{
	QString path = rawPath.trimmed();
	if (path.startsWith(QLatin1String("~/")))
		path = QDir::homePath() + path.mid(1);
	if (path.isEmpty())
		return;
	path = QDir::cleanPath(path);
	MobileBackend::AddGamePath(path.toStdString());
	Q_EMIT gamePathsChanged();
}

void Controller::removeGamePath(const QString& path)
{
	MobileBackend::RemoveGamePath(path.toStdString());
	Q_EMIT gamePathsChanged();
}

void Controller::setLaunching(bool launching)
{
	if (m_launching == launching)
		return;
	m_launching = launching;
	Q_EMIT launchingChanged();
}

void Controller::launch(const QString& titleId)
{
	if (m_window || m_gameRunning || m_launching)
		return;
	bool ok = false;
	m_pendingTitleId = titleId.toULongLong(&ok, 16);
	if (!ok)
		return;
	setLaunching(true);

	m_window = new GameWindow();
	// queued: creating the swapchain from inside the expose handler would re-enter the platform plugin
	connect(m_window, &GameWindow::ready, this, &Controller::onWindowReady, Qt::QueuedConnection);
	connect(m_window, &GameWindow::closed, this, &Controller::onWindowClosed);
	m_window->showFullScreen();
}

void Controller::onWindowReady()
{
	if (!m_window || m_gameRunning)
		return;
	std::string error;
	const MobileBackend::NativeWindow nativeWindow = m_window->nativeWindow();
	if (!MobileBackend::LaunchGame(m_pendingTitleId, nativeWindow, error))
	{
		GameWindow* window = m_window;
		m_window = nullptr;
		window->hide();
		window->deleteLater();
		setLaunching(false);
		Q_EMIT errorOccurred(QString::fromStdString(error));
		return;
	}
	m_gameRunning = true;
	Q_EMIT gameRunningChanged();
	setLaunching(false);
	m_window->requestActivate();
}

void Controller::onWindowClosed()
{
	if (m_gameRunning)
	{
		endSession();
		return;
	}
	// closed before the game started
	if (m_window)
	{
		m_window->deleteLater();
		m_window = nullptr;
	}
	setLaunching(false);
}

void Controller::endSession()
{
	if (m_sessionEnded)
		return;
	m_sessionEnded = true;
	// Stopping a title and starting another one inside the same process is not reliable in the emulator core
	// (the desktop UI has the same limitation). Shut down cleanly and start over with a fresh process instead,
	// which brings the user straight back to the game list.
	shutdown();
	MobileBackend::RestartProcess();
	// Only reached if the restart failed. Leave without running static destructors: the Vulkan swapchain
	// still references this window's surface and tearing both down in arbitrary order can crash.
	::_exit(0);
}

// ---- accounts ----

QVariantList Controller::accounts() const
{
	QVariantList result;
	for (const auto& a : MobileBackend::GetAccounts())
	{
		QVariantMap m;
		m[QStringLiteral("id")] = static_cast<double>(a.persistentId);
		m[QStringLiteral("miiName")] = QString::fromStdString(a.miiName);
		m[QStringLiteral("birthYear")] = a.birthYear;
		m[QStringLiteral("birthMonth")] = a.birthMonth;
		m[QStringLiteral("birthDay")] = a.birthDay;
		m[QStringLiteral("gender")] = a.gender;
		m[QStringLiteral("email")] = QString::fromStdString(a.email);
		m[QStringLiteral("country")] = a.country;
		m[QStringLiteral("valid")] = a.validOnline;
		result.push_back(m);
	}
	return result;
}

QVariantList Controller::accountCountries() const
{
	QVariantList result;
	for (const auto& c : MobileBackend::GetAccountCountries())
	{
		QVariantMap m;
		m[QStringLiteral("index")] = c.index;
		m[QStringLiteral("name")] = QString::fromStdString(c.name);
		result.push_back(m);
	}
	return result;
}

double Controller::activeAccountId() const
{
	return MobileBackend::GetActiveAccountId();
}

void Controller::setActiveAccount(double id)
{
	MobileBackend::SetActiveAccountId(static_cast<uint32_t>(id));
	Q_EMIT accountsChanged();
}

int Controller::accountNetworkService(double id) const
{
	return MobileBackend::GetAccountNetworkService(static_cast<uint32_t>(id));
}

void Controller::setAccountNetworkService(double id, int service)
{
	MobileBackend::SetAccountNetworkService(static_cast<uint32_t>(id), service);
	Q_EMIT accountsChanged();
}

bool Controller::createAccount(const QString& hexId, const QString& miiName)
{
	bool parsed = false;
	QString digits = hexId.trimmed();
	if (digits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
		digits = digits.mid(2);
	const qulonglong id = digits.toULongLong(&parsed, 16);
	if (!parsed || id > 0xFFFFFFFFull)
	{
		Q_EMIT errorOccurred(tr("The persistent id must be a hexadecimal number (for example 80000002)"));
		return false;
	}
	if (id < MobileBackend::kMinPersistentId)
	{
		Q_EMIT errorOccurred(tr("The persistent id must be at least 80000001"));
		return false;
	}
	if (miiName.trimmed().isEmpty())
	{
		Q_EMIT errorOccurred(tr("The account name may not be empty"));
		return false;
	}
	std::string error;
	if (!MobileBackend::CreateAccount(static_cast<uint32_t>(id), miiName.trimmed().toStdString(), error))
	{
		Q_EMIT errorOccurred(tr("Could not create the account: %1").arg(QString::fromStdString(error)));
		return false;
	}
	Q_EMIT accountsChanged();
	return true;
}

double Controller::suggestedAccountId() const
{
	uint32_t id = MobileBackend::kMinPersistentId;
	bool used = true;
	const auto accounts = MobileBackend::GetAccounts();
	while (used)
	{
		used = false;
		for (const auto& a : accounts)
		{
			if (a.persistentId == id)
			{
				used = true;
				++id;
				break;
			}
		}
	}
	return id;
}

void Controller::saveAccount(const QVariantMap& account)
{
	MobileBackend::AccountEntry a;
	a.persistentId = static_cast<uint32_t>(account.value(QStringLiteral("id")).toDouble());
	a.miiName = account.value(QStringLiteral("miiName")).toString().toStdString();
	a.birthYear = account.value(QStringLiteral("birthYear")).toInt();
	a.birthMonth = account.value(QStringLiteral("birthMonth")).toInt();
	a.birthDay = account.value(QStringLiteral("birthDay")).toInt();
	a.gender = account.value(QStringLiteral("gender")).toInt();
	a.email = account.value(QStringLiteral("email")).toString().toStdString();
	a.country = account.value(QStringLiteral("country")).toInt();
	if (!MobileBackend::SaveAccount(a))
		Q_EMIT errorOccurred(tr("Could not save the account"));
	Q_EMIT accountsChanged();
}

bool Controller::deleteAccount(double id)
{
	const bool ok = MobileBackend::DeleteAccount(static_cast<uint32_t>(id));
	if (!ok)
		Q_EMIT errorOccurred(tr("Could not delete the account (at least one account must remain)"));
	Q_EMIT accountsChanged();
	return ok;
}

bool Controller::otpPresent() const
{
	return MobileBackend::IsOTPPresent();
}

bool Controller::seepromPresent() const
{
	return MobileBackend::IsSEEPROMPresent();
}

int Controller::maxAccountCount() const
{
	return MobileBackend::kMaxAccountCount;
}

QStringList Controller::accountValidationErrors(double id) const
{
	QStringList result;
	for (const std::string& e : MobileBackend::GetAccountValidationErrors(static_cast<uint32_t>(id)))
	{
		const QString code = QString::fromStdString(e);
		if (code == QLatin1String("missingOTP"))
			result << tr("otp.bin is missing");
		else if (code == QLatin1String("corruptedOTP"))
			result << tr("otp.bin is corrupted");
		else if (code == QLatin1String("missingSEEPROM"))
			result << tr("seeprom.bin is missing");
		else if (code == QLatin1String("corruptedSEEPROM"))
			result << tr("seeprom.bin is corrupted");
		else if (code.startsWith(QLatin1String("missingFile:")))
			result << tr("Missing file: %1").arg(code.mid(12));
		else if (code == QLatin1String("accountError:1"))
			result << tr("AccountId missing (the account is not connected to a NNID/PNID)");
		else if (code == QLatin1String("accountError:2"))
			result << tr("AccountPasswordCache is missing (the remember password option must be enabled on the Wii U for this account before dumping it)");
		else if (code == QLatin1String("accountError:3"))
			result << tr("AccountPasswordCache is empty (the remember password option must be enabled on the Wii U for this account before dumping it)");
		else if (code == QLatin1String("accountError:4"))
			result << tr("PrincipalId missing");
		else
			result << code;
	}
	return result;
}

// ---- graphic packs download ----

void Controller::downloadGraphicPacks()
{
	if (m_downloadingPacks)
		return;
	if (m_downloadThread.joinable())
		m_downloadThread.join();
	m_downloadCancel = false;
	setDownloadState(true, MobileBackend::GraphicPackDownloadStatus::kCheckingVersion, 0.0);
	m_downloadThread = std::thread([this] {
		MobileBackend::DownloadGraphicPacks(
			[this](MobileBackend::GraphicPackDownloadStatus status, double progress) {
				QMetaObject::invokeMethod(this, [this, status, progress] {
					const bool done = status == MobileBackend::GraphicPackDownloadStatus::kNoUpdateAvailable ||
									  status == MobileBackend::GraphicPackDownloadStatus::kFinished ||
									  status == MobileBackend::GraphicPackDownloadStatus::kError ||
									  status == MobileBackend::GraphicPackDownloadStatus::kCancelled;
					setDownloadState(!done, status, progress);
				}, Qt::QueuedConnection);
			},
			&m_downloadCancel);
	});
}

void Controller::cancelGraphicPackDownload()
{
	m_downloadCancel = true;
}

void Controller::setDownloadState(bool running, MobileBackend::GraphicPackDownloadStatus status, double progress)
{
	using S = MobileBackend::GraphicPackDownloadStatus;
	QString text;
	switch (status)
	{
	case S::kCheckingVersion: text = tr("Checking version..."); break;
	case S::kNoUpdateAvailable: text = tr("No updates available."); break;
	case S::kDownloading: text = tr("Downloading..."); break;
	case S::kExtracting: text = tr("Extracting..."); break;
	case S::kFinished: text = tr("Graphic packs downloaded."); break;
	case S::kError: text = tr("Failed to download the graphic packs."); break;
	case S::kCancelled: text = tr("Cancelled."); break;
	}
	m_downloadingPacks = running;
	m_downloadText = text;
	m_downloadProgress = progress;
	Q_EMIT graphicPackDownloadChanged();
}

void Controller::shutdown()
{
	m_scanPollTimer.stop();
	m_reloadTimer.stop();
	MobileBackend::Shutdown();
}

// ---------------------------------------------------------------- settings

namespace
{
	quint64 ParseTitleId(const QString& titleId)
	{
		bool ok = false;
		const quint64 id = titleId.toULongLong(&ok, 16);
		return ok ? id : 0;
	}
} // namespace

int Controller::setting(const QString& key) const
{
	return MobileBackend::GetSetting(key.toStdString());
}

void Controller::setSetting(const QString& key, int value)
{
	MobileBackend::SetSetting(key.toStdString(), value);
}

QVariantList Controller::audioDevices() const
{
	QVariantList result;
	result.push_back(QVariantMap{{"id", QString()}, {"name", tr("Disabled")}});
	for (const auto& device : MobileBackend::GetAudioDevices())
		result.push_back(QVariantMap{{"id", QString::fromStdString(device.id)}, {"name", QString::fromStdString(device.name)}});
	return result;
}

QString Controller::tvAudioDevice() const
{
	return QString::fromStdString(MobileBackend::GetTVAudioDevice());
}

void Controller::setTvAudioDevice(const QString& id)
{
	MobileBackend::SetTVAudioDevice(id.toStdString());
}

// ---------------------------------------------------------------- per game actions

int Controller::shaderCacheCount(const QString& titleId) const
{
	const quint64 id = ParseTitleId(titleId);
	return id ? MobileBackend::CountShaderCaches(id) : 0;
}

int Controller::removeShaderCaches(const QString& titleId)
{
	const quint64 id = ParseTitleId(titleId);
	return id ? MobileBackend::RemoveShaderCaches(id) : 0;
}

QVariantList Controller::graphicPacks(const QString& titleId) const
{
	QVariantList result;
	// "universal" = the packs that apply to every game
	const bool universal = titleId == QLatin1String("universal");
	const quint64 id = universal ? 0 : ParseTitleId(titleId);
	if (!id && !universal)
		return result;
	for (const auto& gp : MobileBackend::GetGraphicPacks(id))
	{
		QVariantList groups;
		for (const auto& group : gp.presetGroups)
		{
			QStringList presets;
			for (const auto& p : group.presets)
				presets.push_back(QString::fromStdString(p));
			groups.push_back(QVariantMap{
				{"category", QString::fromStdString(group.category)},
				{"presets", presets},
				{"active", QString::fromStdString(group.active)},
			});
		}
		result.push_back(QVariantMap{
			{"index", gp.index},
			{"name", QString::fromStdString(gp.name)},
			{"path", QString::fromStdString(gp.path)},
			{"description", QString::fromStdString(gp.description)},
			{"enabled", gp.enabled},
			{"presets", groups},
		});
	}
	return result;
}

void Controller::setGraphicPackEnabled(int index, bool enabled)
{
	MobileBackend::SetGraphicPackEnabled(index, enabled);
}

void Controller::setGraphicPackPreset(int index, const QString& category, const QString& preset)
{
	MobileBackend::SetGraphicPackPreset(index, category.toStdString(), preset.toStdString());
}

// ---------------------------------------------------------------- controllers

namespace
{
	QVariantMap deviceToVariant(const MobileBackend::InputDevice& d)
	{
		QVariantMap m;
		m[QStringLiteral("uuid")] = QString::fromStdString(d.uuid);
		m[QStringLiteral("name")] = QString::fromStdString(d.name);
		m[QStringLiteral("api")] = QString::fromStdString(d.api);
		m[QStringLiteral("connected")] = d.connected;
		return m;
	}
}

QStringList Controller::inputTypes() const
{
	QStringList result;
	for (const std::string& t : MobileBackend::GetInputControllerTypes())
		result.push_back(QString::fromStdString(t));
	return result;
}

QVariantMap Controller::inputPlayer(int player) const
{
	const MobileBackend::InputPlayer info = MobileBackend::GetInputPlayer(player);
	QVariantMap m;
	m[QStringLiteral("enabled")] = info.enabled;
	m[QStringLiteral("type")] = QString::fromStdString(info.type);
	QVariantList devices;
	for (const auto& d : info.devices)
		devices.push_back(deviceToVariant(d));
	m[QStringLiteral("devices")] = devices;
	return m;
}

QVariantList Controller::availableInputDevices() const
{
	QVariantList result;
	for (const auto& d : MobileBackend::GetAvailableInputDevices())
		result.push_back(deviceToVariant(d));
	return result;
}

bool Controller::setInputPlayerType(int player, const QString& type)
{
	std::string error;
	const bool ok = MobileBackend::SetInputPlayerType(player, type.toStdString(), error);
	if (!ok)
		Q_EMIT errorOccurred(QString::fromStdString(error));
	Q_EMIT inputChanged();
	return ok;
}

bool Controller::assignInputDevice(int player, const QString& uuid)
{
	std::string error;
	const bool ok = MobileBackend::AssignInputDevice(player, uuid.toStdString(), error);
	if (!ok)
		Q_EMIT errorOccurred(QString::fromStdString(error));
	Q_EMIT inputChanged();
	return ok;
}

void Controller::removeInputDevice(int player, const QString& uuid)
{
	MobileBackend::RemoveInputDevice(player, uuid.toStdString());
	Q_EMIT inputChanged();
}

bool Controller::resetInputMapping(int player)
{
	std::string error;
	const bool ok = MobileBackend::ResetInputMapping(player, error);
	if (!ok)
		Q_EMIT errorOccurred(QString::fromStdString(error));
	Q_EMIT inputChanged();
	return ok;
}

QVariantList Controller::pressedInputButtons(int player) const
{
	QVariantList result;
	for (const auto& b : MobileBackend::GetPressedInputButtons(player))
	{
		QVariantMap m;
		m[QStringLiteral("id")] = b.id;
		m[QStringLiteral("name")] = QString::fromStdString(b.emulatedName);
		m[QStringLiteral("device")] = QString::fromStdString(b.deviceName);
		result.push_back(m);
	}
	return result;
}
