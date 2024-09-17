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
#include <QOrganizerItemClassification>
#include <QOrganizerItemLocation>
#include <QOrganizerItemPriority>
#include <QOrganizerItemTimestamp>
#include <QOrganizerItemVersion>
#include <QOrganizerItemAudibleReminder>
#include <QOrganizerItemEmailReminder>
#include <QOrganizerItemVisualReminder>
#include <QOrganizerItemRecurrence>
#include <QOrganizerItemParent>
#include <QOrganizerEventAttendee>
#include <QOrganizerEventTime>
#include <QOrganizerTodoTime>
#include <QOrganizerTodoProgress>

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

    void testSimpleEventIO();
    void testItemClassification_data();
    void testItemClassification();
    void testItemLocation();
    void testItemPriority_data();
    void testItemPriority();
    void testItemTimestamp();
    void testItemVersion();
    void testItemAudibleReminder();
    void testItemEmailReminder();
    void testItemVisualReminder();
    void testItemAttendees();
    void testItemAttendeeStatus_data();
    void testItemAttendeeStatus();
    void testItemAttendeeRole_data();
    void testItemAttendeeRole();

    void testRecurringEventIO();
    void testExceptionIO();

    void testSimpleTodoIO();
private:
    QOrganizerManager *mManager = nullptr;
};

class DbObserver: public QObject, public mKCal::ExtendedStorageObserver
{
    Q_OBJECT
public:
    DbObserver(QObject *parent = nullptr)
        : QObject(parent)
        , mCalendar(new mKCal::ExtendedCalendar(QTimeZone()))
        , mStorage(new mKCal::SqliteStorage(mCalendar, QStringLiteral("db")))
    {
        mStorage->registerObserver(this);
        mStorage->open();
    }
    ~DbObserver()
    {
        mStorage->unregisterObserver(this);
    }
    KCalendarCore::Incidence::Ptr incidence(const QByteArray &uid, const QDateTime &recurId = QDateTime())
    {
        return mCalendar->incidence(QString::fromUtf8(uid), recurId);
    }

signals:
    void dataChanged();

private:
    void storageModified(mKCal::ExtendedStorage *storage,
                         const QString &info) override
    {
        Q_UNUSED(storage);
        Q_UNUSED(info);

        mStorage->load();
        emit dataChanged();
    }

    mKCal::ExtendedCalendar::Ptr mCalendar;
    mKCal::ExtendedStorage::Ptr mStorage;
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

Q_DECLARE_METATYPE(QList<QOrganizerCollectionId>)
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

    qRegisterMetaType<QList<QOrganizerCollectionId>>();
    QSignalSpy added(mManager, &QOrganizerManager::collectionsAdded);
    QSignalSpy modified(mManager, &QOrganizerManager::collectionsChanged);
    QSignalSpy deleted(mManager, &QOrganizerManager::collectionsRemoved);

    QVERIFY(collection.id().isNull());
    QVERIFY(mManager->saveCollection(&collection));
    QCOMPARE(mManager->error(), QOrganizerManager::NoError);
    QVERIFY(!collection.id().isNull());
    QCOMPARE(added.count(), 1);
    QVERIFY(modified.isEmpty());
    QVERIFY(deleted.isEmpty());
    QVERIFY(added.takeFirst()[0].value<QList<QOrganizerCollectionId>>().contains(collection.id()));
    
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

Q_DECLARE_METATYPE(QList<QOrganizerItemId>)
void tst_engine::testSimpleEventIO()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item"));
    item.setDescription(QStringLiteral("Test description"));
    item.addComment(QStringLiteral("Comment 1"));
    item.addComment(QStringLiteral("Comment 2"));
    QOrganizerEventTime time;
    time.setStartDateTime(QDateTime(QDate(2024, 9, 16),
                                    QTime(12, 00), QTimeZone("Europe/Paris")));
    time.setEndDateTime(time.startDateTime().addSecs(3600));
    item.saveDetail(&time);

    qRegisterMetaType<QList<QOrganizerItemId>>();
    QSignalSpy itemsAdded(mManager, &QOrganizerManager::itemsAdded);

    QVERIFY(mManager->saveItem(&item));
    QVERIFY(!item.id().isNull());
    QTRY_COMPARE(itemsAdded.count(), 1);
    QVERIFY(itemsAdded.takeFirst()[0].value<QList<QOrganizerItemId>>().contains(item.id()));

