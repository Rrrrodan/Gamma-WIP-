#include "bitboard.h"

#include <utility>
#include <vector>

#include "random.h"

namespace g2::attacks {

namespace detail {

std::array<Magic, 64> bishopMagics;
std::array<Magic, 64> rookMagics;
std::array<Bitboard, 64> knightTable;
std::array<Bitboard, 64> kingTable;
std::array<std::array<Bitboard, 64>, 2> pawnTable;
std::array<std::array<Bitboard, 64>, 64> betweenTable;
std::array<std::array<Bitboard, 64>, 64> lineTable;

}

namespace {

using Step = std::pair<int, int>;

constexpr std::array<Step, 4> DiagonalSteps{{{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
constexpr std::array<Step, 4> StraightSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
constexpr std::array<Step, 8> KnightSteps{
    {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
constexpr std::array<Step, 8> KingSteps{
    {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}}};

constexpr std::size_t BishopTableSize = 5248;
constexpr std::size_t RookTableSize = 102400;

std::vector<Bitboard> slideStorage;

bool onBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

template <std::size_t N>
Bitboard reach(Square from, Bitboard occupied, const std::array<Step, N>& steps, bool slides) {
    Bitboard reached = 0;

    for (const auto& [fileStep, rankStep] : steps) {
        int file = fileOf(from) + fileStep;
        int rank = rankOf(from) + rankStep;

        while (onBoard(file, rank)) {
            const Bitboard target = bit(makeSquare(file, rank));
            reached |= target;

            if (!slides || (occupied & target))
                break;

            file += fileStep;
            rank += rankStep;
        }
    }

    return reached;
}

void buildMagics(std::array<detail::Magic, 64>& magics, const std::array<Step, 4>& steps,
                 Bitboard*& cursor, SplitMix& random) {
    std::vector<Bitboard> occupancies;
    std::vector<Bitboard> answers;
    std::vector<int> claimedBy;

    for (int index = 0; index < 64; ++index) {
        auto Square s = Square(index);
        const Bitboard edges = ((Rank1 | Rank8) & ~rankMask(rankOf(s)))
                             | ((FileA | FileH) & ~fileMask(fileOf(s)));

        detail::Magic& magic = magics[s];
        magic.mask = reach(s, 0, steps, true) & ~edges;
        magic.shift = 64 - popcount(magic.mask);
        magic.table = cursor;

        occupancies.clear();
        answers.clear();

        Bitboard subset = 0;
        do {
            occupancies.push_back(subset);
            answers.push_back(reach(s, subset, steps, true));
            subset = (subset - magic.mask) & magic.mask;
        } while (subset);

        claimedBy.assign(occupancies.size(), 0);

        for (int attempt = 1;; ++attempt) {
            magic.factor = random.sparse();

            if (popcount((magic.factor * magic.mask) >> 56) < 6)
                continue;

            bool collides = false;

            for (std::size_t i = 0; i < occupancies.size() && !collides; ++i) {
                const std::size_t slot = (occupancies[i] * magic.factor) >> magic.shift;

                if (claimedBy[slot] != attempt) {
                    claimedBy[slot] = attempt;
                    cursor[slot] = answers[i];
                } else if (cursor[slot] != answers[i]) {
                    collides = true;
                }
            }

            if (!collides)
                break;
        }

        cursor += occupancies.size();
    }
}

void buildLines() {
    for (int index = 0; index < 64; ++index) {
        auto Square from = Square(index);

        for (const auto& [fileStep, rankStep] : KingSteps) {
            const std::array<Step, 1> forward{{{fileStep, rankStep}}};
            const std::array<Step, 1> backward{{{-fileStep, -rankStep}}};
            const Bitboard fullLine = reach(from, 0, forward, true) | reach(from, 0, backward, true)
                                    | bit(from);

            Bitboard path = 0;
            int file = fileOf(from) + fileStep;
            int rank = rankOf(from) + rankStep;

            while (onBoard(file, rank)) {
                auto Square to = makeSquare(file, rank);
                detail::betweenTable[from][to] = path;
                detail::lineTable[from][to] = fullLine;
                path |= bit(to);
                file += fileStep;
                rank += rankStep;
            }
        }
    }
}

}

void init() {
    for (int index = 0; index < 64; ++index) {
        auto Square s = Square(index);
        const Bitboard origin = bit(s);

        detail::knightTable[s] = reach(s, 0, KnightSteps, false);
        detail::kingTable[s] = reach(s, 0, KingSteps, false);
        detail::pawnTable[White][s] = northEast(origin) | northWest(origin);
        detail::pawnTable[Black][s] = southEast(origin) | southWest(origin);
    }

    slideStorage.assign(BishopTableSize + RookTableSize, 0);
    Bitboard* cursor = slideStorage.data();
    SplitMix random(0x6A09E667F3BCC909ULL);

    buildMagics(detail::bishopMagics, DiagonalSteps, cursor, random);
    buildMagics(detail::rookMagics, StraightSteps, cursor, random);
    buildLines();
}

}
