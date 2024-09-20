/*
 * Copyright (C) 2024 Damien Caliste <dcaliste@free.fr>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#ifndef ITEMCALENDARS_H
#define ITEMCALENDARS_H

#include <extendedcalendar.h>

#include <QtOrganizer/QOrganizerItem>
#include <QtOrganizer/QOrganizerItemFilter>
#include <QtOrganizer/QOrganizerItemDetail>

class ItemCalendars: public mKCal::ExtendedCalendar
{
public:
    ItemCalendars(const QTimeZone &timezone);

    QtOrganizer::QOrganizerItem item(const QtOrganizer::QOrganizerItemId &id,
                                     const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &details = QList<QtOrganizer::QOrganizerItemDetail::DetailType>()) const;
    QList<QtOrganizer::QOrganizerItem> items(const QString &managerUri,
                                             const QtOrganizer::QOrganizerItemFilter &filter,
                                             const QDateTime &startDateTime,
                                             const QDateTime &endDateTime,
                                             int maxCount,
                                             const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &details) const;
    QList<QtOrganizer::QOrganizerItem> occurrences(const QString &managerUri,
                                                   const QtOrganizer::QOrganizerItem &parentItem,
                                                   const QDateTime &startDateTime,
                                                   const QDateTime &endDateTime,
                                                   int maxCount,
                                                   const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &details) const;
    
    QByteArray addItem(const QtOrganizer::QOrganizerItem &item);
    bool updateItem(const QtOrganizer::QOrganizerItem &item,
                    const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask = QList<QtOrganizer::QOrganizerItemDetail::DetailType>());
    bool removeItem(const QtOrganizer::QOrganizerItem &item);
};

#endif
