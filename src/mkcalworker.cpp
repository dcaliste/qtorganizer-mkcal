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

#include "mkcalworker.h"

#include <QtOrganizer/QOrganizerItemOccurrenceFetchRequest>
#include <QtOrganizer/QOrganizerItemFetchRequest>
#include <QtOrganizer/QOrganizerItemIdFetchRequest>
#include <QtOrganizer/QOrganizerItemFetchByIdRequest>
#include <QtOrganizer/QOrganizerItemRemoveRequest>
#include <QtOrganizer/QOrganizerItemRemoveByIdRequest>
#include <QtOrganizer/QOrganizerItemSaveRequest>
#include <QtOrganizer/QOrganizerCollectionFetchRequest>
#include <QtOrganizer/QOrganizerCollectionSaveRequest>
#include <QtOrganizer/QOrganizerCollectionRemoveRequest>

#include <QtOrganizer/QOrganizerEvent>
#include <QtOrganizer/QOrganizerEventOccurrence>
#include <QtOrganizer/QOrganizerTodo>
#include <QtOrganizer/QOrganizerTodoOccurrence>
#include <QtOrganizer/QOrganizerJournal>

#include "helper.h"

using namespace QtOrganizer;

mKCalWorker::mKCalWorker(QObject *parent)
    : QOrganizerManagerEngine(parent)
{
}

mKCalWorker::~mKCalWorker()
{
    if (mStorage) {
        mStorage->unregisterObserver(this);
        mStorage->close();
    }
}

QString mKCalWorker::managerName() const
{
    return QStringLiteral("mkcal");
}

QMap<QString, QString> mKCalWorker::managerParameters() const
{
    QMap<QString, QString> parameters;

    if (mCalendars && mStorage) {
        parameters.insert(QStringLiteral("timeZone"),
                          QString::fromUtf8(mCalendars->timeZone().id()));
        parameters.insert(QStringLiteral("databaseName"), mStorage->databaseName());
    }

    return parameters;
}

bool mKCalWorker::init(const QTimeZone &timeZone, const QString &databaseName)
{
    mCalendars = QSharedPointer<ItemCalendars>(new ItemCalendars(timeZone));
    if (databaseName.isEmpty()) {
        mStorage = mKCal::SqliteStorage::Ptr(new mKCal::SqliteStorage(mCalendars));
    } else {
        mStorage = mKCal::SqliteStorage::Ptr(new mKCal::SqliteStorage(mCalendars, databaseName));
    }
    mOpened = mStorage->open();
    mKCal::Notebook::Ptr nb = mStorage->defaultNotebook();
    if (mOpened && !nb) {
        nb = mKCal::Notebook::Ptr(new mKCal::Notebook(QStringLiteral("Default"),
                                                      QString()));
        if (!mStorage->setDefaultNotebook(nb)) {
            nb.clear();
        }
    }
    if (nb) {
        mDefaultNotebookUid = nb->uid();
        emit defaultCollectionIdChanged(mDefaultNotebookUid);
    }
    mStorage->registerObserver(this);

    return mOpened;
}

void mKCalWorker::storageModified(mKCal::ExtendedStorage *storage,
                                  const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);

    mKCal::Notebook::Ptr nb = mStorage->defaultNotebook();
    if (nb) {
        if (nb->uid() != mDefaultNotebookUid) {
            mDefaultNotebookUid = nb->uid();
            emit defaultCollectionIdChanged(mDefaultNotebookUid);
        }
    }
    emit dataChanged();
}

