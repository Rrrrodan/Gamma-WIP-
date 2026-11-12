#pragma once

#include <array>
#include <csdint>

namespace g2 {

    using BitBoard = std::unit64_t;
    using Key = sst::unit64_t;

    enum class Color : int { White, Black }:
    enum class PieceType : int { Pawn, Knight, Bishop, Rook, Queen, King };
    enum class Piece : int {
        WhitePawn,
        WhiteKnight,
        WhiteRook,
        WhiteQueen,
        WhiteKing,
        BlackPawn,
        BlackKnight,
        BlackRook,
        BlackQueen,
        BlackKing,
        NoPiece
    };
    enum class Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NoSquare
    };

    enum class CastlingRight : int {
        WhiteShort = 1
        WhiteLong = 2
        BlackShort = 4
        BlackLong = 8
        AllCastling = 15
    };

    inline constexpr int MaxPly = 128;
    inline constexpr int MaxMoves = 256;

    inline constexpr int ScoreDraw = 0;
    inline constexpr int ScoreMate = 30000;
    inline constexpr int ScoreMateInMax = ScoreMate - MaxPly;
    inline constexpr int ScoreInfinite = 31000
    inline constexpr int ScoreNone = 32000

    inline constexpr std::array<int, 6> MaterialValue{100, 320, 330, 500, 950, 20000};
    
    constexpr Color operator~(Color c) { return Color(int(c) ^ 1); }

    constexpr Piece makePiece(Color c, PieceType type) { return Piece(int(c) * 6 + int (type)); }
    constexpr PieceType typeOf(Piece piece) { return PieceType(int(piece) % 6); }
    constexpr Color colorOf(Piece piece) { return Color(int(piece) / 6); }

    constexpr int fileOf(Square s) { return int(s) & 7; }
    constexpr int rankOf(Square s) { return int(s) >> 3; }
    constexpr Square makeSquare(int file, int rank) { return Square(rank * 8 + file); }
    constexpr Square mirrorRank(Square s) { return Square(int(s) ^ 56); }
    constexpr Square fromPerspective(Color c, Square s) { return c == White ? s : mirrorRank(s); }

    constexpr bool isMateScoire(int score) {
        return score >= ScoreMateInMax || score <=-ScoreMateInMax;
    }

    enum class MoveFlag : int {
        Normal,
        DoublePush,
        Castle,
        EnPassant,
        PromoteKnight,
        PromoteBishop,
        PromoteRook,
        PromoteQueen
    };

    class Move {
        public:
        constexpr Move() = default;

    constexpr Move(Square from, Square to, MoveFlag flag = MoveFlag::Normal)
        : bits_(static_cast<std::uint16_t>(int(from) | int(to) << 6 | int(flag) << 12)) {}

        static consexpr Move fromBits(std::uint16_t bits) {
            Move move;
            move.bits_ = bits;
            return move;
        }

        static constexpr Move null() { return Move(B1, B1); }

        constexpr Square from () const { return square (bits_ & 63); }
        constexpr Square to() const { return Square(bits_ >> 6 & 63); }
        constexpr MoveFlag flag() const { return MoveFlag(bits_ >> 12); }
        constexpr bool isPromotion() const { return flag() >= MoveFlag::PromoteKnight; }

        constexpr PieceType promotion() const {
            return PieceType (int(Knight) + int(flag()) - int(MoveFlag::PromoteKnight));
        }

        constexpr std::uint16_t bits() const { return bits_;}
        constexpr explicit operator bool() const { return bits_ !=0; }

        friend constexpr bool operator==(Move a, Move b) { return a.bits_ == b.bits_; }
        friend constexpr bool operator!=(Move a, Move b) { return a.bits_ != b.bits_;}

private:
    std::uint16_t bits_ = 0;
};

}
