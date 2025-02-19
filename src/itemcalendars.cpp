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

#include <QtOrganizer/QOrganizerItemCollectionFilter>
#include <QtOrganizer/QOrganizerEventOccurrence>
#include <QtOrganizer/QOrganizerTodoOccurrence>
#include <QtOrganizer/QOrganizerManagerEngine>

#include <KCalendarCore/Event>
#include <KCalendarCore/Todo>
#include <KCalendarCore/Journal>
#include <KCalendarCore/OccurrenceIterator>

using namespace QtOrganizer;

static KCalendarCore::Alarm::Ptr toAlarm(KCalendarCore::Incidence::Ptr incidence,
                                         const QOrganizerItemReminder &reminder)
{
    KCalendarCore::Alarm::Ptr alarm = incidence->newAlarm();
    alarm->setStartOffset(KCalendarCore::Duration(-reminder.secondsBeforeStart()));
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
    incidence->clearAlarms();
  
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

static void toItemReminder(QOrganizerItemReminder *reminder,
                           const KCalendarCore::Alarm::Ptr &alarm)
{
    reminder->setSecondsBeforeStart(-alarm->startOffset().asSeconds());
    reminder->setRepetition(alarm->repeatCount(), alarm->snoozeTime().asSeconds());
}

static QOrganizerRecurrenceRule fromRecurrenceRule(const KCalendarCore::RecurrenceRule &rule)
{
    QOrganizerRecurrenceRule result;

    switch (rule.recurrenceType()) {
    case KCalendarCore::RecurrenceRule::rDaily:
        result.setFrequency(QOrganizerRecurrenceRule::Daily);
        break;
    case KCalendarCore::RecurrenceRule::rWeekly:
        result.setFrequency(QOrganizerRecurrenceRule::Weekly);
        break;
    case KCalendarCore::RecurrenceRule::rMonthly:
        result.setFrequency(QOrganizerRecurrenceRule::Monthly);
        break;
    case KCalendarCore::RecurrenceRule::rYearly:
        result.setFrequency(QOrganizerRecurrenceRule::Yearly);
        break;
    default:
        result.setFrequency(QOrganizerRecurrenceRule::Invalid);
    }
    result.setInterval(rule.frequency());
    if (rule.duration() > 0) {
        result.setLimit(rule.duration());
    } else if (rule.duration() == 0) {
        result.setLimit(rule.endDt().date());
    }
    if (!rule.byDays().isEmpty()) {
        QSet<Qt::DayOfWeek> days;
        for (const KCalendarCore::RecurrenceRule::WDayPos pos : rule.byDays()) {
            days.insert(Qt::DayOfWeek(pos.day()));
        }
        result.setDaysOfWeek(days);
    } else if (!rule.byMonthDays().isEmpty()) {
        QSet<int> days;
        for (int day : rule.byMonthDays()) {
            days.insert(day);
        }
        result.setDaysOfMonth(days);
    } else if (!rule.byYearDays().isEmpty()) {
        QSet<int> days;
        for (int day : rule.byYearDays()) {
            days.insert(day);
        }
        result.setDaysOfYear(days);
    } else if (!rule.byMonths().isEmpty()) {
        QSet<QOrganizerRecurrenceRule::Month> months;
        for (const int &month : rule.byMonths()) {
            months.insert(QOrganizerRecurrenceRule::Month(month));
        }
        result.setMonthsOfYear(months);
    } else if (!rule.byWeekNumbers().isEmpty()) {
        QSet<int> weeks;
        for (int week : rule.byWeekNumbers()) {
            weeks.insert(week);
        }
        result.setWeeksOfYear(weeks);
    } else if (!rule.bySetPos().isEmpty()) {
        QSet<int> pos;
        for (int p : rule.bySetPos()) {
            pos.insert(p);
        }
        result.setPositions(pos);
    }

    return result;
}

static void toItemIncidence(QOrganizerItem *item,
                            const KCalendarCore::Incidence::Ptr &incidence,
                            const QList<QOrganizerItemDetail::DetailType> &details)
{
    item->setDisplayLabel(incidence->summary());
    item->setDescription(incidence->description());
    item->setComments(incidence->comments());
    if (incidence->dirtyFields().contains(KCalendarCore::Incidence::FieldSecrecy)
        && (details.isEmpty() || details.contains(QOrganizerItemDetail::TypeClassification))) {
        QOrganizerItemClassification classification;
        switch (incidence->secrecy()) {
        case KCalendarCore::Incidence::SecrecyPrivate:
            classification.setClassification(QOrganizerItemClassification::AccessPrivate);
            break;
        case KCalendarCore::Incidence::SecrecyConfidential:
            classification.setClassification(QOrganizerItemClassification::AccessConfidential);
            break;
        default:
            classification.setClassification(QOrganizerItemClassification::AccessPublic);
            break;
        }
        item->saveDetail(&classification);
    }
    if ((!incidence->location().isEmpty() || incidence->hasGeo())
        && (details.isEmpty()
            || details.contains(QOrganizerItemDetail::TypeLocation))) {
        QOrganizerItemLocation loc;
        loc.setLabel(incidence->location());
        if (incidence->hasGeo()) {
            loc.setLatitude(incidence->geoLatitude());
            loc.setLongitude(incidence->geoLongitude());
        }
        item->saveDetail(&loc);
    }
    if (incidence->dirtyFields().contains(KCalendarCore::Incidence::FieldPriority)
        && (details.isEmpty()
            || details.contains(QOrganizerItemDetail::TypePriority))) {
        QOrganizerItemPriority priority;
        priority.setPriority(QOrganizerItemPriority::Priority(incidence->priority()));
        item->saveDetail(&priority);
    }
    if ((incidence->dirtyFields().contains(KCalendarCore::Incidence::FieldCreated)
         || incidence->dirtyFields().contains(KCalendarCore::Incidence::FieldLastModified))
        && (details.isEmpty()
            || details.contains(QOrganizerItemDetail::TypeTimestamp))) {
        QOrganizerItemTimestamp stamp;
        stamp.setCreated(incidence->created());
        stamp.setLastModified(incidence->lastModified());
        item->saveDetail(&stamp);
    }
    if (incidence->dirtyFields().contains(KCalendarCore::Incidence::FieldRevision)
        && (details.isEmpty()
            || details.contains(QOrganizerItemDetail::TypeVersion))) {
        QOrganizerItemVersion stamp;
        stamp.setVersion(incidence->revision());
        item->saveDetail(&stamp);
    }
    if (details.isEmpty()
        || details.contains(QOrganizerItemDetail::TypeReminder)) {
        for (const KCalendarCore::Alarm::Ptr alarm : incidence->alarms()) {
            switch (alarm->type()) {
            case KCalendarCore::Alarm::Audio: {
                QOrganizerItemAudibleReminder audio;
                audio.setDataUrl(QUrl(alarm->audioFile()));
                toItemReminder(&audio, alarm);
                item->saveDetail(&audio);
                break;
            }
            case KCalendarCore::Alarm::Email: {
                QOrganizerItemEmailReminder email;
                email.setContents(alarm->mailSubject(), alarm->mailText(), QVariantList());
                QStringList recipients;
                for (const KCalendarCore::Person &person : alarm->mailAddresses()) {
                    recipients.append(person.fullName());
                }
                email.setRecipients(recipients);
                toItemReminder(&email, alarm);
                item->saveDetail(&email);
                break;
            }
            case KCalendarCore::Alarm::Display: {
                QOrganizerItemVisualReminder visual;
                visual.setMessage(alarm->text());
                toItemReminder(&visual, alarm);
                item->saveDetail(&visual);
                break;
            }
            default:
                break;
            }
        }
    }
    if (incidence->recurs()) {
        QOrganizerItemRecurrence recurrence;
        QSet<QDate> rdates;
        if (incidence->allDay()) {
            for (const QDate &date : incidence->recurrence()->rDates()) {
                rdates.insert(date);
            }
        } else {
            for (const QDateTime &dt : incidence->recurrence()->rDateTimes()) {
                rdates.insert(dt.date());
            }
        }
        recurrence.setRecurrenceDates(rdates);
        QSet<QOrganizerRecurrenceRule> rrules;
        for (const KCalendarCore::RecurrenceRule *rule : incidence->recurrence()->rRules()) {
            rrules.insert(fromRecurrenceRule(*rule));
        }
        recurrence.setRecurrenceRules(rrules);
        QSet<QDate> exdates;
        if (incidence->allDay()) {
            for (const QDate &date : incidence->recurrence()->exDates()) {
                exdates.insert(date);
            }
        } else {
            for (const QDateTime &dt : incidence->recurrence()->exDateTimes()) {
                exdates.insert(dt.date());
            }
        }
        recurrence.setExceptionDates(exdates);
        QSet<QOrganizerRecurrenceRule> exrules;
        for (const KCalendarCore::RecurrenceRule *rule : incidence->recurrence()->exRules()) {
            exrules.insert(fromRecurrenceRule(*rule));
        }
        recurrence.setExceptionRules(exrules);
        item->saveDetail(&recurrence);
    }
}

static void toItemEvent(QOrganizerItem *item, const KCalendarCore::Event::Ptr &event,
                        const QList<QOrganizerItemDetail::DetailType> &details,
                        const QDateTime &occurrenceStart = QDateTime(),
                        const QDateTime &occurrenceEnd = QDateTime(),
                        const QDateTime &recurrenceId = QDateTime())
{
    if (event->hasRecurrenceId() || recurrenceId.isValid()) {
        item->setType(QOrganizerItemType::TypeEventOccurrence);
        QOrganizerItemParent parent;
        parent.setParentId(QOrganizerItemId(item->collectionId().managerUri(),
                                            event->uid().toUtf8()));
        if (event->hasRecurrenceId()) {
            parent.setOriginalDate(event->recurrenceId().date());
        } else {
            parent.setOriginalDate(recurrenceId.date());
        }
        item->saveDetail(&parent);
    } else {
        item->setType(QOrganizerItemType::TypeEvent);
    }
    QOrganizerEventTime time;
    time.setStartDateTime(occurrenceStart.isValid() ? occurrenceStart : event->dtStart());
    time.setEndDateTime(occurrenceEnd.isValid() ? occurrenceEnd : event->dtEnd());
    time.setAllDay(event->allDay());
    item->saveDetail(&time);
    if (event->dirtyFields().contains(KCalendarCore::Incidence::FieldOrganizer)
        && (details.isEmpty() || details.contains(QOrganizerItemDetail::TypeEventRsvp))) {
        QOrganizerEventRsvp rsvp;
        rsvp.setOrganizerName(event->organizer().name());
        rsvp.setOrganizerEmail(event->organizer().email());
        item->saveDetail(&rsvp);
    }
    if (details.isEmpty() || details.contains(QOrganizerItemDetail::TypeEventAttendee)) {
        for (const KCalendarCore::Attendee &att : event->attendees()) {
            QOrganizerEventAttendee attendee;
            switch (att.status()) {
            case KCalendarCore::Attendee::Accepted:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusAccepted);
                break;
            case KCalendarCore::Attendee::Declined:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusDeclined);
                break;
            case KCalendarCore::Attendee::Tentative:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusTentative);
                break;
            case KCalendarCore::Attendee::Delegated:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusDelegated);
                break;
            case KCalendarCore::Attendee::InProcess:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusInProcess);
                break;
            case KCalendarCore::Attendee::Completed:
                attendee.setParticipationStatus(QOrganizerEventAttendee::StatusCompleted);
                break;
            default:
                break;
            }
            switch (att.role()) {
            case KCalendarCore::Attendee::ReqParticipant:
                attendee.setParticipationRole(QOrganizerEventAttendee::RoleRequiredParticipant);
                break;
            case KCalendarCore::Attendee::OptParticipant:
                attendee.setParticipationRole(QOrganizerEventAttendee::RoleOptionalParticipant);
                break;
            case KCalendarCore::Attendee::NonParticipant:
                attendee.setParticipationRole(QOrganizerEventAttendee::RoleNonParticipant);
                break;
            case KCalendarCore::Attendee::Chair:
                attendee.setParticipationRole(QOrganizerEventAttendee::RoleChairperson);
                break;
            default:
                break;
            }
            attendee.setAttendeeId(att.uid());
            attendee.setName(att.name());
            attendee.setEmailAddress(att.email());
            item->saveDetail(&attendee);
        }
    }
    toItemIncidence(item, event, details);
}