void mKCalWorker::storageUpdated(mKCal::ExtendedStorage *storage,
                                 const KCalendarCore::Incidence::List &added,
                                 const KCalendarCore::Incidence::List &modified,
                                 const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);

    QStringList addedIds;
    QStringList modifiedIds;
    QStringList removedIds;

    QList<QPair<QOrganizerItemId, QOrganizerManager::Operation>> ops;

    QList<QOrganizerItemId> ids;

    ids.clear();
    for (const KCalendarCore::Incidence::Ptr &incidence : added) {
        addedIds << incidence->instanceIdentifier();
        const QOrganizerItemId id = itemId(incidence->instanceIdentifier().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Add);
    }
    if (!ids.isEmpty()) {
        emit itemsAdded(ids);
    }

    ids.clear();
    for (const KCalendarCore::Incidence::Ptr &incidence : modified) {
        modifiedIds << incidence->instanceIdentifier();
        const QOrganizerItemId id = itemId(incidence->instanceIdentifier().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Change);
    }
    if (!ids.isEmpty()) {
        emit itemsChanged(ids, QList<QOrganizerItemDetail::DetailType>());
    }

    ids.clear();
    QMap<QString, KCalendarCore::Incidence::List> purgeList;
    for (const KCalendarCore::Incidence::Ptr &incidence : deleted) {
        removedIds << incidence->instanceIdentifier();
        const QOrganizerItemId id = itemId(incidence->instanceIdentifier().toUtf8());
        ids << id;
        ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Remove);
        // if the incidence was stored in a local (non-synced) notebook, purge it.
        mKCal::Notebook::Ptr notebook = mStorage->notebook(mCalendars->notebook(incidence));
        if (notebook
            && notebook->isMaster()
            && !notebook->isShared()
            && notebook->pluginName().isEmpty()) {
            QMap<QString, KCalendarCore::Incidence::List>::Iterator it = purgeList.find(notebook->uid());
            if (it == purgeList.end()) {
                purgeList.insert(notebook->uid(), KCalendarCore::Incidence::List() << incidence);
            } else {
                it->append(incidence);
            }
        }
    }
    if (!ids.isEmpty()) {
        emit itemsRemoved(ids);
    }

    for (QMap<QString, KCalendarCore::Incidence::List>::ConstIterator it = purgeList.constBegin(); it != purgeList.constEnd(); ++it) {
        mStorage->purgeDeletedIncidences(it.value(), it.key());
    }

    if (!ops.isEmpty()) {
        emit itemsModified(ops);
    }
    
    emit itemsUpdated(addedIds, modifiedIds, removedIds);
}

