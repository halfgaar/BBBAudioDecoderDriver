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

#ifndef LCDI2C_H
#define LCDI2C_H

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <QObject>
#include <QFile>
#include <annotatedexception.h>

#define DEV_PATH "/dev/i2c-1"
#define DEVICE_ADDRESS 0x3c
#define INSTRUCTION_ADDRESS 0x00
#define DATA_ADDRESS 0x40

/**
 * @brief The LCDi2c class controls a midas-MCCOG22005A6W-BNMLWI I2c display, which is simply a ST7032 controller.
 */
class LCDi2c : public QObject
{
    Q_OBJECT

    QFile devFile;

    /**
     * @brief checkError checks the return code and generates a message using the kernel's strerror function and
     *        raises an exception with that message.
     * @param retcode return code returned by kernel functions.
     *
     * Because this method has limited scope (only works properly with kernel error codes), keeping it local to this class.
     */
    void checkError(int retcode);

    void i2c_access(char rw, uint8_t command, union i2c_smbus_data *data);
    void i2c_write(uint8_t daddress, uint8_t value);
    void writeText(uint8_t startAddress, QString &msg);
public:
    explicit LCDi2c(QObject *parent = nullptr);
    ~LCDi2c();
    void open();
    void setLineOne(QString msg);
    void setLineTwo(QString msg);

signals:

public slots:
};

#endif // LCDI2C_H
