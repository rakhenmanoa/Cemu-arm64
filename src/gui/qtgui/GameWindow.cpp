#include "GameWindow.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QTouchEvent>
#include <qpa/qplatformnativeinterface.h>

#include <cmath>

GameWindow::GameWindow(QWindow* parent) : QWindow(parent)
{
	// Qt never draws into this window, the emulator presents to it through its own Vulkan swapchain
	setSurfaceType(QSurface::VulkanSurface);
	setTitle(QStringLiteral("Cemu"));
	setMinimumSize(QSize(160, 90));

	// some compositors only send the first expose once a buffer is attached; don't wait forever
	m_readyFallback.setSingleShot(true);
	m_readyFallback.setInterval(2000);
	connect(&m_readyFallback, &QTimer::timeout, this, [this] {
		if (isVisible())
			notifyReady();
	});
	connect(this, &QWindow::visibleChanged, this, [this](bool visible) {
		if (visible && !m_readyEmitted)
			m_readyFallback.start();
	});
	connect(this, &QWindow::screenChanged, this, [this] { pushSize(); });
}

MobileBackend::NativeWindow GameWindow::nativeWindow()
{
	MobileBackend::NativeWindow nw;
	create(); // make sure the platform window exists
	QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
	const QString platform = QGuiApplication::platformName();
	if (pni)
	{
		if (platform.startsWith(QLatin1String("wayland")))
		{
			nw.isWayland = true;
			nw.display = pni->nativeResourceForWindow(QByteArrayLiteral("display"), this);
			nw.surface = pni->nativeResourceForWindow(QByteArrayLiteral("surface"), this);
		}
		else if (platform == QLatin1String("xcb"))
		{
			nw.isWayland = false;
			nw.display = pni->nativeResourceForWindow(QByteArrayLiteral("display"), this);
			nw.surface = reinterpret_cast<void*>(static_cast<quintptr>(winId()));
		}
	}
	const qreal dpr = devicePixelRatio();
	nw.width = width();
	nw.height = height();
	nw.physWidth = qMax(1, static_cast<int>(std::lround(width() * dpr)));
	nw.physHeight = qMax(1, static_cast<int>(std::lround(height() * dpr)));
	nw.scale = dpr;
	return nw;
}

void GameWindow::notifyReady()
{
	if (m_readyEmitted)
		return;
	m_readyEmitted = true;
	m_readyFallback.stop();
	pushSize();
	Q_EMIT ready();
}

void GameWindow::pushSize()
{
	if (width() <= 0 || height() <= 0)
		return;
	const qreal dpr = devicePixelRatio();
	MobileBackend::SetWindowSize(width(), height(), qMax(1, static_cast<int>(std::lround(width() * dpr))),
								 qMax(1, static_cast<int>(std::lround(height() * dpr))), dpr);
}

bool GameWindow::event(QEvent* event)
{
	switch (event->type())
	{
	case QEvent::Close:
		Q_EMIT closed();
		break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
	case QEvent::DevicePixelRatioChange:
		pushSize();
		break;
#endif
	default:
		break;
	}
	return QWindow::event(event);
}

void GameWindow::exposeEvent(QExposeEvent*)
{
	if (isExposed())
		notifyReady();
}

void GameWindow::resizeEvent(QResizeEvent*)
{
	pushSize();
}

void GameWindow::keyPressEvent(QKeyEvent* event)
{
	if (!event->isAutoRepeat() && event->nativeVirtualKey() != 0)
		MobileBackend::SetKeyState(event->nativeVirtualKey(), true);
}

void GameWindow::keyReleaseEvent(QKeyEvent* event)
{
	if (!event->isAutoRepeat() && event->nativeVirtualKey() != 0)
		MobileBackend::SetKeyState(event->nativeVirtualKey(), false);
}

void GameWindow::pushMouse(QMouseEvent* event)
{
	const qreal dpr = devicePixelRatio();
	const QPointF pos = event->position() * dpr;
	MobileBackend::SetMouse(static_cast<int>(pos.x()), static_cast<int>(pos.y()), event->buttons().testFlag(Qt::LeftButton),
							event->buttons().testFlag(Qt::RightButton));
}

void GameWindow::mousePressEvent(QMouseEvent* event)
{
	pushMouse(event);
}

void GameWindow::mouseReleaseEvent(QMouseEvent* event)
{
	pushMouse(event);
}

void GameWindow::mouseMoveEvent(QMouseEvent* event)
{
	pushMouse(event);
}

void GameWindow::touchEvent(QTouchEvent* event)
{
	// every finger goes to the backend: with the on-screen gamepad some of them drive the controls, the first other
	// one is the single-touch Wii U GamePad screen
	const qreal dpr = devicePixelRatio();
	const bool ending = event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel;
	std::vector<MobileBackend::TouchPoint> points;
	for (const QEventPoint& point : event->points())
	{
		const QPointF pos = point.position() * dpr;
		MobileBackend::TouchPoint p;
		p.id = point.id();
		p.x = static_cast<int>(pos.x());
		p.y = static_cast<int>(pos.y());
		p.down = !ending && point.state() != QEventPoint::Released;
		points.push_back(p);
	}
	MobileBackend::SetTouchPoints(points);
	event->accept();
}

void GameWindow::focusInEvent(QFocusEvent*)
{
	MobileBackend::SetAppActive(true);
}

void GameWindow::focusOutEvent(QFocusEvent*)
{
	// drop held keys, otherwise they stay pressed while the window is in the background
	MobileBackend::ReleaseAllKeys();
}
