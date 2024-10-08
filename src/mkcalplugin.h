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

#ifndef MKCALPLUGIN_H
#define MKCALPLUGIN_H

#include <QSharedPointer>
#include <QThread>
#include <QQueue>

#include <QOrganizerManagerEngineFactoryInterface>
#include <QOrganizerManagerEngine>

#include "mkcalworker.h"

class mKCalFactory : public QtOrganizer::QOrganizerManagerEngineFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QOrganizerManagerEngineFactoryInterface" FILE "mkcal.json")

public:
    QtOrganizer::QOrganizerManagerEngine* engine(const QMap<QString, QString>& parameters,
                                                 QtOrganizer::QOrganizerManager::Error*);
    QString managerName() const;
};

class mKCalEngine : public QtOrganizer::QOrganizerManagerEngine
{
    Q_OBJECT

public:
    mKCalEngine(const QTimeZone &timeZone, const QString &databaseName,
                QObject *parent = nullptr);
    ~mKCalEngine();

    bool isOpened() const;

    QString managerName() const override;
    QMap<QString, QString> managerParameters() const override;

    QList<QtOrganizer::QOrganizerItemFilter::FilterType> supportedFilters() const override;
    QList<QtOrganizer::QOrganizerItemDetail::DetailType> supportedItemDetails(QtOrganizer::QOrganizerItemType::ItemType itemType) const override;
    QList<QtOrganizer::QOrganizerItemType::ItemType> supportedItemTypes() const override;

    QList<QtOrganizer::QOrganizerItem>
        items(const QList<QtOrganizer::QOrganizerItemId> &itemIds,
              const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
              QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
              QtOrganizer::QOrganizerManager::Error *error) override;
    QList<QtOrganizer::QOrganizerItem>
        items(const QtOrganizer::QOrganizerItemFilter &filter,
              const QDateTime &startDateTime,
              const QDateTime &endDateTime, int maxCount,
              const QList<QtOrganizer::QOrganizerItemSortOrder> &sortOrders,
              const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
              QtOrganizer::QOrganizerManager::Error *error) override;
    QList<QtOrganizer::QOrganizerItemId>
        itemIds(const QtOrganizer::QOrganizerItemFilter &filter,
                const QDateTime &startDateTime,
                const QDateTime &endDateTime,
                const QList<QtOrganizer::QOrganizerItemSortOrder> &sortOrders,
                QtOrganizer::QOrganizerManager::Error *error) override;
    QList<QtOrganizer::QOrganizerItem>
        itemOccurrences(const QtOrganizer::QOrganizerItem &parentItem,
                        const QDateTime &startDateTime,
                        const QDateTime &endDateTime, int maxCount,
                        const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
                        QtOrganizer::QOrganizerManager::Error *error) override;
    
    bool saveItems(QList<QtOrganizer::QOrganizerItem> *items,
                   const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask,
                   QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                   QtOrganizer::QOrganizerManager::Error *error) override;
    bool removeItems(const QList<QtOrganizer::QOrganizerItemId> &itemIds,
                     QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                     QtOrganizer::QOrganizerManager::Error *error) override;
    bool removeItems(const QList<QtOrganizer::QOrganizerItem> *items,
                     QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                     QtOrganizer::QOrganizerManager::Error *error) override;

    QtOrganizer::QOrganizerCollectionId defaultCollectionId() const override;
    QtOrganizer::QOrganizerCollection collection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                                                 QtOrganizer::QOrganizerManager::Error *error) const override;
    QList<QtOrganizer::QOrganizerCollection> collections(QtOrganizer::QOrganizerManager::Error *error) const override;
    bool saveCollection(QtOrganizer::QOrganizerCollection *collection,
                        QtOrganizer::QOrganizerManager::Error *error) override;
    bool removeCollection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                          QtOrganizer::QOrganizerManager::Error *error) override;

    void requestDestroyed(QtOrganizer::QOrganizerAbstractRequest *request) override;
    bool startRequest(QtOrganizer::QOrganizerAbstractRequest *request) override;
    bool cancelRequest(QtOrganizer::QOrganizerAbstractRequest *request) override;
    bool waitForRequestFinished(QtOrganizer::QOrganizerAbstractRequest *request, int msecs) override;

private:
    void processRequests();
    bool waitForCurrentRequestFinished(int msecs);

    QMap<QString, QString> mParameters;
    QThread mWorkerThread;
    mKCalWorker *mWorker = nullptr;
    bool mOpened = false;
    QtOrganizer::QOrganizerCollectionId mDefaultCollectionId;
    QtOrganizer::QOrganizerAbstractRequest *mRunningRequest = nullptr;
    QQueue<QtOrganizer::QOrganizerAbstractRequest*> mRequests;
};

#endif
