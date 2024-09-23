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

#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>

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

Q_DECLARE_METATYPE(QTimeZone)
mKCalEngine::mKCalEngine(const QTimeZone &timeZone, const QString &databaseName,
                         QObject *parent)
    : QOrganizerManagerEngine(parent)
{
    qRegisterMetaType<QOrganizerAbstractRequest*>();

    mWorker = new mKCalWorker;
    mWorker->moveToThread(&mWorkerThread);
    connect(&mWorkerThread, &QThread::finished,
            mWorker, &QObject::deleteLater);

    connect(mWorker, &mKCalWorker::dataChanged,
            this, &mKCalEngine::dataChanged);
    connect(mWorker, &mKCalWorker::itemsUpdated,
            this, [this] (const QStringList &added,
                          const QStringList &modified,
                          const QStringList &deleted) {
                      QList<QOrganizerItemId> ids;
                      QList<QPair<QOrganizerItemId, QOrganizerManager::Operation>> ops;

                      ids.clear();
                      for (const QString &uid : added) {
                          const QOrganizerItemId id = itemId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Add);
                      }
                      if (!ids.isEmpty()) {
                          emit itemsAdded(ids);
                      }

                      ids.clear();
                      for (const QString &uid : modified) {
                          const QOrganizerItemId id = itemId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Change);
                      }
                      if (!ids.isEmpty()) {
                          emit itemsChanged(ids, QList<QOrganizerItemDetail::DetailType>());
                      }

                      ids.clear();
                      for (const QString &uid : deleted) {
                          const QOrganizerItemId id = itemId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerItemId, QOrganizerManager::Operation>(id, QOrganizerManager::Remove);
                      }
                      if (!ids.isEmpty()) {
                          emit itemsRemoved(ids);
                      }

                      if (!ops.isEmpty()) {
                          emit itemsModified(ops);
                      }
                  });
    connect(mWorker, &mKCalWorker::collectionsUpdated,
            this, [this] (const QStringList &added,
                          const QStringList &modified,
                          const QStringList &deleted) {
                      QList<QOrganizerCollectionId> ids;
                      QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation>> ops;

                      ids.clear();
                      for (const QString &uid : added) {
                          const QOrganizerCollectionId id = collectionId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(id, QOrganizerManager::Add);
                      }
                      if (!ids.isEmpty()) {
                          emit collectionsAdded(ids);
                      }

                      ids.clear();
                      for (const QString &uid : modified) {
                          const QOrganizerCollectionId id = collectionId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(id, QOrganizerManager::Change);
                      }
                      if (!ids.isEmpty()) {
                          emit collectionsChanged(ids);
                      }

                      ids.clear();
                      for (const QString &uid : deleted) {
                          const QOrganizerCollectionId id = collectionId(uid.toUtf8());
                          ids << id;
                          ops << QPair<QOrganizerCollectionId, QOrganizerManager::Operation>(id, QOrganizerManager::Remove);
                      }
                      if (!ids.isEmpty()) {
                          emit collectionsRemoved(ids);
                      }

                      if (!ops.isEmpty()) {
                          emit collectionsModified(ops);
                      }
                  });
    connect(mWorker, &mKCalWorker::defaultCollectionIdChanged,
            this, [this] (const QString &id) {
                      if (id.toUtf8() != mDefaultCollectionId.localId()) {
                          mDefaultCollectionId = collectionId(id.toUtf8());
                      }
                  });

    mWorkerThread.setObjectName("mKCal worker");
    mWorkerThread.start();

    qRegisterMetaType<QTimeZone>();
    QMetaObject::invokeMethod(mWorker, "init", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, mOpened),
                              Q_ARG(QTimeZone, timeZone),
                              Q_ARG(QString, databaseName));
    mParameters.insert(QStringLiteral("timeZone"),
                       QString::fromUtf8(timeZone.id()));
    mParameters.insert(QStringLiteral("databaseName"), databaseName);
    QMetaObject::invokeMethod(mWorker, "defaultCollectionId",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QtOrganizer::QOrganizerCollectionId, mDefaultCollectionId));
}

