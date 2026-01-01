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

#include "GameLogic.h"

#include <QPoint>
#include <QSize>
#include <queue>
#include <optional>

#define TILE_DIR(dir)   (cc2::Tile::Direction)((dir) & 0x03)

static void getPreferredDirections(const cc2::Tile* tile, cc2::Tile::Direction dirs[])
{
    const int myDir = tile->direction();

    switch (tile->type()) {
    case cc2::Tile::Ant:        // L,F,R,B
        dirs[0] = TILE_DIR(myDir + 3);
        dirs[1] = TILE_DIR(myDir    );
        dirs[2] = TILE_DIR(myDir + 1);
        dirs[3] = TILE_DIR(myDir + 2);
        break;
    case cc2::Tile::FireBox:    // F,R,L,B
        dirs[0] = TILE_DIR(myDir    );
        dirs[1] = TILE_DIR(myDir + 1);
        dirs[2] = TILE_DIR(myDir + 3);
        dirs[3] = TILE_DIR(myDir + 2);
        break;
    case cc2::Tile::Ball:       // F,B
    case cc2::Tile::BlueTank:   // F,B  -- Treat tank as going both directions
        dirs[0] = TILE_DIR(myDir    );
        dirs[1] = TILE_DIR(myDir + 2);
        dirs[2] = cc2::Tile::InvalidDir;
        dirs[3] = cc2::Tile::InvalidDir;
        break;
    case cc2::Tile::Ship:       // F,L,R,B
    case cc2::Tile::Ghost:      // F,L,R,B
        dirs[0] = TILE_DIR(myDir    );
        dirs[1] = TILE_DIR(myDir + 3);
        dirs[2] = TILE_DIR(myDir + 1);
        dirs[3] = TILE_DIR(myDir + 2);
        break;
    case cc2::Tile::Centipede:  // R,F,L,B
        dirs[0] = TILE_DIR(myDir + 1);
        dirs[1] = TILE_DIR(myDir    );
        dirs[2] = TILE_DIR(myDir + 3);
        dirs[3] = TILE_DIR(myDir + 2);
        break;
    default:
        dirs[0] = cc2::Tile::InvalidDir;
        dirs[1] = cc2::Tile::InvalidDir;
        dirs[2] = cc2::Tile::InvalidDir;
        dirs[3] = cc2::Tile::InvalidDir;
        break;
    }
}

static uint32_t effectiveTracks(uint32_t tracks)
{
    if (tracks & cc2::TileModifier::TrackSwitch) {
        // Treat as if only one track is present
        switch (tracks & cc2::TileModifier::ActiveTrack_MASK) {
        case cc2::TileModifier::ActiveTrack_NE:
            tracks &= cc2::TileModifier::Track_NE;
            break;
        case cc2::TileModifier::ActiveTrack_SE:
            tracks &= cc2::TileModifier::Track_SE;
            break;
        case cc2::TileModifier::ActiveTrack_SW:
            tracks &= cc2::TileModifier::Track_SW;
            break;
        case cc2::TileModifier::ActiveTrack_NW:
            tracks &= cc2::TileModifier::Track_NW;
            break;
        case cc2::TileModifier::ActiveTrack_WE:
            tracks &= cc2::TileModifier::Track_WE;
            break;
        case cc2::TileModifier::ActiveTrack_NS:
            tracks &= cc2::TileModifier::Track_NS;
            break;
        default:
            tracks = 0;
            break;
        }
    }
    return tracks;
}

