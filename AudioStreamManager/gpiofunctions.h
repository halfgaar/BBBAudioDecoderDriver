/*
 * Copyright (C) 2018  Wiebe Cazemier <wiebe@halfgaar.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * https://www.gnu.org/licenses/gpl-2.0.html
 */

#ifndef GPIO_H
#define GPIO_H

#include <QFile>
#include <iostream>
#include <QThread>
#include <QTimer>

#define DIR9001_GPIO_FORMAT_PATH "/sys/class/gpio/gpio51/value"

class GpIOFunctions : public QObject
{
    Q_OBJECT

    QFile mGpIODIR9001AudioPin;
    QTimer mWatchDIRGpioFileTimer;
    bool mLastAudioFormat = false;
public:
    GpIOFunctions();

    /**
     * The DIR9001 has a pin AUDIO-active-low, which means it's low when the data is PCM audio and high when it's encoded.
     */
    bool DIR9001SeesEncodedAudio();

private slots:
    void onWatchDIRGpioFileTimerTimeout();

signals:
    void signalError(const QString &error);
    void signalAudioFormatChanged(bool encoded);
};

#endif // GPIO_H
