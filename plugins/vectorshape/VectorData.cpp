/* This file is part of the KDE project
 *
 * Copyright (C) 2007, 2009 Thomas Zander <zander@kde.org>
 * Copyright (C) 2007 Jan Hambrecht <jaham@gmx.net>
 * Copyright (C) 2008 Casper Boemann <cbr@boemann.dk>
 * Copyright (C) 2008 Thorsten Zachmann <zachmann@kde.org>
 * Copyright (C) 2010 Inge Wallin <inge@lysator.liu.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "VectorData.h"

#include "VectorCollection.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QPainter>

#include <kdebug.h>
#include <KTemporaryFile>

#include <KoUnit.h>
#include <KoStore.h>
#include <KoStoreDevice.h>


VectorData::VectorData()
    : KoShapeUserData()
    , key(0)
    , errorCode(VectorData::Success)
    , collection(0)
    , dataStoreState(StateEmpty)
    , temporaryFile(0)
{
}

VectorData::VectorData(const VectorData &vectorData)
    : KoShapeUserData()
{
    Q_UNUSED(vectorData);
    //TODO copy the vectordata - this is a copy constructor
}

VectorData::~VectorData()
{
    if (collection)
        collection->removeOnKey(key);

    delete temporaryFile;
}

QString VectorData::tagForSaving(int &counter)
{
    if (!saveName.isEmpty())
        return saveName;

    if (suffix.isEmpty()) {
        return saveName = QString("Vectors/vector%1").arg(++counter);
    } else {
        return saveName = QString("Vectors/vector%1.%2").arg(++counter).arg(suffix);
    }
}

void VectorData::setExternalVector(const QUrl &location, VectorCollection *col)
{
    if (collection) {
        // Let the collection first check if it already has one of
        // these. If it doesn't it will call this method again and
        // we'll go to the other clause.
        VectorData *other = col->createExternalVectorData(location);
        this->operator=(*other);
        delete other;
    } else {
        vectorLocation = location;
        setSuffix(location.toEncoded());
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(location.toEncoded());
        key = VectorData::generateKey(md5.result());
    }
}

void VectorData::setVector(const QString &url, KoStore *store, VectorCollection *collection)
{
    if (collection) {
        // let the collection first check if it already has one. If it doesn't it'll call this method
        // again and we'll go to the other clause
        VectorData *other = collection->createVectorData(url, store);
        this->operator=(*other);
        delete other;
    } else {
        setSuffix(url);
        if (store->open(url)) {
            KoStoreDevice device(store);
            QByteArray data = device.readAll();
            if (!device.open(QIODevice::ReadOnly)) {
                kWarning(30006) << "open file from store " << url << "failed";
                errorCode = OpenFailed;
                store->close();
                return;
            }
            copyToTemporary(device);
        } else {
            kWarning(30006) << "Find file in store " << url << "failed";
            errorCode = OpenFailed;
            return;
        }
    }
}

// FIXME: This isn't usable in the vector shape
QUrl VectorData::playableUrl() const
{
    if (dataStoreState == StateSpooled) {
        return QUrl(temporaryFile->fileName());
    } else {
        return vectorLocation;
    }
}

bool VectorData::isValid() const
{
    return dataStoreState != VectorData::StateEmpty
        && errorCode == Success;
}

bool VectorData::operator==(const VectorData &other) const
{
    Q_UNUSED(other);
    return false;
}

VectorData &VectorData::operator=(const VectorData &other)
{
    Q_UNUSED(other);
    return *this;
}

bool VectorData::saveData(QIODevice &device)
{
    if (dataStoreState == StateSpooled) {
        Q_ASSERT(temporaryFile); // otherwise the collection should not have called this
        if (temporaryFile) {
            if (!temporaryFile->open()) {
                kWarning(30006) << "Read file from temporary store failed";
                return false;
            }
            char buf[8192];
            while (true) {
                temporaryFile->waitForReadyRead(-1);
                qint64 bytes = temporaryFile->read(buf, sizeof(buf));
                if (bytes <= 0)
                    break; // done!
                do {
                    qint64 nWritten = device.write(buf, bytes);
                    if (nWritten == -1) {
                        temporaryFile->close();
                        return false;
                    }
                    bytes -= nWritten;
                } while (bytes > 0);
            }
            temporaryFile->close();
        }
        return true;
    } else if (!vectorLocation.isEmpty()) {
        if (true) { //later on this should check if the user wants us to do this
            // An external vector have been specified
            QFile file(vectorLocation.toLocalFile());

            if (!file.open(QIODevice::ReadOnly)) {
                kWarning(30006) << "Read file failed";
                return false;
            }
            char buf[8192];
            while (true) {
                file.waitForReadyRead(-1);
                qint64 bytes = file.read(buf, sizeof(buf));
                if (bytes <= 0)
                    break; // done!
                do {
                    qint64 nWritten = device.write(buf, bytes);
                    if (nWritten == -1) {
                        file.close();
                        return false;
                    }
                    bytes -= nWritten;
                } while (bytes > 0);
            }
            file.close();
        }
    }
    return false;
}

void VectorData::copyToTemporary(QIODevice &device)
{
    delete temporaryFile;
    temporaryFile = new KTemporaryFile();
    temporaryFile->setPrefix("KoVectorData");
    if (!temporaryFile->open()) {
        kWarning(30006) << "open temporary file for writing failed";
        errorCode = VectorData::StorageFailed;
        return;
    }
    QCryptographicHash md5(QCryptographicHash::Md5);
    char buf[8096];
    while (true) {
        device.waitForReadyRead(-1);
        qint64 bytes = device.read(buf, sizeof(buf));
        if (bytes <= 0)
            break; // done!
        md5.addData(buf, bytes);
        do {
            bytes -= temporaryFile->write(buf, bytes);
        } while (bytes > 0);
    }
    key = VectorData::generateKey(md5.result());
    temporaryFile->close();

    QFileInfo fi(*temporaryFile);
    dataStoreState = StateSpooled;
}

void VectorData::setSuffix(const QString &name)
{
    QRegExp rx("\\.([^/]+$)"); // TODO does this work on windows or do we have to use \ instead of / for a path separator?
    if (rx.indexIn(name) != -1) {
        suffix = rx.cap(1);
    }
}

qint64 VectorData::generateKey(const QByteArray &bytes)
{
    qint64 answer = 1;
    const int max = qMin(8, bytes.count());
    for (int x = 0; x < max; ++x)
        answer += bytes[x] << (8 * x);
    return answer;
}

#include <VectorData.moc>
