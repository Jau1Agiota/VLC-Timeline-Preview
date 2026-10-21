/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include "util/input_slider.hpp"
#include "util/seekhover.hpp"
#include "util/thumbnailpreview.hpp"
#include <QUrl>
#include <cstdlib>
#include "util/timetooltip.hpp"
#include "adapters/seekpoints.hpp"
#include "input_manager.hpp"
#include "imagehelper.hpp"
#include "customwidgets.hpp"
#include <vlc_actions.h>

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QStyleOptionSlider>
#include <QLinearGradient>
#include <QTimer>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QSize>
#include <QPalette>
#include <QColor>
#include <QPoint>
#include <QPropertyAnimation>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QSequentialAnimationGroup>

namespace {
    int const MIN_SLIDER_VALUE = 0;
    int const MAX_SLIDER_VALUE = 10000;

    int const CHAPTER_SPOT_SIZE = 3;

    int const FADE_DURATION = 300;
    int const FADEOUT_DELAY = 2000;
}

SeekSlider::SeekSlider( intf_thread_t *p_intf, Qt::Orientation q, QWidget *_parent, bool _static )
          : QSlider( q, _parent ),
            hoverDiagnostics( qgetenv( "VLC_TIMELINE_HOVER_DEBUG" ) == "1" ), thumbnailPreview( NULL ),
            p_intf( p_intf ), b_classic( _static ), animLoading( NULL )
{
    isSliding = false;
    isJumping = false;
    f_buffering = 0.0;
    mHandleOpacity = 1.0;
    mLoading = 0.0;
    chapters = NULL;
    mHandleLength = -1;
    b_seekable = true;
    alternativeStyle = NULL;

    // prepare some static colors
    QPalette p = palette();
    QColor background = p.color( QPalette::Active, QPalette::Window );
    tickpointForeground = p.color( QPalette::Active, QPalette::WindowText );
    tickpointForeground.setHsv( tickpointForeground.hue(),
            ( background.saturation() + tickpointForeground.saturation() ) / 2,
            ( background.value() + tickpointForeground.value() ) / 2 );

    // set the background color and gradient
    QColor backgroundBase( p.window().color() );
    backgroundGradient.setColorAt( 0.0, backgroundBase.darker( 140 ) );
    backgroundGradient.setColorAt( 1.0, backgroundBase );

    // set the foreground color and gradient
    QColor foregroundBase( 50, 156, 255 );
    foregroundGradient.setColorAt( 0.0,  foregroundBase );
    foregroundGradient.setColorAt( 1.0,  foregroundBase.darker( 140 ) );

    // prepare the handle's gradient
    handleGradient.setColorAt( 0.0, p.window().color().lighter( 120 ) );
    handleGradient.setColorAt( 0.9, p.window().color().darker( 120 ) );

    // prepare the handle's shadow gradient
    QColor shadowBase = p.shadow().color();
    if( shadowBase.lightness() > 100 )
        shadowBase = QColor( 60, 60, 60 ); // Palette's shadow is too bright
    shadowDark = shadowBase.darker( 150 );
    shadowLight = shadowBase.lighter( 180 );
    shadowLight.setAlpha( 50 );

    /* Timer used to fire intermediate updatePos() when sliding */
    seekLimitTimer = new QTimer( this );
    seekLimitTimer->setSingleShot( true );

    /* Tooltip bubble */
    mTimeTooltip = new TimeTooltip( NULL );
    mTimeTooltip->setMouseTracking( true );
    thumbnailPreview = new TimelinePreview::Controller( this,
        [this]( const QString &token ) { return thumbnailCurrent( token ); },
        [this]( const QImage &image, qint64 time ) {
            QWidget *owner = window();
            mTimeTooltip->setPreview( image, time, QRect(owner->mapToGlobal(QPoint(0,0)), owner->size()) );
        },
        [this]( const QString &message ) {
            if( hoverDiagnostics ) msg_Dbg( this->p_intf, "timeline-preview: %s", qUtf8Printable(message) );
        } );
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, this, [this]( int state ) {
        if( state != PLAYING_S && state != PAUSE_S ) thumbnailPreview->cancel();
    } );

    /* Properties */
    setRange( MIN_SLIDER_VALUE, MAX_SLIDER_VALUE );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking( true );
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );

    /* Use the new/classic style */
    setMinimumHeight( 18 );
    if( !b_classic )
    {
        alternativeStyle = new SeekStyle;
        setStyle( alternativeStyle );
    }

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );
    secstotimestr( psz_length, 0 );

    animHandle = new QPropertyAnimation( this, "handleOpacity", this );
    animHandle->setDuration( FADE_DURATION );
    animHandle->setStartValue( 0.0 );
    animHandle->setEndValue( 1.0 );

    QPropertyAnimation *animLoadingIn = new QPropertyAnimation( this, "loadingProperty", this );
    animLoadingIn->setDuration( 2000 );
    animLoadingIn->setStartValue( 0.0 );
    animLoadingIn->setEndValue( 1.0 );
    animLoadingIn->setEasingCurve( QEasingCurve::OutBounce );
    QPropertyAnimation *animLoadingOut = new QPropertyAnimation( this, "loadingProperty", this );
    animLoadingOut->setDuration( 2000 );
    animLoadingOut->setStartValue( 1.0 );
    animLoadingOut->setEndValue( 0.0 );
    animLoadingOut->setEasingCurve( QEasingCurve::OutBounce );

    animLoading = new QSequentialAnimationGroup( this );
    animLoading->addAnimation( animLoadingIn );
    animLoading->addAnimation( animLoadingOut );
    animLoading->setLoopCount( -1 );

    hideHandleTimer = new QTimer( this );
    hideHandleTimer->setSingleShot( true );
    hideHandleTimer->setInterval( FADEOUT_DELAY );

    startAnimLoadingTimer = new QTimer( this );
    startAnimLoadingTimer->setSingleShot( true );
    startAnimLoadingTimer->setInterval( 500 );

    connect( MainInputManager::getInstance(), &MainInputManager::inputChanged, this, &SeekSlider::inputUpdated );
    connect( this, &SeekSlider::sliderMoved, this, &SeekSlider::startSeekTimer );
    connect( seekLimitTimer, &QTimer::timeout, this, &SeekSlider::updatePos );
    connect( hideHandleTimer, &QTimer::timeout, this, &SeekSlider::hideHandle );
    connect( startAnimLoadingTimer, &QTimer::timeout, this, &SeekSlider::startAnimLoading );
    mTimeTooltip->installEventFilter( this );

    connect(&wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, this, [this](int vlcButton){
        vlc_tick_t i_size = var_InheritInteger( this->p_intf->obj.libvlc, "short-jump-size" );
        int i_mode = var_InheritInteger( this->p_intf->obj.libvlc, "hotkeys-x-wheel-mode" );

        //ignore modifiers
        switch (vlcButton & 0x00FF0000) {
        case KEY_MOUSEWHEELDOWN:
        case KEY_MOUSEWHEELLEFT:
            if (i_mode != 3)
                i_size = - i_size;
            break;
        case KEY_MOUSEWHEELUP:
        case KEY_MOUSEWHEELRIGHT:
            if (i_mode == 3)
                i_size = - i_size;
            break;
        default:
            break;
        }

        float posOffset = static_cast<float>( i_size ) / static_cast<float>( inputLength );
        setValue( value() + posOffset * maximum() );
        emit sliderDragged( value() / static_cast<float>( maximum() ) );
    });
}