static cc2::Tile::Direction trackDirection(cc2::Tile::Direction dir,
                                           cc2::Tile::Direction facing,
                                           uint32_t tracks)
{
    Q_ASSERT(dir != TILE_DIR(facing + 2));

    if (dir == facing) {
        if (((dir == cc2::Tile::North || dir == cc2::Tile::South)
                && (tracks & cc2::TileModifier::Track_NS) != 0)
            || ((dir == cc2::Tile::West || dir == cc2::Tile::East)
                && (tracks & cc2::TileModifier::Track_WE) != 0))
            return dir;
    } else if (dir == TILE_DIR(facing + 1)) {
        if ((dir == cc2::Tile::North && (tracks & cc2::TileModifier::Track_NE) != 0)
            || (dir == cc2::Tile::East && (tracks & cc2::TileModifier::Track_SE) != 0)
            || (dir == cc2::Tile::South && (tracks & cc2::TileModifier::Track_SW) != 0)
            || (dir == cc2::Tile::West && (tracks & cc2::TileModifier::Track_NW) != 0))
            return dir;
    } else if (dir == TILE_DIR(facing + 3)) {
        if ((dir == cc2::Tile::North && (tracks & cc2::TileModifier::Track_NW) != 0)
            || (dir == cc2::Tile::East && (tracks & cc2::TileModifier::Track_NE) != 0)
            || (dir == cc2::Tile::South && (tracks & cc2::TileModifier::Track_SE) != 0)
            || (dir == cc2::Tile::West && (tracks & cc2::TileModifier::Track_SW) != 0))
            return dir;
    }
    return cc2::Tile::InvalidDir;
}

static void getPreferredTrackDirections(const cc2::Tile* tile,
                                        cc2::Tile::Direction dirs[],
                                        uint32_t tracks)
{
    tracks = effectiveTracks(tracks);

    const cc2::Tile::Direction myDir = tile->direction();
    dirs[0] = cc2::Tile::InvalidDir;
    dirs[1] = cc2::Tile::InvalidDir;
    dirs[2] = cc2::Tile::InvalidDir;
    dirs[3] = cc2::Tile::InvalidDir;

    switch (tile->type()) {
    case cc2::Tile::Ant:        // L,F,R
        dirs[0] = trackDirection(TILE_DIR(myDir + 3), myDir, tracks);
        dirs[1] = trackDirection(TILE_DIR(myDir    ), myDir, tracks);
        dirs[2] = trackDirection(TILE_DIR(myDir + 1), myDir, tracks);
        break;
    case cc2::Tile::FireBox:    // F,R,L
    case cc2::Tile::Ball:       // F,R,L
    case cc2::Tile::BlueTank:   // F,R,L  -- Treat tank as going both directions
        dirs[0] = trackDirection(TILE_DIR(myDir    ), myDir, tracks);
        dirs[1] = trackDirection(TILE_DIR(myDir + 1), myDir, tracks);
        dirs[2] = trackDirection(TILE_DIR(myDir + 3), myDir, tracks);
        break;
    case cc2::Tile::Ship:       // F,L,R
    case cc2::Tile::Ghost:      // F,L,R
        dirs[0] = trackDirection(TILE_DIR(myDir    ), myDir, tracks);
        dirs[1] = trackDirection(TILE_DIR(myDir + 3), myDir, tracks);
        dirs[2] = trackDirection(TILE_DIR(myDir + 1), myDir, tracks);
        break;
    case cc2::Tile::Centipede:  // R,F,L
        dirs[0] = trackDirection(TILE_DIR(myDir + 1), myDir, tracks);
        dirs[1] = trackDirection(TILE_DIR(myDir    ), myDir, tracks);
        dirs[2] = trackDirection(TILE_DIR(myDir + 3), myDir, tracks);
        break;
    default:
        break;
    }
}

static const cc2::Tile* findPanel(const cc2::Tile* tile)
{
    do {
        if (tile->isPanelCanopy())
            return tile;
        tile = tile->lower();
    } while (tile);

    return nullptr;
}

static const cc2::Tile* peekTile(const cc2::MapData& map, int x, int y,
                                 cc2::Tile::Direction dir)
{
    switch (dir) {
    case cc2::Tile::North:
        return (y > 0) ? &map.tile(x, y - 1) : nullptr;
    case cc2::Tile::East:
        return (x < map.width() - 1) ? &map.tile(x + 1, y) : nullptr;
    case cc2::Tile::South:
        return (y < map.height() - 1) ? &map.tile(x, y + 1) : nullptr;
    case cc2::Tile::West:
        return (x > 0) ? &map.tile(x - 1, y) : nullptr;
    default:
        return nullptr;
    }
}