void mKCalWorker::runRequest(QOrganizerAbstractRequest *request)
{
    QOrganizerManager::Error error = QOrganizerManager::NoError;
    switch (request->type()) {
    case QOrganizerAbstractRequest::ItemOccurrenceFetchRequest: {
        QOrganizerItemOccurrenceFetchRequest *r = qobject_cast<QOrganizerItemOccurrenceFetchRequest*>(request);
        QList<QOrganizerItem> items
            = itemOccurrences(r->parentItem(), r->startDate(), r->endDate(),
                              r->maxOccurrences(), r->fetchHint(), &error);
        QOrganizerManagerEngine::updateItemOccurrenceFetchRequest(r, items, error, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemFetchRequest: {
        QOrganizerItemFetchRequest *r = qobject_cast<QOrganizerItemFetchRequest*>(request);
        if (r->filter().type() == QOrganizerItemFilter::InvalidFilter) {
            QOrganizerManagerEngine::updateItemFetchRequest(r, QList<QOrganizerItem>(), error, QOrganizerAbstractRequest::FinishedState);
            return;
        }

        QList<QOrganizerItem> results
            = items(r->filter(), r->startDate(), r->endDate(),
                    r->maxCount(), r->sorting(), r->fetchHint(), &error);
        QOrganizerManagerEngine::updateItemFetchRequest(r, results, error, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemIdFetchRequest: {
        QOrganizerItemIdFetchRequest *r = qobject_cast<QOrganizerItemIdFetchRequest*>(request);
        QList<QOrganizerItemId> ids
            = itemIds(r->filter(), r->startDate(), r->endDate(),
                      r->sorting(), &error);
        QOrganizerManagerEngine::updateItemIdFetchRequest(r, ids, error, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemFetchByIdRequest: {
        QOrganizerItemFetchByIdRequest *r = qobject_cast<QOrganizerItemFetchByIdRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        QList<QOrganizerItem> results
            = items(r->ids(), r->fetchHint(), &errors, &error);
        QOrganizerManagerEngine::updateItemFetchByIdRequest(r, results, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemRemoveRequest: {
        QOrganizerItemRemoveRequest *r = qobject_cast<QOrganizerItemRemoveRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        QList<QOrganizerItem> items = r->items();
        removeItems(&items, &errors, &error);
        QOrganizerManagerEngine::updateItemRemoveRequest(r, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemRemoveByIdRequest: {
        QOrganizerItemRemoveByIdRequest *r = qobject_cast<QOrganizerItemRemoveByIdRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        removeItems(r->itemIds(), &errors, &error);
        QOrganizerManagerEngine::updateItemRemoveByIdRequest(r, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::ItemSaveRequest: {
        QOrganizerItemSaveRequest *r = qobject_cast<QOrganizerItemSaveRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        QList<QOrganizerItem> items = r->items();
        saveItems(&items, r->detailMask(), &errors, &error);
        QOrganizerManagerEngine::updateItemSaveRequest(r, items, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::CollectionFetchRequest: {
        QOrganizerCollectionFetchRequest *r = qobject_cast<QOrganizerCollectionFetchRequest*>(request);
        const QList<QOrganizerCollection> results = collections(&error);
        QOrganizerManagerEngine::updateCollectionFetchRequest(r, results, error, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::CollectionSaveRequest: {
        QOrganizerCollectionSaveRequest *r = qobject_cast<QOrganizerCollectionSaveRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        QList<QOrganizerCollection> collections = r->collections();
        saveCollections(&collections, &errors, &error);
        QOrganizerManagerEngine::updateCollectionSaveRequest(r, collections, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    case QOrganizerAbstractRequest::CollectionRemoveRequest: {
        QOrganizerCollectionRemoveRequest *r = qobject_cast<QOrganizerCollectionRemoveRequest*>(request);
        QMap<int, QOrganizerManager::Error> errors;
        removeCollections(r->collectionIds(), &errors, &error);
        QOrganizerManagerEngine::updateCollectionRemoveRequest(r, error, errors, QOrganizerAbstractRequest::FinishedState);
        return;
    }
    default:
        break;
    }
}

static QDateTime itemStartDateTime(const QOrganizerItem &item)
{
    switch (item.type()) {
    case QOrganizerItemType::TypeEvent:
        return QOrganizerEvent(item).startDateTime();
    case QOrganizerItemType::TypeEventOccurrence:
        return QOrganizerEventOccurrence(item).startDateTime();
    case QOrganizerItemType::TypeTodo:
        return QOrganizerTodo(item).startDateTime();
    case QOrganizerItemType::TypeTodoOccurrence:
        return QOrganizerTodoOccurrence(item).startDateTime();
    case QOrganizerItemType::TypeJournal:
        return QOrganizerJournal(item).dateTime();
    }
    return QDateTime();
}

QList<QOrganizerItem> mKCalWorker::items(const QList<QOrganizerItemId> &itemIds,
                                         const QOrganizerItemFetchHint &fetchHint,
                                         QMap<int, QOrganizerManager::Error> *errorMap,
                                         QOrganizerManager::Error *error)
{
    QList<QOrganizerItem> items;
    if (mOpened) {
        int index = 0;
        for (const QOrganizerItemId &id : itemIds) {
            if (id.managerUri() == managerUri()
                && mStorage->loadIncidenceInstance(id.localId())) {
                const QOrganizerItem item = mCalendars->item(id, fetchHint.detailTypesHint());
                if (!item.isEmpty()) {
                    items.append(item);
                } else {
                    *error = QOrganizerManager::PermissionsError;
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
            index += 1;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return items;
}

QList<QOrganizerItem> mKCalWorker::items(const QOrganizerItemFilter &filter,
                                         const QDateTime &startDateTime,
                                         const QDateTime &endDateTime, int maxCount,
                                         const QList<QOrganizerItemSortOrder> &sortOrders,
                                         const QOrganizerItemFetchHint &fetchHint,
                                         QOrganizerManager::Error *error)
{
    QList<QOrganizerItem> items;
    if (mOpened && mStorage->load(startDateTime.date(), endDateTime.date().addDays(1))) {
        items = mCalendars->items(managerUri(), filter,
                                  startDateTime, endDateTime, maxCount,
                                  fetchHint.detailTypesHint());
        std::sort(items.begin(), items.end(),
                  [sortOrders] (const QOrganizerItem &item1, const QOrganizerItem &item2) {
                      int cmp = compareItem(item1, item2, sortOrders);
                      if (cmp == 0) {
                          return itemStartDateTime(item1) < itemStartDateTime(item2);
                      } else {
                          return (cmp < 0);
                      }
                  });
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return items;
}

QList<QOrganizerItemId> mKCalWorker::itemIds(const QOrganizerItemFilter &filter,
                                             const QDateTime &startDateTime,
                                             const QDateTime &endDateTime,
                                             const QList<QOrganizerItemSortOrder> &sortOrders,
                                             QOrganizerManager::Error *error)
{
    QList<QOrganizerItemId> ids;
    if (mOpened && mStorage->load(startDateTime.date(), endDateTime.date().addDays(1))) {
        QList<QOrganizerItem> items = mCalendars->items(managerUri(), filter,
                                                        startDateTime, endDateTime,
                                                        0, QList<QOrganizerItemDetail::DetailType>());
        std::sort(items.begin(), items.end(),
                  [sortOrders] (const QOrganizerItem &item1, const QOrganizerItem &item2) {
                      int cmp = compareItem(item1, item2, sortOrders);
                      if (cmp == 0) {
                          return itemStartDateTime(item1) < itemStartDateTime(item2);
                      } else {
                          return (cmp < 0);
                      }
                  });
        QSet<QString> localIds;
        for (const QOrganizerItem &item : items) {
            if (!item.id().isNull()) {
                ids.append(item.id());
                localIds.insert(item.id().localId());
            } else if (item.type() == QOrganizerItemType::TypeEventOccurrence) {
                const QOrganizerEventOccurrence occurrence(item);
                if (!localIds.contains(occurrence.parentId().localId())) {
                    ids.append(occurrence.parentId());
                    localIds.insert(occurrence.parentId().localId());
                }
            } else if (item.type() == QOrganizerItemType::TypeTodoOccurrence) {
                const QOrganizerTodoOccurrence occurrence(item);
                if (!localIds.contains(occurrence.parentId().localId())) {
                    ids.append(occurrence.parentId());
                    localIds.insert(occurrence.parentId().localId());
                }
            }
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return ids;
}

QList<QOrganizerItem> mKCalWorker::itemOccurrences(const QOrganizerItem &parentItem,
                                                   const QDateTime &startDateTime,
                                                   const QDateTime &endDateTime, int maxCount,
                                                   const QOrganizerItemFetchHint &fetchHint,
                                                   QOrganizerManager::Error *error)
{
    QList<QOrganizerItem> items;
    if (mOpened
        && parentItem.id().managerUri() == managerUri()
        && mStorage->load(parentItem.id().localId())) {
        items = mCalendars->occurrences(managerUri(), parentItem,
                                        startDateTime, endDateTime,
                                        maxCount, fetchHint.detailTypesHint());
        std::sort(items.begin(), items.end(),
                  [] (const QOrganizerItem &item1, const QOrganizerItem &item2) {
                      return itemStartDateTime(item1) < itemStartDateTime(item2);
                  });
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return items;
}

bool mKCalWorker::saveItems(QList<QOrganizerItem> *items,
                            const QList<QOrganizerItemDetail::DetailType> &detailMask,
                            QMap<int, QOrganizerManager::Error> *errorMap,
                            QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (mOpened) {
        int index = 0;
        for (QOrganizerItem &item : *items) {
            if (item.id().isNull()) {
                if (item.collectionId().isNull()) {
                    item.setCollectionId(defaultCollectionId());
                }
                const QByteArray localId = mCalendars->addItem(item);
                if (localId.isEmpty()) {
                    errorMap->insert(index, QOrganizerManager::InvalidItemTypeError);
                } else {
                    item.setId(itemId(localId));
                }
            } else if (item.id().managerUri() == managerUri()) {
                if (!mCalendars->updateItem(item, detailMask)) {
                    errorMap->insert(index, QOrganizerManager::DoesNotExistError);
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
            index += 1;
        }
        if (!mStorage->save()) {
            *error = QOrganizerManager::PermissionsError;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

bool mKCalWorker::removeItems(const QList<QOrganizerItemId> &itemIds,
                              QMap<int, QOrganizerManager::Error> *errorMap,
                              QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (mOpened) {
        int index = 0;
        for (const QOrganizerItemId &id : itemIds) {
            if (id.managerUri() == managerUri() && !id.localId().isEmpty()) {
                KCalendarCore::Incidence::Ptr doomed = mCalendars->instance(id.localId());
                if (doomed && !mCalendars->deleteIncidence(doomed)) {
                    errorMap->insert(index, QOrganizerManager::PermissionsError);
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
            index += 1;
        }
        if (!mStorage->save()) {
            *error = QOrganizerManager::PermissionsError;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

bool mKCalWorker::removeItems(const QList<QOrganizerItem> *items,
                              QMap<int, QOrganizerManager::Error> *errorMap,
                              QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (mOpened) {
        int index = 0;
        for (const QOrganizerItem &item : *items) {
            if (item.id().isNull()
                || (item.id().managerUri() == managerUri()
                    && !item.id().localId().isEmpty())) {
                if (!mCalendars->removeItem(item)) {
                    errorMap->insert(index, QOrganizerManager::PermissionsError);
                }
            } else {
                *error = QOrganizerManager::DoesNotExistError;
            }
            index += 1;
        }
        if (!mStorage->save()) {
            *error = QOrganizerManager::PermissionsError;
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

QOrganizerCollectionId mKCalWorker::defaultCollectionId() const
{
    return (mStorage && mStorage->defaultNotebook())
        ? collectionId(mStorage->defaultNotebook()->uid().toUtf8())
        : QOrganizerCollectionId();
}

QList<QOrganizerCollection> mKCalWorker::collections(QOrganizerManager::Error *error) const
{
    QList<QOrganizerCollection> ret;

    *error = QOrganizerManager::NoError;
    if (mOpened) {
        for (const mKCal::Notebook::Ptr &nb : mStorage->notebooks()) {
            ret.append(toCollection(managerUri(), nb));
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return ret;
}

bool mKCalWorker::saveCollections(QList<QOrganizerCollection> *collections,
                                  QMap<int, QOrganizerManager::Error> *errors,
                                  QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (mOpened) {
        QStringList addedIds;
        QStringList modifiedIds;
        QList<QOrganizerCollectionId> added;
        QList<QOrganizerCollectionId> changed;
        int index = 0;
        for (QList<QOrganizerCollection>::Iterator it = collections->begin();
             it != collections->end(); ++it, ++index) {
            if (it->id().isNull()) {
                mKCal::Notebook::Ptr nb(new mKCal::Notebook);
                updateNotebook(nb, *it);
                if (!mStorage->addNotebook(nb)) {
                    errors->insert(index, QOrganizerManager::PermissionsError);
                } else {
                    it->setId(collectionId(nb->uid().toUtf8()));
                    addedIds.prepend(nb->uid());
                    added.prepend(it->id());
                }
            } else {
                mKCal::Notebook::Ptr nb = mStorage->notebook(it->id().localId());
                if (nb) {
                    updateNotebook(nb, *it);
                    if (!mStorage->updateNotebook(nb)) {
                        errors->insert(index, QOrganizerManager::PermissionsError);
                    } else {
                        modifiedIds.prepend(nb->uid());
                        changed.prepend(it->id());
                    }
                } else {
                    errors->insert(index, QOrganizerManager::DoesNotExistError);
                }
            }
        }
        if (!addedIds.isEmpty() || !modifiedIds.isEmpty()) {
            emit collectionsUpdated(addedIds, modifiedIds, QStringList());
        }
        if (!added.isEmpty()) {
            emit collectionsAdded(added);
        }
        if (!changed.isEmpty()) {
            emit collectionsChanged(changed);            
        }
        QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation>> mods;
        for (const QOrganizerCollectionId &id : changed) {
            mods.prepend(QPair<QOrganizerCollectionId,
                         QOrganizerManager::Operation>(id, QOrganizerManager::Change));
        }
        for (const QOrganizerCollectionId &id : added) {
            mods.prepend(QPair<QOrganizerCollectionId,
                         QOrganizerManager::Operation>(id, QOrganizerManager::Add));
        }
        if (!mods.isEmpty()) {
            emit collectionsModified(mods);
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return (*error == QOrganizerManager::NoError) && errors->isEmpty();
}

bool mKCalWorker::saveCollection(QOrganizerCollection *collection,
                                 QOrganizerManager::Error *error)
{
    QMap<int, QOrganizerManager::Error> errors;
    QList<QOrganizerCollection> collections;
    collections << *collection;
    bool res = saveCollections(&collections, &errors, error);
    *error = errors.isEmpty() ? QOrganizerManager::NoError : errors.first();
    *collection = collections.first();
    return res;
}

bool mKCalWorker::removeCollections(const QList<QOrganizerCollectionId> &collectionIds,
                                    QMap<int, QOrganizerManager::Error> *errors,
                                    QOrganizerManager::Error *error)
{
    *error = QOrganizerManager::NoError;
    if (mOpened) {
        QStringList ids;
        QList<QOrganizerCollectionId> removedIds;
        QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation>> mods;
        int index = 0;
        for (const QOrganizerCollectionId &collectionId : collectionIds) {
            mKCal::Notebook::Ptr nb = mStorage->notebook(collectionId.localId());
            if (nb) {
                if (!mStorage->deleteNotebook(nb)) {
                    errors->insert(index, QOrganizerManager::PermissionsError);
                } else {
                    ids.prepend(nb->uid());
                    removedIds.prepend(collectionId);
                    mods.prepend(QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(collectionId, QOrganizerManager::Remove));
                }
            } else {
                errors->insert(index, QOrganizerManager::DoesNotExistError);
            }
            index += 1;
        }
        if (!ids.isEmpty()) {
            emit collectionsUpdated(QStringList(), QStringList(), ids);
        }
        if (!removedIds.isEmpty()) {
            emit collectionsRemoved(removedIds);
        }
        if (!mods.isEmpty()) {
            emit collectionsModified(mods);
        }
    } else {
        *error = QOrganizerManager::PermissionsError;
    }

    return (*error == QOrganizerManager::NoError) && errors->isEmpty();
}

bool mKCalWorker::removeCollection(const QOrganizerCollectionId &collectionId,
                                   QOrganizerManager::Error *error)
{
    QMap<int, QOrganizerManager::Error> errors;
    QList<QOrganizerCollectionId> collectionIds;
    collectionIds << collectionId;
    bool res = removeCollections(collectionIds, &errors, error);
    *error = errors.isEmpty() ? QOrganizerManager::NoError : errors.first();
    return res;
}
