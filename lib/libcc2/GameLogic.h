/******************************************************************************
 * This file is part of CCTools.                                              *
 *                                                                            *
 * CCTools is free software: you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * CCTools is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with CCTools.  If not, see <http://www.gnu.org/licenses/>.           *
 ******************************************************************************/

#ifndef _CC2_GAMELOGIC_H
#define _CC2_GAMELOGIC_H

#include "Map.h"
#include <QPoint>
#include <optional>

namespace cc2 {

enum MoveState {
    MoveNorth,      // OK to move, moves North
    MoveEast,       // OK to move, moves East
    MoveSouth,      // OK to move, moves South
    MoveWest,       // OK to move, moves West
    MoveBlocked,    // All 4 directions are blocked
    MoveDirMask = 0x0F,

    // Extra data
    MoveTrapped  = (1<<4),  // Monster is on unreleased trap
    MoveDeath    = (1<<5),  // OK to move, but results in death
    MoveTeleport = (1<<6),  // Entered teleporter, move to exit teleport
};

MoveState CheckMove(const MapData& map, const Tile* tile, int x, int y);
void TurnCreature(Tile* tile, MoveState state);
QPoint AdvanceCreature(const QPoint& pos, MoveState state);

struct QPointCompareReadingOrder {
    bool operator()(const QPoint& left, const QPoint& right) const {
        if (left.y() < right.y()) return true;
        if (left.y() > right.y()) return false;
        if (left.x() < right.x()) return true;
        return false;
    };
};

struct WireNetwork {
    std::map<const QPoint, uint8_t, QPointCompareReadingOrder> members;
};
std::optional<QPoint> GetNeighborTile(const QPoint& point, cc2::Tile::Direction dir, QSize size, bool wrap);
WireNetwork TraceNetworkFromTileInDirection(const MapData& map, const QPoint& pos, Tile::Direction dir);
std::map<uint8_t, WireNetwork> TraceNetworksFromTile(const MapData& map, const QPoint& pos);

void ToggleGreens(MapData& map);

}

#endif