SeekSlider::~SeekSlider()
{
    delete thumbnailPreview;
    thumbnailPreview = NULL;
    delete chapters;
    if ( alternativeStyle )
        delete alternativeStyle;
    delete mTimeTooltip;
}

/***
 * \brief Sets the chapters seekpoints adapter
 *
 * \params SeekPoints initilized with current intf thread
***/
void SeekSlider::setChapters( SeekPoints *chapters_ )
{
    delete chapters;
    chapters = chapters_;
    chapters->setParent( this );
}

/***
 * \brief Main public method, superseeding setValue. Disabling the slider when neeeded
 *
 * \param pos Position, between 0 and 1. -1 disables the slider
 * \param time Elapsed time. Unused
 * \param legnth Duration time.
 ***/
void SeekSlider::setPosition( float pos, int64_t time, int length )
{
    VLC_UNUSED(time);
    if( pos == -1.0  || ! b_seekable )
    {
        if( thumbnailPreview ) thumbnailPreview->cancel();
        setEnabled( false );
        mTimeTooltip->hide();
        isSliding = false;
        setValue( 0 );
        return;
    }
    else
        setEnabled( true );

    if( !isSliding )
    {
        setValue( pos * static_cast<float>( maximum() ) );
        if ( animLoading != NULL && pos >= 0.0f && animLoading->state() != QAbstractAnimation::Stopped )
        {
            animLoading->stop();
            mLoading = 0.0f;
        }

    }

    inputLength = length;
}

