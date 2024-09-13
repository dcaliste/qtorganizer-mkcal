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

#include <QOrganizerManagerEngineFactoryInterface>
#include <QOrganizerManagerEngine>

#include <extendedcalendar.h>
#include <sqlitestorage.h>

class mKCalFactory : public QtOrganizer::QOrganizerManagerEngineFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QOrganizerManagerEngineFactoryInterface")

public:
    QtOrganizer::QOrganizerManagerEngine* engine(const QMap<QString, QString>& parameters,
                                                 QtOrganizer::QOrganizerManager::Error*);
    QString managerName() const;
};

class mKCalEngine : public QtOrganizer::QOrganizerManagerEngine,
    public mKCal::ExtendedStorageObserver
{
    Q_OBJECT

public:
    mKCalEngine(const QTimeZone &timeZone, const QString &databaseName,
                QObject *parent = nullptr);
    ~mKCalEngine();

    bool isOpened() const;

    QString managerName() const override;
    QMap<QString, QString> managerParameters() const override;

    QtOrganizer::QOrganizerCollectionId defaultCollectionId() const override;
    QtOrganizer::QOrganizerCollection collection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                                                 QtOrganizer::QOrganizerManager::Error *error) const override;
    QList<QtOrganizer::QOrganizerCollection> collections(QtOrganizer::QOrganizerManager::Error *error) const override;
    bool saveCollection(QtOrganizer::QOrganizerCollection *collection,
                        QtOrganizer::QOrganizerManager::Error *error) override;
    bool removeCollection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                          QtOrganizer::QOrganizerManager::Error *error) override;

private:
    void storageModified(mKCal::ExtendedStorage *storage, const QString &info) override;

    QtOrganizer::QOrganizerCollection toCollection(const mKCal::Notebook::Ptr &nb) const;
    void updateNotebook(mKCal::Notebook::Ptr nb,
                        const QtOrganizer::QOrganizerCollection &collection) const;

    mKCal::ExtendedCalendar::Ptr mCalendars;
    mKCal::SqliteStorage::Ptr mStorage;
    bool mOpened = false;
};

#endif