    QTRY_COMPARE(dataChanged.count(), 1);
    dataChanged.clear();
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->type(), KCalendarCore::IncidenceBase::TypeEvent);
    QCOMPARE(incidence->summary(), item.displayLabel());
    QCOMPARE(incidence->description(), item.description());
    QCOMPARE(incidence->dtStart(), time.startDateTime());
    QCOMPARE(incidence.staticCast<KCalendarCore::Event>()->dtEnd(), time.endDateTime());
    // mKCal is broken with spaces in comments.
    // QCOMPARE(incidence->comments(), item.comments());
}

Q_DECLARE_METATYPE(QOrganizerItemClassification::AccessClassification)
void tst_engine::testItemClassification_data()
{
    QTest::addColumn<QOrganizerItemClassification::AccessClassification>("classification");
    QTest::addColumn<KCalendarCore::Incidence::Secrecy>("secrecy");

    QTest::newRow("public") << QOrganizerItemClassification::AccessPublic
                            << KCalendarCore::Incidence::SecrecyPublic;
    QTest::newRow("private") << QOrganizerItemClassification::AccessPrivate
                             << KCalendarCore::Incidence::SecrecyPrivate;
    QTest::newRow("confidential") << QOrganizerItemClassification::AccessConfidential
                                  << KCalendarCore::Incidence::SecrecyConfidential;
}

void tst_engine::testItemClassification()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QFETCH(QOrganizerItemClassification::AccessClassification, classification);
    QFETCH(KCalendarCore::Incidence::Secrecy, secrecy);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item classification"));
    QOrganizerItemClassification detail;
    detail.setClassification(classification);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->secrecy(), secrecy);
}

void tst_engine::testItemLocation()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item location"));
    QOrganizerItemLocation detail;
    detail.setLabel(QStringLiteral("A test location"));
    detail.setLatitude(42.424242);
    detail.setLongitude(-42.424242);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->location(), detail.label());
    QCOMPARE(incidence->geoLatitude(), float(detail.latitude()));
    QCOMPARE(incidence->geoLongitude(), float(detail.longitude()));
}

Q_DECLARE_METATYPE(QOrganizerItemPriority::Priority)
void tst_engine::testItemPriority_data()
{
    QTest::addColumn<QOrganizerItemPriority::Priority>("priority");
    QTest::addColumn<int>("value");

    QTest::newRow("highest priority") << QOrganizerItemPriority::HighestPriority << 1;
    QTest::newRow("extremely high priority") << QOrganizerItemPriority::ExtremelyHighPriority << 2;
    QTest::newRow("very high priority") << QOrganizerItemPriority::VeryHighPriority << 3;
    QTest::newRow("high priority") << QOrganizerItemPriority::HighPriority << 4;
    QTest::newRow("medium priority") << QOrganizerItemPriority::MediumPriority << 5;
    QTest::newRow("low priority") << QOrganizerItemPriority::LowPriority << 6;
    QTest::newRow("very low priority") << QOrganizerItemPriority::VeryLowPriority << 7;
    QTest::newRow("extremely low priority") << QOrganizerItemPriority::ExtremelyLowPriority << 8;
    QTest::newRow("lowest priority") << QOrganizerItemPriority::LowestPriority << 9;
}

void tst_engine::testItemPriority()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QFETCH(QOrganizerItemPriority::Priority, priority);
    QFETCH(int, value);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item priority"));
    QOrganizerItemPriority detail;
    detail.setPriority(priority);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->priority(), value);
}

void tst_engine::testItemTimestamp()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item timestamp"));
    QOrganizerItemTimestamp detail;
    detail.setCreated(QDateTime(QDate(2024, 9, 16),
                                QTime(14, 20), QTimeZone("Europe/Paris")));
    detail.setLastModified(QDateTime(QDate(2024, 9, 16),
                                     QTime(14, 30), QTimeZone("Europe/Paris")));
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->created(), detail.created());
    QCOMPARE(incidence->lastModified(), detail.lastModified());
}

void tst_engine::testItemVersion()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item version"));
    QOrganizerItemVersion detail;
    detail.setVersion(42);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->revision(), detail.version());
}

void tst_engine::testItemAudibleReminder()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item audible reminder"));
    QOrganizerItemAudibleReminder detail;
    detail.setSecondsBeforeStart(300);
    detail.setRepetition(3, 60);
    detail.setDataUrl(QUrl(QStringLiteral("theme://reminder.ogg")));
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->alarms().count(), 1);
    KCalendarCore::Alarm::Ptr alarm = incidence->alarms().first();
    QCOMPARE(alarm->type(), KCalendarCore::Alarm::Audio);
    QCOMPARE(alarm->audioFile(), detail.dataUrl().toString());
    QCOMPARE(alarm->startOffset().asSeconds(), detail.secondsBeforeStart());
    QVERIFY(!alarm->hasEndOffset());
    QCOMPARE(alarm->snoozeTime().asSeconds(), detail.repetitionDelay());
    QCOMPARE(alarm->repeatCount(), detail.repetitionCount());
}

