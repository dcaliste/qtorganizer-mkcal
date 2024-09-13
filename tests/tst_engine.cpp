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

#include <QObject>
#include <QTest>
#include <QDebug>
#include <QString>
#include <QSignalSpy>
#include <QFileInfo>

#include <QOrganizerManager>
#include <extendedcalendar.h>
#include <sqlitestorage.h>

using namespace QtOrganizer;

class tst_engine: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testCollections();
    void testCollectionIO();
    void testCollectionExternal();
private:
    QOrganizerManager *mManager = nullptr;
};

void tst_engine::initTestCase()
{
    const QString db = QStringLiteral("db");
    QVERIFY(!QFileInfo(db).exists());

    QMap<QString, QString> parameters;
    parameters.insert(QStringLiteral("databaseName"), db);
    mManager = new QOrganizerManager(QString::fromLatin1("mkcal"), parameters);
    QCOMPARE(mManager->error(), QOrganizerManager::NoError);
    QCOMPARE(mManager->managerParameters().value(QStringLiteral("databaseName")), db);
    QVERIFY(!mManager->defaultCollectionId().isNull());
}

void tst_engine::cleanupTestCase()
{
    delete mManager;
}

void tst_engine::testCollections()
{
    QCOMPARE(mManager->collections().count(), 1);
    QCOMPARE(mManager->collections().first().id(), mManager->defaultCollectionId());

    QVERIFY(mManager->collection(QOrganizerCollectionId()).id().isNull());
    QVERIFY(mManager->collection(QOrganizerCollectionId(mManager->managerUri(), "not a valid collection id")).id().isNull());
    
    QOrganizerCollection defCollection =
        mManager->collection(mManager->defaultCollectionId());
    QCOMPARE(defCollection.id(), mManager->defaultCollectionId());
    QCOMPARE(defCollection.metaData(QOrganizerCollection::KeyName).toString(),
             QStringLiteral("Default"));
    QCOMPARE(defCollection.metaData(QOrganizerCollection::KeyDescription).toString(),
             QString());
    QCOMPARE(defCollection.metaData(QOrganizerCollection::KeyColor).toString(),
             QStringLiteral("#0000FF"));
}

