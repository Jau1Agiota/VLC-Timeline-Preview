/*****************************************************************************
 * Copyright © 2011 VideoLAN
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

#ifndef TIMETOOLTIP_H
#define TIMETOOLTIP_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <QtGlobal>

#include <QWidget>
#include <QImage>
#include <QPainterPath>
#include "previewdiagnostics.hpp"

class TimeTooltip : public QWidget
{
    Q_OBJECT
public:
    explicit TimeTooltip( QWidget *parent = 0 );
    void setTip( const QPoint& pos, const QString& time, const QString& text );
    virtual void show();
    void setDiagnosticLogger( TimelinePreview::Diagnostics::Log logger ) { mTrace.setLogger(logger); }
    void setPreview( const QImage &, qint64 requestedMs, const QRect &windowBounds );

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    bool event( QEvent * ) Q_DECL_OVERRIDE;

private:
    TimelinePreview::Diagnostics mTrace;
    void diagnose( const QString &, const QString &, int intervalMs = 250 );
    void adjustPosition();
    void buildPath();
    QPoint mTarget;
    QString mTime;
    QString mText;
    QString mDisplayedText;
    QFont mFont;
    QRect mBox;
    QPainterPath mPainterPath;
    int mTipX;
    QImage mPreview;
    QRect mWindowBounds;
    QRect mPreviewTextBox;
    QString mPreviewTime;
    QString mPreviewLabel;
};

#endif // TIMETOOLTIP_H
