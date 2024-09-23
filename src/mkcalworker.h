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

#ifndef MKCALWORKER_H
#define MKCALWORKER_H

#include <QObject>
#include <QSharedPointer>

#include <QtOrganizer/QOrganizerManagerEngine>

#include <sqlitestorage.h>
#include <extendedstorageobserver.h>

#include "itemcalendars.h"

class mKCalWorker : public QtOrganizer::QOrganizerManagerEngine, public mKCal::ExtendedStorageObserver
{
    Q_OBJECT
    
public:
    mKCalWorker(QObject *parent = nullptr);
    ~mKCalWorker();

    QString managerName() const override;
    QMap<QString, QString> managerParameters() const override;

public slots:
    bool init(const QTimeZone &timeZone, const QString &databaseName);
    void runRequest(QtOrganizer::QOrganizerAbstractRequest *request);
    QtOrganizer::QOrganizerCollectionId defaultCollectionId() const override;

signals:
    void defaultCollectionIdChanged(const QString &id);
    void itemsUpdated(const QStringList &added,
                      const QStringList &modified,
                      const QStringList &deleted);
    void collectionsUpdated(const QStringList &added,
                            const QStringList &modified,
                            const QStringList &deleted);

private:
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

    QList<QtOrganizer::QOrganizerCollection> collections(QtOrganizer::QOrganizerManager::Error *error) const override;
    bool saveCollection(QtOrganizer::QOrganizerCollection *collection,
                        QtOrganizer::QOrganizerManager::Error *error) override;
    bool saveCollections(QList<QtOrganizer::QOrganizerCollection> *collections,
                         QMap<int, QtOrganizer::QOrganizerManager::Error> *errors,
                         QtOrganizer::QOrganizerManager::Error *error);
    bool removeCollection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                          QtOrganizer::QOrganizerManager::Error *error) override;
    bool removeCollections(const QList<QtOrganizer::QOrganizerCollectionId> &collectionIds,
                           QMap<int, QtOrganizer::QOrganizerManager::Error> *errors,
                           QtOrganizer::QOrganizerManager::Error *error);

    void storageModified(mKCal::ExtendedStorage *storage, const QString &info) override;
    void storageUpdated(mKCal::ExtendedStorage *storage,
                        const KCalendarCore::Incidence::List &added,
                        const KCalendarCore::Incidence::List &modified,
                        const KCalendarCore::Incidence::List &deleted) override;

    QSharedPointer<ItemCalendars> mCalendars;
    mKCal::SqliteStorage::Ptr mStorage;
    bool mOpened = false;
    QString mDefaultNotebookUid;
};

#endif