static void toItemTodo(QOrganizerItem *item, const KCalendarCore::Todo::Ptr &todo,
                       const QList<QOrganizerItemDetail::DetailType> &details,
                       const QDateTime &occurrenceStart = QDateTime(),
                       const QDateTime &occurrenceEnd = QDateTime(),
                       const QDateTime &recurrenceId = QDateTime())
{
    if (todo->hasRecurrenceId() || recurrenceId.isValid()) {
        item->setType(QOrganizerItemType::TypeTodoOccurrence);
        QOrganizerItemParent parent;
        parent.setParentId(QOrganizerItemId(item->collectionId().managerUri(),
                                            todo->uid().toUtf8()));
        if (todo->hasRecurrenceId()) {
            parent.setOriginalDate(todo->recurrenceId().date());
        } else {
            parent.setOriginalDate(recurrenceId.date());
        }
        item->saveDetail(&parent);
    } else {
        item->setType(QOrganizerItemType::TypeTodo);
    }
    QOrganizerTodoTime time;
    time.setStartDateTime(occurrenceStart.isValid() ? occurrenceStart : todo->dtStart());
    time.setDueDateTime(occurrenceEnd.isValid() ? occurrenceEnd : todo->dtDue());
    time.setAllDay(todo->allDay());
    item->saveDetail(&time);
    if ((todo->dirtyFields().contains(KCalendarCore::Incidence::FieldPercentComplete)
         || todo->dirtyFields().contains(KCalendarCore::Incidence::FieldCompleted))
        && (details.isEmpty()
            || details.contains(QOrganizerItemDetail::TypeTodoProgress))) {
        QOrganizerTodoProgress progress;
        progress.setFinishedDateTime(todo->completed());
        progress.setPercentageComplete(todo->percentComplete());
        item->saveDetail(&progress);
    }
    toItemIncidence(item, todo, details);
}

