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

#include <QCoreApplication>
#include <streammanager.h>
#include <lcdi2c.h>

int main(int argc, char *argv[])
{
    try
    {
        QCoreApplication a(argc, argv);

        LCDi2c lcd;
        lcd.open();

        StreamManager manager(lcd);
        manager.start();

        return a.exec();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 99;
    }
}