cc2::MoveState cc2::CheckMove(const MapData& map, const Tile* tile, int x, int y)
{
    Tile::Direction dirs[4];
    getPreferredDirections(tile, dirs);
    if (dirs[0] == Tile::InvalidDir)
        return MoveBlocked;

    const Tile& mapTile = map.tile(x, y);
    const Tile& baseTile = mapTile.bottom();
    const int myDir = tile->direction();
    int state = 0;

    switch (baseTile.type()) {
    case Tile::Force_N:
        if (tile->type() != Tile::Ghost) {
            dirs[0] = Tile::North;
            dirs[1] = Tile::InvalidDir;
            dirs[2] = Tile::InvalidDir;
            dirs[3] = Tile::InvalidDir;
        }
        break;
    case Tile::Force_E:
        if (tile->type() != Tile::Ghost) {
            dirs[0] = Tile::East;
            dirs[1] = Tile::InvalidDir;
            dirs[2] = Tile::InvalidDir;
            dirs[3] = Tile::InvalidDir;
        }
        break;
    case Tile::Force_S:
        if (tile->type() != Tile::Ghost) {
            dirs[0] = Tile::South;
            dirs[1] = Tile::InvalidDir;
            dirs[2] = Tile::InvalidDir;
            dirs[3] = Tile::InvalidDir;
        }
        break;
    case Tile::Force_W:
        if (tile->type() != Tile::Ghost) {
            dirs[0] = Tile::West;
            dirs[1] = Tile::InvalidDir;
            dirs[2] = Tile::InvalidDir;
            dirs[3] = Tile::InvalidDir;
        }
        break;
    case Tile::Force_Rand:
        if (tile->type() != Tile::Ghost) {
            // TODO: Blocked isn't really accurate here...
            return MoveBlocked;
        }
        break;
    case Tile::Trap:
        if (tile->type() != Tile::Ghost) {
            state |= MoveTrapped;
            // TODO: Check if the corresponding button is pressed...
        }
        break;
    case Tile::Ice:
        if (tile->type() != Tile::Ghost) {
            // Preferred directions are only straight and back when on ice.
            dirs[0] = (Tile::Direction)((myDir    ) & 0x03);
            dirs[1] = (Tile::Direction)((myDir + 2) & 0x03);
            dirs[2] = Tile::InvalidDir;
            dirs[3] = Tile::InvalidDir;
        }
        break;
    case Tile::Ice_SE:
        if (tile->type() != Tile::Ghost) {
            if (tile->direction() == Tile::North) {
                dirs[0] = Tile::East;
                dirs[1] = Tile::South;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            } else if (tile->direction() == Tile::West) {
                dirs[0] = Tile::South;
                dirs[1] = Tile::East;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            }
        }
        break;
    case Tile::Ice_SW:
        if (tile->type() != Tile::Ghost) {
            if (tile->direction() == Tile::North) {
                dirs[0] = Tile::West;
                dirs[1] = Tile::South;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            } else if (tile->direction() == Tile::East) {
                dirs[0] = Tile::South;
                dirs[1] = Tile::West;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            }
        }
        break;
    case Tile::Ice_NW:
        if (tile->type() != Tile::Ghost) {
            if (tile->direction() == Tile::South) {
                dirs[0] = Tile::West;
                dirs[1] = Tile::North;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            } else if (tile->direction() == Tile::East) {
                dirs[0] = Tile::North;
                dirs[1] = Tile::West;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            }
        }
        break;
    case Tile::Ice_NE:
        if (tile->type() != Tile::Ghost) {
            if (tile->direction() == Tile::South) {
                dirs[0] = Tile::East;
                dirs[1] = Tile::North;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            } else if (tile->direction() == Tile::West) {
                dirs[0] = Tile::North;
                dirs[1] = Tile::East;
                dirs[2] = Tile::InvalidDir;
                dirs[3] = Tile::InvalidDir;
            }
        }
        break;
    case Tile::TrainTracks:
        if (tile->type() != Tile::Ghost)
            getPreferredTrackDirections(tile, dirs, baseTile.modifier());
        break;
    default:
        break;
    }

    for (Tile::Direction dir : dirs) {
        if (dir == Tile::InvalidDir)
            continue;

        const Tile* panelTile = findPanel(&mapTile);
        if (panelTile && (tile->type() != Tile::Ghost)) {
            switch (panelTile->type()) {
            case Tile::CC1_Barrier_S:
                if (dir == Tile::South)
                    continue;
                break;
            case Tile::CC1_Barrier_E:
                if (dir == Tile::East)
                    continue;
                break;
            case Tile::CC1_Barrier_SE:
                if (dir == Tile::South || dir == Tile::East)
                    continue;
                break;
            case Tile::PanelCanopy:
                if ((panelTile->tileFlags() & Tile::PanelNorth) && dir == Tile::North)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelEast) && dir == Tile::East)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelSouth) && dir == Tile::South)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelWest) && dir == Tile::West)
                    continue;
                break;
            default:
                // Should never get here unless isPanelCanopy() returns
                // with an unexpected tile type
                Q_ASSERT(false);
            }
        }

        const Tile* peek = peekTile(map, x, y, dir);
        if (!peek)
            continue;

        const Tile* peekBase = &peek->bottom();
        if (peekBase->type() == Tile::SteelWall || peekBase->type() == Tile::StyledWall
                || peekBase->type() >= Tile::NUM_TILE_TYPES
                || peek->haveTile({Tile::MirrorPlayer, Tile::MirrorPlayer2, Tile::BlueTank,
                                   Tile::YellowTank, Tile::DirtBlock, Tile::IceBlock,
                                   Tile::DirBlock})) {
            // These tiles block ALL mobs
            continue;
        }
        if ((peekBase->type() == Tile::Wall || peekBase->type() == Tile::ToggleWall
                || peekBase->type() == Tile::Exit || peekBase->type() == Tile::DirtBlock
                || peekBase->type() == Tile::IceBlock || peekBase->type() == Tile::Gravel
                || (peekBase->type() >= Tile::Door_Red && peekBase->type() <= Tile::Door_Green)
                || (peekBase->type() >= Tile::Socket && peekBase->type() <= Tile::Dirt)
                || peekBase->type() == Tile::ToolThief || peekBase->type() == Tile::KeyThief
                || (peekBase->type() >= Tile::CC1_Cloner && peekBase->type() <= Tile::Clue)
                || peekBase->type() == Tile::LSwitchWall || peekBase->type() == Tile::StayUpGWall
                || ((peekBase->type() == Tile::Ice_NE || peekBase->type() == Tile::Ice_NW) && dir == Tile::North)
                || ((peekBase->type() == Tile::Ice_SE || peekBase->type() == Tile::Ice_NE) && dir == Tile::East)
                || ((peekBase->type() == Tile::Ice_SE || peekBase->type() == Tile::Ice_SW) && dir == Tile::South)
                || ((peekBase->type() == Tile::Ice_NW || peekBase->type() == Tile::Ice_SW) && dir == Tile::West)
                || (peekBase->type() == Tile::RevolvDoor_SW && (dir == Tile::North || dir == Tile::East))
                || (peekBase->type() == Tile::RevolvDoor_NW && (dir == Tile::South || dir == Tile::East))
                || (peekBase->type() == Tile::RevolvDoor_NE && (dir == Tile::South || dir == Tile::West))
                || (peekBase->type() == Tile::RevolvDoor_SE && (dir == Tile::North || dir == Tile::West))
                || (peek->haveTile({Tile::Chip, Tile::ExtraChip, Tile::GreenChip,
                                    Tile::Key_Yellow, Tile::Key_Green,
                                    Tile::TimeBonus, Tile::TimePenalty, Tile::ToggleClock,
                                    Tile::Flag10, Tile::Flag100, Tile::Flag1000, Tile::Flag2x,
                                    Tile::IceCleats, Tile::MagnoShoes, Tile::FireShoes,
                                    Tile::Flippers, Tile::SpeedShoes, Tile::HikingBoots,
                                    Tile::TimeBomb, Tile::Lightning, Tile::BowlingBall,
                                    Tile::Helmet, Tile::RRSign, Tile::SteelFoil,
                                    Tile::Eye, Tile::Bribe, Tile::Hook})))
                && (tile->type() != Tile::Ghost))
            continue;
        if (peekBase->type() == Tile::StyledFloor && tile->type() == Tile::Ghost)
            continue;

        panelTile = findPanel(peek);
        if (panelTile && (tile->type() != Tile::Ghost)) {
            switch (panelTile->type()) {
            case Tile::CC1_Barrier_S:
                if (dir == Tile::North)
                    continue;
                break;
            case Tile::CC1_Barrier_E:
                if (dir == Tile::West)
                    continue;
                break;
            case Tile::CC1_Barrier_SE:
                if (dir == Tile::North || dir == Tile::West)
                    continue;
                break;
            case Tile::PanelCanopy:
                if ((panelTile->tileFlags() & Tile::PanelNorth) && dir == Tile::South)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelEast) && dir == Tile::West)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelSouth) && dir == Tile::North)
                    continue;
                if ((panelTile->tileFlags() & Tile::PanelWest) && dir == Tile::East)
                    continue;
                break;
            default:
                // Should never get here unless isPanelCanopy() returns
                // with an unexpected tile type
                Q_ASSERT(false);
            }
        }
        if (peekBase->type() == Tile::TrainTracks && tile->type() != Tile::Ghost) {
            const uint32_t tracks = effectiveTracks(peekBase->modifier());
            switch (dir) {
            case cc2::Tile::North:
                if ((tracks & (cc2::TileModifier::Track_SE | cc2::TileModifier::Track_SW
                               | cc2::TileModifier::Track_NS)) == 0)
                    continue;
                break;
            case cc2::Tile::East:
                if ((tracks & (cc2::TileModifier::Track_NW | cc2::TileModifier::Track_SW
                               | cc2::TileModifier::Track_WE)) == 0)
                    continue;
                break;
            case cc2::Tile::South:
                if ((tracks & (cc2::TileModifier::Track_NE | cc2::TileModifier::Track_NW
                               | cc2::TileModifier::Track_NS)) == 0)
                    continue;
                break;
            case cc2::Tile::West:
                if ((tracks & (cc2::TileModifier::Track_NE | cc2::TileModifier::Track_SE
                               | cc2::TileModifier::Track_WE)) == 0)
                    continue;
                break;
            default:
                break;
            }
        }

        if (peekBase->type() == Tile::Water) {
            if (tile->type() == Tile::Ghost)
                continue;
            if (tile->type() != Tile::Ship)
                state |= MoveDeath;
        }
        if (peekBase->type() == Tile::Turtle && (tile->type() == Tile::Ghost
                || tile->type() == Tile::FireBox))
            continue;
        if (peekBase->type() == Tile::Fire && tile->type() != Tile::FireBox
                && tile->type() != Tile::Ghost)
            continue;
        if (peekBase->type() == Tile::FlameJet_On && tile->type() != Tile::FireBox)
            state |= MoveDeath;
        if ((peekBase->type() == Tile::Slime || peek->haveTile({Tile::RedBomb, Tile::GreenBomb}))
                && tile->type() != Tile::Ghost)
            state |= MoveDeath;
        if (peekBase->type() >= Tile::Teleport_Red && peekBase->type() <= Tile::Teleport_Green)
            state |= MoveTeleport;

        return (MoveState)(state | dir);
    }

    return (MoveState)(state | MoveBlocked);
}

