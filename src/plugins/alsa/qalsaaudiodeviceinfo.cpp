/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// INTERNAL USE ONLY: Do NOT use for any other purpose.
//

#include "qalsaaudiodeviceinfo.h"

#include <alsa/version.h>

QT_BEGIN_NAMESPACE

QAlsaAudioDeviceInfo::QAlsaAudioDeviceInfo(const QByteArray &dev, QAudio::Mode mode)
{
    handle = 0;

    device = QLatin1String(dev);
    this->mode = mode;

    updateLists();
}

QAlsaAudioDeviceInfo::~QAlsaAudioDeviceInfo()
{
    close();
}

bool QAlsaAudioDeviceInfo::isFormatSupported(const QAudioFormat& format) const
{
    return testSettings(format);
}

QAudioFormat QAlsaAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat nearest;
    if(mode == QAudio::AudioOutput) {
        nearest.setSampleRate(44100);
        nearest.setChannelCount(2);
        nearest.setByteOrder(QAudioFormat::LittleEndian);
        nearest.setSampleType(QAudioFormat::SignedInt);
        nearest.setSampleSize(16);
        nearest.setCodec(QLatin1String("audio/pcm"));
    } else {
        nearest.setSampleRate(8000);
        nearest.setChannelCount(1);
        nearest.setSampleType(QAudioFormat::UnSignedInt);
        nearest.setSampleSize(8);
        nearest.setCodec(QLatin1String("audio/pcm"));
        if(!testSettings(nearest)) {
            nearest.setChannelCount(2);
            nearest.setSampleSize(16);
            nearest.setSampleType(QAudioFormat::SignedInt);
        }
    }
    return nearest;
}

QString QAlsaAudioDeviceInfo::deviceName() const
{
    return "Alsa - " + device;
}

QStringList QAlsaAudioDeviceInfo::supportedCodecs()
{
    return codecz;
}

QList<int> QAlsaAudioDeviceInfo::supportedSampleRates()
{
    return sampleRatez;
}

QList<int> QAlsaAudioDeviceInfo::supportedChannelCounts()
{
    return channelz;
}

QList<int> QAlsaAudioDeviceInfo::supportedSampleSizes()
{
    return sizez;
}

QList<QAudioFormat::Endian> QAlsaAudioDeviceInfo::supportedByteOrders()
{
    return byteOrderz;
}

QList<QAudioFormat::SampleType> QAlsaAudioDeviceInfo::supportedSampleTypes()
{
    return typez;
}

QByteArray QAlsaAudioDeviceInfo::defaultDevice(QAudio::Mode mode)
{
    const auto &devices = availableDevices(mode);
    if (devices.size() == 0)
        return QByteArray();

    return devices.first();
}

bool QAlsaAudioDeviceInfo::open()
{
    qDebug()<<__func__;
    int err = 0;
    QString dev;

    if (!availableDevices(mode).contains(device.toLocal8Bit())){
        qCritical()<<__func__<<"device"<<device<<"is not available";
        return false;
    }

#if SND_LIB_VERSION < 0x1000e  // 1.0.14
    if (device.compare(QLatin1String("default")) != 0)
        dev = deviceFromCardName(device);
    else
#endif
        dev = device;

    if(mode == QAudio::AudioOutput) {
        err=snd_pcm_open( &handle,dev.toLocal8Bit().constData(),SND_PCM_STREAM_PLAYBACK,0);
    } else {
        err=snd_pcm_open( &handle,dev.toLocal8Bit().constData(),SND_PCM_STREAM_CAPTURE,0);
    }
    if(err < 0) {
        qCritical()<<__func__<<"failed to open device"<<device<<"error:"<<snd_strerror(err);
        handle = 0;
        return false;
    }
    snd_pcm_nonblock(handle, 0);
    return true;
}

void QAlsaAudioDeviceInfo::close()
{
    if(handle)
        snd_pcm_close(handle);
    handle = 0;
}

