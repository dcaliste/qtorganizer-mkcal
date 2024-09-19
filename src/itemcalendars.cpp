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

#include "itemcalendars.h"

#include <QtOrganizer/QOrganizerRecurrenceRule>
#include <QtOrganizer/QOrganizerItemDetail>
#include <QtOrganizer/QOrganizerItemClassification>
#include <QtOrganizer/QOrganizerItemLocation>
#include <QtOrganizer/QOrganizerItemPriority>
#include <QtOrganizer/QOrganizerItemTimestamp>
#include <QtOrganizer/QOrganizerItemVersion>
#include <QtOrganizer/QOrganizerItemAudibleReminder>
#include <QtOrganizer/QOrganizerItemEmailReminder>
#include <QtOrganizer/QOrganizerItemVisualReminder>
#include <QtOrganizer/QOrganizerItemParent>
#include <QtOrganizer/QOrganizerItemRecurrence>
#include <QtOrganizer/QOrganizerEventTime>
#include <QtOrganizer/QOrganizerEventRsvp>
#include <QtOrganizer/QOrganizerTodoTime>
#include <QtOrganizer/QOrganizerTodoProgress>
#include <QtOrganizer/QOrganizerJournalTime>

#include <KCalendarCore/Event>
#include <KCalendarCore/Todo>
#include <KCalendarCore/Journal>

using namespace QtOrganizer;

static KCalendarCore::Alarm::Ptr toAlarm(KCalendarCore::Incidence::Ptr incidence,
                                         const QOrganizerItemReminder &reminder)
{
    KCalendarCore::Alarm::Ptr alarm = incidence->newAlarm();
    alarm->setStartOffset(KCalendarCore::Duration(reminder.secondsBeforeStart()));
    alarm->setRepeatCount(reminder.repetitionCount());
    alarm->setSnoozeTime(KCalendarCore::Duration(reminder.repetitionDelay()));

    return alarm;
}

static KCalendarCore::RecurrenceRule* toRecurrenceRule(const KCalendarCore::Incidence &incidence,
                                                       const QOrganizerRecurrenceRule &rule)
{
    KCalendarCore::RecurrenceRule *r = new KCalendarCore::RecurrenceRule;
    r->setAllDay(incidence.allDay());
    r->setStartDt(incidence.dtStart());
    switch (rule.frequency()) {
    case QOrganizerRecurrenceRule::Daily:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rDaily);
        break;
    case QOrganizerRecurrenceRule::Weekly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rWeekly);
        break;
    case QOrganizerRecurrenceRule::Monthly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rMonthly);
        break;
    case QOrganizerRecurrenceRule::Yearly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rYearly);
        break;
    default:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rNone);
    }
    r->setFrequency(rule.interval());
    switch (rule.limitType()) {
    case QOrganizerRecurrenceRule::CountLimit:
        r->setDuration(rule.limitCount());
        break;
    case QOrganizerRecurrenceRule::DateLimit: {
        QDateTime end(incidence.dtStart());
        end.setDate(rule.limitDate());
        r->setEndDt(end);
        break;
    }
    default:
        break;
    }
    if (!rule.daysOfWeek().isEmpty()) {
        QList<KCalendarCore::RecurrenceRule::WDayPos> days;
        for (const Qt::DayOfWeek &day : rule.daysOfWeek()) {
            days << KCalendarCore::RecurrenceRule::WDayPos(0, day);
        }
        r->setByDays(days);
    }
    if (!rule.daysOfMonth().isEmpty()) {
        r->setByMonthDays(rule.daysOfMonth().toList());
    }
    if (!rule.daysOfYear().isEmpty()) {
        r->setByYearDays(rule.daysOfYear().toList());
    }
    if (!rule.monthsOfYear().isEmpty()) {
        QList<int> months;
        for (const QOrganizerRecurrenceRule::Month &month : rule.monthsOfYear()) {
            months << int(month);
        }
        r->setByMonths(months);
    }
    if (!rule.weeksOfYear().isEmpty()) {
        r->setByWeekNumbers(rule.weeksOfYear().toList());
    }
    if (!rule.positions().isEmpty()) {
        r->setBySetPos(rule.positions().toList());
    }
    return r;
}