static void toItemJournal(QOrganizerItem *item,
                          const KCalendarCore::Journal::Ptr &journal,
                          const QList<QOrganizerItemDetail::DetailType> &details)
{
    item->setType(QOrganizerItemType::TypeJournal);
    QOrganizerJournalTime time;
    time.setEntryDateTime(journal->dtStart());
    item->saveDetail(&time);
    toItemIncidence(item, journal, details);
}

ItemCalendars::ItemCalendars(const QTimeZone &timezone)
    : mKCal::ExtendedCalendar(timezone)
{
}

QOrganizerItem ItemCalendars::item(const QOrganizerItemId &id,
                                   const QList<QOrganizerItemDetail::DetailType> &details) const
{
    QOrganizerItem item;

    KCalendarCore::Incidence::Ptr incidence = instance(id.localId());
    if (incidence) {
        item.setId(id);
        item.setCollectionId(QOrganizerCollectionId(id.managerUri(),
                                                    notebook(incidence).toUtf8()));
        switch (incidence->type()) {
        case KCalendarCore::Incidence::TypeEvent:
            toItemEvent(&item, incidence.staticCast<KCalendarCore::Event>(), details);
            break;
        case KCalendarCore::Incidence::TypeTodo:
            toItemTodo(&item, incidence.staticCast<KCalendarCore::Todo>(), details);
            break;
        case KCalendarCore::Incidence::TypeJournal:
            toItemJournal(&item, incidence.staticCast<KCalendarCore::Journal>(), details);
            break;
        default:
            break;
        }
    }

    return item;
}

