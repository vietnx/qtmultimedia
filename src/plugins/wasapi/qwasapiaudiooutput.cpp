/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwasapiaudiooutput.h"
#include "qwasapiutils.h"
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <Audioclient.h>
#include <avrt.h>
#include <functional>

using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmAudioOutput, "qt.multimedia.audiooutput")

class WasapiOutputDevicePrivate : public QIODevice
{
    Q_OBJECT
public:
    WasapiOutputDevicePrivate(QWasapiAudioOutput* output);
    ~WasapiOutputDevicePrivate();

    qint64 readData(char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

private:
    QWasapiAudioOutput *m_output;
    QTimer m_timer;
};

WasapiOutputDevicePrivate::WasapiOutputDevicePrivate(QWasapiAudioOutput* output)
    : m_output(output)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, [=](){
        if (m_output->m_currentState == QAudio::ActiveState) {
            m_output->m_currentState = QAudio::IdleState;
            emit m_output->stateChanged(m_output->m_currentState);
            m_output->m_currentError = QAudio::UnderrunError;
            emit m_output->errorChanged(m_output->m_currentError);
        }
        });
}

WasapiOutputDevicePrivate::~WasapiOutputDevicePrivate()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
}

qint64 WasapiOutputDevicePrivate::readData(char* data, qint64 len)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    Q_UNUSED(data)
    Q_UNUSED(len)

    return 0;
}

qint64 WasapiOutputDevicePrivate::writeData(const char* data, qint64 len)
{
    if (m_output->state() != QAudio::ActiveState && m_output->state() != QAudio::IdleState)
        return 0;

    QMutexLocker locker(&m_output->m_mutex);
    m_timer.stop();

    const quint32 channelCount = m_output->m_currentFormat.channelCount();
    const quint32 sampleBytes = m_output->m_currentFormat.sampleSize() / 8;
    const quint32 freeBytes = static_cast<quint32>(m_output->bytesFree());
    quint32 bytesToWrite = qMin(freeBytes, static_cast<quint32>(len));
    const quint32 framesToWrite = bytesToWrite / (channelCount * sampleBytes);
    bytesToWrite = framesToWrite * channelCount * sampleBytes;

    BYTE *buffer;
    HRESULT hr;
    hr = m_output->m_renderer->GetBuffer(framesToWrite, &buffer);
    if (hr != S_OK) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "GetBuffer() failed with error code hr ="<<hr;
        m_output->m_currentError = QAudio::UnderrunError;
        QMetaObject::invokeMethod(m_output, "errorChanged", Qt::QueuedConnection,
                                  Q_ARG(QAudio::Error, QAudio::UnderrunError));
        // Also Error Buffers need to be released
        hr = m_output->m_renderer->ReleaseBuffer(framesToWrite, 0);
        return 0;
    }

    memcpy_s(buffer, bytesToWrite, data, bytesToWrite);

    hr = m_output->m_renderer->ReleaseBuffer(framesToWrite, 0);
    if (hr != S_OK)
        qFatal("Could not release buffer");

    if (m_output->m_interval && m_output->m_openTime.elapsed() - m_output->m_openTimeOffset > m_output->m_interval) {
        QMetaObject::invokeMethod(m_output, "notify", Qt::QueuedConnection);
        m_output->m_openTimeOffset = m_output->m_openTime.elapsed();
    }

    m_output->m_bytesProcessed += bytesToWrite;

    if (m_output->m_currentState != QAudio::ActiveState) {
        m_output->m_currentState = QAudio::ActiveState;
        emit m_output->stateChanged(m_output->m_currentState);
    }
    if (m_output->m_currentError != QAudio::NoError) {
        m_output->m_currentError = QAudio::NoError;
        emit m_output->errorChanged(m_output->m_currentError);
    }

    quint32 paddingFrames;
    hr = m_output->m_interface->m_client->GetCurrentPadding(&paddingFrames);
    const quint32 paddingBytes = paddingFrames * m_output->m_currentFormat.channelCount() * m_output->m_currentFormat.sampleSize() / 8;

    m_timer.setInterval(m_output->m_currentFormat.durationForBytes(paddingBytes) / 1000);
    m_timer.start();
    return bytesToWrite;
}