static void updateIncidence(KCalendarCore::Incidence::Ptr incidence,
                            const QOrganizerItem &item,
                            const QList<QOrganizerItemDetail::DetailType> &detailMask = QList<QOrganizerItemDetail::DetailType>())
{
    if (detailMask.isEmpty()
        || detailMask.contains(QOrganizerItemDetail::TypeDisplayLabel)) {
        incidence->setSummary(item.displayLabel());
    }
    if (detailMask.isEmpty()
        || detailMask.contains(QOrganizerItemDetail::TypeDescription)) {
        incidence->setDescription(item.description());
    }
    if (detailMask.isEmpty()
        || detailMask.contains(QOrganizerItemDetail::TypeComment)) {
        incidence->clearComments();
        for (const QString &comment : item.comments()) {
            incidence->addComment(comment);
        }
    }
    for (const QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QOrganizerItemDetail::TypeClassification:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemClassification classification(detail);
                switch (classification.classification()) {
                case QOrganizerItemClassification::AccessPrivate:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyPrivate);
                    break;
                case QOrganizerItemClassification::AccessConfidential:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyConfidential);
                    break;
                default:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyPublic);
                    break;
                }
            }
            break;
        case QOrganizerItemDetail::TypeLocation:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemLocation loc(detail);
                incidence->setLocation(loc.label());
                incidence->setGeoLatitude(loc.latitude());
                incidence->setGeoLongitude(loc.longitude());
            }
            break;
        case QOrganizerItemDetail::TypePriority:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemPriority priority(detail);
                incidence->setPriority(priority.priority());
            }
            break;
        case QOrganizerItemDetail::TypeTimestamp:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemTimestamp stamp(detail);
                incidence->setCreated(stamp.created());
                incidence->setLastModified(stamp.lastModified());
            }
            break;
        case QOrganizerItemDetail::TypeVersion:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemVersion stamp(detail);
                incidence->setRevision(stamp.version());
            }
            break;
        case QOrganizerItemDetail::TypeAudibleReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemAudibleReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                alarm->setAudioAlarm(reminder.dataUrl().toString());
            }
            break;
        case QOrganizerItemDetail::TypeEmailReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemEmailReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                KCalendarCore::Person::List recipients;
                for (const QString &recipient : reminder.recipients()) {
                    recipients.append(KCalendarCore::Person::fromFullName(recipient));
                }
                alarm->setEmailAlarm(reminder.subject(), reminder.body(), recipients);
            }
            break;
        case QOrganizerItemDetail::TypeVisualReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemVisualReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                alarm->setDisplayAlarm(reminder.message());
            }
            break;
        case QOrganizerItemDetail::TypeEventRsvp:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerEventRsvp rsvp(detail);
                incidence->setOrganizer(KCalendarCore::Person(rsvp.organizerName(),
                                                              rsvp.organizerEmail()));
            }
            break;
        case QOrganizerItemDetail::TypeEventAttendee:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerEventAttendee attendee(detail);
                KCalendarCore::Attendee::PartStat part;
                switch (attendee.participationStatus()) {
                case QOrganizerEventAttendee::StatusAccepted:
                    part = KCalendarCore::Attendee::Accepted;
                    break;
                case QOrganizerEventAttendee::StatusDeclined:
                    part = KCalendarCore::Attendee::Declined;
                    break;
                case QOrganizerEventAttendee::StatusTentative:
                    part = KCalendarCore::Attendee::Tentative;
                    break;
                case QOrganizerEventAttendee::StatusDelegated:
                    part = KCalendarCore::Attendee::Delegated;
                    break;
                case QOrganizerEventAttendee::StatusInProcess:
                    part = KCalendarCore::Attendee::InProcess;
                    break;
                case QOrganizerEventAttendee::StatusCompleted:
                    part = KCalendarCore::Attendee::Completed;
                    break;
                default:
                    part = KCalendarCore::Attendee::NeedsAction;
                }
                KCalendarCore::Attendee::Role role;
                switch (attendee.participationRole()) {
                case QOrganizerEventAttendee::RoleRequiredParticipant:
                    role = KCalendarCore::Attendee::ReqParticipant;
                    break;
                case QOrganizerEventAttendee::RoleOptionalParticipant:
                    role = KCalendarCore::Attendee::OptParticipant;
                    break;
                case QOrganizerEventAttendee::RoleNonParticipant:
                    role = KCalendarCore::Attendee::NonParticipant;
                    break;
                case QOrganizerEventAttendee::RoleChairperson:
                    role = KCalendarCore::Attendee::Chair;
                    break;
                default:
                    role = KCalendarCore::Attendee::ReqParticipant;
                }
                KCalendarCore::Attendee att(attendee.name(), attendee.emailAddress(),
                                            false, part, role, attendee.attendeeId());
                incidence->addAttendee(att);
            }
            break;
        case QOrganizerItemDetail::TypeRecurrence:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerItemRecurrence recurrence(detail);
                KCalendarCore::Recurrence *recur = incidence->recurrence();
                for (const QOrganizerRecurrenceRule &rule : recurrence.recurrenceRules()) {
                    recur->addRRule(toRecurrenceRule(*incidence, rule));
                }
                for (const QDate &date : recurrence.recurrenceDates()) {
                    if (incidence->allDay()) {
                        recur->addRDate(date);
                    } else {
                        QDateTime dt(incidence->dtStart());
                        dt.setDate(date);
                        recur->addRDateTime(dt);
                    }
                }
                for (const QOrganizerRecurrenceRule &rule : recurrence.exceptionRules()) {
                    recur->addExRule(toRecurrenceRule(*incidence, rule));
                }
                for (const QDate &date : recurrence.exceptionDates()) {
                    if (incidence->allDay()) {
                        recur->addExDate(date);
                    } else {
                        QDateTime dt(incidence->dtStart());
                        dt.setDate(date);
                        recur->addExDateTime(dt);
                    }
                }
            }
            break;
        default:
            break;
        }
    }
}