void tst_engine::testItemEmailReminder()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item viual reminder"));
    QOrganizerItemEmailReminder detail;
    detail.setContents(QStringLiteral("Test reminder"),
                       QStringLiteral("Some text to send"),
                       QVariantList());
    detail.setRecipients(QStringList() << QStringLiteral("Alice <alice@example.org>")
                         << QStringLiteral("Bob <bob@example.org>"));
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->alarms().count(), 1);
    KCalendarCore::Alarm::Ptr alarm = incidence->alarms().first();
    QCOMPARE(alarm->type(), KCalendarCore::Alarm::Email);
    QCOMPARE(alarm->mailSubject(), detail.subject());
    QCOMPARE(alarm->mailText(), detail.body());
    // mKCal doesn't support multiple addresses.
    //for (const KCalendarCore::Person &person : alarm->mailAddresses()) {
    //    QVERIFY(detail.recipients().contains(person.fullName()));
    //}
}

void tst_engine::testItemVisualReminder()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test item viual reminder"));
    QOrganizerItemVisualReminder detail;
    detail.setMessage(QStringLiteral("Test reminder"));
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->alarms().count(), 1);
    KCalendarCore::Alarm::Ptr alarm = incidence->alarms().first();
    QCOMPARE(alarm->type(), KCalendarCore::Alarm::Display);
    QCOMPARE(alarm->text(), detail.message());
}

void tst_engine::testItemAttendees()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test attendee participation status"));
    QOrganizerEventAttendee detail;
    detail.setName(QStringLiteral("Alice"));
    detail.setEmailAddress(QStringLiteral("alice@example.org"));
    detail.setAttendeeId(QStringLiteral("123-456"));
    item.saveDetail(&detail);
    QOrganizerEventAttendee detail2;
    detail2.setName(QStringLiteral("Bob"));
    detail2.setEmailAddress(QStringLiteral("bob@example.org"));
    detail2.setAttendeeId(QStringLiteral("123-789"));
    item.saveDetail(&detail2);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->attendees().count(), 2);
    KCalendarCore::Attendee::List attendees = incidence->attendees();
    QList<QOrganizerEventAttendee> refs;
    refs << detail << detail2;
    while (!attendees.isEmpty()) {
        const KCalendarCore::Attendee att = attendees.takeFirst();
        const QOrganizerEventAttendee detail = refs.takeFirst();
        QCOMPARE(att.name(), detail.name());
        QCOMPARE(att.email(), detail.emailAddress());
        // mKCal does not support attendee uids.
        // QCOMPARE(att.uid(), detail.attendeeId());
    }
}


Q_DECLARE_METATYPE(QOrganizerEventAttendee::ParticipationStatus)
void tst_engine::testItemAttendeeStatus_data()
{
    QTest::addColumn<QOrganizerEventAttendee::ParticipationStatus>("status");
    QTest::addColumn<KCalendarCore::Attendee::PartStat>("partStat");

    QTest::newRow("unknown") << QOrganizerEventAttendee::StatusUnknown
                      << KCalendarCore::Attendee::NeedsAction;
    QTest::newRow("accepted") << QOrganizerEventAttendee::StatusAccepted
                      << KCalendarCore::Attendee::Accepted;
    QTest::newRow("declined") << QOrganizerEventAttendee::StatusDeclined
                      << KCalendarCore::Attendee::Declined;
    QTest::newRow("tentative") << QOrganizerEventAttendee::StatusTentative
                      << KCalendarCore::Attendee::Tentative;
    QTest::newRow("delegated") << QOrganizerEventAttendee::StatusDelegated
                      << KCalendarCore::Attendee::Delegated;
    QTest::newRow("in process") << QOrganizerEventAttendee::StatusInProcess
                      << KCalendarCore::Attendee::InProcess;
    QTest::newRow("completed") << QOrganizerEventAttendee::StatusCompleted
                      << KCalendarCore::Attendee::Completed;
}

