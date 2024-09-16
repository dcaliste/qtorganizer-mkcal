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

#include "mkcalplugin.h"

#include <QtOrganizer/QOrganizerItemParent>

#include "helper.h"

using namespace QtOrganizer;

QOrganizerManagerEngine* mKCalFactory::engine(const QMap<QString, QString>& parameters, QOrganizerManager::Error* error)
{
    Q_UNUSED(error);
    QString tzname = parameters.value(QStringLiteral("timeZone"));
    QString dbname = parameters.value(QStringLiteral("databaseName"));

    mKCalEngine *engine = new mKCalEngine(QTimeZone(tzname.toUtf8()), dbname);
    if (!engine->isOpened())
        *error = QOrganizerManager::PermissionsError;
    return engine; // manager takes ownership and will clean up.
}

QString mKCalFactory::managerName() const
{
    return QStringLiteral("mkcal");
}

mKCalEngine::mKCalEngine(const QTimeZone &timeZone, const QString &databaseName,
                         QObject *parent)
    : QOrganizerManagerEngine(parent)
    , mCalendars(new mKCal::ExtendedCalendar(timeZone))
{
    if (databaseName.isEmpty()) {
        mStorage = mKCal::SqliteStorage::Ptr(new mKCal::SqliteStorage(mCalendars));
    } else {
        mStorage = mKCal::SqliteStorage::Ptr(new mKCal::SqliteStorage(mCalendars, databaseName));
    }
    mOpened = mStorage->open();
    mStorage->registerObserver(this);
}

mKCalEngine::~mKCalEngine()
{
    mStorage->unregisterObserver(this);
    mStorage->close();
}

bool mKCalEngine::isOpened() const
{
    return mOpened;
}

QString mKCalEngine::managerName() const
{
    return QStringLiteral("mkcal");
}

QMap<QString, QString> mKCalEngine::managerParameters() const
{
    QMap<QString, QString> parameters;

    parameters.insert(QStringLiteral("timeZone"),
                      QString::fromUtf8(mCalendars->timeZone().id()));
    parameters.insert(QStringLiteral("databaseName"), mStorage->databaseName());

    return parameters;
}

void mKCalEngine::storageModified(mKCal::ExtendedStorage *storage,
                                  const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);

    emit dataChanged();
}

void mKCalEngine::storageUpdated(mKCal::ExtendedStorage *storage,
                                 const KCalendarCore::Incidence::List &added,
                                 const KCalendarCore::Incidence::List &modified,
                                 const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);

    QList<QOrganizerItemId> ids;
    QList<QPair<QOrganizerItemId, QOrganizerManager::Operation>> ops;

    ids.clear();
    ops.clear();
    for (const KCalendarCore::Incidence::Ptr &incidence : added) {
        const QOrganizerItemId id(managerUri(), incidence->uid().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Add);
    }
    if (!ids.isEmpty()) {
        emit itemsAdded(ids);
    }
    if (!ops.isEmpty()) {
        emit itemsModified(ops);
    }

    ids.clear();
    ops.clear();
    for (const KCalendarCore::Incidence::Ptr &incidence : modified) {
        const QOrganizerItemId id(managerUri(), incidence->uid().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Change);
    }
    if (!ids.isEmpty()) {
        emit itemsChanged(ids, QList<QOrganizerItemDetail::DetailType>());
    }
    if (!ops.isEmpty()) {
        emit itemsModified(ops);
    }

    ids.clear();
    ops.clear();
    for (const KCalendarCore::Incidence::Ptr &incidence : deleted) {
        const QOrganizerItemId id(managerUri(), incidence->uid().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Remove);
    }
    if (!ids.isEmpty()) {
        emit itemsRemoved(ids);
    }
    if (!ops.isEmpty()) {
        emit itemsModified(ops);
    }
}

QList<QOrganizerItemType::ItemType> mKCalEngine::supportedItemTypes() const
{
    return QList<QOrganizerItemType::ItemType>()
        << QOrganizerItemType::TypeEvent
        << QOrganizerItemType::TypeEventOccurrence
        << QOrganizerItemType::TypeTodo
        << QOrganizerItemType::TypeTodoOccurrence
        << QOrganizerItemType::TypeJournal;
}