static void updateEvent(KCalendarCore::Event::Ptr event,
                        const QOrganizerItem &item,
                        const QList<QOrganizerItemDetail::DetailType> &detailMask = QList<QOrganizerItemDetail::DetailType>())
{
    updateIncidence(event, item, detailMask);
    for (const QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QOrganizerItemDetail::TypeEventTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerEventTime time(detail);
                event->setDtStart(time.startDateTime());
                event->setDtEnd(time.endDateTime());
                event->setAllDay(time.isAllDay());
            }
            break;
        default:
            break;
        }
    }
}

static void updateTodo(KCalendarCore::Todo::Ptr todo,
                       const QOrganizerItem &item,
                       const QList<QOrganizerItemDetail::DetailType> &detailMask = QList<QOrganizerItemDetail::DetailType>())
{
    updateIncidence(todo, item, detailMask);
    for (const QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QOrganizerItemDetail::TypeTodoTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerTodoTime time(detail);
                todo->setDtStart(time.startDateTime());
                todo->setDtDue(time.dueDateTime());
                todo->setAllDay(time.isAllDay());
            }
            break;
        case QOrganizerItemDetail::TypeTodoProgress:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerTodoProgress progress(detail);
                todo->setCompleted(progress.finishedDateTime());
                todo->setPercentComplete(progress.percentageComplete());
            }
            break;
        default:
            break;
        }
    }
}

static void updateJournal(KCalendarCore::Journal::Ptr journal,
                          const QOrganizerItem &item,
                          const QList<QOrganizerItemDetail::DetailType> &detailMask = QList<QOrganizerItemDetail::DetailType>())
{
    updateIncidence(journal, item, detailMask);
    for (const QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QOrganizerItemDetail::TypeJournalTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QOrganizerJournalTime time(detail);
                journal->setDtStart(time.entryDateTime());
            }
            break;
        default:
            break;
        }
    }
}

ItemCalendars::ItemCalendars(const QTimeZone &timezone)
    : mKCal::ExtendedCalendar(timezone)
{
}

QByteArray ItemCalendars::addItem(const QOrganizerItem &item)
{
    // Need to sort *items to insert parent first.
    KCalendarCore::Incidence::Ptr newIncidence;
    switch (item.type()) {
    case QOrganizerItemType::TypeEvent:
        newIncidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Event);
        updateEvent(newIncidence.staticCast<KCalendarCore::Event>(), item);
        break;
    case QOrganizerItemType::TypeEventOccurrence: {
        const QOrganizerItemParent detail(item.detail(QOrganizerItemDetail::TypeParent));
        const QString uid = QString::fromUtf8(detail.parentId().localId());
        KCalendarCore::Incidence::Ptr parent = incidence(uid);
        if (parent) {
            QDateTime recurId = parent->dtStart();
            recurId.setDate(detail.originalDate());
            newIncidence = createException(parent, recurId);
            updateEvent(newIncidence.staticCast<KCalendarCore::Event>(), item);
        }
        break;
    }
    case QOrganizerItemType::TypeTodo:
        newIncidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Todo);
        updateTodo(newIncidence.staticCast<KCalendarCore::Todo>(), item);
        break;
    case QOrganizerItemType::TypeTodoOccurrence: {
        const QOrganizerItemParent detail(item.detail(QOrganizerItemDetail::TypeParent));
        const QString uid = QString::fromUtf8(detail.parentId().localId());
        KCalendarCore::Incidence::Ptr parent = incidence(uid);
        if (parent) {
            QDateTime recurId = parent->dtStart();
            recurId.setDate(detail.originalDate());
            newIncidence = createException(parent, recurId);
            updateTodo(newIncidence.staticCast<KCalendarCore::Todo>(), item);
        }
        break;
    }
    case QOrganizerItemType::TypeJournal:
        newIncidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Journal);
        updateJournal(newIncidence.staticCast<KCalendarCore::Journal>(), item);
        break;
    default:
        break;
    }
    if (newIncidence) {
        bool valid;
        const QString nbuid = QString::fromUtf8(item.collectionId().localId());
        if (nbuid.isEmpty()) {
            valid = addIncidence(newIncidence);
        } else {
            valid = addIncidence(newIncidence, nbuid);
        }
        if (valid) {
            return newIncidence->uid().toUtf8();
        }
    }
    return QByteArray();
}