bool QAlsaAudioDeviceInfo::testSettings(const QAudioFormat& format) const
{
    qDebug()<<__func__<< device << format;
    // Set nearest to closest settings that do work.
    // See if what is in settings will work (return value).
    int err = 0;
    snd_pcm_t* pcmHandle;
    snd_pcm_hw_params_t *params;

    // For now, just accept only audio/pcm codec
    if (!format.codec().startsWith(QLatin1String("audio/pcm"))){
        return false;
    }
    bool openNeed = false;
    if(!handle){
        QString dev;

#if SND_LIB_VERSION < 0x1000e  // 1.0.14
        if (device.compare(QLatin1String("default")) != 0)
            dev = deviceFromCardName(device);
        else
    #endif
            dev = device;

        if(mode == QAudio::AudioOutput) {
            err=snd_pcm_open( &pcmHandle, dev.toLocal8Bit().constData(), SND_PCM_STREAM_PLAYBACK, 0);
        } else {
            err=snd_pcm_open( &pcmHandle, dev.toLocal8Bit().constData(), SND_PCM_STREAM_CAPTURE, 0);
        }
        if(err < 0) {
            return false;
        }
        snd_pcm_nonblock(pcmHandle, 0);
        openNeed = true;
    }
    else{
        pcmHandle = handle;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcmHandle, params);

    snd_pcm_format_t pcmFormat = SND_PCM_FORMAT_UNKNOWN;
    switch (format.sampleSize()) {
    case 8:
        if (format.sampleType() == QAudioFormat::SignedInt)
            pcmFormat = SND_PCM_FORMAT_S8;
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            pcmFormat = SND_PCM_FORMAT_U8;
        break;
    case 16:
        if (format.sampleType() == QAudioFormat::SignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_S16_BE;
        } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_U16_LE : SND_PCM_FORMAT_U16_BE;
        }
        break;
    case 20:
        if (format.sampleType() == QAudioFormat::SignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_S20_3LE : SND_PCM_FORMAT_S20_3BE;
        } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_U20_3LE : SND_PCM_FORMAT_U20_3BE;
        }
        break;
    case 24:
        if (format.sampleType() == QAudioFormat::SignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_S24_3LE : SND_PCM_FORMAT_S24_3BE;
        } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_U24_3LE : SND_PCM_FORMAT_U24_3BE;
        }
        break;
    case 32:
        if (format.sampleType() == QAudioFormat::SignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_S32_LE : SND_PCM_FORMAT_S32_BE;
        } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_U32_LE : SND_PCM_FORMAT_U32_BE;
        } else if (format.sampleType() == QAudioFormat::Float) {
            pcmFormat = format.byteOrder() == QAudioFormat::LittleEndian
                      ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_FLOAT_BE;
        }
    }

    if(pcmFormat == SND_PCM_FORMAT_UNKNOWN){
        if(openNeed){
            snd_pcm_close(pcmHandle);
        }
        return false;
    }

    err = snd_pcm_hw_params_test_format(pcmHandle, params, pcmFormat);
    if(err < 0){
        qDebug() << __func__ << device << "This format is not supported" << format.sampleType() << format.sampleSize();
    }

    if (err >= 0 && format.channelCount() > 0) {
        err = snd_pcm_hw_params_test_channels(pcmHandle, params, format.channelCount());
    }
    if(err < 0){
        qDebug() << __func__ << device <<"This channel count is not supported" << format.channelCount();
    }

    if (err >= 0 && format.sampleRate() != -1) {
        err = snd_pcm_hw_params_test_rate(pcmHandle, params, format.sampleRate(), 0);
    }
    if(err < 0){
        qDebug() << __func__ << device <<"This sample rate is not supported" << format.sampleRate();
    }

    if(openNeed){
        snd_pcm_close(pcmHandle);
    }

    return (err == 0);
}

