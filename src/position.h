#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "bitboard.h"

namespace g2 {

struct PieceChange {
    Piece piece = NoPiece;
    Square square = NoSquare;
};

struct BoardDelta {
    std::array<PieceChange, 2> removed{};
    std::array<PieceChange, 2> added{};
    int removedCount = 0;
    int addedCount = 0;
};

inline constexpr std::string_view StartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::string moveToUci(Move move);

class Position {
public:
    Position();

    static void initKeys();

    bool setFen(std::string_view notation);
    std::string fen() const;

    void makeMove(Move move);
    void undoMove();
    void makeNullMove();
    void undoNullMove();

    Color sideToMove() const { return side_; }
    Piece pieceOn(Square s) const { return board_[s]; }
    Bitboard pieces(Color c) const { return byColor_[c]; }
    Bitboard pieces(PieceType type) const { return byType_[type]; }
    Bitboard pieces(Color c, PieceType type) const { return byColor_[c] & byType_[type]; }
    Bitboard occupied() const { return byColor_[White] | byColor_[Black]; }
    Square kingSquare(Color c) const { return lsb(pieces(c, King)); }

    Key key() const { return current().key; }
    Key pawnKey() const { return current().pawnKey; }
    Bitboard checkers() const { return current().checkers; }
    Bitboard pinned() const { return current().pinned; }
    bool inCheck() const { return current().checkers != 0; }
    Square enPassant() const { return current().enPassant; }
    int castlingRights() const { return current().castling; }
    int halfmoveClock() const { return current().halfmoves; }
    const BoardDelta& lastDelta() const { return current().delta; }

    Bitboard attackersTo(Square s, Bitboard occupancy) const;
    bool attacked(Square s, Color by, Bitboard occupancy) const;
    bool canCastle(int right) const;
    bool isLegal(Move move) const;
    bool isCapture(Move move) const;
    bool isTactical(Move move) const;
    Piece movedPiece(Move move) const { return board_[move.from()]; }
    PieceType capturedType(Move move) const;
    int exchange(Move move) const;
    bool exchangeAtLeast(Move move, int threshold) const { return exchange(move) >= threshold; }
    bool hasNonPawnMaterial(Color c) const;
    bool isDraw(int searchPly) const;

private:
    struct Empty {};

    struct State {
        Key key = 0;
        Key pawnKey = 0;
        Bitboard checkers = 0;
        Bitboard pinned = 0;
        Move move;
        Square enPassant = NoSquare;
        int castling = 0;
        int halfmoves = 0;
        BoardDelta delta;
    };

    explicit Position(Empty);

    const State& current() const { return states_.back(); }
    State& current() { return states_.back(); }

    void place(Piece piece, Square s);
    void lift(Square s);
    void computeKeys();
    void updateCheckInfo();
    bool pawnMoveFits(Move move) const;

    std::array<Piece, 64> board_{};
    std::array<Bitboard, 6> byType_{};
    std::array<Bitboard, 2> byColor_{};
    Color side_ = White;
    int fullmove_ = 1;
    std::vector<State> states_;
};

}