void SeekSlider::setSeekable( bool b )
{
    b_seekable = b;
    if( !b && thumbnailPreview ) thumbnailPreview->cancel();
}

void SeekSlider::startSeekTimer()
{
    /* Only fire one update, when sliding, every 150ms */
    if( isSliding && !seekLimitTimer->isActive() )
        seekLimitTimer->start( 150 );
}

void SeekSlider::updatePos()
{
    float f_pos = value() / static_cast<float>( maximum() );
    emit sliderDragged( f_pos ); /* Send new position to VLC's core */
}

void SeekSlider::updateBuffering( float f_buffering_ )
{
    if ( f_buffering_ < f_buffering )
        bufferingStart = QTime::currentTime();
    f_buffering = f_buffering_;
    if ( f_buffering > 0.0 || isEnabled() ) {
        animLoading->stop();
        startAnimLoadingTimer->stop();
        mLoading = 0.0;
    }
    repaint();
}

void SeekSlider::inputUpdated( bool b_has_input )
{
    thumbnailPreview->cancel( true );
    if ( b_has_input == false ) {
        animLoading->stop();
        startAnimLoadingTimer->stop();
        mLoading = 0.0;
        repaint();
    }
    else if ( f_buffering == 0.0 && !isEnabled() )
        startAnimLoadingTimer->start();
}

void SeekSlider::processReleasedButton()
{
    if ( !isSliding && !isJumping ) return;
    isSliding = false;
    bool b_seekPending = seekLimitTimer->isActive();
    seekLimitTimer->stop(); /* We're not sliding anymore: only last seek on release */
    if ( isJumping )
    {
        isJumping = false;
        return;
    }
    if( b_seekPending && isEnabled() )
        updatePos();
}

void SeekSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if ( event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton )
    {
        QSlider::mouseReleaseEvent( event );
        return;
    }
    event->accept();
    processReleasedButton();
}

void SeekSlider::mousePressEvent( QMouseEvent* event )
{
    thumbnailPreview->cancel();
    /* Right-click */
    if ( !isEnabled() ||
         ( event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton )
       )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    isJumping = false;
    /* handle chapter clicks */
    int i_width = size().width();
    if ( chapters && inputLength && i_width)
    {
        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
             /* only on chapters zone */
            if ( event->y() < CHAPTER_SPOT_SIZE ||
                 event->y() > ( size().height() - CHAPTER_SPOT_SIZE ) )
            {
                QList<SeekPoint> points = chapters->getPoints();
                int i_selected = -1;
                bool b_startsnonzero = false; /* as we always starts at 1 */
                if ( points.count() > 0 ) /* do we need an extra offset ? */
                    b_startsnonzero = ( points.at(0).time > 0 );
                int i_min_diff = i_width + 1;
                for( int i = 0 ; i < points.count() ; i++ )
                {
                    int x = points.at(i).time / 1000000.0 / inputLength * i_width;
                    int diff_x = abs( x - event->x() );
                    if ( diff_x < i_min_diff )
                    {
                        i_min_diff = diff_x;
                        i_selected = i + ( ( b_startsnonzero )? 1 : 0 );
                    } else break;
                }
                if ( i_selected && i_min_diff < 4 ) // max 4px around mark
                {
                    chapters->jumpTo( i_selected );
                    event->accept();
                    isJumping = true;
                    return;
                }
            }
        }
    }

    isSliding = true ;

    setValue( getValueFromXPos( event->x() ) );
    emit sliderMoved( value() );
    event->accept();
}

