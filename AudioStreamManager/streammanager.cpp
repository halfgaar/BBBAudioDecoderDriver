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

#include "streammanager.h"
#include <QNetworkInterface>

void StreamManager::setIpAddressOnLcd()
{
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    foreach (const QNetworkInterface &interface, interfaces)
    {
        if (interface.name() == "eth0")
        {
            QList<QNetworkAddressEntry> addressEntries = interface.addressEntries();
            foreach (const QNetworkAddressEntry &entry, addressEntries)
            {
                QHostAddress adr = entry.ip();
                if (adr.protocol() == QAbstractSocket::IPv4Protocol)
                {
                    mLcd.setLineTwo(adr.toString());
                    mIpAddrSetAt = QDateTime::currentDateTime();
                }
            }
        }
    }
}

StreamManager::StreamManager(LCDi2c &lcd, QObject *parent) : QObject(parent),
    mRingBuffer(mGpIOFunctions),
    mLcd(lcd),
    mIpDisplayExpired(false)
{
    connect(&mRingBuffer, &AudioRingBuffer::newCodecName, this, &StreamManager::onNewCodecName);
    connect(&mRingBuffer, &AudioRingBuffer::bufferBytesInfo, this, &StreamManager::onSecondLineInfo);

    setIpAddressOnLcd();
}

StreamManager::~StreamManager()
{

}

void StreamManager::start()
{
    mRingBuffer.startThreads();
}

void StreamManager::onError(const QString &error)
{
    std::cerr << error.toLatin1().data() << std::endl;
    exit(1);
}

void StreamManager::onNewCodecName(const QString &name)
{
#ifdef QT_DEBUG
    std::cout << qPrintable(name) << std::endl;
#endif
    mLcd.setLineOne(name);
}

void StreamManager::onSecondLineInfo(const QString &line)
{
    if (mIpDisplayExpired || mIpAddrSetAt.addSecs(5) < QDateTime::currentDateTime())
    {
        mIpDisplayExpired = true;
        mLcd.setLineTwo(line);
    }
}