QWasapiAudioOutput::QWasapiAudioOutput(const QByteArray &device)
    : m_deviceName(device)
    , m_volumeCache(qreal(1.))
    , m_currentState(QAudio::StoppedState)
    , m_currentError(QAudio::NoError)
    , m_interval(1000)
    , m_pullMode(true)
    , m_bufferFrames(0)
    , m_bufferBytes(4096)
    , m_eventThread(0)
    , m_hTask(NULL)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << device;
}

QWasapiAudioOutput::~QWasapiAudioOutput()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    stop();
}

void QWasapiAudioOutput::setVolume(qreal vol)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << vol;
    m_volumeCache = vol;
    if(WASAPI_MODE == AUDCLNT_SHAREMODE_SHARED){
        if (m_volumeControl) {
            quint32 channelCount;
            HRESULT hr = m_volumeControl->GetChannelCount(&channelCount);
            for (quint32 i = 0; i < channelCount; ++i) {
                hr = m_volumeControl->SetChannelVolume(i, vol);
                RETURN_VOID_IF_FAILED("Could not set audio volume.");
            }
        }
    }
}

qreal QWasapiAudioOutput::volume() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_volumeCache;
}

void QWasapiAudioOutput::process()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    DWORD waitRet;
    DWORD taskIndex = 0;
    m_hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    if (m_hTask == NULL) {
        m_currentError = QAudio::OpenError;
        emit errorChanged(m_currentError);
        return;
    }

    m_processing = true;
    do {
        waitRet = WaitForSingleObjectEx(m_event, 2000, FALSE);
        if (waitRet != WAIT_OBJECT_0) {
            qCritical()<< __FUNCTION__ << "Returned while waiting for event.";
            break;
        }
        ResetEvent(m_event);

        QMutexLocker locker(&m_mutex);

        if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
            break;
        QMetaObject::invokeMethod(this, "processBuffer", Qt::QueuedConnection);
    } while (m_processing);

    if (m_hTask) {
        AvRevertMmThreadCharacteristics(m_hTask);
    }
}

void QWasapiAudioOutput::processBuffer()
{
    QMutexLocker locker(&m_mutex);

    const quint32 channelCount = m_currentFormat.channelCount();
    const quint32 sampleBytes = m_currentFormat.sampleSize() / 8;
    BYTE* buffer;
    HRESULT hr;

    quint32 availableFrames = m_bufferFrames;

    if(WASAPI_MODE == AUDCLNT_SHAREMODE_SHARED){
        quint32 paddingFrames;
        hr = m_interface->m_client->GetCurrentPadding(&paddingFrames);
        if(FAILED(hr))
            qCDebug(lcMmAudioOutput) <<  "GetCurrentPadding() failed hr ="<<hr;
        availableFrames = m_bufferFrames - paddingFrames;
    }

    hr = m_renderer->GetBuffer(availableFrames, &buffer);
    if (hr != S_OK) {
        qCCritical(lcMmAudioOutput) <<  "GetBuffer() failed hr ="<<hr;
        m_currentError = QAudio::UnderrunError;
        emit errorChanged(m_currentError);
        // Also Error Buffers need to be released
        hr = m_renderer->ReleaseBuffer(availableFrames, 0);
        return;
    }

    quint32 readBytes = availableFrames * channelCount * sampleBytes;
    qint64 read = m_eventDevice->read((char*)buffer, readBytes);
    if (read > 0 && (read < static_cast<qint64>(readBytes))) {
        qCDebug(lcMmAudioOutput) <<  "Read return" << read <<"bytes";
        // Fill the rest of the buffer with zero to avoid audio glitches
        if (m_currentError != QAudio::UnderrunError) {
            m_currentError = QAudio::UnderrunError;
            emit errorChanged(m_currentError);
        }
        if (m_currentState != QAudio::IdleState) {
            m_currentState = QAudio::IdleState;
            emit stateChanged(m_currentState);
        }
    }

    hr = m_renderer->ReleaseBuffer(availableFrames, 0);
    if (hr != S_OK)
        qFatal("Could not release buffer");

    if (m_interval && m_openTime.elapsed() - m_openTimeOffset > m_interval) {
        emit notify();
        m_openTimeOffset = m_openTime.elapsed();
    }

    m_bytesProcessed += read;
    m_processing = m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState;
}