QString SeekSlider::thumbnailToken() const
{
    input_thread_t *input = THEMIM->getInput();
    if( !input ) return QString();
    char *uri = input_item_GetURI( input_GetItem(input) );
    if( !uri ) return QString();
    const QString token = QString::number(reinterpret_cast<quintptr>(input),16) +
                          QLatin1Char(':') + QString::fromUtf8(uri);
    free(uri);
    return token;
}

bool SeekSlider::thumbnailCurrent( const QString &token ) const
{
    input_thread_t *input = THEMIM->getInput();
    if( !input || token.isEmpty() || token != thumbnailToken() || !isVisible() ||
        !underMouse() || isSliding || qApp->mouseButtons() != Qt::NoButton || !isEnabled() || !b_seekable ||
        var_CountChoices(input,"video-es") < 2 || qApp->activePopupWidget() ||
        qApp->applicationState() != Qt::ApplicationActive ) return false;
    const int state = var_GetInteger(input,"state");
    return (state == PLAYING_S || state == PAUSE_S) && var_GetBool(input,"can-seek") &&
           var_GetInteger(input,"length") > 0;
}

void SeekSlider::updateHover( const QPoint &pos, const char *event, bool hovering )
{
    if( !hovering && thumbnailPreview ) thumbnailPreview->cancel();
    if( !hovering && !hoverDiagnostics ) return;
    // Qt owns the current-input reference; this runs only on its GUI thread.
    // Read current values rather than the whole-second positionUpdated duration.
    input_thread_t *input = THEMIM->getInput();
    const int state = input ? var_GetInteger( input, "state" ) : END_S;
    const bool active = input && (state == PLAYING_S || state == PAUSE_S);
    const bool seekable = input && b_seekable && var_GetBool( input, "can-seek" );
    const int64_t durationMs = input ? MS_FROM_VLC_TICK( var_GetInteger( input, "length" ) ) : 0;
    QStyleOptionSlider option;
    initStyleOption( &option );
    // SeekStyle paints left-to-right; classic QSlider uses initStyleOption's
    // direction. Ask the style afresh, including stylesheet/DPI offsets.
    if( alternativeStyle ) option.upsideDown = false;
    option.sliderPosition = option.sliderValue = minimum();
    const QRect first = style()->subControlRect( QStyle::CC_Slider, &option,
                                                 QStyle::SC_SliderHandle, this );
    option.sliderPosition = option.sliderValue = maximum();
    const QRect last = style()->subControlRect( QStyle::CC_Slider, &option,
                                                QStyle::SC_SliderHandle, this );
    const SeekHover::Position hover = SeekHover::map( pos.x(), pos.y(), width(), height(),
                                                     first.center().x(), last.center().x(),
                                                     durationMs, orientation() == Qt::Horizontal &&
                                                     first.isValid() && last.isValid() );
    const bool valid = hovering && hover.inside && durationMs > 0 && active &&
                       seekable && isEnabled() && !isSliding && !qApp->activePopupWidget();
    if( thumbnailPreview && hovering )
    {
        const QString token = thumbnailToken();
        if( valid && thumbnailCurrent(token) )
        {
            char *uri = input_item_GetURI( input_GetItem(input) );
            const QUrl url = QUrl::fromEncoded( QByteArray(uri ? uri : "") );
            free(uri);
            if( url.isLocalFile() ) thumbnailPreview->hover(url.toLocalFile(), token, hover.timestampMs);
            else thumbnailPreview->cancel();
        }
        else thumbnailPreview->cancel();
    }
    if( !hoverDiagnostics ) return;
    const QString message = QStringLiteral(
        "event=%1 x=%2 y=%3 width=%4 height=%5 start_x=%6 end_x=%7 span=%8 "
        "timestamp_ms=%9 duration_ms=%10 inside=%11 valid=%12 "
        "active=%13 seekable=%14 dragging=%15" )
        .arg( QString::fromLatin1( event ) ).arg( pos.x() ).arg( pos.y() )
        .arg( width() ).arg( height() ).arg( hover.start ).arg( hover.end ).arg( hover.span )
        .arg( static_cast<qlonglong>( hover.timestampMs ) )
        .arg( static_cast<qlonglong>( durationMs ) ).arg( hover.inside ).arg( valid )
        .arg( active ).arg( seekable ).arg( isSliding );
    msg_Dbg( p_intf, "timeline-hover widget=%p %s", static_cast<void *>( this ),
             qUtf8Printable( message ) );
}

