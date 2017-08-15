// Disable assertions unless the player is explicitly compiled with -DDEBUG
#ifndef DEBUG
#define NDEBUG 1
#endif

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using std::string;
using std::vector;

const int SIZE = 8;
const int NUM_FIELDS = 36;
const int INITIAL_STONES = 5;
const int MAX_VALUE = 15;

int max_search_depth = 4;

const int neighbours[36][7] = {
  {1,8,-1},                //  0: A1 -> A2,B1
  {0,2,8,9,-1},            //  1: A2 -> A1,A3,B1,B2
  {1,3,9,10,-1},           //  2: A3 -> A2,A4,B2,B3
  {2,4,10,11,-1},          //  3: A4 -> A3,A5,B3,B4
  {3,5,11,12,-1},          //  4: A5 -> A4,A6,B4,B5
  {4,6,12,13,-1},          //  5: A6 -> A5,A7,B5,B6
  {5,7,13,14,-1},          //  6: A7 -> A6,A8,B6,B7
  {6,14,-1},               //  7: A8 -> A7,B7
  {0,1,9,15,-1},           //  8: B1 -> A1,A2,B2,C1
  {1,2,8,10,15,16,-1},     //  9: B2 -> A2,A3,B1,B3,C1,C2
  {2,3,9,11,16,17,-1},     // 10: B3 -> A3,A4,B2,B4,C2,C3
  {3,4,10,12,17,18,-1},    // 11: B4 -> A4,A5,B3,B5,C3,C4
  {4,5,11,13,18,19,-1},    // 12: B5 -> A5,A6,B4,B6,C4,C5
  {5,6,12,14,19,20,-1},    // 13: B6 -> A6,A7,B5,B7,C5,C6
  {6,7,13,20,-1},          // 14: B7 -> A7,A8,B6,C6
  {8,9,16,21,-1},          // 15: C1 -> B1,B2,C2,D1
  {9,10,15,17,21,22,-1},   // 16: C2 -> B2,B3,C1,C3,D1,D2
  {10,11,16,18,22,23,-1},  // 17: C3 -> B3,B4,C2,C4,D2,D3
  {11,12,17,19,23,24,-1},  // 18: C4 -> B4,B5,C3,C5,D3,D4
  {12,13,18,20,24,25,-1},  // 19: C5 -> B5,B6,C4,C6,D4,D5
  {13,14,19,25,-1},        // 20: C6 -> B6,B7,C5,D5
  {15,16,22,26,-1},        // 21: D1 -> C1,C2,D2,E1
  {16,17,21,23,26,27,-1},  // 22: D2 -> C2,C3,D1,D3,E1,E2
  {17,18,22,24,27,28,-1},  // 23: D3 -> C3,C4,D2,D4,E2,E3
  {18,19,23,25,28,29,-1},  // 24: D4 -> C4,C5,D3,D5,E3,E4
  {19,20,24,29,-1},        // 25: D5 -> C5,C6,D4,E4
  {21,22,27,30,-1},        // 26: E1 -> D1,D2,E2,F1
  {22,23,26,28,30,31,-1},  // 27: E2 -> D2,D3,E1,E3,F1,F2
  {23,24,27,29,31,32,-1},  // 28: E3 -> D3,D4,E2,E4,F2,F3
  {24,25,28,32,-1},        // 29: E4 -> D4,D5,E3,F3
  {26,27,31,33,-1},        // 30: F1 -> E1,E2,F2,G1
  {27,28,30,32,33,34,-1},  // 31: F2 -> E2,E3,F1,F3,G1,G2
  {28,29,31,34,-1},        // 32: F3 -> E3,E4,F2,G2
  {30,31,34,35,-1},        // 33: G1 -> F1,F2,G2,H1
  {31,32,33,35,-1},        // 34: G2 -> F2,F3,G1,H1
  {33,34,-1}};             // 35: H1 -> G1,G2

struct State {
  int moves_played = 0;  // excludes initial stones!
  bool used[2][MAX_VALUE + 1] = {};
  bool occupied[NUM_FIELDS] = {};
  int value[NUM_FIELDS] = {};  // is this even used for anything?
  int score[NUM_FIELDS] = {};
};

struct Move {
  int field;
  int value;
};

__attribute__((noreturn))
void CheckFail(int line, const char *func, const char *expr) {
  fprintf(stderr, "[%s:%d] %s(): CHECK(%s) failed\n", __FILE__, line, func, expr);
  // TODO: print a stack trace?
  // TODO: dump global state?
  abort();
}

