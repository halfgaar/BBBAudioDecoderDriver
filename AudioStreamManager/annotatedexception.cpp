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

#include "annotatedexception.h"

AnnotatedException::AnnotatedException(const QString &message)
{
    this->message = message;
}

AnnotatedException::~AnnotatedException() throw()
{

}

/*!
 * \brief AnnotatedException::toString returns the actual QString, not a C-string that what() returns.
 * \return
 */
const QString AnnotatedException::toString()
{
    return message;
}

const char *AnnotatedException::what() const throw()
{
    return message.toLatin1().data();
}