QList<QOrganizerItemDetail::DetailType> mKCalEngine::supportedItemDetails(QOrganizerItemType::ItemType itemType) const
{
    QList<QOrganizerItemDetail::DetailType> base;
    base << QOrganizerItemDetail::TypeClassification
         << QOrganizerItemDetail::TypeComment
         << QOrganizerItemDetail::TypeDescription
         << QOrganizerItemDetail::TypeDisplayLabel
         << QOrganizerItemDetail::TypeItemType
         << QOrganizerItemDetail::TypeLocation
         << QOrganizerItemDetail::TypePriority
         << QOrganizerItemDetail::TypeTimestamp
         << QOrganizerItemDetail::TypeVersion
         << QOrganizerItemDetail::TypeAudibleReminder
         << QOrganizerItemDetail::TypeEmailReminder
         << QOrganizerItemDetail::TypeVisualReminder;

    switch (itemType) {
    case QOrganizerItemType::TypeEvent:
        return base << QOrganizerItemDetail::TypeRecurrence
                    << QOrganizerItemDetail::TypeEventAttendee
                    << QOrganizerItemDetail::TypeEventRsvp
                    << QOrganizerItemDetail::TypeEventTime;
    case QOrganizerItemType::TypeEventOccurrence:
        return base << QOrganizerItemDetail::TypeParent
                    << QOrganizerItemDetail::TypeEventAttendee
                    << QOrganizerItemDetail::TypeEventRsvp
                    << QOrganizerItemDetail::TypeEventTime;
    case QOrganizerItemType::TypeTodo:
        return base << QOrganizerItemDetail::TypeRecurrence
                    << QOrganizerItemDetail::TypeTodoProgress
                    << QOrganizerItemDetail::TypeTodoTime;
    case QOrganizerItemType::TypeTodoOccurrence:
        return base << QOrganizerItemDetail::TypeParent
                    << QOrganizerItemDetail::TypeTodoProgress
                    << QOrganizerItemDetail::TypeTodoTime;
    case QOrganizerItemType::TypeJournal:
        return base << QOrganizerItemDetail::TypeJournalTime;
    default:
        return QList<QOrganizerItemDetail::DetailType>();
    }
}

QList<QOrganizerItemFilter::FilterType> mKCalEngine::supportedFilters() const
{
    return QList<QOrganizerItemFilter::FilterType>()
        << QOrganizerItemFilter::DetailFilter
        << QOrganizerItemFilter::DetailFieldFilter
        << QOrganizerItemFilter::DetailRangeFilter
        << QOrganizerItemFilter::IntersectionFilter
        << QOrganizerItemFilter::UnionFilter
        << QOrganizerItemFilter::IdFilter
        << QOrganizerItemFilter::CollectionFilter;
}