bool QWasapiAudioOutput::initStart(bool pull)
{
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        stop();

    QMutexLocker locker(&m_mutex);

    m_interface = QWasapiUtils::createOrGetInterface(m_deviceName, QAudio::AudioOutput);
    Q_ASSERT(m_interface);

    m_pullMode = pull;
    WAVEFORMATEXTENSIBLE nFmt;
    WAVEFORMATEXTENSIBLE closest;
    WAVEFORMATEX *pClose = &closest.Format;

    if (!m_currentFormat.isValid() || !QWasapiUtils::convertToNativeFormat(m_currentFormat, &nFmt)) {
        m_currentError = QAudio::OpenError;
        emit errorChanged(m_currentError);
        return false;
    }

    HRESULT hr;

    hr = m_interface->m_client->IsFormatSupported(WASAPI_MODE, &nFmt.Format, &pClose);
    if (hr != S_OK) {
        QWasapiUtils::convertToNativeFormat(m_currentFormat, &nFmt, true);
        hr = m_interface->m_client->IsFormatSupported(WASAPI_MODE, &nFmt.Format, &pClose);
        if(hr != S_OK){
            m_currentError = QAudio::OpenError;
            emit errorChanged(m_currentError);
            return false;
        }
    }

    REFERENCE_TIME devicePeriod = 0;
    REFERENCE_TIME miniumDevicePeriod;
    if(WASAPI_MODE == AUDCLNT_SHAREMODE_EXCLUSIVE){
        hr = m_interface->m_client->GetDevicePeriod(NULL, &miniumDevicePeriod);
    }
    else {
        hr = m_interface->m_client->GetDevicePeriod(&miniumDevicePeriod, NULL);
    }
    if (hr != S_OK) {
        m_currentError = QAudio::OpenError;
        emit errorChanged(m_currentError);
        return false;
    }
    if (m_bufferBytes) {
        devicePeriod = m_currentFormat.durationForBytes(m_bufferBytes) * 10;
    }
    if(devicePeriod < miniumDevicePeriod){
        devicePeriod = miniumDevicePeriod;
    }
    DWORD flags = pull ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0;
    hr = m_interface->m_client->Initialize(WASAPI_MODE,
                                           flags,
                                           (WASAPI_MODE == AUDCLNT_SHAREMODE_SHARED && flags == AUDCLNT_STREAMFLAGS_EVENTCALLBACK) ? 0 : devicePeriod,
                                           (WASAPI_MODE == AUDCLNT_SHAREMODE_EXCLUSIVE) ? devicePeriod : 0,
                                           &nFmt.Format,
                                           NULL);
    if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){
        hr = m_interface->m_client->GetBufferSize(&m_bufferFrames);
        EMIT_RETURN_FALSE_IF_FAILED("Could not access buffer size.", QAudio::OpenError)
        devicePeriod = (REFERENCE_TIME)
                        ((10000.0 * 1000 / nFmt.Format.nSamplesPerSec * m_bufferFrames) + 0.5);
        m_interface = QWasapiUtils::createOrGetInterface(m_deviceName, QAudio::AudioOutput);
        Q_ASSERT(m_interface);
        hr = m_interface->m_client->Initialize(WASAPI_MODE, flags, devicePeriod, devicePeriod, &nFmt.Format, nullptr);
    }
    EMIT_RETURN_FALSE_IF_FAILED("Could not initialize audio client.", QAudio::OpenError)

    hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_renderer));
    EMIT_RETURN_FALSE_IF_FAILED("Could not acquire render service.", QAudio::OpenError)

    if(WASAPI_MODE == AUDCLNT_SHAREMODE_SHARED){
        hr = m_interface->m_client->GetService(IID_PPV_ARGS(&m_volumeControl));
        if (FAILED(hr))
            qCDebug(lcMmAudioOutput) << "Could not acquire volume control.";
    }

    hr = m_interface->m_client->GetBufferSize(&m_bufferFrames);
    EMIT_RETURN_FALSE_IF_FAILED("Could not access buffer size.", QAudio::OpenError)
    qCDebug(lcMmAudioOutput) << "Buffer frame count:"<<m_bufferFrames;
    if(flags == AUDCLNT_STREAMFLAGS_EVENTCALLBACK)
    {
        BYTE* data = NULL;
        hr = m_renderer->GetBuffer(m_bufferFrames, &data);
        if (FAILED(hr))
            qCDebug(lcMmAudioOutput) << "Could not acquire render buffer.";
        quint32 readBytes = (m_bufferFrames * m_currentFormat.channelCount() * m_currentFormat.sampleSize()) / 8;
        qint64 read = m_eventDevice->read((char*)data, readBytes);
        hr = m_renderer->ReleaseBuffer(m_bufferFrames, (read < static_cast<qint64>(readBytes)) ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
        if (FAILED(hr))
            qCDebug(lcMmAudioOutput) << "Could not release render buffer.";
    }
    if (m_pullMode) {
        m_eventThread = new QWasapiProcessThread(this);
        m_event = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, SYNCHRONIZE | EVENT_MODIFY_STATE);
        m_eventThread->m_event = m_event;

        hr = m_interface->m_client->SetEventHandle(m_event);
        EMIT_RETURN_FALSE_IF_FAILED("Could not set event handle.", QAudio::OpenError)
    } else {
        m_eventDevice = new WasapiOutputDevicePrivate(this);
        m_eventDevice->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
        DWORD taskIndex = 0;
        m_hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
        if (m_hTask == NULL) {
            m_currentError = QAudio::OpenError;
            emit errorChanged(m_currentError);
            return false;
        }
    }
    hr = m_interface->m_client->Start();
    EMIT_RETURN_FALSE_IF_FAILED("Could not start audio render client.", QAudio::OpenError)

    m_openTime.restart();
    m_openTimeOffset = 0;
    m_bytesProcessed = 0;
    return true;
}