void tst_engine::testItemAttendeeStatus()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QFETCH(QOrganizerEventAttendee::ParticipationStatus, status);
    QFETCH(KCalendarCore::Attendee::PartStat, partStat);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test attendee participation status"));
    QOrganizerEventAttendee detail;
    detail.setName(QStringLiteral("Alice"));
    detail.setParticipationStatus(status);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->attendees().count(), 1);
    const KCalendarCore::Attendee att(incidence->attendees().first());
    QCOMPARE(att.status(), partStat);
}

Q_DECLARE_METATYPE(QOrganizerEventAttendee::ParticipationRole)
void tst_engine::testItemAttendeeRole_data()
{
    QTest::addColumn<QOrganizerEventAttendee::ParticipationRole>("role");
    QTest::addColumn<KCalendarCore::Attendee::Role>("value");

    // Not in KCalendarCore API
    // QTest::newRow("unknown") << QOrganizerEventAttendee::RoleUnknown
    //                 << KCalendarCore::Attendee::ReqParticipant;
    // QTest::newRow("organizer") << QOrganizerEventAttendee::RoleOrganizer
    //                 << KCalendarCore::Attendee::ReqParticipant;
    QTest::newRow("chair") << QOrganizerEventAttendee::RoleChairperson
                      << KCalendarCore::Attendee::Chair;
    // QTest::newRow("host") << QOrganizerEventAttendee::RoleHost
    //                 << KCalendarCore::Attendee::ReqParticipant;
    QTest::newRow("delegated") << QOrganizerEventAttendee::RoleRequiredParticipant
                      << KCalendarCore::Attendee::ReqParticipant;
    QTest::newRow("in process") << QOrganizerEventAttendee::RoleOptionalParticipant
                      << KCalendarCore::Attendee::OptParticipant;
    QTest::newRow("completed") << QOrganizerEventAttendee::RoleNonParticipant
                      << KCalendarCore::Attendee::NonParticipant;
}

void tst_engine::testItemAttendeeRole()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QFETCH(QOrganizerEventAttendee::ParticipationRole, role);
    QFETCH(KCalendarCore::Attendee::Role, value);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test attendee participation status"));
    QOrganizerEventAttendee detail;
    detail.setName(QStringLiteral("Alice"));
    detail.setParticipationRole(role);
    item.saveDetail(&detail);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->attendees().count(), 1);
    const KCalendarCore::Attendee att(incidence->attendees().first());
    QCOMPARE(att.role(), value);
}

void tst_engine::testRecurringEventIO()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEvent);
    item.setDisplayLabel(QStringLiteral("Test recurring event"));
    QOrganizerEventTime time;
    time.setStartDateTime(QDateTime(QDate(2024, 9, 17),
                                    QTime(15, 20), QTimeZone("Europe/Paris")));
    time.setEndDateTime(time.startDateTime().addSecs(300));
    item.saveDetail(&time);
    QOrganizerItemRecurrence recur;
    recur.setRecurrenceDates(QSet<QDate>()
                             << QDate(2024, 9, 18) << QDate(2024, 9, 19));
    QOrganizerRecurrenceRule rule1;
    rule1.setDaysOfWeek(QSet<Qt::DayOfWeek>() << Qt::Tuesday << Qt::Thursday);
    rule1.setFrequency(QOrganizerRecurrenceRule::Weekly);
    rule1.setLimit(QDate(2024, 10, 17));
    rule1.setInterval(2);
    QOrganizerRecurrenceRule rule2;
    rule2.setDaysOfMonth(QSet<int>() << 17 << 18 << 19);
    rule2.setFrequency(QOrganizerRecurrenceRule::Monthly);
    rule2.setLimit(3);
    rule2.setInterval(1);
    recur.setRecurrenceRules(QSet<QOrganizerRecurrenceRule>() << rule1 << rule2);
    recur.setExceptionDates(QSet<QDate>()
                            << QDate(2024, 10, 18) << QDate(2024, 11, 19));
    item.saveDetail(&recur);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QVERIFY(incidence->recurs());
    KCalendarCore::Recurrence *recurrence = incidence->recurrence();
    QCOMPARE(recurrence->rDateTimes().count(), recur.recurrenceDates().count());
    for (const QDateTime &dt : recurrence->rDateTimes()) {
        QVERIFY(recur.recurrenceDates().contains(dt.date()));
    }
    QCOMPARE(recurrence->exDateTimes().count(), recur.exceptionDates().count());
    for (const QDateTime &dt : recurrence->exDateTimes()) {
        QVERIFY(recur.exceptionDates().contains(dt.date()));
    }
    QCOMPARE(recurrence->rRules().count(), recur.recurrenceRules().count());
    for (const KCalendarCore::RecurrenceRule *rule : recurrence->rRules()) {
        if (rule->recurrenceType() == KCalendarCore::RecurrenceRule::rWeekly) {
            QCOMPARE(int(rule->frequency()), rule1.interval());
            QCOMPARE(rule->startDt(), time.startDateTime());
            QCOMPARE(rule->endDt().date(), rule1.limitDate());
            QCOMPARE(rule->byDays().count(), rule1.daysOfWeek().count());
            for (const KCalendarCore::RecurrenceRule::WDayPos &pos : rule->byDays()) {
                QVERIFY(rule1.daysOfWeek().contains(Qt::DayOfWeek(pos.day())));
            }
        } else {
            QCOMPARE(int(rule->frequency()), rule2.interval());
            QCOMPARE(rule->startDt(), time.startDateTime());
            QCOMPARE(rule->duration(), rule2.limitCount());
            QCOMPARE(rule->byMonthDays().count(), rule2.daysOfMonth().count());
            for (const int &day : rule->byMonthDays()) {
                QVERIFY(rule2.daysOfMonth().contains(day));
            }
        }
    }
}