void tst_engine::testCollectionIO()
{
    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName,
                           QStringLiteral("Test collection"));
    collection.setMetaData(QOrganizerCollection::KeyDescription,
                           QStringLiteral("Description for test collection"));
    collection.setMetaData(QOrganizerCollection::KeyColor,
                           QStringLiteral("#AAFF55"));
    collection.setMetaData(QOrganizerCollection::KeySecondaryColor,
                           QStringLiteral("violet"));
    collection.setMetaData(QOrganizerCollection::KeyImage,
                           QStringLiteral("theme://notebook.png"));
    collection.setExtendedMetaData(QStringLiteral("visible"), true);

    // qRegisterMetaType<QOrganizerCollectionId>();
    // QSignalSpy added(mManager, &QOrganizerManager::collectionsAdded);
    // QSignalSpy modified(mManager, &QOrganizerManager::collectionsChanged);
    // QSignalSpy deleted(mManager, &QOrganizerManager::collectionsRemoved);

    QVERIFY(collection.id().isNull());
    QVERIFY(mManager->saveCollection(&collection));
    QCOMPARE(mManager->error(), QOrganizerManager::NoError);
    QVERIFY(!collection.id().isNull());
    // QCOMPARE(added.count(), 1);
    // QVERIFY(modified.isEmpty());
    // QVERIFY(deleted.isEmpty());
    // QVERIFY(added.takeFirst()[0].value<QList<QOrganizerCollectionId>>().contains(collection.id()));
    
    QCOMPARE(mManager->collections().count(), 2);
    QOrganizerCollection read = mManager->collection(collection.id());
    QCOMPARE(read.id(), collection.id());
    QCOMPARE(read.metaData(QOrganizerCollection::KeyName),
             collection.metaData(QOrganizerCollection::KeyName));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyDescription),
             collection.metaData(QOrganizerCollection::KeyDescription));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyColor),
             collection.metaData(QOrganizerCollection::KeyColor));
    QCOMPARE(read.metaData(QOrganizerCollection::KeySecondaryColor),
             collection.metaData(QOrganizerCollection::KeySecondaryColor));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyImage),
             collection.metaData(QOrganizerCollection::KeyImage));
    QCOMPARE(read.extendedMetaData(QStringLiteral("visible")),
             collection.extendedMetaData(QStringLiteral("visible")));

    collection.setMetaData(QOrganizerCollection::KeyDescription,
                           QStringLiteral("Updated description."));
    collection.setExtendedMetaData(QStringLiteral("visible"), false);
    QVERIFY(mManager->saveCollection(&collection));
    QCOMPARE(mManager->error(), QOrganizerManager::NoError);

    QCOMPARE(mManager->collections().count(), 2);
    read = mManager->collection(collection.id());
    QCOMPARE(read.id(), collection.id());
    QCOMPARE(read.metaData(QOrganizerCollection::KeyName),
             collection.metaData(QOrganizerCollection::KeyName));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyDescription),
             collection.metaData(QOrganizerCollection::KeyDescription));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyColor),
             collection.metaData(QOrganizerCollection::KeyColor));
    QCOMPARE(read.metaData(QOrganizerCollection::KeySecondaryColor),
             collection.metaData(QOrganizerCollection::KeySecondaryColor));
    QCOMPARE(read.metaData(QOrganizerCollection::KeyImage),
             collection.metaData(QOrganizerCollection::KeyImage));
    QCOMPARE(read.extendedMetaData(QStringLiteral("visible")),
             collection.extendedMetaData(QStringLiteral("visible")));

    QVERIFY(mManager->removeCollection(collection.id()));
    QCOMPARE(mManager->error(), QOrganizerManager::NoError);
    QCOMPARE(mManager->collections().count(), 1);
}

void tst_engine::testCollectionExternal()
{
    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(QTimeZone()));
    mKCal::SqliteStorage storage(cal, QStringLiteral("db"));
    storage.open();

    QSignalSpy dataChanged(mManager, &QOrganizerManager::dataChanged);

    mKCal::Notebook::Ptr nb(new mKCal::Notebook(QStringLiteral("External test notebook"),
                                                QStringLiteral("Description")));
    QVERIFY(storage.addNotebook(nb));
    QTRY_COMPARE(dataChanged.count(), 1);
    dataChanged.takeFirst();

    QOrganizerCollection collection = mManager->collection(QOrganizerCollectionId(mManager->managerUri(), nb->uid().toUtf8()));
    QVERIFY(!collection.id().isNull());
    QCOMPARE(collection.metaData(QOrganizerCollection::KeyName).toString(),
             nb->name());
    QCOMPARE(collection.metaData(QOrganizerCollection::KeyDescription).toString(),
             nb->description());

    nb->setDescription(QStringLiteral("Updated description."));
    nb->setIsReadOnly(true);
    QVERIFY(storage.updateNotebook(nb));
    QTRY_COMPARE(dataChanged.count(), 1);
    dataChanged.takeFirst();

    collection = mManager->collection(QOrganizerCollectionId(mManager->managerUri(), nb->uid().toUtf8()));
    QVERIFY(!collection.id().isNull());
    QCOMPARE(collection.metaData(QOrganizerCollection::KeyName).toString(),
             nb->name());
    QCOMPARE(collection.metaData(QOrganizerCollection::KeyDescription).toString(),
             nb->description());
    QCOMPARE(collection.extendedMetaData(QStringLiteral("readOnly")).toBool(),
                                         nb->isReadOnly());

    QVERIFY(storage.deleteNotebook(nb));
    QTRY_COMPARE(dataChanged.count(), 1);
    dataChanged.takeFirst();

    collection = mManager->collection(QOrganizerCollectionId(mManager->managerUri(), nb->uid().toUtf8()));
    QVERIFY(collection.id().isNull());
}

#include "tst_engine.moc"
QTEST_MAIN(tst_engine)
