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

#include "helper.h"

#include <QtOrganizer/QOrganizerRecurrenceRule>
#include <QtOrganizer/QOrganizerItemClassification>
#include <QtOrganizer/QOrganizerItemLocation>
#include <QtOrganizer/QOrganizerItemPriority>
#include <QtOrganizer/QOrganizerItemTimestamp>
#include <QtOrganizer/QOrganizerItemVersion>
#include <QtOrganizer/QOrganizerItemAudibleReminder>
#include <QtOrganizer/QOrganizerItemEmailReminder>
#include <QtOrganizer/QOrganizerItemVisualReminder>
#include <QtOrganizer/QOrganizerItemRecurrence>
#include <QtOrganizer/QOrganizerEventTime>
#include <QtOrganizer/QOrganizerEventRsvp>
#include <QtOrganizer/QOrganizerTodoTime>
#include <QtOrganizer/QOrganizerTodoProgress>
#include <QtOrganizer/QOrganizerJournalTime>

static KCalendarCore::Alarm::Ptr toAlarm(KCalendarCore::Incidence::Ptr incidence,
                                         const QtOrganizer::QOrganizerItemReminder &reminder)
{
    KCalendarCore::Alarm::Ptr alarm = incidence->newAlarm();
    alarm->setStartOffset(KCalendarCore::Duration(reminder.secondsBeforeStart()));
    alarm->setRepeatCount(reminder.repetitionCount());
    alarm->setSnoozeTime(KCalendarCore::Duration(reminder.repetitionDelay()));

    return alarm;
}