QList<QOrganizerItem> ItemCalendars::items(const QString &managerUri,
                                           const QOrganizerItemFilter &filter,
                                           const QDateTime &startDateTime,
                                           const QDateTime &endDateTime,
                                           int maxCount,
                                           const QList<QOrganizerItemDetail::DetailType> &details) const
{
    QList<QOrganizerItem> items;

    int count = 0;
    KCalendarCore::OccurrenceIterator it(*this, startDateTime, endDateTime);
    while (it.hasNext() && (count < maxCount || maxCount < 1)) {
        it.next();
        KCalendarCore::Incidence::Ptr incidence = it.incidence();
        const QByteArray notebookUid = notebook(incidence).toUtf8();
        if (filter.type() == QOrganizerItemFilter::CollectionFilter) {
            bool match = false;
            for (const QOrganizerCollectionId &id : QOrganizerItemCollectionFilter(filter).collectionIds()) {
                if (id.localId() == notebookUid) {
                    match = true;
                    break;
                }
            }
            if (!match) {
                continue;
            }
        }
        QOrganizerItem item;
        if (!it.recurrenceId().isValid() || incidence->hasRecurrenceId()) {
            // A "real" occurrence, either a non-recurring incidence or an exception.
            item.setId(QOrganizerItemId(managerUri,
                                        incidence->instanceIdentifier().toUtf8()));
        }
        item.setCollectionId(QOrganizerCollectionId(managerUri, notebookUid));
        switch (incidence->type()) {
        case KCalendarCore::Incidence::TypeEvent:
            toItemEvent(&item, incidence.staticCast<KCalendarCore::Event>(), details,
                        it.occurrenceStartDate(), it.occurrenceEndDate(), it.recurrenceId());
            break;
        case KCalendarCore::Incidence::TypeTodo:
            toItemTodo(&item, incidence.staticCast<KCalendarCore::Todo>(), details,
                       it.occurrenceStartDate(), it.occurrenceEndDate(), it.recurrenceId());
            break;
        case KCalendarCore::Incidence::TypeJournal:
            toItemJournal(&item, incidence.staticCast<KCalendarCore::Journal>(), details);
            break;
        default:
            break;
        }
        if (QOrganizerManagerEngine::testFilter(filter, item)) {
            items.append(item);
            count += 1;
        }
    }

    return items;
}

