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
#include <QFontMetrics>
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
        return;
    }

    // Get the bounding box required to print the text and add some padding
    QFontMetrics metrics( mFont );
    mPreviewLabel = metrics.elidedText(mPreviewTime +
        (mText.isEmpty() ? QString() : QStringLiteral(" - ") + mText),
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
    position.setX( qMax( screen.left(), qMin( position.x(),
        screen.left() + screen.width() - size.width() ) ) );
    position.setY( qMax( screen.top(), qMin( position.y(),
        screen.top() + screen.height() - size.height() ) ) );

    if( !mPreview.isNull() )
    {
        const QRect placed = TimelinePreview::popupRect( size, mTarget,
                                                        screen.intersected( mWindowBounds ) );
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

    update();
    raise();
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
}

void TimeTooltip::show()
{
    setVisible( true );
    raise();
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
