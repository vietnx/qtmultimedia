/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#ifndef QMEDIAPLAYLISTCONTROL_P_H
#define QMEDIAPLAYLISTCONTROL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qobject.h>
#include "qmediacontrol.h"
#include <private/qmediaplaylistnavigator_p.h>


QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Multimedia)


class QMediaPlaylistProvider;

class Q_MULTIMEDIA_EXPORT QMediaPlaylistControl : public QMediaControl
{
    Q_OBJECT

public:
    virtual ~QMediaPlaylistControl();

    virtual QMediaPlaylistProvider* playlistProvider() const = 0;
    virtual bool setPlaylistProvider(QMediaPlaylistProvider *playlist) = 0;

    virtual int currentIndex() const = 0;
    virtual void setCurrentIndex(int position) = 0;
    virtual int nextIndex(int steps) const = 0;
    virtual int previousIndex(int steps) const = 0;

    virtual void next() = 0;
    virtual void previous() = 0;

    virtual QMediaPlaylist::PlaybackMode playbackMode() const = 0;
    virtual void setPlaybackMode(QMediaPlaylist::PlaybackMode mode) = 0;

Q_SIGNALS:
    void playlistProviderChanged();
    void currentIndexChanged(int position);
    void currentMediaChanged(const QMediaContent&);
    void playbackModeChanged(QMediaPlaylist::PlaybackMode mode);

protected:
    QMediaPlaylistControl(QObject* parent = 0);
};

#define QMediaPlaylistControl_iid "org.qt-project.qt.mediaplaylistcontrol/5.0"
Q_MEDIA_DECLARE_CONTROL(QMediaPlaylistControl, QMediaPlaylistControl_iid)

QT_END_NAMESPACE

QT_END_HEADER


#endif // QMEDIAPLAYLISTCONTROL_P_H