void SeekSlider::mouseMoveEvent( QMouseEvent *event )
{
    updateHover( event->pos(), "move" );
    if ( ! ( event->buttons() & ( Qt::LeftButton | Qt::MiddleButton ) ) )
    {
        /* Handle button release when mouserelease has been hijacked by popup */
        processReleasedButton();
    }

    if ( !isEnabled() ) return event->accept();

    if( isSliding )
    {
        setValue( getValueFromXPos( event->x() ) );
        emit sliderMoved( value() );
    }

    /* Tooltip */
    if ( inputLength > 0 )
    {
        int margin = handleLength();
        int posX = qMax( rect().left() + margin, qMin( rect().right() - margin, event->x() ) );

        QString chapterLabel;

        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
            QList<SeekPoint> points = chapters->getPoints();
            int i_selected = -1;
            for( int i = 0 ; i < points.count() ; i++ )
            {
                int x = margin + points.at(i).time / 1000000.0 / inputLength * (size().width() - 2*margin);
                if ( event->x() >= x )
                    i_selected = i;
            }
            if ( i_selected >= 0 && i_selected < points.size() )
            {
                chapterLabel = points.at( i_selected ).name;
            }
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const auto pos = event->globalPosition();
#else
        const auto pos = event->globalPos();
#endif
        QPoint target( pos.x() - ( event->x() - posX ),
                QWidget::mapToGlobal( QPoint( 0, 0 ) ).y() );
        if( likely( size().width() > handleLength() ) ) {
            secstotimestr( psz_length, getValuePercentageFromXPos( event->x() ) * inputLength );
            mTimeTooltip->setTip( target, psz_length, chapterLabel );
        }
    }
    event->accept();
}