void cc2::TurnCreature(Tile* tile, MoveState state)
{
    if ((state & MoveDirMask) >= MoveBlocked)
        return;

    tile->setDirection((Tile::Direction)(state & MoveDirMask));
}

QPoint cc2::AdvanceCreature(const QPoint& pos, MoveState state)
{
    if ((state & MoveDirMask) >= MoveBlocked)
        return pos;

    QPoint result = pos;
    switch (state & MoveDirMask) {
    case MoveNorth:
        result.setY(pos.y() - 1);
        break;
    case MoveEast:
        result.setX(pos.x() + 1);
        break;
    case MoveSouth:
        result.setY(pos.y() + 1);
        break;
    case MoveWest:
        result.setX(pos.x() - 1);
        break;
    default:
        result = pos;
        break;
    }

    return result;
}

void cc2::ToggleGreens(MapData& mapData)
{
    for (int y = 0; y < mapData.height(); ++y) {
        for (int x = 0; x < mapData.width(); ++x) {
            cc2::Tile* tile = &mapData.tile(x, y);
            while (tile) {
                switch (tile->type()) {
                case cc2::Tile::ToggleWall:
                    tile->setType(cc2::Tile::ToggleFloor);
                    break;
                case cc2::Tile::ToggleFloor:
                    tile->setType(cc2::Tile::ToggleWall);
                    break;
                case cc2::Tile::GreenBomb:
                    tile->setType(cc2::Tile::GreenChip);
                    break;
                case cc2::Tile::GreenChip:
                    tile->setType(cc2::Tile::GreenBomb);
                    break;
                default:
                    break;
                }
                tile = tile->lower();
            }
        }
    }
}