QAudio::Error QWasapiAudioOutput::error() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << m_currentError;
    return m_currentError;
}

QAudio::State QWasapiAudioOutput::state() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_currentState;
}

void QWasapiAudioOutput::start(QIODevice *device)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << device;
    if(!device || !device->isOpen()){
        qCCritical(lcMmAudioOutput) << __FUNCTION__ << "Device is null or not openned";
        return;
    }
    m_eventDevice = device;
    if (!initStart(true)) {
        qCritical(lcMmAudioOutput) << __FUNCTION__ << "failed";
        return;
    }

    m_mutex.lock();
    m_currentState = QAudio::ActiveState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    m_eventThread->start();
}

QIODevice *QWasapiAudioOutput::start()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (!initStart(false)) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "failed";
        return nullptr;
    }

    m_mutex.lock();
    m_currentState = QAudio::IdleState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);

    return m_eventDevice;
}

void QWasapiAudioOutput::stop()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return;

    if (!m_pullMode) {
        m_eventDevice->deleteLater();
        m_eventDevice = nullptr;
        if (m_hTask) {
            AvRevertMmThreadCharacteristics(m_hTask);
        }
    }

    m_mutex.lock();
    m_currentState = QAudio::StoppedState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    if (m_currentError != QAudio::NoError) {
        m_mutex.lock();
        m_currentError = QAudio::NoError;
        m_mutex.unlock();
        emit errorChanged(m_currentError);
    }

    if (m_eventThread) {
        SetEvent(m_eventThread->m_event);
        while (m_eventThread->isRunning())
            QThread::yieldCurrentThread();
        m_eventThread->deleteLater();
        m_eventThread = 0;
    }
    m_interface->m_client->Stop();
    m_interface->m_client->Reset();
}

int QWasapiAudioOutput::bytesFree() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
        return 0;

    quint32 paddingFrames;
    HRESULT hr = m_interface->m_client->GetCurrentPadding(&paddingFrames);
    if (FAILED(hr)) {
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "Could not query padding frames.";
        return bufferSize();
    }

    const quint32 availableFrames = m_bufferFrames - paddingFrames;
    const quint32 res = availableFrames * m_currentFormat.channelCount() * m_currentFormat.sampleSize() / 8;
    return res;
}