void SeekSlider::wheelEvent( QWheelEvent *event )
{
    /* Don't do anything if we are for somehow reason sliding */
    if( !isSliding && isEnabled() )
        wheelEventConverter.wheelEvent(event);
    event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SeekSlider::enterEvent( QEnterEvent * )
#else
void SeekSlider::enterEvent( QEvent * )
#endif
{
    updateHover( mapFromGlobal( QCursor::pos() ), "enter" );
    /* Cancel the fade-out timer */
    hideHandleTimer->stop();
    /* Only start the fade-in if needed */
    if( isEnabled() && animHandle->direction() != QAbstractAnimation::Forward )
    {
        /* If pause is called while not running Qt will complain */
        if( animHandle->state() == QAbstractAnimation::Running )
            animHandle->pause();
        animHandle->setDirection( QAbstractAnimation::Forward );
        animHandle->start();
    }
    /* Don't show the tooltip if the slider is disabled or a menu is open */
    if( isEnabled() && inputLength > 0 && !qApp->activePopupWidget() )
        mTimeTooltip->show();
}

void SeekSlider::leaveEvent( QEvent * )
{
    updateHover( mapFromGlobal( QCursor::pos() ), "leave", false );
    hideHandleTimer->start();
    /* Hide the tooltip
       - if the mouse leave the slider rect (Note: it can still be
         over the tooltip!)
       - if another window is on the way of the cursor */
    if( !rect().contains( mapFromGlobal( QCursor::pos() ) ) ||
      ( !isActiveWindow() && !mTimeTooltip->isActiveWindow() ) )
    {
        mTimeTooltip->hide();
    }
}

void SeekSlider::paintEvent( QPaintEvent *ev )
{
    if ( alternativeStyle )
    {
        SeekStyle::SeekStyleOption option;
        option.initFrom( this );
        if ( QTime::currentTime() > bufferingStart.addSecs( 1 ) )
            option.buffering = f_buffering;
        else
            option.buffering = 0.0;
        option.length = inputLength;
        option.animate = ( animHandle->state() == QAbstractAnimation::Running
                           || hideHandleTimer->isActive() );
        option.animationopacity = mHandleOpacity;
        option.animationloading = mLoading;
        option.sliderPosition = sliderPosition();
        option.sliderValue = value();
        option.maximum = maximum();
        option.minimum = minimum();
        if ( chapters ) foreach( const SeekPoint &point, chapters->getPoints() )
            option.points << point.time;
        QPainter painter( this );
        style()->drawComplexControl( QStyle::CC_Slider, &option, &painter, this );
    }
    else
        QSlider::paintEvent( ev );
}

void SeekSlider::hideEvent( QHideEvent * )
{
    updateHover( mapFromGlobal( QCursor::pos() ), "hide", false );
    mTimeTooltip->hide();
}

bool SeekSlider::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == mTimeTooltip )
    {
        if( event->type() == QEvent::MouseMove )
        {
            QMouseEvent* mev = static_cast<QMouseEvent*>( event );

            if( rect().contains( mapFromGlobal( mev->globalPos() ) ) )
                return false;
        }

        if( event->type() == QEvent::Leave ||
            event->type() == QEvent::MouseMove )
        {
            mTimeTooltip->hide();
        }

        return false;
    }

    return QSlider::eventFilter( obj, event );
}

QSize SeekSlider::sizeHint() const
{
    if ( b_classic )
        return QSlider::sizeHint();
    return ( orientation() == Qt::Horizontal ) ? QSize( 100, 18 )
                                               : QSize( 18, 100 );
}

qreal SeekSlider::handleOpacity() const
{
    return mHandleOpacity;
}

qreal SeekSlider::loading() const
{
    return mLoading;
}

void SeekSlider::setHandleOpacity(qreal opacity)
{
    mHandleOpacity = opacity;
    /* Request a new paintevent */
    update();
}

void SeekSlider::setLoading(qreal loading)
{
    mLoading = loading;
    /* Request a new paintevent */
    update();
}

inline int SeekSlider::handleLength()
{
    if ( mHandleLength > 0 )
        return mHandleLength;

    /* Ask for the length of the handle to the underlying style */
    QStyleOptionSlider option;
    initStyleOption( &option );
    mHandleLength = style()->pixelMetric( QStyle::PM_SliderLength, &option );
    return mHandleLength;
}

inline int SeekSlider::getValueFromXPos( int posX )
{
    return QStyle::sliderValueFromPosition(
        minimum(), maximum(),
        posX    - handleLength() / 2,
        width() - handleLength(),
        false
    );
}

inline float SeekSlider::getValuePercentageFromXPos( int posX )
{
    return getValueFromXPos( posX ) / static_cast<float>( maximum() );
}

void SeekSlider::hideHandle()
{
    /* If pause is called while not running Qt will complain */
    if( animHandle->state() == QAbstractAnimation::Running )
        animHandle->pause();
    /* Play the animation backward */
    animHandle->setDirection( QAbstractAnimation::Backward );
    animHandle->start();
}