std::optional<QPoint> cc2::GetNeighborTile(const QPoint& point, Tile::Direction dir, QSize size, bool wrap) {
    switch(dir) {
    case cc2::Tile::North:
        if (point.y() == 0) return {};
        return QPoint(point.x(), point.y() - 1);
    case cc2::Tile::South:
        if (point.y() == size.height() - 1) return {};
        return QPoint(point.x(), point.y() + 1);
    case cc2::Tile::East:
        if (point.x() == size.width() - 1) {
            if (!wrap) return {};
            if (point.y() == size.height() - 1) return {};
            return QPoint(0, point.y() + 1);
        }
        return QPoint(point.x() + 1, point.y());
    case cc2::Tile::West:
        if (point.x() == 0) {
            if (!wrap) return {};
            if (point.y() == 0) return {};
            return QPoint(size.width() - 1, point.y() - 1);
        }
        return QPoint(point.x() - 1, point.y());
    default:
        return {};
    }
}

static uint8_t dirToWire(cc2::Tile::Direction dir) {
    return 1 << dir;
}
static cc2::Tile::Direction oppositeDir(cc2::Tile::Direction dir) {
    switch (dir) {
    case cc2::Tile::North: return cc2::Tile::South;
    case cc2::Tile::South: return cc2::Tile::North;
    case cc2::Tile::West: return cc2::Tile::East;
    case cc2::Tile::East: return cc2::Tile::West;
    default: return cc2::Tile::InvalidDir;
    }
}
static cc2::Tile::Direction wireToDir(uint8_t wire) {
    if (wire & 0x1) return cc2::Tile::North;
    if (wire & 0x2) return cc2::Tile::East;
    if (wire & 0x4) return cc2::Tile::South;
    if (wire & 0x8) return cc2::Tile::West;
    return cc2::Tile::InvalidDir;
}

