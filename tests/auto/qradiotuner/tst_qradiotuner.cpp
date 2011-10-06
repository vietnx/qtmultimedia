/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/multimedia

#include "tst_qradiotuner.h"

QT_USE_NAMESPACE

void tst_QRadioTuner::initTestCase()
{
    qRegisterMetaType<QRadioTuner::State>("QRadioTuner::State");
    qRegisterMetaType<QRadioTuner::Band>("QRadioTuner::Band");

    mock = new MockRadioTunerControl(this);
    service = new MockMediaService(this, mock);
    provider = new MockMediaServiceProvider(service);
    radio = new QRadioTuner(0,provider);
    QVERIFY(radio->service() != 0);
    QVERIFY(radio->isAvailable());
    QVERIFY(radio->availabilityError() == QtMultimedia::NoError);

    QSignalSpy stateSpy(radio, SIGNAL(stateChanged(QRadioTuner::State)));

    QCOMPARE(radio->state(), QRadioTuner::StoppedState);    
    radio->start();
    QVERIFY(radio->availabilityError() == QtMultimedia::NoError);
    QCOMPARE(radio->state(), QRadioTuner::ActiveState);

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.first()[0].value<QRadioTuner::State>(), QRadioTuner::ActiveState);
}

void tst_QRadioTuner::cleanupTestCase()
{
    QVERIFY(radio->error() == QRadioTuner::NoError);
    QVERIFY(radio->errorString().isEmpty());

    QSignalSpy stateSpy(radio, SIGNAL(stateChanged(QRadioTuner::State)));

    radio->stop();
    QVERIFY(radio->availabilityError() == QtMultimedia::NoError);
    QCOMPARE(radio->state(), QRadioTuner::StoppedState);
    QCOMPARE(stateSpy.count(), 1);

    QCOMPARE(stateSpy.first()[0].value<QRadioTuner::State>(), QRadioTuner::StoppedState);

    delete radio;
    delete service;
    delete provider;
}

void tst_QRadioTuner::testNullService()
{
    const QPair<int, int> nullRange(0, 0);

    MockMediaServiceProvider provider(0);
    QRadioTuner radio(0, &provider);
    QVERIFY(!radio.isAvailable());
    radio.start();
    QCOMPARE(radio.error(), QRadioTuner::ResourceError);
    QCOMPARE(radio.errorString(), QString());
    QCOMPARE(radio.band(), QRadioTuner::FM);
    QCOMPARE(radio.isBandSupported(QRadioTuner::AM), false);
    QCOMPARE(radio.isBandSupported(QRadioTuner::FM), false);
    QCOMPARE(radio.frequency(), 0);
    QCOMPARE(radio.frequencyStep(QRadioTuner::AM), 0);
    QCOMPARE(radio.frequencyStep(QRadioTuner::FM), 0);
    QCOMPARE(radio.frequencyRange(QRadioTuner::AM), nullRange);
    QCOMPARE(radio.frequencyRange(QRadioTuner::FM), nullRange);
    QCOMPARE(radio.isStereo(), false);
    QCOMPARE(radio.stereoMode(), QRadioTuner::Auto);
    QCOMPARE(radio.signalStrength(), 0);
    QCOMPARE(radio.volume(), 0);
    QCOMPARE(radio.isMuted(), false);
    QCOMPARE(radio.isSearching(), false);
    radio.stop();
}

void tst_QRadioTuner::testNullControl()
{
    const QPair<int, int> nullRange(0, 0);

    MockMediaService service(0, 0);
    MockMediaServiceProvider provider(&service);
    QRadioTuner radio(0, &provider);
    QVERIFY(!radio.isAvailable());
    radio.start();

    QCOMPARE(radio.error(), QRadioTuner::ResourceError);
    QCOMPARE(radio.errorString(), QString());

    QCOMPARE(radio.band(), QRadioTuner::FM);
    QCOMPARE(radio.isBandSupported(QRadioTuner::AM), false);
    QCOMPARE(radio.isBandSupported(QRadioTuner::FM), false);
    QCOMPARE(radio.frequency(), 0);
    QCOMPARE(radio.frequencyStep(QRadioTuner::AM), 0);
    QCOMPARE(radio.frequencyStep(QRadioTuner::FM), 0);
    QCOMPARE(radio.frequencyRange(QRadioTuner::AM), nullRange);
    QCOMPARE(radio.frequencyRange(QRadioTuner::FM), nullRange);
    {
        QSignalSpy bandSpy(&radio, SIGNAL(bandChanged(QRadioTuner::Band)));
        QSignalSpy frequencySpy(&radio, SIGNAL(frequencyChanged(int)));

        radio.setFrequency(107500);
        QCOMPARE(radio.band(), QRadioTuner::FM);
        QCOMPARE(radio.frequency(), 0);
        QCOMPARE(bandSpy.count(), 0);
        QCOMPARE(frequencySpy.count(), 0);

        radio.setBand(QRadioTuner::AM);
        QCOMPARE(radio.band(), QRadioTuner::FM);
        QCOMPARE(radio.frequency(), 0);
        QCOMPARE(bandSpy.count(), 0);
        QCOMPARE(frequencySpy.count(), 0);
    }
    QCOMPARE(radio.isStereo(), false);
    QCOMPARE(radio.stereoMode(), QRadioTuner::Auto);

    radio.setStereoMode(QRadioTuner::ForceStereo);
    QCOMPARE(radio.stereoMode(), QRadioTuner::Auto);

    QCOMPARE(radio.signalStrength(), 0);

    QCOMPARE(radio.volume(), 0);
    QCOMPARE(radio.isMuted(), false);
    {
        QSignalSpy volumeSpy(&radio, SIGNAL(volumeChanged(int)));
        QSignalSpy muteSpy(&radio, SIGNAL(mutedChanged(bool)));

        radio.setVolume(76);
        QCOMPARE(radio.volume(), 0);
        QCOMPARE(volumeSpy.count(), 0);

        radio.setMuted(true);
        QCOMPARE(radio.isMuted(), false);
        QCOMPARE(muteSpy.count(), 0);
    }
    QCOMPARE(radio.isSearching(), false);
    {
        QSignalSpy spy(&radio, SIGNAL(searchingChanged(bool)));

        radio.searchBackward();
        QCOMPARE(radio.isSearching(), false);
        QCOMPARE(spy.count(), 0);

        radio.searchForward();
        QCOMPARE(radio.isSearching(), false);
        QCOMPARE(spy.count(), 0);

        radio.cancelSearch();
        QCOMPARE(radio.isSearching(), false);
        QCOMPARE(spy.count(), 0);
    }

    radio.stop();
}

