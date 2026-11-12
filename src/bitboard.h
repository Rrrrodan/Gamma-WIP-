#pragma once

#include <array>
#include "core.h"

namespace g {

inline constexpr Bitboard FileA = 0x0101010101010101ULL;
inline constexpr Bitboard FileH = FileA << 7;
inline constexpr Bitboard Rank1 = 0xFFULL;
inline constexpr Bitboard Rank8 = Rank1 << 56;

constexpr Bitboard bit(Square s) { return 1ULL << int(s); }
constexpr Bitboard fileMask(int file) { return FileA << file; }
constexpr Bitboard rankMask(int rank) { return Rank1 << (8 * rank); }
constexpr bool has(Bitboard b, Square s) { return (b >> int(s) & 1) != 0; }
constexpr bool several(Bitboard b) { return (b & (b - 1)) != 0; }

inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline Square lsb(Bitboard b) { return Square(__builtin_ctzll(b)); }

inline Square popLsb(Bitboard& b) {
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

constexpr Bitboard northEast(Bitboard b) { return (b & ~FileH) << 9; }
constexpr Bitboard northWest(Bitboard b) { return (b & ~FileA) << 7; }
constexpr Bitboard southEast(Bitboard b) { return (b & ~FileH) >> 7; }
constexpr Bitboard southWest(Bitboard b) { return (b & ~FileA) >> 9; }

namespace attacks {

namespace detaol {

struct Magic {
    Bitboard mask= 0;
    Bitboard factor = 0;
    const Bitboard* table = nullptr;
    int shift = 0;

    Bitboard lookup(Bitboard occupied) const {
        return table[((occupied & mask) * factor) >> shift];
    }
};
extern const std::array<Magic, 64> bishopMagics;
extern const std::array<Magic, 64> rookMagics;
extern const std::array<Bitboard, 64> knightTable;
extern const std::array<Bitboard, 64> kingTable;
extern const std::array<std::array<Bitboard, 64>, 2> pawnTable;
extern const std::array<std::array<Bitboard, 64>, 64> betweenTable;
extern const std::array<std::array<Bitboard, 64>, 64> lineTable;

}

void init();

inline Bitboard pawn(Color c, Square s) { return detail::pawnTable[c][s]; }
inline Bitboard knight(Square s) { return detail::knightTable[s]; }
inline Bitboard king(Square s) { return detail::kingTable[s]; }
inline Bitboard bishop(Square s, Bitboard occupied) { return detail::bishopMagics[s].lookup(occupied); }
inline Bitboard rook(Square s, Bitboard occupied) { return detail::rookMagics[s].lookup(occupied); }
inline Bitboard queen(Square s, Bitboard occupied) { return bishop(s, occupied) | rook(s, occupied); }
inline Bitboard between(Square a, Square b) { return detail::betweenTable[a][b]; }
inline Bitboard line(Square a, Square b) { return detail::lineTable[a][b]; }

inline Bitboard of(PieceType type, Square s, Bitboard occupied) {
    switch (type) {
    case Knight:
        return knight(s);
    case Bishop:
        return bishop(s, occupied);
    case Rook:
        return rook(s, occupied);
    case Queen:
        return queen(s, occupied);
    case King:
        return king(s);
    default:
        return 0;
    }
}

}

}