void QAlsaAudioDeviceInfo::updateLists()
{
    qDebug() << __func__;
    // redo all lists based on current settings
    sampleRatez.clear();
    channelz.clear();
    sizez.clear();
    byteOrderz.clear();
    typez.clear();
    codecz.clear();
    codecz.append(QLatin1String("audio/pcm"));
    byteOrderz.append(QAudioFormat::LittleEndian);
    byteOrderz.append(QAudioFormat::BigEndian);

    if(!handle)
        open();

    if(!handle)
        return;
    QAudioFormat referenceFormat = preferredFormat();
    const int sizes[] = {16, 20, 24, 32};
    referenceFormat.setSampleType(QAudioFormat::SignedInt);
    for (int s : sizes) {
        QAudioFormat f = referenceFormat;
        f.setSampleSize(s);
        if (isFormatSupported(f)){
            sizez.append(s);
        }
    }
    if(!sizez.isEmpty()){
        referenceFormat.setSampleSize(sizez.first());
        typez.append(QAudioFormat::SignedInt);
    }

    const int rates[] = {32000, 44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000, 705600, 768000};
    for (int rate : rates) {
        QAudioFormat f = referenceFormat;
        f.setSampleRate(rate);
        if (isFormatSupported(f))
            sampleRatez.append(rate);
    }

    for (int i = 1; i <= 8; ++i) {
        QAudioFormat f = referenceFormat;
        f.setChannelCount(i);
        if (isFormatSupported(f))
            channelz.append(i);
    }

    referenceFormat.setSampleType(QAudioFormat::UnSignedInt);
    if (isFormatSupported(referenceFormat))
        typez.append(QAudioFormat::UnSignedInt);

    referenceFormat.setSampleType(QAudioFormat::Float);
    referenceFormat.setSampleSize(32);
    if (isFormatSupported(referenceFormat))
        typez.append(QAudioFormat::Float);
    close();
}

QList<QByteArray> QAlsaAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    qDebug() << __func__;
    QList<QByteArray> devices;
    bool hasDefault = false;

#if SND_LIB_VERSION >= 0x1000e  // 1.0.14
    QByteArray filter;

    // Create a list of all current audio devices that support mode
    void **hints, **n;
    char *name, *descr, *io;

    if(snd_device_name_hint(-1, "pcm", &hints) < 0) {
        qWarning() << __func__ << "no alsa devices available";
        return devices;
    }
    n = hints;

    if(mode == QAudio::AudioInput) {
        filter = "Input";
    } else {
        filter = "Output";
    }

    while (*n != NULL) {
        name = snd_device_name_get_hint(*n, "NAME");
        if (name != 0 && qstrcmp(name, "null") != 0) {
            descr = snd_device_name_get_hint(*n, "DESC");
            io = snd_device_name_get_hint(*n, "IOID");

            if ((descr != NULL) && ((io == NULL) || (io == filter)) && (qstrncmp(name, "hw:", 3) == 0 || qstrncmp(name, "sysdefault:", 11) == 0)) {
                devices.append(name);
                if (strcmp(name, "default") == 0)
                    hasDefault = true;
            }

            free(descr);
            free(io);
        }
        free(name);
        ++n;
    }
    snd_device_name_free_hint(hints);
#else
    int idx = 0;
    char* name;

    while(snd_card_get_name(idx,&name) == 0) {
        devices.append(name);
        if (strcmp(name, "default") == 0)
            hasDefault = true;
        idx++;
    }
#endif

    if (!hasDefault && devices.size() > 0)
        devices.prepend("default");
    qDebug()<<__func__<<"available devices:"<<devices;
    return devices;
}

QString QAlsaAudioDeviceInfo::deviceFromCardName(const QString &card)
{
    int idx = 0;
    char *name;

    QStringRef shortName = card.midRef(card.indexOf(QLatin1String("="), 0) + 1);

    while (snd_card_get_name(idx, &name) == 0) {
        if (shortName.compare(QLatin1String(name)) == 0)
            break;
        idx++;
    }

    return QString(QLatin1String("hw:%1,0")).arg(idx);
}

QT_END_NAMESPACE