int QWasapiAudioOutput::periodSize() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    REFERENCE_TIME miniumDevicePeriod;
    HRESULT hr = m_interface->m_client->GetDevicePeriod((WASAPI_MODE == AUDCLNT_SHAREMODE_EXCLUSIVE) ? NULL : &miniumDevicePeriod, (WASAPI_MODE == AUDCLNT_SHAREMODE_EXCLUSIVE) ? &miniumDevicePeriod : NULL);
    if (FAILED(hr))
        return 0;
    const QAudioFormat f = m_currentFormat.isValid() ? m_currentFormat : m_interface->m_mixFormat;
    const int res = f.bytesForDuration(miniumDevicePeriod / 10);
    return res;
}

void QWasapiAudioOutput::setBufferSize(int value)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << value;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return;
    m_bufferBytes = value;
}

int QWasapiAudioOutput::bufferSize() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::ActiveState || m_currentState == QAudio::IdleState)
        return m_bufferFrames * m_currentFormat.channelCount()* m_currentFormat.sampleSize() / 8;

    return m_bufferBytes;
}

void QWasapiAudioOutput::setNotifyInterval(int ms)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << ms;
    m_interval = qMax(0, ms);
}

int QWasapiAudioOutput::notifyInterval() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_interval;
}

qint64 QWasapiAudioOutput::processedUSecs() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    qint64 res = qint64(1000000) * m_bytesProcessed /
                 (m_currentFormat.channelCount() * (m_currentFormat.sampleSize() / 8)) /
                 m_currentFormat.sampleRate();

    return res;
}

void QWasapiAudioOutput::resume()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (m_currentState != QAudio::SuspendedState)
        return;

    HRESULT hr;
    if(m_pullMode){
        BYTE* data = NULL;
        hr = m_renderer->GetBuffer(m_bufferFrames, &data);
        if (SUCCEEDED(hr)){
            quint32 readBytes = (m_bufferFrames * m_currentFormat.channelCount() * m_currentFormat.sampleSize()) / 8;
            qint64 read = m_eventDevice->read((char*)data, readBytes);
            hr = m_renderer->ReleaseBuffer(m_bufferFrames, (read < static_cast<qint64>(readBytes)) ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
            if (FAILED(hr))
                qCDebug(lcMmAudioOutput) << "Could not release render buffer.";
        }
        else{
            qCDebug(lcMmAudioOutput) << "Could not acquire render buffer.";
        }
    }
    hr = m_interface->m_client->Start();
    EMIT_RETURN_VOID_IF_FAILED("Could not start audio render client.", QAudio::FatalError)

    m_mutex.lock();
    m_currentError = QAudio::NoError;
    m_currentState = m_pullMode ? QAudio::ActiveState : QAudio::IdleState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);
    if (m_eventThread)
        m_eventThread->start();
}

void QWasapiAudioOutput::setFormat(const QAudioFormat& fmt)
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__ << fmt;
    if (m_currentState != QAudio::StoppedState)
        return;
    m_currentFormat = fmt;
}

QAudioFormat QWasapiAudioOutput::format() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    return m_currentFormat;
}

void QWasapiAudioOutput::suspend()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;

    if (m_currentState != QAudio::ActiveState && m_currentState != QAudio::IdleState)
        return;

    m_mutex.lock();
    m_currentState = QAudio::SuspendedState;
    m_mutex.unlock();
    emit stateChanged(m_currentState);

    HRESULT hr = m_interface->m_client->Stop();
    EMIT_RETURN_VOID_IF_FAILED("Could not suspend audio render client.", QAudio::FatalError);
    if (m_eventThread) {
        SetEvent(m_eventThread->m_event);
        while (m_eventThread->isRunning())
            QThread::yieldCurrentThread();
        qCDebug(lcMmAudioOutput) << __FUNCTION__ << "Thread has stopped";
    }
}

qint64 QWasapiAudioOutput::elapsedUSecs() const
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    if (m_currentState == QAudio::StoppedState)
        return 0;
    return m_openTime.elapsed() * qint64(1000);
}

void QWasapiAudioOutput::reset()
{
    qCDebug(lcMmAudioOutput) << __FUNCTION__;
    stop();
}

QT_END_NAMESPACE

#include "qwasapiaudiooutput.moc"
