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

#ifndef ANNOTATEDEXCEPTION_H
#define ANNOTATEDEXCEPTION_H

#include <exception>
#include <QString>

/**
 * @brief The AnnotatedException class provides an exception with a message.
 *
 * The std::runtime_error is also an exception with message, but between the ti-sdk, at least version 03.03.00.04
 * and Debian 8 Beagle Bone images, I get a stdc++/glibc version mismatch. So, I wrote my own.
 */
class AnnotatedException : public std::exception
{
protected:
    QString message;

public:
    AnnotatedException(const QString &message);
    ~AnnotatedException() throw();
    virtual const QString toString();
    virtual const char* what() const throw();
};

#endif // ANNOTATEDEXCEPTION_H