static uint8_t getWires(const cc2::Tile& tile) {
    if (tile.type() != cc2::Tile::LogicGate) return tile.modifier() & 0xf;
    switch (tile.modifier()) {
        case cc2::TileModifier::Inverter_N:
        case cc2::TileModifier::Inverter_S:
            return 0b0101;
        case cc2::TileModifier::Inverter_E:
        case cc2::TileModifier::Inverter_W:
            return 0b1010;
        case cc2::TileModifier::AndGate_N:
        case cc2::TileModifier::OrGate_N:
        case cc2::TileModifier::XorGate_N:
        case cc2::TileModifier::NandGate_N:
        case cc2::TileModifier::LatchGateCW_N:
        case cc2::TileModifier::LatchGateCCW_N:
            return 0b1011;
        case cc2::TileModifier::AndGate_E:
        case cc2::TileModifier::OrGate_E:
        case cc2::TileModifier::XorGate_E:
        case cc2::TileModifier::NandGate_E:
        case cc2::TileModifier::LatchGateCW_E:
        case cc2::TileModifier::LatchGateCCW_E:
            return 0b0111;
        case cc2::TileModifier::AndGate_S:
        case cc2::TileModifier::OrGate_S:
        case cc2::TileModifier::XorGate_S:
        case cc2::TileModifier::NandGate_S:
        case cc2::TileModifier::LatchGateCW_S:
        case cc2::TileModifier::LatchGateCCW_S:
            return 0b1110;
        case cc2::TileModifier::AndGate_W:
        case cc2::TileModifier::OrGate_W:
        case cc2::TileModifier::XorGate_W:
        case cc2::TileModifier::NandGate_W:
        case cc2::TileModifier::LatchGateCW_W:
        case cc2::TileModifier::LatchGateCCW_W:
            return 0b1101;
        case cc2::TileModifier::CounterGate_0:
        case cc2::TileModifier::CounterGate_1:
        case cc2::TileModifier::CounterGate_2:
        case cc2::TileModifier::CounterGate_3:
        case cc2::TileModifier::CounterGate_4:
        case cc2::TileModifier::CounterGate_5:
        case cc2::TileModifier::CounterGate_6:
        case cc2::TileModifier::CounterGate_7:
        case cc2::TileModifier::CounterGate_8:
        case cc2::TileModifier::CounterGate_9:
            return 0b1111;
        default:
            return 0b0000;
    }
}