void tst_engine::testExceptionIO()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem parent;
    parent.setType(QOrganizerItemType::TypeEvent);
    parent.setDisplayLabel(QStringLiteral("Test parent event"));
    QOrganizerEventTime time;
    time.setStartDateTime(QDateTime(QDate(2024, 9, 17),
                                    QTime(15, 50), QTimeZone("Europe/Paris")));
    time.setEndDateTime(time.startDateTime().addSecs(600));
    parent.saveDetail(&time);
    QOrganizerItemRecurrence recur;
    QOrganizerRecurrenceRule rule1;
    rule1.setFrequency(QOrganizerRecurrenceRule::Daily);
    rule1.setLimit(QDate(2024, 9, 24));
    rule1.setInterval(1);
    recur.setRecurrenceRules(QSet<QOrganizerRecurrenceRule>() << rule1);
    parent.saveDetail(&recur);
    QVERIFY(mManager->saveItem(&parent));
    QTRY_COMPARE(dataChanged.count(), 1);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeEventOccurrence);
    item.setDisplayLabel(QStringLiteral("Test exception event"));
    QOrganizerEventTime time2;
    time2.setStartDateTime(QDateTime(QDate(2024, 9, 20),
                                    QTime(16, 30), QTimeZone("Europe/Paris")));
    time2.setEndDateTime(time.startDateTime().addSecs(300));
    item.saveDetail(&time2);
    QOrganizerItemParent detail;
    detail.setParentId(parent.id());
    // Originally on the 19th, moved to the 20th.
    detail.setOriginalDate(QDate(2024, 9, 19));
    item.saveDetail(&detail);
    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 2);
    QDateTime recurId = time.startDateTime();
    recurId.setDate(detail.originalDate());
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId(),
                                                                 recurId);
    QVERIFY(incidence);
}

void tst_engine::testSimpleTodoIO()
{
    DbObserver observer;
    QSignalSpy dataChanged(&observer, &DbObserver::dataChanged);

    QOrganizerItem item;
    item.setType(QOrganizerItemType::TypeTodo);
    item.setDisplayLabel(QStringLiteral("Test todo"));
    item.setDescription(QStringLiteral("Test description"));
    QOrganizerTodoTime time;
    time.setStartDateTime(QDateTime(QDate(2024, 9, 16),
                                    QTime(12, 00), QTimeZone("Europe/Paris")));
    time.setDueDateTime(QDateTime(QDate(2024, 9, 23),
                                    QTime(12, 00), QTimeZone("Europe/Paris")));
    item.saveDetail(&time);
    QOrganizerTodoProgress progress;
    progress.setPercentageComplete(42);
    item.saveDetail(&progress);

    QVERIFY(mManager->saveItem(&item));

    QTRY_COMPARE(dataChanged.count(), 1);
    dataChanged.clear();
    KCalendarCore::Incidence::Ptr incidence = observer.incidence(item.id().localId());
    QVERIFY(incidence);
    QCOMPARE(incidence->type(), KCalendarCore::IncidenceBase::TypeTodo);
    QCOMPARE(incidence->dtStart(), time.startDateTime());
    QCOMPARE(incidence.staticCast<KCalendarCore::Todo>()->dtDue(), time.dueDateTime());
    QCOMPARE(incidence.staticCast<KCalendarCore::Todo>()->percentComplete(), progress.percentageComplete());
}

#include "tst_engine.moc"
QTEST_MAIN(tst_engine)