static KCalendarCore::RecurrenceRule* toRecurrenceRule(const KCalendarCore::Incidence &incidence,
                                                       const QtOrganizer::QOrganizerRecurrenceRule &rule)
{
    KCalendarCore::RecurrenceRule *r = new KCalendarCore::RecurrenceRule;
    r->setAllDay(incidence.allDay());
    r->setStartDt(incidence.dtStart());
    switch (rule.frequency()) {
    case QtOrganizer::QOrganizerRecurrenceRule::Daily:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rDaily);
        break;
    case QtOrganizer::QOrganizerRecurrenceRule::Weekly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rWeekly);
        break;
    case QtOrganizer::QOrganizerRecurrenceRule::Monthly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rMonthly);
        break;
    case QtOrganizer::QOrganizerRecurrenceRule::Yearly:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rYearly);
        break;
    default:
        r->setRecurrenceType(KCalendarCore::RecurrenceRule::rNone);
    }
    r->setFrequency(rule.interval());
    switch (rule.limitType()) {
    case QtOrganizer::QOrganizerRecurrenceRule::CountLimit:
        r->setDuration(rule.limitCount());
        break;
    case QtOrganizer::QOrganizerRecurrenceRule::DateLimit: {
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
        for (const QtOrganizer::QOrganizerRecurrenceRule::Month &month : rule.monthsOfYear()) {
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
                            const QtOrganizer::QOrganizerItem &item,
                            const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask = QList<QtOrganizer::QOrganizerItemDetail::DetailType>())
{
    if (detailMask.isEmpty()
        || detailMask.contains(QtOrganizer::QOrganizerItemDetail::TypeDisplayLabel)) {
        incidence->setSummary(item.displayLabel());
    }
    if (detailMask.isEmpty()
        || detailMask.contains(QtOrganizer::QOrganizerItemDetail::TypeDescription)) {
        incidence->setDescription(item.description());
    }
    if (detailMask.isEmpty()
        || detailMask.contains(QtOrganizer::QOrganizerItemDetail::TypeComment)) {
        incidence->clearComments();
        for (const QString &comment : item.comments()) {
            incidence->addComment(comment);
        }
    }
    for (const QtOrganizer::QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QtOrganizer::QOrganizerItemDetail::TypeClassification:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemClassification classification(detail);
                switch (classification.classification()) {
                case QtOrganizer::QOrganizerItemClassification::AccessPrivate:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyPrivate);
                    break;
                case QtOrganizer::QOrganizerItemClassification::AccessConfidential:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyConfidential);
                    break;
                default:
                    incidence->setSecrecy(KCalendarCore::Incidence::SecrecyPublic);
                    break;
                }
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeLocation:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemLocation loc(detail);
                incidence->setLocation(loc.label());
                incidence->setGeoLatitude(loc.latitude());
                incidence->setGeoLongitude(loc.longitude());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypePriority:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemPriority priority(detail);
                incidence->setPriority(priority.priority());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeTimestamp:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemTimestamp stamp(detail);
                incidence->setCreated(stamp.created());
                incidence->setLastModified(stamp.lastModified());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeVersion:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemVersion stamp(detail);
                incidence->setRevision(stamp.version());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeAudibleReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemAudibleReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                alarm->setAudioAlarm(reminder.dataUrl().toString());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeEmailReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemEmailReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                KCalendarCore::Person::List recipients;
                for (const QString &recipient : reminder.recipients()) {
                    recipients.append(KCalendarCore::Person::fromFullName(recipient));
                }
                alarm->setEmailAlarm(reminder.subject(), reminder.body(), recipients);
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeVisualReminder:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemVisualReminder reminder(detail);
                KCalendarCore::Alarm::Ptr alarm = toAlarm(incidence, reminder);
                alarm->setDisplayAlarm(reminder.message());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeEventRsvp:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerEventRsvp rsvp(detail);
                incidence->setOrganizer(KCalendarCore::Person(rsvp.organizerName(),
                                                              rsvp.organizerEmail()));
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeEventAttendee:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerEventAttendee attendee(detail);
                KCalendarCore::Attendee::PartStat part;
                switch (attendee.participationStatus()) {
                case QtOrganizer::QOrganizerEventAttendee::StatusAccepted:
                    part = KCalendarCore::Attendee::Accepted;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::StatusDeclined:
                    part = KCalendarCore::Attendee::Declined;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::StatusTentative:
                    part = KCalendarCore::Attendee::Tentative;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::StatusDelegated:
                    part = KCalendarCore::Attendee::Delegated;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::StatusInProcess:
                    part = KCalendarCore::Attendee::InProcess;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::StatusCompleted:
                    part = KCalendarCore::Attendee::Completed;
                    break;
                default:
                    part = KCalendarCore::Attendee::NeedsAction;
                }
                KCalendarCore::Attendee::Role role;
                switch (attendee.participationRole()) {
                case QtOrganizer::QOrganizerEventAttendee::RoleRequiredParticipant:
                    role = KCalendarCore::Attendee::ReqParticipant;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::RoleOptionalParticipant:
                    role = KCalendarCore::Attendee::OptParticipant;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::RoleNonParticipant:
                    role = KCalendarCore::Attendee::NonParticipant;
                    break;
                case QtOrganizer::QOrganizerEventAttendee::RoleChairperson:
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
        case QtOrganizer::QOrganizerItemDetail::TypeRecurrence:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerItemRecurrence recurrence(detail);
                KCalendarCore::Recurrence *recur = incidence->recurrence();
                for (const QtOrganizer::QOrganizerRecurrenceRule &rule : recurrence.recurrenceRules()) {
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
                for (const QtOrganizer::QOrganizerRecurrenceRule &rule : recurrence.exceptionRules()) {
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

void updateEvent(KCalendarCore::Event::Ptr event,
                 const QtOrganizer::QOrganizerItem &item,
                 const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask)
{
    updateIncidence(event, item, detailMask);
    for (const QtOrganizer::QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QtOrganizer::QOrganizerItemDetail::TypeEventTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerEventTime time(detail);
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

void updateTodo(KCalendarCore::Todo::Ptr todo,
                const QtOrganizer::QOrganizerItem &item,
                const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask)
{
    updateIncidence(todo, item, detailMask);
    for (const QtOrganizer::QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QtOrganizer::QOrganizerItemDetail::TypeTodoTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerTodoTime time(detail);
                todo->setDtStart(time.startDateTime());
                todo->setDtDue(time.dueDateTime());
                todo->setAllDay(time.isAllDay());
            }
            break;
        case QtOrganizer::QOrganizerItemDetail::TypeTodoProgress:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerTodoProgress progress(detail);
                todo->setCompleted(progress.finishedDateTime());
                todo->setPercentComplete(progress.percentageComplete());
            }
            break;
        default:
            break;
        }
    }
}

void updateJournal(KCalendarCore::Journal::Ptr journal,
                   const QtOrganizer::QOrganizerItem &item,
                   const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask)
{
    updateIncidence(journal, item, detailMask);
    for (const QtOrganizer::QOrganizerItemDetail &detail : item.details()) {
        switch (detail.type()) {
        case QtOrganizer::QOrganizerItemDetail::TypeJournalTime:
            if (detailMask.isEmpty()
                || detailMask.contains(detail.type())) {
                QtOrganizer::QOrganizerJournalTime time(detail);
                journal->setDtStart(time.entryDateTime());
            }
            break;
        default:
            break;
        }
    }
}

QtOrganizer::QOrganizerCollection toCollection(const QString &managerUri,
                                               const mKCal::Notebook::Ptr &nb)
{
    QtOrganizer::QOrganizerCollection collection;
    collection.setId(QtOrganizer::QOrganizerCollectionId(managerUri, nb->uid().toUtf8()));
    collection.setMetaData(QtOrganizer::QOrganizerCollection::KeyName,
                           nb->name());
    collection.setMetaData(QtOrganizer::QOrganizerCollection::KeyDescription,
                           nb->description());
    collection.setMetaData(QtOrganizer::QOrganizerCollection::KeyColor,
                           nb->color());
    collection.setExtendedMetaData(QStringLiteral("shared"),
                                   nb->isShared());
    collection.setExtendedMetaData(QStringLiteral("master"),
                                   nb->isMaster());
    collection.setExtendedMetaData(QStringLiteral("synchronized"),
                                   nb->isSynchronized());
    collection.setExtendedMetaData(QStringLiteral("readOnly"),
                                   nb->isReadOnly());
    collection.setExtendedMetaData(QStringLiteral("visible"),
                                   nb->isVisible());
    collection.setExtendedMetaData(QStringLiteral("syncDate"),
                                   nb->syncDate());
    collection.setExtendedMetaData(QStringLiteral("pluginName"),
                                   nb->pluginName());
    collection.setExtendedMetaData(QStringLiteral("account"),
                                   nb->account());
    collection.setExtendedMetaData(QStringLiteral("attachmentSize"),
                                   nb->attachmentSize());
    collection.setExtendedMetaData(QStringLiteral("creationDate"),
                                   nb->creationDate());
    collection.setExtendedMetaData(QStringLiteral("modifiedDate"),
                                   nb->modifiedDate());
    collection.setExtendedMetaData(QStringLiteral("sharedWith"),
                                   nb->sharedWith());
    collection.setExtendedMetaData(QStringLiteral("syncProfile"),
                                   nb->syncProfile());
    for (const QByteArray &key : nb->customPropertyKeys()) {
        if (key == "secondaryColor") {
            collection.setMetaData(QtOrganizer::QOrganizerCollection::KeySecondaryColor,
                                   nb->customProperty(key));
        } else if (key == "image") {
            collection.setMetaData(QtOrganizer::QOrganizerCollection::KeyImage,
                                   nb->customProperty(key));
        } else {
            collection.setExtendedMetaData(QString::fromUtf8(key),
                                           nb->customProperty(key));
        }
    }
    return collection;
}

void updateNotebook(mKCal::Notebook::Ptr nb,
                    const QtOrganizer::QOrganizerCollection &collection)
{
    nb->setName(collection.metaData(QtOrganizer::QOrganizerCollection::KeyName).toString());
    nb->setDescription(collection.metaData(QtOrganizer::QOrganizerCollection::KeyDescription).toString());
    nb->setColor(collection.metaData(QtOrganizer::QOrganizerCollection::KeyColor).toString());
    nb->setCustomProperty("secondaryColor", collection.metaData(QtOrganizer::QOrganizerCollection::KeySecondaryColor).toString());
    nb->setCustomProperty("image", collection.metaData(QtOrganizer::QOrganizerCollection::KeyImage).toString());
    const QVariantMap props = collection.extendedMetaData();
    for (QVariantMap::ConstIterator it = props.constBegin();
         it != props.constEnd(); ++it) {
        if (it.key() == QStringLiteral("shared")) {
            nb->setIsShared(it.value().toBool());
        } else if (it.key() == QStringLiteral("master")) {
            nb->setIsMaster(it.value().toBool());
        } else if (it.key() == QStringLiteral("synchronized")) {
            nb->setIsSynchronized(it.value().toBool());
        } else if (it.key() == QStringLiteral("readOnly")) {
            nb->setIsReadOnly(it.value().toBool());
        } else if (it.key() == QStringLiteral("visible")) {
            nb->setIsVisible(it.value().toBool());
        } else if (it.key() == QStringLiteral("syncDate")) {
            nb->setSyncDate(it.value().toDateTime());
        } else if (it.key() == QStringLiteral("creationDate")) {
            nb->setCreationDate(it.value().toDateTime());
        } else if (it.key() == QStringLiteral("modifiedDate")) {
            nb->setModifiedDate(it.value().toDateTime());
        } else if (it.key() == QStringLiteral("pluginName")) {
            nb->setPluginName(it.value().toString());
        } else if (it.key() == QStringLiteral("account")) {
            nb->setAccount(it.value().toString());
        } else if (it.key() == QStringLiteral("syncProfile")) {
            nb->setSyncProfile(it.value().toString());
        } else if (it.key() == QStringLiteral("attachmentSize")) {
            nb->setAttachmentSize(it.value().toInt());
        } else if (it.key() == QStringLiteral("sharedWith")) {
            nb->setSharedWith(it.value().toStringList());
        } else {
            nb->setCustomProperty(it.key().toUtf8(), it.value().toString());
        }
    }
}
