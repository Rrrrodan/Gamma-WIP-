#include "position.h"

#include <algorithm>
#include <charconv>

#include "random.h"

namespace g2 {

namespace {

std::array<std::array<Key, 64>, 12> pieceKeys;
std::array<Key, 16> castlingKeys;
std::array<Key, 8> enPassantKeys;
Key sideKey;

constexpr std::string_view PieceLetters = "PNBRQKpnbrqk";

constexpr std::array<int, 64> CastlingKept = [] {
    std::array<int, 64> kept{};
    for (int& rights : kept)
        rights = AllCastling;
    kept[E1] = BlackShort | BlackLong;
    kept[H1] = AllCastling & ~WhiteShort;
    kept[A1] = AllCastling & ~WhiteLong;
    kept[E8] = WhiteShort | WhiteLong;
    kept[H8] = AllCastling & ~BlackShort;
    kept[A8] = AllCastling & ~BlackLong;
    return kept;
}();

std::vector<std::string_view> splitFields(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;

    while (start < text.size()) {
        const std::size_t end = std::min(text.find(' ', start), text.size());
        if (end > start)
            fields.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    return fields;
}

bool parseSquare(std::string_view text, Square& square) {
    if (text.size() != 2 || text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8')
        return false;

    square = makeSquare(text[0] - 'a', text[1] - '1');
    return true;
}

int parseCount(std::string_view text, int fallback) {
    int value = fallback;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

}

std::string moveToUci(Move move) {
    if (!move)
        return "0000";

    std::string text;
    for (const Square s : {move.from(), move.to()}) {
        text += static_cast<char>('a' + fileOf(s));
        text += static_cast<char>('1' + rankOf(s));
    }

    if (move.isPromotion())
        text += "nbrq"[move.promotion() - Knight];

    return text;
}

void Position::initKeys() {
    SplitMix random(0xC2B2AE3D27D4EB4FULL);

    const auto draw = [&random] { return random.next(); };

    for (auto& squares : pieceKeys)
        std::generate(squares.begin(), squares.end(), draw);

    std::generate(castlingKeys.begin(), castlingKeys.end(), draw);
    std::generate(enPassantKeys.begin(), enPassantKeys.end(), draw);
    sideKey = draw();
}

Position::Position() {
    states_.reserve(1024);
    setFen(StartFen);
}

Position::Position(Empty) {
    board_.fill(NoPiece);
    states_.emplace_back();
}

bool Position::setFen(std::string_view notation) {
    const std::vector<std::string_view> fields = splitFields(notation);
    if (fields.size() < 4)
        return false;

    Position parsed{Empty{}};
    int rank = 7;
    int file = 0;

    for (const char c : fields[0]) {
        if (c == '/') {
            if (file != 8 || rank == 0)
                return false;
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8)
                return false;
        } else {
            const std::size_t index = PieceLetters.find(c);
            if (index == std::string_view::npos || file > 7)
                return false;
            parsed.place(Piece(index), makeSquare(file, rank));
            ++file;
        }
    }

    if (rank != 0 || file != 8)
        return false;

    if (popcount(parsed.pieces(White, King)) != 1 || popcount(parsed.pieces(Black, King)) != 1)
        return false;

    if (parsed.pieces(Pawn) & (Rank1 | Rank8))
        return false;

    if (fields[1] == "w")
        parsed.side_ = White;
    else if (fields[1] == "b")
        parsed.side_ = Black;
    else
        return false;

    State& state = parsed.current();

    if (fields[2] != "-") {
        for (const char c : fields[2]) {
            switch (c) {
            case 'K':
                state.castling |= WhiteShort;
                break;
            case 'Q':
                state.castling |= WhiteLong;
                break;
            case 'k':
                state.castling |= BlackShort;
                break;
            case 'q':
                state.castling |= BlackLong;
                break;
            default:
                return false;
            }
        }
    }

    const auto standsOn = [&](Piece piece, Square s) { return parsed.board_[s] == piece; };

    if (!standsOn(WhiteKing, E1) || !standsOn(WhiteRook, H1))
        state.castling &= ~WhiteShort;
    if (!standsOn(WhiteKing, E1) || !standsOn(WhiteRook, A1))
        state.castling &= ~WhiteLong;
    if (!standsOn(BlackKing, E8) || !standsOn(BlackRook, H8))
        state.castling &= ~BlackShort;
    if (!standsOn(BlackKing, E8) || !standsOn(BlackRook, A8))
        state.castling &= ~BlackLong;

    if (fields[3] != "-") {
        Square target = NoSquare;
        if (!parseSquare(fields[3], target))
            return false;

        const Color us = parsed.side_;
        if (rankOf(fromPerspective(us, target)) == 5
            && (attacks::pawn(~us, target) & parsed.pieces(us, Pawn)))
            state.enPassant = target;
    }

    state.halfmoves = fields.size() > 4 ? parseCount(fields[4], 0) : 0;
    parsed.fullmove_ = std::max(1, fields.size() > 5 ? parseCount(fields[5], 1) : 1);

    const Color waiting = ~parsed.side_;
    if (parsed.attacked(parsed.kingSquare(waiting), parsed.side_, parsed.occupied()))
        return false;

    parsed.computeKeys();
    parsed.updateCheckInfo();
    parsed.states_.reserve(1024);
    *this = std::move(parsed);
    return true;
}

std::string Position::fen() const {
    std::string text;

    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;

        for (int file = 0; file < 8; ++file) {
            const Piece piece = board_[makeSquare(file, rank)];

            if (piece == NoPiece) {
                ++empty;
                continue;
            }

            if (empty)
                text += static_cast<char>('0' + empty);
            empty = 0;
            text += PieceLetters[piece];
        }

        if (empty)
            text += static_cast<char>('0' + empty);
        if (rank)
            text += '/';
    }

    text += side_ == White ? " w " : " b ";

    const int rights = castlingRights();
    if (!rights)
        text += '-';
    if (rights & WhiteShort)
        text += 'K';
    if (rights & WhiteLong)
        text += 'Q';
    if (rights & BlackShort)
        text += 'k';
    if (rights & BlackLong)
        text += 'q';

    text += ' ';

    if (enPassant() == NoSquare) {
        text += '-';
    } else {
        text += static_cast<char>('a' + fileOf(enPassant()));
        text += static_cast<char>('1' + rankOf(enPassant()));
    }

    return text + ' ' + std::to_string(halfmoveClock()) + ' ' + std::to_string(fullmove_);
}

void Position::place(Piece piece, Square s) {
    board_[s] = piece;
    byType_[typeOf(piece)] |= bit(s);
    byColor_[colorOf(piece)] |= bit(s);
}

void Position::lift(Square s) {
    const Piece piece = board_[s];
    byType_[typeOf(piece)] ^= bit(s);
    byColor_[colorOf(piece)] ^= bit(s);
    board_[s] = NoPiece;
}

void Position::computeKeys() {
    State& state = current();
    state.key = state.pawnKey = 0;

    for (Bitboard remaining = occupied(); remaining;) {
        const Square s = popLsb(remaining);
        state.key ^= pieceKeys[board_[s]][s];
        if (typeOf(board_[s]) == Pawn)
            state.pawnKey ^= pieceKeys[board_[s]][s];
    }

    state.key ^= castlingKeys[state.castling];

    if (state.enPassant != NoSquare)
        state.key ^= enPassantKeys[fileOf(state.enPassant)];

    if (side_ == Black)
        state.key ^= sideKey;
}

void Position::updateCheckInfo() {
    State& state = current();
    const Color them = ~side_;
    const Square king = kingSquare(side_);
    const Bitboard occupancy = occupied();

    state.checkers = attackersTo(king, occupancy) & pieces(them);
    state.pinned = 0;

    const Bitboard diagonal = pieces(them, Bishop) | pieces(them, Queen);
    const Bitboard straight = pieces(them, Rook) | pieces(them, Queen);
    Bitboard snipers = (attacks::bishop(king, 0) & diagonal) | (attacks::rook(king, 0) & straight);

    while (snipers) {
        const Bitboard blockers = attacks::between(king, popLsb(snipers)) & occupancy;
        if (blockers && !several(blockers))
            state.pinned |= blockers & pieces(side_);
    }
}

void Position::makeMove(Move move) {
    const Color us = side_;
    const Color them = ~us;
    const Square from = move.from();
    const Square to = move.to();
    const Piece piece = board_[from];

    State next = current();
    next.move = move;
    next.delta = BoardDelta{};
    ++next.halfmoves;

    if (next.enPassant != NoSquare) {
        next.key ^= enPassantKeys[fileOf(next.enPassant)];
        next.enPassant = NoSquare;
    }

    const auto remove = [&](Square s) {
        const Piece removed = board_[s];
        next.key ^= pieceKeys[removed][s];
        if (typeOf(removed) == Pawn)
            next.pawnKey ^= pieceKeys[removed][s];
        next.delta.removed[next.delta.removedCount++] = {removed, s};
        lift(s);
    };

    const auto add = [&](Piece added, Square s) {
        next.key ^= pieceKeys[added][s];
        if (typeOf(added) == Pawn)
            next.pawnKey ^= pieceKeys[added][s];
        next.delta.added[next.delta.addedCount++] = {added, s};
        place(added, s);
    };

    if (move.flag() == MoveFlag::Castle) {
        const bool kingside = to > from;
        const Square rookFrom = kingside ? Square(to + 1) : Square(to - 2);
        const Square rookTo = kingside ? Square(to - 1) : Square(to + 1);

        remove(from);
        remove(rookFrom);
        add(piece, to);
        add(makePiece(us, Rook), rookTo);
    } else {
        const Square victim = move.flag() == MoveFlag::EnPassant ? Square(to ^ 8) : to;

        if (board_[victim] != NoPiece) {
            remove(victim);
            next.halfmoves = 0;
        }

        remove(from);
        add(move.isPromotion() ? makePiece(us, move.promotion()) : piece, to);

        if (typeOf(piece) == Pawn) {
            next.halfmoves = 0;

            if (move.flag() == MoveFlag::DoublePush) {
                const Square skipped = Square((from + to) / 2);
                if (attacks::pawn(us, skipped) & pieces(them, Pawn)) {
                    next.enPassant = skipped;
                    next.key ^= enPassantKeys[fileOf(skipped)];
                }
            }
        }
    }

    next.key ^= castlingKeys[next.castling];
    next.castling &= CastlingKept[from] & CastlingKept[to];
    next.key ^= castlingKeys[next.castling];
    next.key ^= sideKey;

    side_ = them;
    if (us == Black)
        ++fullmove_;

    states_.push_back(next);
    updateCheckInfo();
}

void Position::undoMove() {
    const BoardDelta& delta = current().delta;

    for (int i = delta.addedCount - 1; i >= 0; --i)
        lift(delta.added[i].square);

    for (int i = delta.removedCount - 1; i >= 0; --i)
        place(delta.removed[i].piece, delta.removed[i].square);

    side_ = ~side_;
    if (side_ == Black)
        --fullmove_;

    states_.pop_back();
}

void Position::makeNullMove() {
    State next = current();
    next.move = Move::null();
    next.delta = BoardDelta{};
    ++next.halfmoves;

    if (next.enPassant != NoSquare) {
        next.key ^= enPassantKeys[fileOf(next.enPassant)];
        next.enPassant = NoSquare;
    }

    next.key ^= sideKey;
    side_ = ~side_;
    states_.push_back(next);
    updateCheckInfo();
}

void Position::undoNullMove() {
    side_ = ~side_;
    states_.pop_back();
}

Bitboard Position::attackersTo(Square s, Bitboard occupancy) const {
    return (attacks::pawn(Black, s) & pieces(White, Pawn))
         | (attacks::pawn(White, s) & pieces(Black, Pawn))
         | (attacks::knight(s) & pieces(Knight))
         | (attacks::bishop(s, occupancy) & (pieces(Bishop) | pieces(Queen)))
         | (attacks::rook(s, occupancy) & (pieces(Rook) | pieces(Queen)))
         | (attacks::king(s) & pieces(King));
}

bool Position::attacked(Square s, Color by, Bitboard occupancy) const {
    return (attackersTo(s, occupancy) & pieces(by)) != 0;
}

bool Position::canCastle(int right) const {
    if (!(castlingRights() & right) || inCheck())
        return false;

    const bool white = (right & (WhiteShort | WhiteLong)) != 0;
    if (white != (side_ == White))
        return false;

    const bool kingside = (right & (WhiteShort | BlackShort)) != 0;
    const Square king = fromPerspective(side_, E1);
    const Square rook = fromPerspective(side_, kingside ? H1 : A1);

    if (board_[rook] != makePiece(side_, Rook) || (attacks::between(king, rook) & occupied()))
        return false;

    const Square crossed = fromPerspective(side_, kingside ? F1 : D1);
    const Square landing = fromPerspective(side_, kingside ? G1 : C1);

    return !attacked(crossed, ~side_, occupied()) && !attacked(landing, ~side_, occupied());
}

bool Position::pawnMoveFits(Move move) const {
    const Square from = move.from();
    const Square to = move.to();
    const int step = side_ == White ? 8 : -8;
    const bool reachesEnd = rankOf(fromPerspective(side_, to)) == 7;

    if (move.isPromotion() != reachesEnd)
        return false;

    if (move.flag() == MoveFlag::EnPassant)
        return to == enPassant() && has(attacks::pawn(side_, from), to);

    if (has(attacks::pawn(side_, from), to))
        return has(pieces(~side_), to) && move.flag() != MoveFlag::DoublePush;

    if (has(occupied(), to))
        return false;

    if (to - from == step)
        return move.flag() != MoveFlag::DoublePush;

    return to - from == 2 * step && move.flag() == MoveFlag::DoublePush
        && rankOf(fromPerspective(side_, from)) == 1 && !has(occupied(), Square(from + step));
}

bool Position::isLegal(Move move) const {
    if (!move || move == Move::null() || move.flag() > MoveFlag::PromoteQueen)
        return false;

    const Square from = move.from();
    const Square to = move.to();
    const Piece piece = board_[from];

    if (piece == NoPiece || colorOf(piece) != side_ || has(pieces(side_), to))
        return false;

    const PieceType type = typeOf(piece);
    const Square king = kingSquare(side_);

    if (move.flag() == MoveFlag::Castle) {
        if (type != King || from != fromPerspective(side_, E1))
            return false;
        if (to == fromPerspective(side_, G1))
            return canCastle(side_ == White ? WhiteShort : BlackShort);
        if (to == fromPerspective(side_, C1))
            return canCastle(side_ == White ? WhiteLong : BlackLong);
        return false;
    }

    if (type == Pawn) {
        if (!pawnMoveFits(move))
            return false;
    } else if (move.flag() != MoveFlag::Normal || !has(attacks::of(type, from, occupied()), to)) {
        return false;
    }

    if (type == King)
        return !attacked(to, ~side_, occupied() ^ bit(from));

    if (move.flag() == MoveFlag::EnPassant) {
        const Square victim = Square(to ^ 8);
        const Bitboard after = (occupied() ^ bit(from) ^ bit(victim)) | bit(to);
        return !(attackersTo(king, after) & pieces(~side_) & ~bit(victim));
    }

    const Bitboard threats = checkers();

    if (several(threats))
        return false;

    if (threats && !has(attacks::between(king, lsb(threats)) | threats, to))
        return false;

    return !has(pinned(), from) || has(attacks::line(king, from), to);
}

bool Position::isCapture(Move move) const {
    return move.flag() == MoveFlag::EnPassant
        || (board_[move.to()] != NoPiece && move.flag() != MoveFlag::Castle);
}

bool Position::isTactical(Move move) const {
    return isCapture(move) || move.flag() == MoveFlag::PromoteQueen;
}

PieceType Position::capturedType(Move move) const {
    if (move.flag() == MoveFlag::EnPassant)
        return Pawn;
    return isCapture(move) ? typeOf(board_[move.to()]) : King;
}

int Position::exchange(Move move) const {
    if (move.flag() == MoveFlag::Castle)
        return 0;

    const Square to = move.to();
    std::array<int, 32> gain{};
    int depth = 0;

    gain[0] = isCapture(move) ? MaterialValue[capturedType(move)] : 0;
    int standing = MaterialValue[typeOf(board_[move.from()])];

    if (move.isPromotion()) {
        gain[0] += MaterialValue[move.promotion()] - MaterialValue[Pawn];
        standing = MaterialValue[move.promotion()];
    }

    Bitboard occupancy = occupied() ^ bit(move.from());
    if (move.flag() == MoveFlag::EnPassant)
        occupancy ^= bit(Square(to ^ 8));

    Color turn = ~side_;

    while (depth < 31) {
        const Bitboard attackers = attackersTo(to, occupancy) & occupancy;
        const Bitboard mine = attackers & pieces(turn);

        if (!mine)
            break;

        PieceType cheapest = Pawn;
        while (!(mine & pieces(cheapest)))
            cheapest = PieceType(cheapest + 1);

        if (cheapest == King && (attackers & pieces(~turn)))
            break;

        ++depth;
        gain[depth] = standing - gain[depth - 1];
        standing = MaterialValue[cheapest];
        occupancy ^= bit(lsb(mine & pieces(cheapest)));
        turn = ~turn;
    }

    for (; depth > 0; --depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}

bool Position::hasNonPawnMaterial(Color c) const {
    return (pieces(c) & ~(pieces(Pawn) | pieces(King))) != 0;
}

bool Position::isDraw(int searchPly) const {
    const State& state = current();

    if (state.halfmoves >= 100 && !state.checkers)
        return true;

    if (!(pieces(Pawn) | pieces(Rook) | pieces(Queen)) && popcount(occupied()) <= 3)
        return true;

    const int reach = std::min(state.halfmoves, int(states_.size()) - 1);
    int repeats = 0;

    for (int back = 4; back <= reach; back += 2) {
        if (states_[states_.size() - 1 - back].key != state.key)
            continue;
        if (back <= searchPly || ++repeats == 2)
            return true;
    }

    return false;
}

}