#define CHECK(expr) \
    do { if (!(expr)) CheckFail(__LINE__, __func__, #expr); } while(0)
#define CHECK_EQ(x, y) CHECK((x) == (y))

__attribute__((__format__ (__printf__, 1, 2)))
string Sprintf(const char *fmt, ...) {
  string result;

  va_list ap;
  va_start(ap, fmt);
  int size = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  CHECK(size >= 0);

  result.resize(size + 1);

  va_start(ap, fmt);
  size = vsnprintf(&result[0], result.size(), fmt, ap);
  va_end(ap);

  CHECK(size == static_cast<int>(result.size()) - 1);
  result.resize(size);
  return result;
}

const char *FormatMove(const Move &move) {
  static char buf[32];
  int u = 0, v = move.field;
  while (v >= SIZE - u) {
    v -= SIZE - u;
    ++u;
    CHECK(u < SIZE);
  }
  int n = snprintf(buf, sizeof(buf), "%c%d=%d", 'A' + u, v + 1, move.value);
  CHECK(n > 0 && n < static_cast<int>(sizeof(buf)));
  return buf;
}

void MakeHole(State &state, int field) {
  CHECK(!state.occupied[field]);
  state.occupied[field] = true;
}

void DoMove(State &state, const Move &move) {
  assert(!state.occupied[move.field]);
  state.occupied[move.field] = true;
  int player = state.moves_played & 1;
  assert(!state.used[player][move.value]);
  state.used[player][move.value] = true;
  int v = player == 0 ? move.value : -move.value;
  state.value[move.field] = v;
  for (int i : neighbours[move.field]) {
    if (i < 0) break;
    state.score[i] += v;
  }
  ++state.moves_played;
}

void UndoMove(State &state, const Move &move) {
  assert(state.moves_played > 0);
  --state.moves_played;
  int player = state.moves_played & 1;
  int v = player == 0 ? move.value : -move.value;
  for (int i : neighbours[move.field]) {
    if (i < 0) break;
    state.score[i] -= v;
  }
  assert(state.value[move.field] == v);
  state.value[move.field] = 0;
  assert(state.used[player][move.value]);
  state.used[player][move.value] = false;
  assert(state.occupied[move.field]);
  state.occupied[move.field] = false;
}

int DecodeBase36Char(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
  return -1;
}

/*
char EncodeBase36Char(int i) {
  if (i >= 0 && i < 10) return '0' + i;
  if (i >= 10 && i < 36) return 'a' + i - 10;
  return '?';
}
*/

const char *ReadNextLine() {
  static char buf[100];
  if (fgets(buf, sizeof(buf), stdin) == nullptr) {
    fprintf(stderr, "EOF reached!\n");
    return nullptr;
  }
  char *eol = strchr(buf, '\n');
  if (eol == nullptr) {
    fprintf(stderr, "EOL not found!\n");
    return nullptr;
  }
  CHECK(eol[1] == '\0');
  *eol = '\0';
  if (strcmp(buf, "Quit") == 0) {
    fprintf(stderr, "Quit received.\n");
    return nullptr;
  }
  return buf;
}

bool IsValidState(const State &state, const vector<Move> &history, string *reason) {
#define EXPECT(x, ...) if (!(x)) { if (reason) *reason = Sprintf(__VA_ARGS__); return false; }
  int initial_stones = 0;
  int red_stones = 0;
  int blue_stones = 0;
  for (int field = 0; field < NUM_FIELDS; ++field) {
    int v = state.value[field];
    if (state.occupied[field]) {
      if (v > 0) {
        ++red_stones;
        EXPECT(state.used[0][v], "unused red value field=%d v=%d", field, v);
      } else if (v < 0) {
        EXPECT(state.used[1][-v], "unused blue value field=%d v=%d", field, -v);
        ++blue_stones;
      } else {
        ++initial_stones;
      }
    } else {
      EXPECT(v == 0, "unoccupied field=%d v=%d", field, v);
    }
  }
  EXPECT(initial_stones == INITIAL_STONES, "initial_stones=%d", initial_stones);
  // Note: moves_played does not count placement of the initial stones.
  EXPECT(red_stones + blue_stones == state.moves_played,
      "red_stones=%d + blue_stones=%d != moves_played=%d",
      red_stones, blue_stones, state.moves_played);
  EXPECT(red_stones == blue_stones || red_stones == blue_stones + 1,
      "red_stones=%d blue_stones=%d", red_stones, blue_stones);

  int red_values_used = 0;
  int blue_values_used = 0;
  for (int i = 1; i <= MAX_VALUE; ++i) {
    if (state.used[0][i]) ++red_values_used;
    if (state.used[1][i]) ++blue_values_used;
  }
  EXPECT(red_values_used == red_stones,
      "red_values_used=%d red_stones=%d", red_values_used, red_stones);
  EXPECT(blue_values_used == blue_stones,
      "blue_values_used=%d blue_stones=%d", blue_values_used, blue_stones);

  int history_moves = static_cast<int>(history.size()) - INITIAL_STONES;
  EXPECT(state.moves_played == history_moves,
      "state.moves_played=%d history_moves=%d",
      state.moves_played, history_moves);
  for (int i = 0; i < static_cast<int>(history.size()); ++i) {
    int field = history[i].field;
    int value = history[i].value;
    EXPECT(state.occupied[field], "not occupied i=%d field=%d", i, field);
    if (i < INITIAL_STONES) {
      EXPECT(state.value[field] == 0, "not empty i=%d field=%d", i, field);
    } else if (((i - INITIAL_STONES) & 1) == 0) {
      EXPECT(state.used[0][value], "red stone not used i=%d value=%d", i, value);
      EXPECT(state.value[field] == value,
          "invalid red field i=%d field=%d expected value=%d actual value=%d",
          i, field, value, state.value[field]);
    } else {
      EXPECT(state.used[1][value], "blue stone not used i=%d value=%d", i, value);
      EXPECT(state.value[field] == -value,
          "invalid blue field i=%d field=%d expected value=%d actual value=%d",
          i, field, value, state.value[field]);
    }
  }
#undef EXPECT
  return true;
}

template<class T, int N>
void DumpArray(const T (&a)[N], FILE *fp) {
  fputc('{', fp);
  for (int i = 0; i < N; ++i) {
    if (i > 0) fputc(',', fp);
    fprintf(fp, "%d", int{a[i]});
  }
  fputc('}', fp);
}

void DumpState(const State &state, FILE *fp) {
  fprintf(fp, "state.moves_played=%d", state.moves_played);
  for (int i = 0; i < 2; ++i) {
    fprintf(fp, "\nstate.used[%d]=", i);
    DumpArray(state.used[i], fp);
  }
  fputs("\nstate.occupied=", fp);
  DumpArray(state.occupied, fp);
  fputs("\nstate.value=", fp);
  DumpArray(state.value, fp);
  fputs("\nstate.score=", fp);
  DumpArray(state.value, fp);
  fputc('\n', fp);
}

void Validate(const State &state, const vector<Move> history) {
  string reason;
  if (!IsValidState(state, history, &reason)) {
    fprintf(stderr, "State validation failed: %s\n", reason.c_str());
    // TODO: print move history (encoded?)
    DumpState(state, stderr);
    abort();
  }
}

int Evaluate(State &state) {
  int score = 0;
  for (int f = 0; f < NUM_FIELDS; ++f) {
    if (state.occupied[f]) continue;
    score += state.score[f] + (
        state.score[f] > 0 ? +5 :
        state.score[f] < 0 ? -5 : 0);
  }
  return (state.moves_played & 1) == 0 ? score : -score;
}

int Search(State &state, int depth, int lo, int hi, vector<Move> *best_moves) {
  if (depth == 0 || state.moves_played >= 2*MAX_VALUE) {
    assert(!best_moves);
    return Evaluate(state);
  }

  int best_value = INT_MIN;

  // We always play the highest-value piece next.
  int player = state.moves_played & 1;
  Move move = {0, MAX_VALUE};
  while (move.value > 0 && state.used[player][move.value]) --move.value;
  CHECK(move.value > 0);

  for (; move.field < NUM_FIELDS; ++move.field) {
    if (state.occupied[move.field]) continue;
    DoMove(state, move);
    int value = -Search(state, depth - 1, -hi, -lo, nullptr);
    if (best_moves) {
      // Debug-print top-level evaluation.
      fprintf(stderr, "depth=%d move=%s value=%d [%d:%d]\n",
          depth, FormatMove(move), value, lo, hi);
    }
    UndoMove(state, move);
    if (best_moves && value >= best_value) {
      // Collect all maximum-value moves in best_moves. This works even when
      // using alpha-beta pruning, because best_moves is non-null only at the
      // top level where hi = +inf, so best_value is never approximate and
      // beta cut-offs do not occur.
      if (value > best_value) best_moves->clear();
      best_moves->push_back(move);
    }
    if (value > best_value) {
      best_value = value;
      if (best_value > lo) lo = value;
      if (best_value >= hi) break;  // beta cut-off
    }
  }
  return best_value;
}

Move SelectMove(State &state) {
  vector<Move> best_moves;
  int value = Search(state, max_search_depth, -1000, +1000, &best_moves);
  fprintf(stderr, "value: %d\n", value);
  CHECK(!best_moves.empty());
  // TODO: pick a best move at random
  return best_moves[0];
}

int CoordsToFieldIndex(int u, int v) {
  return SIZE*u - (u*(u - 1) >> 1) + v;
}

int ParseField(const char *buf) {
  int u = buf[0] - 'A';
  int v = buf[1] - '1';
  CHECK(u >= 0 && u < SIZE);
  CHECK(v >= 0 && v < SIZE);
  CHECK(u + v < SIZE);
  return CoordsToFieldIndex(u, v);
}

Move ParseMove(const char *line) {
  int field = ParseField(line);
  CHECK(line[2] == '=');
  int value = atoi(line + 3);
  CHECK(value > 0);
  CHECK(value <= MAX_VALUE);
  return Move{field, value};
}

void WriteMove(const Move &move) {
  fprintf(stdout, "%s\n", FormatMove(move));
  fflush(stdout);
}

State GetState(const vector<Move> &history) {
  State state;
  for (const Move &move : history) {
    if (move.value == 0) {
      MakeHole(state, move.field);
    } else {
      DoMove(state, move);
    }
  }
  Validate(state, history);
  return state;
}

void RunGame(vector<Move> history) {
  State state = GetState(history);
  const char *line = ReadNextLine();
  if (line == nullptr) return;
  int my_player = 1;
  if (strcmp(line, "Start") == 0) {
    my_player = 0;
    line = nullptr;
  }
  for (;;) {
    Validate(state, history);
    Move move;
    int player = state.moves_played & 1;
    if (player == my_player) {
      move = SelectMove(state);
      WriteMove(move);
    } else {
      if (line == nullptr) {
        line = ReadNextLine();
        if (line == nullptr) return;
      }
      move = ParseMove(line);
      line = nullptr;
    }
    history.push_back(move);
    DoMove(state, move);
  }
}

vector<Move> ReadInitialStones() {
  vector<Move> result;
  vector<Move> moves;
  State state;
  for (int i = 0; i < INITIAL_STONES; ++i) {
    const char *line = ReadNextLine();
    if (line == nullptr) return result;
    int field = ParseField(line);
    if (field < 0 || field >= NUM_FIELDS || state.occupied[field]) return result;
    MakeHole(state, field);
    moves.push_back(Move{field, 0});
  }
  result.swap(moves);
  return result;
}

vector<Move> DecodeStateString(const char *str) {
  vector<Move> result;
  vector<Move> moves;
  State state;
  int len = strlen(str);
  if (len < 2*INITIAL_STONES ||
      len > 2*(INITIAL_STONES + 2*MAX_VALUE) ||
      len%2 != 0) {
    return result;
  }
  for (int i = 0; i < len; i += 2) {
    int field = DecodeBase36Char(str[i + 0]);
    int value = DecodeBase36Char(str[i + 1]);
    if (state.occupied[field]) {
      return result;
    }
    if (i < 2*INITIAL_STONES) {
      if (value != 0) return result;
      MakeHole(state, field);
      moves.push_back(Move{field, 0});
    } else {
      int player = ((i - INITIAL_STONES) >> 1) & 1;
      value -= player*MAX_VALUE;
      if (value < 1 || value > MAX_VALUE) return result;
      if (state.used[player][value]) return result;
      DoMove(state, Move{field, value});
      moves.push_back(Move{field, value});
    }
  }
  result.swap(moves);
  return result;
}

}  // namespace

int main(int argc, char *argv[]) {
  vector<Move> history;
  for (int i = 1; i < argc; ++i) {
    vector<Move> moves = DecodeStateString(argv[i]);
    if (!moves.empty()) {
      CHECK(history.empty());
      history.swap(moves);
      continue;
    }
    fprintf(stderr, "Ignored argument %d: [%s]\n", i, argv[i]);
  }
  if (history.empty()) {
    history = ReadInitialStones();
    if (history.empty()) {
      fprintf(stderr, "Failed to read initial stones!\n");
      return 1;
    }
  }
  RunGame(std::move(history));
  fprintf(stderr, "Exiting.\n");
  return 0;
}
