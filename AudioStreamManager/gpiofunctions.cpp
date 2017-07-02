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

#include "gpiofunctions.h"

GpIOFunctions::GpIOFunctions() : QObject(0),
    mGpIODIR9001AudioPin(DIR9001_GPIO_FORMAT_PATH)
{
    QFile DIR9001AudioGpio("/sys/class/gpio/gpio51");
    if (!DIR9001AudioGpio.exists())
    {
        std::cout << "Exporting GPIO 51 for DIR9001 format detection." << std::endl;

        QFile exportFile("/sys/class/gpio/export");
        exportFile.open(QFile::WriteOnly);
        exportFile.write("51");
        exportFile.close();

        // The loop doesn't seem necessary in tests, but seems like a good safe-guard.
        QFile directionFile("/sys/class/gpio/gpio51/direction");
        int tries = 0;
        while (!directionFile.exists() && tries < 10)
        {
            QThread::msleep(100);
            tries++;
        }
        directionFile.open(QFile::WriteOnly);
        directionFile.write("in");
        directionFile.close();
    }
    if (!mGpIODIR9001AudioPin.exists())
    {
        emit signalError(QString("Can't find '%1', for determing DIR9001 audio format.").arg(DIR9001_GPIO_FORMAT_PATH));
    }

    mGpIODIR9001AudioPin.open(QFile::ReadOnly);

    mWatchDIRGpioFileTimer.setInterval(500);
    connect(&mWatchDIRGpioFileTimer, &QTimer::timeout, this, &GpIOFunctions::onWatchDIRGpioFileTimerTimeout);
    //mWatchDIRGpioFileTimer.start(); // Disabled the period checker, and now check every time I write to Alsa.
    mLastAudioFormat = DIR9001SeesEncodedAudio();

}

bool GpIOFunctions::DIR9001SeesEncodedAudio()
{
    mGpIODIR9001AudioPin.seek(0);
    char data;
    mGpIODIR9001AudioPin.read(&data, 1);
    return data == '1';
}

void GpIOFunctions::onWatchDIRGpioFileTimerTimeout()
{
    bool current = mLastAudioFormat;
    bool new_value = DIR9001SeesEncodedAudio();

    if (current != new_value)
    {
        std::cout << "Audio formated changed. New format is encoded: " << new_value << std::endl;
        mLastAudioFormat = new_value;
        emit signalAudioFormatChanged(new_value);
    }
}
