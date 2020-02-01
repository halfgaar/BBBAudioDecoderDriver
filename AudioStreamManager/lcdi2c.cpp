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

#include "lcdi2c.h"
#include <QThread>
#include <iostream>
#include <errno.h>

void LCDi2c::checkError(int retcode)
{
    if (retcode < 0)
    {
        QString msg = strerror(retcode);
        throw AnnotatedException(msg);
    }
}

void LCDi2c::i2c_access(char rw, uint8_t command, i2c_smbus_data *data)
{
    if (!isOpen)
        return;

    struct i2c_smbus_ioctl_data args;
    args.read_write = rw;
    args.command = command; // command being the data address
    args.size = I2C_SMBUS_BYTE_DATA;
    args.data = data;

    if(ioctl(devFile.handle(), I2C_SMBUS, &args) < 0)
    {
        int err = errno;
        std::cerr << "Writing to LCD failed: '" << strerror(err) << "'. Disabling LCD." << std::endl;
        isOpen = false;
    }
}

void LCDi2c::i2c_write(uint8_t daddress, uint8_t value)
{
    if (!isOpen)
        return;

    union i2c_smbus_data data;
    data.byte = value;
    i2c_access(I2C_SMBUS_WRITE, daddress, &data);
}

LCDi2c::LCDi2c(QObject *parent) : QObject(parent),
    devFile(DEV_PATH)
{

}

LCDi2c::~LCDi2c()
{
    devFile.close();
}

void LCDi2c::open()
{
    if (!devFile.exists())
    {
        throw AnnotatedException(QString("Can't open '%1', it doesn't exist.").arg(DEV_PATH));
    }

    if (!devFile.open(QFile::ReadWrite))
    {
        std::cerr << qPrintable(QString("Can't open %1. It's there, but ... ?").arg(devFile.fileName())) << std::endl;
        return;
    }

    if(ioctl(devFile.handle(), I2C_SLAVE, DEVICE_ADDRESS) < 0)
    {
        std::cerr << qPrintable(QString("Constructing ioctl of %1 failed. Is the LCD working?").arg(devFile.fileName())) << std::endl;
        return;
    }

    isOpen = true;

    // 5V init routine, as described by datasheet.
    i2c_write(INSTRUCTION_ADDRESS, 0x38); // Function set
    i2c_write(INSTRUCTION_ADDRESS, 0x39); // Function set
    i2c_write(INSTRUCTION_ADDRESS, 0x14); // Internal OSC frequency
    i2c_write(INSTRUCTION_ADDRESS, 0x79); // Contrast set
    i2c_write(INSTRUCTION_ADDRESS, 0x50); // Power/ICON control/Contrast set
    i2c_write(INSTRUCTION_ADDRESS, 0x6c); // Follower control
    i2c_write(INSTRUCTION_ADDRESS, 0x0c); // Display ON/OFF
    i2c_write(INSTRUCTION_ADDRESS, 0x01); // Clear Display

    QThread::msleep(10); // TODO: read the state from the chip to see when it's ready.
}

void LCDi2c::writeText(uint8_t startAddress, QString &msg)
{
    if (!isOpen)
        return;

    i2c_write(INSTRUCTION_ADDRESS, 0b10000000 | startAddress);
    int i = 0;
    foreach (QChar c, msg)
    {
        i2c_write(DATA_ADDRESS, c.toLatin1());

        // Even though the display is 20 chars wide, the extra RAM can be used for scrolling text. Currently I didn't implement that yet.
        i++;
        if (i >= 40)
            break;
    }

    // Clear the rest of the line
    while (i++ < 40)
    {
        i2c_write(DATA_ADDRESS, 0);
    }
}

void LCDi2c::setLineOne(QString msg)
{
    writeText(0, msg);
}

void LCDi2c::setLineTwo(QString msg)
{
    writeText(0x40, msg);
}
