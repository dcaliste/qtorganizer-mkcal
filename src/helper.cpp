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