void SeekSlider::startAnimLoading()
{
    animLoading->start();
}

/* This work is derived from Amarok's work under GPLv2+
    - Mark Kretschmann
    - Gábor Lehel
   */
#define WLENGTH   85  // px
#define WHEIGHT   26  // px
#define PADDINGL  6   // px
#define PADDINGR  6   // px
#define SOUNDMIN  0   // %

SoundSlider::SoundSlider( QWidget *_parent, float _i_step,
                          char *psz_colors, int max )
                        : QAbstractSlider( _parent )
{
    f_step = (float)(_i_step * 10000)
           / (float)((max - SOUNDMIN) * AOUT_VOLUME_DEFAULT);
    setRange( SOUNDMIN, max);
    setMouseTracking( true );
    isSliding = false;
    b_mouseOutside = true;
    b_isMuted = false;

    setFixedSize( WLENGTH, WHEIGHT );

    pixOutside = ImageHelper::loadSvgToPixmap(":/toolbar/volslide-outside.svg", width(), height() );

    const QPixmap temp = ImageHelper::loadSvgToPixmap(":/toolbar/volslide-inside.svg", width(), height() );
    const QBitmap mask( temp.createHeuristicMask() );

    pixGradient = QPixmap( pixOutside.size() );
    pixGradient2 = QPixmap( pixOutside.size() );
    dark = isDarkPaletteEnabled(nullptr);

#if HAS_QT56
    pixGradient.setDevicePixelRatio(QApplication::primaryScreen()->devicePixelRatio());
    pixGradient2.setDevicePixelRatio(QApplication::primaryScreen()->devicePixelRatio());
#endif

    /* Gradient building from the preferences */
    QLinearGradient gradient( PADDINGL, 2, width() - PADDINGR, 2 );
    QLinearGradient gradient2( PADDINGL, 2, width()- PADDINGR, 2 );

    QStringList colorList = qfu( psz_colors ).split( ";" );
    free( psz_colors );

    /* Fill with 255 if the list is too short */
    if( colorList.count() < 12 )
        for( int i = colorList.count(); i < 12; i++)
            colorList.append( "255" );

    background = palette().color( QPalette::Active, QPalette::Window );
    foreground = palette().color( QPalette::Active, QPalette::WindowText );
    foreground.setHsv( foreground.hue(),
                    ( background.saturation() + foreground.saturation() ) / 2,
                    ( background.value() + foreground.value() ) / 2 );

    textfont.setPointSize( 7 );
    textrect.setRect( 0, 0, 34, 15 );

    /* Regular colors */
#define c(i) colorList.at(i).toInt()
#define add_color(gradient, range, c1, c2, c3) \
    gradient.setColorAt( range, QColor( c(c1), c(c2), c(c3) ) );

    /* Desaturated colors */
#define desaturate(c) c->setHsvF( c->hueF(), 0.2 , 0.5, 1.0 )
#define add_desaturated_color(gradient, range, c1, c2, c3) \
    foo = new QColor( c(c1), c(c2), c(c3) );\
    desaturate( foo ); gradient.setColorAt( range, *foo );\
    delete foo;

    /* combine the two helpers */
#define add_colors( gradient1, gradient2, range, c1, c2, c3 )\
    add_color( gradient1, range, c1, c2, c3 ); \
    add_desaturated_color( gradient2, range, c1, c2, c3 );

    float f_mid_point = ( 100.0 / maximum() );
    QColor * foo;
    add_colors( gradient, gradient2, 0.0, 0, 1, 2 );
    add_colors( gradient, gradient2, f_mid_point - 0.05, 3, 4, 5 );
    add_colors( gradient, gradient2, f_mid_point + 0.05, 6, 7, 8 );
    add_colors( gradient, gradient2, 1.0, 9, 10, 11 );

    painter.begin( &pixGradient );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient );
    painter.drawRect( pixGradient.rect() );
    painter.end();

    painter.begin( &pixGradient2 );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient2 );
    painter.drawRect( pixGradient2.rect() );
    painter.end();

    pixGradient.setMask( mask );
    pixGradient2.setMask( mask );

    connect(&wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, this, [this](int vlcButton){
        int newvalue = 0;
        //ignore modifiers
        switch (vlcButton & 0x00FF0000) {
        case KEY_MOUSEWHEELDOWN:
        case KEY_MOUSEWHEELLEFT:
            newvalue = value() - f_step;
            break;
        case KEY_MOUSEWHEELUP:
        case KEY_MOUSEWHEELRIGHT:
            newvalue = value() + f_step;
            break;
        default:
            return;
        }

        setValue( __MIN( __MAX( minimum(), newvalue ), maximum() ) );
        emit sliderMoved( value() );
    });
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    wheelEventConverter.wheelEvent(event);
    event->accept();
    emit sliderReleased();
}