static uint8_t getWireTunnels(const cc2::Tile& tile) {
    return tile.type() == cc2::Tile::Floor ? ((tile.modifier() & 0xf0) >> 4) : 0;
}

static uint8_t getOutWires(const cc2::Tile& tile, cc2::Tile::Direction checkingFrom, bool allowTunnel) {
    cc2::Tile::WireSupport wireSupport = tile.wireSupport();
    uint8_t wires = getWires(tile);
    uint8_t wireTunnels = getWireTunnels(tile);
    uint8_t exposedWires = allowTunnel ? wires : wires & ~(wireTunnels);
    uint8_t connectingWire = dirToWire(checkingFrom);

    if (wireSupport == cc2::Tile::NoSupport) return 0;
    if (wireSupport == cc2::Tile::ReceivePower) {
        if (tile.type() == cc2::Tile::Teleport_Red && wires == 0) return 0;
        return connectingWire;
    }
    if (!(exposedWires & connectingWire)) return 0;

    switch (wireSupport) {
    case cc2::Tile::ConductNone: return connectingWire;
    case cc2::Tile::ConductCross: if (wires != 0xf) return wires;
        /* fallthrough */
    case cc2::Tile::ConductAlwaysCross: return (connectingWire & 0b0101 ? 0b0101 : 0b1010) & wires;
    case cc2::Tile::ConductEverywhere: return wires;
    default: return 0;
    }
}

