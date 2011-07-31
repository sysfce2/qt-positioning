/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "marker.h"
#include "mapswidget.h"

#include <QPixmap>

#include "qlandmark.h"
#include "qgeoboundingcircle.h"

class MarkerPrivate
{
public:
    Marker::MarkerType type;
    QString name;
    bool moveable;
    QGeoAddress address;
};

Marker::Marker(MarkerType type) :
    QGeoMapPixmapObject(),
    d(new MarkerPrivate)
{
    setMarkerType(type);
}

void Marker::setMarkerType(MarkerType type)
{
    QString filename;
    QPoint offset;
    int scale;

    d->type = type;

    switch (d->type) {
    case MyLocationMarker:
        filename = ":/icons/mylocation.png";
        break;
    case SearchMarker:
        filename = ":/icons/searchmarker.png";
        break;
    case WaypointMarker:
        filename = ":/icons/waypointmarker.png";
        break;
    case StartMarker:
        filename = ":/icons/startmarker.png";
        break;
    case EndMarker:
        filename = ":/icons/endmarker.png";
        break;
    case PathMarker:
        filename = ":/icons/pathmarker.png";
        break;
    }

    if (d->type == MyLocationMarker) {
        offset = QPoint(-13,-13);
        scale = 25;
    } else {
        offset = QPoint(-15, -36);
        scale = 30;
    }

    setOffset(offset);
    setPixmap(QPixmap(filename).scaledToWidth(scale, Qt::SmoothTransformation));
}

void Marker::setAddress(QGeoAddress addr)
{
    if (d->address != addr) {
        d->address = addr;
        emit addressChanged(d->address);
    }
}

Marker::MarkerType Marker::markerType() const
{
    return d->type;
}

QString Marker::name() const
{
    return d->name;
}

QGeoAddress Marker::address() const
{
    return d->address;
}

bool Marker::moveable() const
{
    return d->moveable;
}

void Marker::setName(QString name)
{
    if (d->name != name) {
        d->name = name;
        emit nameChanged(d->name);
    }
}

void Marker::setMoveable(bool moveable)
{
    if (d->moveable != moveable) {
        d->moveable = moveable;
        emit moveableChanged(d->moveable);
    }
}


class MarkerManagerPrivate
{
public:
    Marker *myLocation;
    QList<Marker*> searchMarkers;

    // a reverse geocode request is currently running
    bool revGeocodeRunning;
    // a request is currently running, and my location has changed
    // since it started (ie, the request is stale)
    bool myLocHasMoved;

    QGraphicsGeoMap *map;
    StatusBarItem *status;
    QGeocodingManager *searchManager;

    QSet<QGeocodeReply*> forwardReplies;
    QSet<QGeocodeReply*> reverseReplies;
};

MarkerManager::MarkerManager(QGeocodingManager *searchManager, QObject *parent) :
    QObject(parent),
    d(new MarkerManagerPrivate)
{
    d->searchManager = searchManager;
    d->status = 0;
    d->revGeocodeRunning = false;
    d->myLocHasMoved = false;

    d->myLocation = new Marker(Marker::MyLocationMarker);
    d->myLocation->setName("Me");

    // hook the coordinateChanged() signal for reverse geocoding
    connect(d->myLocation, SIGNAL(coordinateChanged(QGeoCoordinate)),
            this, SLOT(myLocationChanged(QGeoCoordinate)));

    connect(d->searchManager, SIGNAL(finished(QGeocodeReply*)),
            this, SLOT(replyFinished(QGeocodeReply*)));
    connect(d->searchManager, SIGNAL(finished(QGeocodeReply*)),
            this, SLOT(reverseReplyFinished(QGeocodeReply*)));
}

MarkerManager::~MarkerManager()
{
    d->map->removeMapObject(d->myLocation);
    delete d->myLocation;
    removeSearchMarkers();
}

void MarkerManager::setStatusBar(StatusBarItem *bar)
{
    d->status = bar;
}

void MarkerManager::setMap(QGraphicsGeoMap *map)
{
    d->map = map;
    map->addMapObject(d->myLocation);
}

void MarkerManager::setMyLocation(QGeoCoordinate coord)
{
    d->myLocation->setCoordinate(coord);
}

void MarkerManager::search(QString query, qreal radius)
{
    QGeocodeReply *reply;
    if (radius > 0) {
        QGeoBoundingCircle *boundingCircle = new QGeoBoundingCircle(
                    d->myLocation->coordinate(), radius);
        reply = d->searchManager->geocode(query,
                                        -1, 0,
                                        boundingCircle);
    } else {
        reply = d->searchManager->geocode(query);
    }

    d->forwardReplies.insert(reply);

    if (d->status) {
        d->status->setText("Searching...");
        d->status->show();
    }

    if (reply->isFinished()) {
        replyFinished(reply);
    } else {
        connect(reply, SIGNAL(error(QGeocodeReply::Error,QString)),
                this, SIGNAL(searchError(QGeocodeReply::Error,QString)));
    }
}

void MarkerManager::removeSearchMarkers()
{
    foreach (Marker *m, d->searchMarkers) {
        d->map->removeMapObject(m);
        delete m;
    }
}

QGeoCoordinate MarkerManager::myLocation() const
{
    return d->myLocation->coordinate();
}

void MarkerManager::myLocationChanged(QGeoCoordinate location)
{
    if (d->revGeocodeRunning) {
        d->myLocHasMoved = true;
    } else {
        QGeocodeReply *reply = d->searchManager->reverseGeocode(location);
        d->reverseReplies.insert(reply);
        d->myLocHasMoved = false;

        if (reply->isFinished()) {
            d->revGeocodeRunning = false;
            reverseReplyFinished(reply);
        } else {
            d->revGeocodeRunning = true;
        }
    }
}

void MarkerManager::reverseReplyFinished(QGeocodeReply *reply)
{
    if (!d->reverseReplies.contains(reply))
        return;

    if (!reply->locations().isEmpty()) {
        QGeoLocation location = reply->locations().first();
        d->myLocation->setAddress(location.address());
    }

    d->revGeocodeRunning = false;
    if (d->myLocHasMoved)
        myLocationChanged(d->myLocation->coordinate());

    d->reverseReplies.remove(reply);
    reply->deleteLater();
}

void MarkerManager::replyFinished(QGeocodeReply *reply)
{
    if (!d->forwardReplies.contains(reply))
        return;

    // generate the markers and add them to the map
    foreach (const QGeoLocation &location, reply->locations()) {
        Marker *m = new Marker(Marker::SearchMarker);
        m->setCoordinate(location.coordinate());

        m->setName(QString("%1, %2").arg(location.address().street())
                                        .arg(location.address().city()));
        m->setAddress(location.address());
        m->setMoveable(false);

        d->searchMarkers.append(m);

        if (d->map) {
            d->map->addMapObject(m);
            // also zoom out until marker is visible
            while (!d->map->viewport().contains(location.coordinate()))
                d->map->setZoomLevel(d->map->zoomLevel()-1);
        }
    }

    d->forwardReplies.remove(reply);
    reply->deleteLater();

    emit searchFinished();

    if (d->status)
        d->status->hide();
}