void SoundSlider::mousePressEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        /* We enter the sliding mode */
        isSliding = true;
        i_oldvalue = value();
        emit sliderPressed();
        changeValue( event->x() );
        emit sliderMoved( value() );
    }
}

void SoundSlider::processReleasedButton()
{
    if( !b_mouseOutside && value() != i_oldvalue )
    {
        emit sliderReleased();
        setValue( value() );
        emit sliderMoved( value() );
    }
    isSliding = false;
    b_mouseOutside = false;
}

void SoundSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
        processReleasedButton();
}

void SoundSlider::mouseMoveEvent( QMouseEvent *event )
{
    /* handle mouserelease hijacking */
    if ( isSliding && ( event->buttons() & ~Qt::RightButton ) == Qt::NoButton )
        processReleasedButton();

    if( isSliding )
    {
        QRect rect( PADDINGL - 15,    -1,
                    width() - PADDINGR + 15 * 2 , width() + 5 );
        if( !rect.contains( event->pos() ) )
        { /* We are outside */
            if ( !b_mouseOutside )
                setValue( i_oldvalue );
            b_mouseOutside = true;
        }
        else
        { /* We are inside */
            b_mouseOutside = false;
            changeValue( event->x() );
            emit sliderMoved( value() );
        }
    }
    else
    {
        int i = ( ( event->x() - PADDINGL ) * maximum() ) / ( width() - ( PADDINGR + PADDINGL ) );
        i = __MIN( __MAX( 0, i ), maximum() );
        setToolTip( QString("%1  %" ).arg( i ) );
    }
}

void SoundSlider::changeValue( int x )
{
    setValue( ( ( x - PADDINGL ) * maximum() ) / ( width() - ( PADDINGR + PADDINGL ) ) );
}

void SoundSlider::setMuted( bool m )
{
    b_isMuted = m;
    update();
}

void SoundSlider::paintEvent( QPaintEvent *e )
{
    QPixmap *paintGradient;
    if (b_isMuted)
        paintGradient = &this->pixGradient2;
    else
        paintGradient = &this->pixGradient;

    painter.begin( this );

    float f_scale = paintGradient->width() / float( width() );
    const int offsetDst = int( ( ( width() - ( PADDINGR + PADDINGL ) ) * value() + 100 ) / maximum() ) + PADDINGL;
    const int offsetSrc = int( ( ( paintGradient->width() - ( PADDINGR + PADDINGL ) * f_scale ) * value() + 100 ) / maximum() + PADDINGL * f_scale );

    painter.drawPixmap( 0, 0, offsetDst, height(), *paintGradient, 0, 0, offsetSrc, paintGradient->height() );
    painter.drawPixmap( 0, 0, width(), height(), pixOutside, 0, 0,  pixOutside.width(), pixOutside.height() );

    if (dark)
        painter.setPen(Qt::white);
    else
        painter.setPen(foreground);
    painter.setFont( textfont );
    painter.drawText( textrect, Qt::AlignRight | Qt::AlignVCenter,
                      QString::number( value() ) + '%' );

    painter.end();
    e->accept();
}