void tst_QRadioTuner::testBand()
{
    QVERIFY(radio->isBandSupported(QRadioTuner::FM));
    QVERIFY(!radio->isBandSupported(QRadioTuner::SW));

    if(radio->isBandSupported(QRadioTuner::AM)) {
        QSignalSpy readSignal(radio, SIGNAL(bandChanged(QRadioTuner::Band)));
        radio->setBand(QRadioTuner::AM);
        QTestEventLoop::instance().enterLoop(1);
        QVERIFY(radio->band() == QRadioTuner::AM);
        QVERIFY(readSignal.count() == 1);
    }
}

void tst_QRadioTuner::testFrequency()
{
    QSignalSpy readSignal(radio, SIGNAL(frequencyChanged(int)));
    radio->setFrequency(104500000);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radio->frequency() == 104500000);
    QVERIFY(readSignal.count() == 1);

    QVERIFY(radio->frequencyStep(QRadioTuner::FM) == 1);
    QPair<int,int> test = radio->frequencyRange(QRadioTuner::FM);
    QVERIFY(test.first == 1);
    QVERIFY(test.second == 2);
}

void tst_QRadioTuner::testMute()
{
    QSignalSpy readSignal(radio, SIGNAL(mutedChanged(bool)));
    radio->setMuted(true);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radio->isMuted());
    QVERIFY(readSignal.count() == 1);
}

void tst_QRadioTuner::testSearch()
{
    QSignalSpy readSignal(radio, SIGNAL(searchingChanged(bool)));
    QVERIFY(!radio->isSearching());

    radio->searchForward();
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radio->isSearching());
    QVERIFY(readSignal.count() == 1);

    radio->cancelSearch();
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(!radio->isSearching());
    QVERIFY(readSignal.count() == 2);

    radio->searchBackward();
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radio->isSearching());
    QVERIFY(readSignal.count() == 3);

    radio->cancelSearch();
    QVERIFY(!radio->isSearching());
}

void tst_QRadioTuner::testVolume()
{
    QVERIFY(radio->volume() == 100);
    QSignalSpy readSignal(radio, SIGNAL(volumeChanged(int)));
    radio->setVolume(50);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radio->volume() == 50);
    QVERIFY(readSignal.count() == 1);
}

void tst_QRadioTuner::testSignal()
{
    QVERIFY(radio->signalStrength() == 0);
    // There is no set of this only a get, do nothing else.
}

void tst_QRadioTuner::testStereo()
{
    /* no set function to toggle stereo status;
    cannot emit stereoStatusChanged() signal */

    QVERIFY(radio->isStereo());
    radio->setStereoMode(QRadioTuner::ForceMono);
    QVERIFY(radio->stereoMode() == QRadioTuner::ForceMono);
}

// QRadioTuner's errorsignal
void tst_QRadioTuner::errorSignal()
{
    qRegisterMetaType<QRadioTuner::Error>("QRadioTuner::Error");
    QObject obj;
    MockRadioTunerControl dctrl(&obj);
    MockMediaService service(&obj, &dctrl);
    MockMediaServiceProvider provider(&service);
    QRadioTuner radio(0,&provider);
    QSignalSpy spy(&radio, SIGNAL(error(QRadioTuner::Error)));
    QVERIFY(radio.service() != 0);
    QVERIFY(radio.isAvailable());
    radio.start();
    radio.setBand(QRadioTuner::FM);
    QVERIFY(spy.count() == 1);
    QVERIFY(qvariant_cast<QRadioTuner::Error>(spy.at(0).at(0)) == QRadioTuner::NoError);
    QVERIFY(radio.error() == QRadioTuner::NoError);
    QVERIFY(radio.error() != QRadioTuner::OpenError);
    QVERIFY(radio.errorString().isEmpty());
    spy.clear();

    /* emits QRadioTuner::OutOfRangeError if band is set to FM2 or LW
           and frequency set to >= 148500000 */

    radio.setBand(QRadioTuner::LW);
    radio.setBand(QRadioTuner::FM2);
    radio.setFrequency(148500000);
    QVERIFY(spy.count() == 3);
    QVERIFY(qvariant_cast<QRadioTuner::Error>(spy.at(0).at(0)) == QRadioTuner::OutOfRangeError);
    QVERIFY(qvariant_cast<QRadioTuner::Error>(spy.at(1).at(0)) == QRadioTuner::OutOfRangeError);
    QVERIFY(qvariant_cast<QRadioTuner::Error>(spy.at(2).at(0)) == QRadioTuner::OutOfRangeError);
    QVERIFY(radio.error() == QRadioTuner::OutOfRangeError);
    QVERIFY2(!radio.errorString().isEmpty(), "band and range not supported");
    spy.clear();
    radio.stop();
}
