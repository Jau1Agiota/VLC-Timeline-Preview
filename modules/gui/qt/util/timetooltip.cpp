/*****************************************************************************
 * Copyright © 2011-2012 VideoLAN
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "timetooltip.hpp"
#include "thumbnailpreview.hpp"

#include <QApplication>
#include <QPainter>
#include <QBitmap>
#include <QWindow>
#ifdef Q_OS_WIN
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#endif
#include <QFontMetrics>
#include <QEvent>
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
#include <QDesktopWidget>
#else
#include <QScreen>
#endif

#define TIP_HEIGHT 5

TimeTooltip::TimeTooltip( QWidget *parent ) :
    QWidget( parent )
{
    setWindowFlags( Qt::ToolTip                 |
                    Qt::WindowStaysOnTopHint    |
                    Qt::FramelessWindowHint     );

    // Tell Qt that it doesn't need to erase the background before
    // a paintEvent occurs. This should save some CPU cycles.
    setAttribute( Qt::WA_OpaquePaintEvent );
    setAttribute( Qt::WA_TranslucentBackground );
    setAttribute( Qt::WA_TransparentForMouseEvents );

    // Inherit from the system default font size -5
    mFont = QFont( "Verdana", qMax( qApp->font().pointSize() - 5, 7 ) );
    mTipX = -1;

    // By default the widget is unintialized and should not be displayed
    resize( 0, 0 );
}

void TimeTooltip::adjustPosition()
{
    if( mDisplayedText.isEmpty() )
    {
        resize( 0, 0 );
        diagnose( QStringLiteral("placement-empty"), QStringLiteral("no-displayed-text"), 0 );
        return;
    }

    // Get the bounding box required to print the text and add some padding
    QFontMetrics metrics( mFont );
    // The experimental backend may retain an intermediate image while the
    // cursor advances. Distinguish current hover from the image's own request.
    const QString imageLabel = mMpvPreview ?
        QStringLiteral("hover ") + mHoverTime + (qgetenv("VLC_MPV_SEEK") == "exact" ? QStringLiteral(" | req ") : QStringLiteral(" | image ")) + mPreviewTime : mPreviewTime;
    mPreviewLabel = metrics.elidedText(imageLabel +
        (mMpvPreview || mText.isEmpty() ? QString() : QStringLiteral(" - ") + mText),
        Qt::ElideRight, qMax(140, mPreview.width()));
    const QString label = mPreview.isNull() ? mDisplayedText : mPreviewLabel;
    QRect textbox = metrics.boundingRect( label );
    textbox.adjust( -2, -2, 2, 2 );
    textbox.moveTo( 0, 0 );

    mPreviewTextBox = QRect();
    if( !mPreview.isNull() )
    {
        const int textHeight = textbox.height();
        textbox.setWidth( qMax( textbox.width(), mPreview.width() + 8 ) );
        textbox.setHeight( textHeight + mPreview.height() + 8 );
        mPreviewTextBox = QRect( 0, mPreview.height() + 8, textbox.width(), textHeight );
    }

    // Resize the widget to fit our needs
    QSize size( textbox.width() + 1, textbox.height() + TIP_HEIGHT + 1 );

    // The desired label position is just above the target
    QPoint position( mTarget.x() - size.width() / 2,
#if defined( Q_OS_WIN )
        mTarget.y() - 2 * size.height() - TIP_HEIGHT / 2 );
#else
        mTarget.y() - size.height() - TIP_HEIGHT / 2 );
#endif

    // Keep the tooltip on the same screen if possible
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QRect screen{};
    auto *screenAt = QGuiApplication::screenAt( mTarget );
    if (screenAt != nullptr)
        screen = screenAt->geometry();
#else
    QRect screen = QApplication::desktop()->screenGeometry( mTarget );
#endif
    QRect available = screen;
    if(mContext == TimelinePreview::PopupContext::FullscreenController)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        if(screenAt) available = screenAt->availableGeometry();
#else
        available = QApplication::desktop()->availableGeometry(mTarget);
#endif
    }
    const QRect textScreen = mContext == TimelinePreview::PopupContext::FullscreenController ? available : screen;
    position.setX( qMax( textScreen.left(), qMin( position.x(),
        textScreen.left() + textScreen.width() - size.width() ) ) );
    position.setY( qMax( textScreen.top(), qMin( position.y(),
        textScreen.top() + textScreen.height() - size.height() ) ) );

    if( !mPreview.isNull() )
    {
        const QRect placed = TimelinePreview::popupRect( size, mTarget,
                                                        TimelinePreview::popupBounds(mContext, mWindowBounds, available), mContext );
        if( mTrace.isEnabled() ) diagnose( QStringLiteral("preview-placement"), QStringLiteral(
            "context=%11 target=%1,%2 requested_size=%3,%4 screen=%5 owner_bounds=%6 intersection=%7 placed=%8 fallback=%9 visible=%10")
            .arg(mTarget.x()).arg(mTarget.y()).arg(size.width()).arg(size.height())
            .arg(TimelinePreview::diagnosticRect(screen)).arg(TimelinePreview::diagnosticRect(mWindowBounds))
            .arg(TimelinePreview::diagnosticRect(TimelinePreview::popupBounds(mContext, mWindowBounds, available)))
            .arg(TimelinePreview::diagnosticRect(placed)).arg(!placed.isValid()).arg(isVisible())
            .arg(mContext == TimelinePreview::PopupContext::FullscreenController ? QStringLiteral("fullscreen") : QStringLiteral("windowed")), placed.isValid() ? 250 : 0 );
        if( !placed.isValid() )
        {
            // A floating fullscreen toolbar/small window may have no room.
            // Restore the original timestamp tooltip rather than leave bounds.
            mPreview = QImage();
            adjustPosition();
            return;
        }
        position = placed.topLeft();
    }
    move( position );

    int tipX = mTarget.x() - position.x();
    if( mBox != textbox || mTipX != tipX )
    {
        mBox = textbox;
        mTipX = tipX;

        resize( size );
        buildPath();
    }
    if( mTrace.isEnabled() ) diagnose( QStringLiteral("placement-final"), QStringLiteral(
        "target=%1,%2 final_rect=%3 screen=%4 owner_bounds=%5 preview=%6 visible=%7 hidden=%8")
        .arg(mTarget.x()).arg(mTarget.y()).arg(TimelinePreview::diagnosticRect(geometry()))
        .arg(TimelinePreview::diagnosticRect(screen)).arg(TimelinePreview::diagnosticRect(mWindowBounds))
        .arg(!mPreview.isNull()).arg(isVisible()).arg(isHidden()) );
}

void TimeTooltip::buildPath()
{
    // Prepare the painter path for future use so
    // we only have to generate the text at runtime.

    // Draw the text box
    mPainterPath = QPainterPath();
    mPainterPath.addRect( mBox );

    // Draw the tip
    QPolygonF polygon;
    polygon << QPoint( qMax( 0, mTipX - 3 ), mBox.height() )
            << QPoint( mTipX, mBox.height() + TIP_HEIGHT )
            << QPoint( qMin( mTipX + 3, mBox.width() ), mBox.height() );
    mPainterPath.addPolygon( polygon );

    // Store the simplified version of the path
    mPainterPath = mPainterPath.simplified();
}

void TimeTooltip::setTip( const QPoint& target, const QString& time, const QString& text )
{
    mHoverTime = time;
    if(mTrace.isEnabled() && mHoverTargetMs >= 0) {
        const qint64 seconds = mHoverTargetMs / 1000;
        mHoverTime = QStringLiteral("%1:%2:%3.%4").arg(seconds/3600,2,10,QLatin1Char('0'))
            .arg(seconds/60%60,2,10,QLatin1Char('0')).arg(seconds%60,2,10,QLatin1Char('0'))
            .arg(mHoverTargetMs%1000,3,10,QLatin1Char('0'));
    }
    mMpvPreview = qgetenv("VLC_THUMBNAIL_BACKEND") == "libmpv";
    mDisplayedText = time;
    if ( !text.isEmpty() )
        mDisplayedText.append( " - " ).append( text );

    if( mTarget != target || time.length() != mTime.length() || mText != text )
    {
        mTarget = target;
        mTime = time;
        mText = text;
        adjustPosition();
    }
    else if(mMpvPreview && !mPreview.isNull()) adjustPosition();

    update();
    raise();
    presentFullscreen();
    if( mTrace.isEnabled() ) diagnose( QStringLiteral("set-tip"), QStringLiteral(
        "visible=%1 hidden=%2 target=%3,%4 time=%5 rect=%6")
        .arg(isVisible()).arg(isHidden()).arg(target.x()).arg(target.y()).arg(time)
        .arg(TimelinePreview::diagnosticRect(geometry())) );
}

void TimeTooltip::setPreview( const QImage &image, qint64 requestedMs, const QRect &bounds )
{
    mPreview = image;
    mWindowBounds = bounds;
    const qint64 seconds = qMax<qint64>(0, requestedMs) / 1000;
    // Approximate requested bucket, never a claim of delivered frame PTS.
    mPreviewTime = QStringLiteral("~%1:%2:%3.%4")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'))
        .arg(qMax<qint64>(0, requestedMs) % 1000, 3, 10, QLatin1Char('0'));
    adjustPosition();
    update();
    presentFullscreen();
    if( mTrace.isEnabled() ) diagnose( QStringLiteral("set-preview"), QStringLiteral(
        "image=%1 requested_ms=%2 visible=%3 hidden=%4 rect=%5")
        .arg(!image.isNull()).arg(requestedMs).arg(isVisible()).arg(isHidden())
        .arg(TimelinePreview::diagnosticRect(geometry())) );
}

void TimeTooltip::show()
{
    if( mTrace.isEnabled() ) diagnose( QStringLiteral("show-call"), QStringLiteral("visible_before=%1 hidden_before=%2")
        .arg(isVisible()).arg(isHidden()), 0 );
    setVisible( true );
    raise();
    presentFullscreen();
}

void TimeTooltip::setPlacementContext(QWidget *owner, TimelinePreview::PopupContext context)
{
    const bool changed = mPlacementOwner != owner || mContext != context;
    mPlacementOwner = owner;
    mContext = context;
    if(owner) mWindowBounds = QRect(owner->mapToGlobal(QPoint(0,0)), owner->size());
    if(changed)
    {
        setAttribute(Qt::WA_ShowWithoutActivating, context == TimelinePreview::PopupContext::FullscreenController);
        // Do not reparent the widget or alter its flags/lifetime. Clear only our
        // fullscreen transient association when returning to the original context.
        if(windowHandle() && context == TimelinePreview::PopupContext::Windowed)
            windowHandle()->setTransientParent(NULL);
        adjustPosition();
    }
}

void TimeTooltip::presentFullscreen()
{
    if(mContext != TimelinePreview::PopupContext::FullscreenController || !mPlacementOwner ||
       !mPlacementOwner->isVisible() || !isVisible() || size().isEmpty()) return;
    // Qt's visibility state is not proof of Win32 mapping/stacking. Associate
    // with the floating controller, then refresh only this visible popup without
    // activating any window. Device verification is still required.
    mPlacementOwner->winId();
    winId();
    if(windowHandle() && windowHandle()->transientParent() != mPlacementOwner->windowHandle())
        windowHandle()->setTransientParent(mPlacementOwner->windowHandle());
#ifdef Q_OS_WIN
    const HWND popup = reinterpret_cast<HWND>(winId());
    const bool before = IsWindowVisible(popup) != FALSE;
    const BOOL placed = SetWindowPos(popup, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    const DWORD nativeError = placed ? 0 : GetLastError();
    RECT nativeRect = {};
    GetWindowRect(popup, &nativeRect);
    if(mTrace.isEnabled()) diagnose(QStringLiteral("native-popup"), QStringLiteral(
        "context=fullscreen hwnd=%1 native_owner=%2 foreground=%3 visible_before=%4 visible_after=%5 topmost=%6 placed=%7 error=%8 native_rect_px=%9,%10,%11,%12")
        .arg(QString::number(quintptr(popup),16))
        .arg(QString::number(quintptr(GetWindow(popup,GW_OWNER)),16))
        .arg(QString::number(quintptr(GetForegroundWindow()),16))
        .arg(before).arg(IsWindowVisible(popup) != FALSE)
        .arg((GetWindowLongPtr(popup,GWL_EXSTYLE) & WS_EX_TOPMOST) != 0)
        .arg(placed != FALSE).arg(nativeError)
        .arg(nativeRect.left).arg(nativeRect.top).arg(nativeRect.right).arg(nativeRect.bottom));
#endif
}

void TimeTooltip::diagnose( const QString &stage, const QString &detail, int intervalMs )
{
    if( mTrace.isEnabled() ) mTrace.record(stage, QStringLiteral("component=tooltip tooltip=%1 stage=%2 %3")
        .arg(TimelinePreview::diagnosticId(this)).arg(stage).arg(detail), intervalMs);
}

bool TimeTooltip::event( QEvent *event )
{
    const QEvent::Type type = event->type();
    const bool observe = mTrace.isEnabled() && (type == QEvent::Show || type == QEvent::Hide ||
        type == QEvent::WindowActivate || type == QEvent::WindowDeactivate ||
        type == QEvent::WindowStateChange || type == QEvent::ZOrderChange);
    const bool before = isVisible();
    const bool result = QWidget::event(event);
    if( observe ) {
        const char *name = type == QEvent::Show ? "show" : type == QEvent::Hide ? "hide" :
            type == QEvent::WindowActivate ? "activate" : type == QEvent::WindowDeactivate ? "deactivate" :
            type == QEvent::ZOrderChange ? "z-order" : "window-state";
        diagnose( QStringLiteral("event-visibility"), QStringLiteral(
            "event=%1 visible_before=%2 visible_after=%3 hidden=%4 active=%5 active_window=%6 app_state=%7 flags=%8 rect=%9")
            .arg(QString::fromLatin1(name)).arg(before).arg(isVisible()).arg(isHidden()).arg(isActiveWindow())
            .arg(TimelinePreview::diagnosticId(qApp->activeWindow())).arg(int(qApp->applicationState()))
            .arg(QString::number(quint32(windowFlags()),16)).arg(TimelinePreview::diagnosticRect(geometry())),
            type == QEvent::ZOrderChange ? 250 : 0 );
    }
    return result;
}

void TimeTooltip::paintEvent( QPaintEvent * )
{
    QPainter p( this );
    p.setRenderHints( QPainter::TextAntialiasing );

    p.setPen( Qt::black );
    p.setBrush( qApp->palette().base() );
    p.drawPath( mPainterPath );

    p.setFont( mFont );
    p.setPen( QPen( qApp->palette().text(), 1 ) );
    if( mPreview.isNull() ) p.drawText( mBox, Qt::AlignCenter, mDisplayedText );
    else
    {
        p.drawImage( QPoint((mBox.width() - mPreview.width()) / 2, 4), mPreview );
        p.drawText( mPreviewTextBox, Qt::AlignCenter, mPreviewLabel );
    }
}