static std::optional<const QPoint> traceWireTunnel(const cc2::MapData& map, const QPoint& initPos, cc2::Tile::Direction initDir) {
    QSize levelSize(map.width(), map.height());
    uint8_t nestedLevel = 0;
    uint8_t openTunnel = dirToWire(initDir);
    uint8_t closeTunnel = dirToWire(oppositeDir(initDir));
    QPoint pos = initPos;
    while (true) {
        std::optional<QPoint> newPos = cc2::GetNeighborTile(pos, initDir, levelSize, false);
        if (!newPos.has_value()) return {};
        pos = std::move(newPos.value());
        const cc2::Tile& tile = map.tile(pos.x(), pos.y()).bottom();
        uint8_t tunnels = getWireTunnels(tile);
        if (tunnels & closeTunnel) {
            if (nestedLevel == 0) return pos;
            else nestedLevel -= 1;
        }
        if (tunnels & openTunnel) {
            nestedLevel += 1;
        }
    }
}

cc2::WireNetwork cc2::TraceNetworkFromTileInDirection(const MapData& map, const QPoint& initPos, cc2::Tile::Direction initDir) {
    WireNetwork network {};
    std::queue<std::tuple<const QPoint, Tile::Direction, bool>> toTrace;
    QSize size(map.width(), map.height());
    toTrace.push({initPos, oppositeDir(initDir), true});
    bool initTrace = true;
    while (!toTrace.empty()) {
        auto [pos, goingInDir, allowTunnelConnection] = toTrace.front();
        toTrace.pop();
        const cc2::Tile& terrain = map.tile(pos.x(), pos.y()).bottom();
        uint8_t wires = getOutWires(terrain, oppositeDir(goingInDir), allowTunnelConnection);
        if (wires == 0 && initTrace) {
            network.members.clear();
            return network;
        }
        initTrace = false;
        if (wires == 0) continue;
        if (network.members.find(pos) == network.members.end()) {
            network.members[pos] = wires;
        } else {
            network.members[pos] |= wires;
        }
        uint8_t tunnels = getWireTunnels(terrain);
        for (int dirIdx = Tile::North; dirIdx <= Tile::West; dirIdx += 1) {
            Tile::Direction dir = static_cast<Tile::Direction>(dirIdx);
            uint8_t dirWire = dirToWire(dir);
            if (!(dirWire & wires)) continue;
            std::optional<const QPoint> neighPosOpt;
            bool isTunnel = dirWire & tunnels;
            // This is very, very dumb. Any better way to do this?
            if (isTunnel) {
                std::optional<const QPoint> neighPosOpt2 = traceWireTunnel(map, pos, dir);
                if (neighPosOpt2.has_value()) {
                    neighPosOpt.emplace(neighPosOpt2.value());
                }
            } else {
                std::optional<const QPoint> neighPosOpt2 = GetNeighborTile(pos, dir, size, true);
                if (neighPosOpt2.has_value()) {
                    neighPosOpt.emplace(neighPosOpt2.value());
                }
            }
            if (!neighPosOpt.has_value()) continue;
            const QPoint& neighPos = neighPosOpt.value();
            if (network.members.find(neighPos) != network.members.end() && (network.members[neighPos] & dirToWire(oppositeDir(dir)))) continue;
            toTrace.push({neighPos, dir, isTunnel});

        }
    }
    return network;
}

std::map<uint8_t, cc2::WireNetwork> cc2::TraceNetworksFromTile(const MapData& map, const QPoint& initPos) {
    std::map<uint8_t, WireNetwork> networks;
    for (int dirIdx = Tile::North; dirIdx <= Tile::West; dirIdx += 1) {
        Tile::Direction dir = static_cast<Tile::Direction>(dirIdx);
        // If any network already has this wire direction, there's no need to retrace it
        bool alreadyTraced = false;
        for (auto& [wires, _]: networks) {
            if (wires & dirToWire(dir)) {
                alreadyTraced = true;
                break;
            }
        }
        if (alreadyTraced) continue;
        WireNetwork network = TraceNetworkFromTileInDirection(map, initPos, dir);
        if (network.members.empty()) continue;
        uint8_t containedWires = network.members[initPos];
        networks[containedWires] = std::move(network);
    }

    return networks;
}