bool ItemCalendars::updateItem(const QOrganizerItem &item,
                               const QList<QOrganizerItemDetail::DetailType> &detailMask)
{
    QOrganizerItemParent detail = item.detail(QOrganizerItemDetail::TypeParent);
    const QString uid = QString::fromUtf8(detail.isEmpty()
                                          ? item.id().localId()
                                          : detail.parentId().localId());
    QDateTime recurId;
    KCalendarCore::Incidence::Ptr incidence;
    switch (item.type()) {
    case QOrganizerItemType::TypeEventOccurrence:
        if (!detail.isEmpty()) {
            KCalendarCore::Incidence::Ptr parent = event(uid);
            if (parent) {
                recurId = parent->dtStart();
                recurId.setDate(detail.originalDate());
            } else {
                return false;
            }
        }
        // Fallthrough
    case QOrganizerItemType::TypeEvent:
        incidence = event(uid, recurId);
        if (incidence) {
            updateEvent(incidence.staticCast<KCalendarCore::Event>(), item, detailMask);
        }
        break;
    case QOrganizerItemType::TypeTodoOccurrence:
        if (!detail.isEmpty()) {
            KCalendarCore::Incidence::Ptr parent = todo(uid);
            if (parent) {
                recurId = parent->dtStart();
                recurId.setDate(detail.originalDate());
            } else {
                return false;
            }
        }
        // Fallthrough
    case QOrganizerItemType::TypeTodo:
        incidence = todo(uid, recurId);
        if (incidence) {
            updateTodo(incidence.staticCast<KCalendarCore::Todo>(), item, detailMask);
        }
        break;
    case QOrganizerItemType::TypeJournal:
        incidence = journal(uid);
        if (incidence) {
            updateJournal(incidence.staticCast<KCalendarCore::Journal>(), item, detailMask);
        }
        break;
    default:
        break;
    }
    return !incidence.isNull();
}

bool ItemCalendars::removeItem(const QtOrganizer::QOrganizerItem &item)
{
    QOrganizerItemParent detail = item.detail(QOrganizerItemDetail::TypeParent);
    const QString uid = QString::fromUtf8(detail.isEmpty()
                                          ? item.id().localId()
                                          : detail.parentId().localId());
    QDateTime recurId;
    KCalendarCore::Incidence::Ptr parent;
    KCalendarCore::Incidence::Ptr incidence;
    switch (item.type()) {
    case QOrganizerItemType::TypeEventOccurrence:
        if (!detail.isEmpty()) {
            parent = event(uid);
            if (parent) {
                recurId = parent->dtStart();
                recurId.setDate(detail.originalDate());
            } else {
                return false;
            }
            incidence = event(uid, recurId);
        }
        break;
    case QOrganizerItemType::TypeEvent:
        incidence = event(uid);
        break;
    case QOrganizerItemType::TypeTodoOccurrence:
        if (!detail.isEmpty()) {
            parent = todo(uid);
            if (parent) {
                recurId = parent->dtStart();
                recurId.setDate(detail.originalDate());
            } else {
                return false;
            }
            incidence = todo(uid, recurId);
        }
        break;
    case QOrganizerItemType::TypeTodo:
        incidence = todo(uid, recurId);
        break;
    case QOrganizerItemType::TypeJournal:
        incidence = journal(uid);
        break;
    default:
        break;
    }
    if (parent && !incidence) {
        if (parent->allDay()) {
            parent->recurrence()->addExDate(detail.originalDate());
        } else{
            parent->recurrence()->addExDateTime(recurId);
        }
        return true;
    }
    return incidence && deleteIncidence(incidence);
}