mKCalEngine::~mKCalEngine()
{
    mWorkerThread.quit();
    mWorkerThread.wait();
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
    return mParameters;
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

QList<QOrganizerItem> mKCalEngine::items(const QList<QOrganizerItemId> &itemIds,
                                         const QOrganizerItemFetchHint &fetchHint,
                                         QMap<int, QOrganizerManager::Error> *errorMap,
                                         QOrganizerManager::Error *error)
{
    QOrganizerItemFetchByIdRequest request(this);
    request.setIds(itemIds);
    request.setFetchHint(fetchHint);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    *errorMap = request.errorMap();
    return request.items();
}

QList<QOrganizerItem> mKCalEngine::items(const QOrganizerItemFilter &filter,
                                         const QDateTime &startDateTime,
                                         const QDateTime &endDateTime, int maxCount,
                                         const QList<QOrganizerItemSortOrder> &sortOrders,
                                         const QOrganizerItemFetchHint &fetchHint,
                                         QOrganizerManager::Error *error)
{
    QOrganizerItemFetchRequest request(this);
    request.setFilter(filter);
    request.setStartDate(startDateTime);
    request.setEndDate(endDateTime);
    request.setMaxCount(maxCount);
    request.setSorting(sortOrders);
    request.setFetchHint(fetchHint);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    return request.items();
}

QList<QOrganizerItemId> mKCalEngine::itemIds(const QOrganizerItemFilter &filter,
                                             const QDateTime &startDateTime,
                                             const QDateTime &endDateTime,
                                             const QList<QOrganizerItemSortOrder> &sortOrders,
                                             QOrganizerManager::Error *error)
{
    QOrganizerItemIdFetchRequest request(this);
    request.setFilter(filter);
    request.setStartDate(startDateTime);
    request.setEndDate(endDateTime);
    request.setSorting(sortOrders);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    return request.itemIds();
}

QList<QOrganizerItem> mKCalEngine::itemOccurrences(const QOrganizerItem &parentItem,
                                                   const QDateTime &startDateTime,
                                                   const QDateTime &endDateTime, int maxCount,
                                                   const QOrganizerItemFetchHint &fetchHint,
                                                   QOrganizerManager::Error *error)
{
    QOrganizerItemOccurrenceFetchRequest request(this);
    request.setParentItem(parentItem);
    request.setStartDate(startDateTime);
    request.setEndDate(endDateTime);
    request.setMaxOccurrences(maxCount);
    request.setFetchHint(fetchHint);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    return request.itemOccurrences();
}

bool mKCalEngine::saveItems(QList<QOrganizerItem> *items,
                            const QList<QOrganizerItemDetail::DetailType> &detailMask,
                            QMap<int, QOrganizerManager::Error> *errorMap,
                            QOrganizerManager::Error *error)
{
    QOrganizerItemSaveRequest request(this);
    request.setItems(*items);
    request.setDetailMask(detailMask);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    *errorMap = request.errorMap();
    *items = request.items();
    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

bool mKCalEngine::removeItems(const QList<QOrganizerItemId> &itemIds,
                              QMap<int, QOrganizerManager::Error> *errorMap,
                              QOrganizerManager::Error *error)
{
    QOrganizerItemRemoveByIdRequest request(this);
    request.setItemIds(itemIds);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    *errorMap = request.errorMap();
    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

bool mKCalEngine::removeItems(const QList<QOrganizerItem> *items,
                              QMap<int, QOrganizerManager::Error> *errorMap,
                              QOrganizerManager::Error *error)
{
    QOrganizerItemRemoveRequest request(this);
    request.setItems(*items);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    *errorMap = request.errorMap();
    return (*error == QOrganizerManager::NoError)
        && errorMap->isEmpty();
}

QOrganizerCollectionId mKCalEngine::defaultCollectionId() const
{
    return mDefaultCollectionId;
}

QOrganizerCollection mKCalEngine::collection(const QOrganizerCollectionId &collectionId,
                                             QOrganizerManager::Error *error) const
{
    QOrganizerCollectionFetchRequest request;
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    for (const QOrganizerCollection &collection : request.collections()) {
        if (collection.id() == collectionId) {
            return collection;
        }
    }

    return QOrganizerCollection();
}

QList<QOrganizerCollection> mKCalEngine::collections(QOrganizerManager::Error *error) const
{
    QOrganizerCollectionFetchRequest request;
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    return request.collections();
}

bool mKCalEngine::saveCollection(QOrganizerCollection *collection,
                                 QOrganizerManager::Error *error)
{
    QOrganizerCollectionSaveRequest request;
    request.setCollection(*collection);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    *collection = request.collections().first();
    return (*error == QOrganizerManager::NoError);
}

bool mKCalEngine::removeCollection(const QOrganizerCollectionId &collectionId,
                                   QOrganizerManager::Error *error)
{
    QOrganizerCollectionRemoveRequest request;
    request.setCollectionId(collectionId);
    QMetaObject::invokeMethod(mWorker, "runRequest", Qt::BlockingQueuedConnection,
                              Q_ARG(QtOrganizer::QOrganizerAbstractRequest*, &request));
    *error = request.error();
    return (*error == QOrganizerManager::NoError);
}

bool mKCalEngine::waitForCurrentRequestFinished(int msecs)
{
    if (!mRunningRequest) {
        return false;
    }

    QTimer timer;
    QEventLoop loop;
    connect(mRunningRequest, &QOrganizerAbstractRequest::resultsAvailable,
            &loop, &QEventLoop::quit);
    if (msecs > 0) {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(msecs);
    }
    loop.exec();

    return msecs <= 0 || !timer.isActive();
}

void mKCalEngine::processRequests()
{
    if (mRunningRequest) {
        disconnect(mRunningRequest, &QOrganizerAbstractRequest::resultsAvailable,
                   this, &mKCalEngine::processRequests);
        mRunningRequest = nullptr;
    }
    if (!mRequests.isEmpty()) {
        QOrganizerAbstractRequest *request = mRequests.dequeue();
        mRunningRequest = request;
        connect(mRunningRequest, &QOrganizerAbstractRequest::resultsAvailable,
                this, &mKCalEngine::processRequests);
        QMetaObject::invokeMethod(mWorker, "runRequest", Qt::QueuedConnection,
                                  Q_ARG(QtOrganizer::QOrganizerAbstractRequest*,
                                        request));
    }
}

void mKCalEngine::requestDestroyed(QOrganizerAbstractRequest *request)
{
    if (mRunningRequest == request) {
        request->waitForFinished();
    } else if (mRequests.contains(request)) {
        cancelRequest(request);
    }
}

bool mKCalEngine::startRequest(QOrganizerAbstractRequest *request)
{
    if (mRequests.contains(request)) {
        return false;
    }
    updateRequestState(request, QOrganizerAbstractRequest::ActiveState);
    mRequests.enqueue(request);
    if (!mRunningRequest) {
        processRequests();
    }
    return true;
}

bool mKCalEngine::cancelRequest(QOrganizerAbstractRequest *request)
{
    if (mRequests.removeAll(request) > 0) {
        updateRequestState(request, QOrganizerAbstractRequest::CanceledState);
    }
    return request->isCanceled();
}

bool mKCalEngine::waitForRequestFinished(QOrganizerAbstractRequest *request, int msecs)
{
    int remaining = msecs;
    if (mRunningRequest && mRunningRequest != request) {
        QElapsedTimer timer;
        if (msecs > 0) {
            timer.start();
        }
        disconnect(mRunningRequest, &QOrganizerAbstractRequest::resultsAvailable,
                   this, &mKCalEngine::processRequests);
        bool finished = waitForCurrentRequestFinished(msecs);
        remaining = timer.isValid() ? qMax(1, msecs - int(timer.elapsed())) : msecs;
        while (finished
               && !mRequests.isEmpty()
               && (mRunningRequest = mRequests.dequeue()) != request) {
            QMetaObject::invokeMethod(mWorker, "runRequest",
                                      Qt::QueuedConnection,
                                      Q_ARG(QtOrganizer::QOrganizerAbstractRequest*,
                                            mRunningRequest));
            finished = waitForCurrentRequestFinished(remaining);
            remaining = timer.isValid() ? qMax(1, msecs - int(timer.elapsed())) : msecs;
        }
        connect(mRunningRequest, &QOrganizerAbstractRequest::resultsAvailable,
                this, &mKCalEngine::processRequests);
        if (!finished) {
            return false;
        }
    }
    return waitForCurrentRequestFinished(remaining);
}
