/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the Qt Mobility Components.
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

#ifndef MFDECODERSOURCEREADER_H
#define MFDECODERSOURCEREADER_H
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>

#include <QtCore/qobject.h>
#include <QtCore/qmutex.h>
#include "qaudioformat.h"

QT_USE_NAMESPACE

class MFDecoderSourceReader : public QObject, public IMFSourceReaderCallback
{
    Q_OBJECT
public:
    MFDecoderSourceReader(QObject *parent = 0);
    void shutdown();

    IMFMediaSource* mediaSource();
    IMFMediaType* setSource(IMFMediaSource *source, const QAudioFormat &audioFormat);

    void reset();
    void readNextSample();
    QList<IMFSample*> takeSamples(); //internal samples will be cleared after this

    //from IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    //from IMFSourceReaderCallback
    STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
        DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);
    STDMETHODIMP OnFlush(DWORD dwStreamIndex);
    STDMETHODIMP OnEvent(DWORD dwStreamIndex, IMFMediaEvent *pEvent);

Q_SIGNALS:
    void sampleAdded();
    void finished();

private:
    long m_cRef;
    QList<IMFSample*>   m_cachedSamples;
    QMutex              m_samplesMutex;

    IMFSourceReader             *m_sourceReader;
    IMFMediaSource              *m_source;
};
#endif//MFDECODERSOURCEREADER_H