bool mKCalEngine::saveItems(QList<QOrganizerItem> *items,
                            const QList<QOrganizerItemDetail::DetailType> &detailMask,
                            QMap<int, QOrganizerManager::Error> *errorMap,
                            QOrganizerManager::Error *error)
{
    int index = 0;
    *error = QOrganizerManager::NoError;
    if (isOpened()) {
        for (QOrganizerItem &item : *items) {
            if (item.id().isNull()) {
                // Need to sort *items to insert parent first.
                KCalendarCore::Incidence::Ptr incidence;
                switch (item.type()) {
                case QOrganizerItemType::TypeEvent:
                    incidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Event);
                    updateEvent(incidence.staticCast<KCalendarCore::Event>(), item);
                    break;
                case QOrganizerItemType::TypeEventOccurrence: {
                    const QOrganizerItemParent detail(item.detail(QOrganizerItemDetail::TypeParent));
                    const QString uid = QString::fromUtf8(detail.parentId().localId());
                    KCalendarCore::Incidence::Ptr parent = mCalendars->incidence(uid);
                    if (parent) {
                        QDateTime recurId = parent->dtStart();
                        recurId.setDate(detail.originalDate());
                        incidence = mCalendars->createException(parent, recurId);
                        updateEvent(incidence.staticCast<KCalendarCore::Event>(), item);
                    }
                    break;
                }
                case QOrganizerItemType::TypeTodo:
                    incidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Todo);
                    updateTodo(incidence.staticCast<KCalendarCore::Todo>(), item);
                    break;
                case QOrganizerItemType::TypeTodoOccurrence: {
                    const QOrganizerItemParent detail(item.detail(QOrganizerItemDetail::TypeParent));
                    const QString uid = QString::fromUtf8(detail.parentId().localId());
                    KCalendarCore::Incidence::Ptr parent = mCalendars->incidence(uid);
                    if (parent) {
                        QDateTime recurId = parent->dtStart();
                        recurId.setDate(detail.originalDate());
                        incidence = mCalendars->createException(parent, recurId);
                        updateTodo(incidence.staticCast<KCalendarCore::Todo>(), item);
                    }
                    break;
                }
                case QOrganizerItemType::TypeJournal:
                    incidence = KCalendarCore::Incidence::Ptr(new KCalendarCore::Journal);
                    updateJournal(incidence.staticCast<KCalendarCore::Journal>(), item);
                    break;
                default:
                    errorMap->insert(index, QOrganizerManager::InvalidItemTypeError);
                }
                if (incidence) {
                    bool valid;
                    const QString nbuid = QString::fromUtf8(item.collectionId().localId());
                    if (nbuid.isEmpty()) {
                        valid = mCalendars->addIncidence(incidence);
                    } else {
                        valid = mCalendars->addIncidence(incidence, nbuid);
                    }
                    if (valid) {
                        item.setId(QOrganizerItemId(managerUri(),
                                                                 incidence->uid().toUtf8()));
                    }
                }
            } else if (item.id().managerUri() == managerUri()) {
                QOrganizerItemParent detail = item.detail(QOrganizerItemDetail::TypeParent);
                const QString uid = QString::fromUtf8(detail.isEmpty()
                                                      ? item.id().localId()
                                                      : detail.parentId().localId());
                QDateTime recurId;
                KCalendarCore::Incidence::Ptr incidence;
                switch (item.type()) {
                case QOrganizerItemType::TypeEventOccurrence:
                    if (!detail.isEmpty()) {
                        KCalendarCore::Incidence::Ptr parent = mCalendars->event(uid);
                        recurId = parent->dtStart();
                        recurId.setDate(detail.originalDate());
                    }
                    // Fallthrough
                case QOrganizerItemType::TypeEvent:
                    incidence = mCalendars->event(uid, recurId);
                    updateEvent(incidence.staticCast<KCalendarCore::Event>(), item, detailMask);
                    break;
                case QOrganizerItemType::TypeTodoOccurrence:
                    if (!detail.isEmpty()) {
                        KCalendarCore::Incidence::Ptr parent = mCalendars->event(uid);
                        recurId = parent->dtStart();
                        recurId.setDate(detail.originalDate());
                    }
                    // Fallthrough
                case QOrganizerItemType::TypeTodo:
                    incidence = mCalendars->todo(uid, recurId);
                    updateTodo(incidence.staticCast<KCalendarCore::Todo>(), item, detailMask);
                    break;
                case QOrganizerItemType::TypeJournal:
                    incidence = mCalendars->journal(uid);
                    updateJournal(incidence.staticCast<KCalendarCore::Journal>(), item, detailMask);
                    break;
                default:
                    errorMap->insert(index, QOrganizerManager::DoesNotExistError);
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
        }
        if (!mStorage->save()) {
            *error = QOrganizerManager::PermissionsError;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return *error == QOrganizerManager::NoError;
}

QOrganizerCollectionId mKCalEngine::defaultCollectionId() const
{
    mKCal::Notebook::Ptr nb = mStorage->defaultNotebook();
    if (isOpened() && !nb) {
        nb = mKCal::Notebook::Ptr(new mKCal::Notebook(QStringLiteral("Default"),
                                                      QString()));
        if (!mStorage->setDefaultNotebook(nb)) {
            nb.clear();
        }
    }

    return (isOpened() && nb)
        ? QOrganizerCollectionId(managerUri(), nb->uid().toUtf8())
        : QOrganizerCollectionId();
}

QOrganizerCollection mKCalEngine::collection(const QOrganizerCollectionId &collectionId,
                                                          QOrganizerManager::Error *error) const
{
    *error = QOrganizerManager::NoError;
    if (isOpened()) {
        mKCal::Notebook::Ptr nb = mStorage->notebook(collectionId.localId());
        if (collectionId.managerUri() == managerUri() && nb) {
            return toCollection(managerUri(), nb);
        } else {
            *error = QOrganizerManager::DoesNotExistError;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return QOrganizerCollection();
}

QList<QOrganizerCollection> mKCalEngine::collections(QOrganizerManager::Error *error) const
{
    QList<QOrganizerCollection> ret;

    *error = QOrganizerManager::NoError;
    if (isOpened()) {
        for (const mKCal::Notebook::Ptr &nb : mStorage->notebooks()) {
            ret.append(toCollection(managerUri(), nb));
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return ret;
}

bool mKCalEngine::saveCollection(QOrganizerCollection *collection,
                                 QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (isOpened()) {
        if (collection->id().isNull()) {
            mKCal::Notebook::Ptr nb(new mKCal::Notebook);
            updateNotebook(nb, *collection);
            if (!mStorage->addNotebook(nb)) {
                *error = QOrganizerManager::PermissionsError;
            } else {
                collection->setId(QOrganizerCollectionId(managerUri(),
                                                                      nb->uid().toUtf8()));
                emit collectionsAdded(QList<QOrganizerCollectionId>() << collection->id());
                emit collectionsModified(QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> >() << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(collection->id(), QOrganizerManager::Add));
            }
        } else {
            mKCal::Notebook::Ptr nb = mStorage->notebook(collection->id().localId());
            if (nb) {
                updateNotebook(nb, *collection);
                if (!mStorage->updateNotebook(nb)) {
                    *error = QOrganizerManager::PermissionsError;
                } else {
                    emit collectionsChanged(QList<QOrganizerCollectionId>() << collection->id());
                    emit collectionsModified(QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> >() << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(collection->id(), QOrganizerManager::Change));
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return *error == QOrganizerManager::NoError;
}

bool mKCalEngine::removeCollection(const QOrganizerCollectionId &collectionId,
                                   QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (isOpened() && collectionId.managerUri() == managerUri()) {
        mKCal::Notebook::Ptr nb = mStorage->notebook(collectionId.localId());
        if (nb) {
            if (!mStorage->deleteNotebook(nb)) {
                *error = QOrganizerManager::PermissionsError;
            } else {
                emit collectionsRemoved(QList<QOrganizerCollectionId>() << collectionId);
                emit collectionsModified(QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> >() << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(collectionId, QOrganizerManager::Remove));
            }
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return *error == QOrganizerManager::NoError;
}