QList<QOrganizerItem> ItemCalendars::occurrences(const QString &managerUri,
                                                 const QOrganizerItem &parentItem,
                                                 const QDateTime &startDateTime,
                                                 const QDateTime &endDateTime,
                                                 int maxCount,
                                                 const QList<QOrganizerItemDetail::DetailType> &details) const
{
    QList<QOrganizerItem> items;

    KCalendarCore::Incidence::Ptr parent = incidence(parentItem.id().localId());
    if (!parent) {
        return items;
    }
    const QByteArray notebookUid = notebook(parent).toUtf8();

    int count = 0;
    KCalendarCore::OccurrenceIterator it(*this, parent, startDateTime, endDateTime);
    while (it.hasNext() && (count < maxCount || maxCount < 1)) {
        it.next();
        QOrganizerItem item;
        if (it.incidence()->hasRecurrenceId()) {
            item.setId(QOrganizerItemId(managerUri,
                                        it.incidence()->instanceIdentifier().toUtf8()));
        }
        item.setCollectionId(QOrganizerCollectionId(managerUri, notebookUid));
        switch (parent->type()) {
        case KCalendarCore::Incidence::TypeEvent:
            toItemEvent(&item, it.incidence().staticCast<KCalendarCore::Event>(), details,
                        it.occurrenceStartDate(), it.occurrenceEndDate(), it.recurrenceId());
            break;
        case KCalendarCore::Incidence::TypeTodo:
            toItemTodo(&item, it.incidence().staticCast<KCalendarCore::Todo>(), details,
                       it.occurrenceStartDate(), it.occurrenceEndDate(), it.recurrenceId());
            break;
        case KCalendarCore::Incidence::TypeJournal:
            toItemJournal(&item, it.incidence().staticCast<KCalendarCore::Journal>(), details);
            break;
        default:
            break;
        }
        items.append(item);
        count += 1;
    }

    return items;
}

QByteArray ItemCalendars::addItem(const QOrganizerItem &item)
{
    if (item.collectionId().isNull()) {
        return QByteArray();
    }

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
    if (newIncidence
        && addIncidence(newIncidence,
                        QString::fromUtf8(item.collectionId().localId()))) {
        return newIncidence->instanceIdentifier().toUtf8();
    }
    return QByteArray();
}

bool ItemCalendars::updateItem(const QOrganizerItem &item,
                               const QList<QOrganizerItemDetail::DetailType> &detailMask)
{
    KCalendarCore::Incidence::Ptr incidence;
    switch (item.type()) {
    case QOrganizerItemType::TypeEventOccurrence:
    case QOrganizerItemType::TypeEvent:
        incidence = instance(item.id().localId());
        if (incidence && incidence->type() == KCalendarCore::Incidence::TypeEvent) {
            updateEvent(incidence.staticCast<KCalendarCore::Event>(), item, detailMask);
        } else {
            incidence.clear();
        }
        break;
    case QOrganizerItemType::TypeTodoOccurrence:
    case QOrganizerItemType::TypeTodo:
        incidence = instance(item.id().localId());
        if (incidence && incidence->type() == KCalendarCore::Incidence::TypeTodo) {
            updateTodo(incidence.staticCast<KCalendarCore::Todo>(), item, detailMask);
        } else {
            incidence.clear();
        }
        break;
    case QOrganizerItemType::TypeJournal:
        incidence = journal(item.id().localId());
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
    if ((item.type() == QOrganizerItemType::TypeEventOccurrence
         || item.type() == QOrganizerItemType::TypeTodoOccurrence)
        && item.id().isNull()) {
        QOrganizerItemParent detail = item.detail(QOrganizerItemDetail::TypeParent);
        KCalendarCore::Incidence::Ptr parent = incidence(detail.parentId().localId());
        if (parent) {
            if (parent->allDay()) {
                parent->recurrence()->addExDate(detail.originalDate());
            } else{
                QDateTime recurId = parent->dtStart();
                recurId.setDate(detail.originalDate());
                parent->recurrence()->addExDateTime(recurId);
            }
        }
        return !parent.isNull();
    } else {
        KCalendarCore::Incidence::Ptr doomed = instance(item.id().localId());
        if (doomed && !deleteIncidence(doomed)) {
            doomed.clear();
        }
        return !doomed.isNull();
    }
}
