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

#include "helper.h"

QtOrganizer::QOrganizerManagerEngine* mKCalFactory::engine(const QMap<QString, QString>& parameters, QtOrganizer::QOrganizerManager::Error* error)
{
    Q_UNUSED(error);
    QString tzname = parameters.value(QStringLiteral("timeZone"));
    QString dbname = parameters.value(QStringLiteral("databaseName"));

    mKCalEngine *engine = new mKCalEngine(QTimeZone(tzname.toUtf8()), dbname);
    if (!engine->isOpened())
        *error = QtOrganizer::QOrganizerManager::PermissionsError;
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

QtOrganizer::QOrganizerCollectionId mKCalEngine::defaultCollectionId() const
{
    mKCal::Notebook::Ptr nb = mStorage->defaultNotebook();

    return isOpened()
        ? QtOrganizer::QOrganizerCollectionId(managerUri(), nb->uid().toUtf8())
        : QtOrganizer::QOrganizerCollectionId();
}

QtOrganizer::QOrganizerCollection mKCalEngine::collection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                                                          QtOrganizer::QOrganizerManager::Error *error) const
{
    *error = QtOrganizer::QOrganizerManager::NoError;
    if (isOpened()) {
        mKCal::Notebook::Ptr nb = mStorage->notebook(collectionId.localId());
        if (collectionId.managerUri() == managerUri() && nb) {
            return toCollection(managerUri(), nb);
        } else {
            *error = QtOrganizer::QOrganizerManager::DoesNotExistError;
        }
    } else {
        *error = QtOrganizer::QOrganizerManager::PermissionsError;
    }

    return QtOrganizer::QOrganizerCollection();
}

QList<QtOrganizer::QOrganizerCollection> mKCalEngine::collections(QtOrganizer::QOrganizerManager::Error *error) const
{
    QList<QtOrganizer::QOrganizerCollection> ret;

    *error = QtOrganizer::QOrganizerManager::NoError;
    if (isOpened()) {
        for (const mKCal::Notebook::Ptr &nb : mStorage->notebooks()) {
            ret.append(toCollection(managerUri(), nb));
        }
    } else {
        *error = QtOrganizer::QOrganizerManager::PermissionsError;
    }

    return ret;
}

bool mKCalEngine::saveCollection(QtOrganizer::QOrganizerCollection *collection,
                                 QtOrganizer::QOrganizerManager::Error *error)
{
    *error = QtOrganizer::QOrganizerManager::NoError;
    if (isOpened()) {
        if (collection->id().isNull()) {
            mKCal::Notebook::Ptr nb(new mKCal::Notebook);
            updateNotebook(nb, *collection);
            if (!mStorage->addNotebook(nb)) {
                *error = QtOrganizer::QOrganizerManager::PermissionsError;
            } else {
                collection->setId(QtOrganizer::QOrganizerCollectionId(managerUri(),
                                                                      nb->uid().toUtf8()));
                emit collectionsAdded(QList<QtOrganizer::QOrganizerCollectionId>() << collection->id());
                emit collectionsModified(QList<QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation> >() << QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation>(collection->id(), QtOrganizer::QOrganizerManager::Add));
            }
        } else {
            mKCal::Notebook::Ptr nb = mStorage->notebook(collection->id().localId());
            if (nb) {
                updateNotebook(nb, *collection);
                if (!mStorage->updateNotebook(nb)) {
                    *error = QtOrganizer::QOrganizerManager::PermissionsError;
                } else {
                    emit collectionsChanged(QList<QtOrganizer::QOrganizerCollectionId>() << collection->id());
                    emit collectionsModified(QList<QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation> >() << QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation>(collection->id(), QtOrganizer::QOrganizerManager::Change));
                }
            } else {
                *error = QtOrganizer::QOrganizerManager::DoesNotExistError;
            }
        }
    } else {
        *error = QtOrganizer::QOrganizerManager::PermissionsError;
    }

    return *error == QtOrganizer::QOrganizerManager::NoError;
}

bool mKCalEngine::removeCollection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                                   QtOrganizer::QOrganizerManager::Error *error)
{
    *error = QtOrganizer::QOrganizerManager::NoError;
    if (isOpened() && collectionId.managerUri() == managerUri()) {
        mKCal::Notebook::Ptr nb = mStorage->notebook(collectionId.localId());
        if (nb) {
            if (!mStorage->deleteNotebook(nb)) {
                *error = QtOrganizer::QOrganizerManager::PermissionsError;
            } else {
                emit collectionsRemoved(QList<QtOrganizer::QOrganizerCollectionId>() << collectionId);
                emit collectionsModified(QList<QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation> >() << QPair<QtOrganizer::QOrganizerCollectionId, QtOrganizer::QOrganizerManager::Operation>(collectionId, QtOrganizer::QOrganizerManager::Remove));
            }
        }
    } else {
        *error = QtOrganizer::QOrganizerManager::PermissionsError;
    }

    return *error == QtOrganizer::QOrganizerManager::NoError;
}
