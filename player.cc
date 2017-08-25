// Disable assertions unless the player is explicitly compiled with -DDEBUG
#ifndef DEBUG
#define NDEBUG 1
#endif

// Increment this before and after uploading a version to the CodeCup server.
// Even numbers are released players. Odd numbers are development versions.
#define PLAYER_VERSION 1

#include <assert.h>
#include <execinfo.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <numeric>
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
const int MAX_MOVES = 2*MAX_VALUE;

int max_search_depth = 6;
bool enable_move_ordering = false;

vector<long long> counter_search;

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

State *debug_state;

struct DebugStateSwapper {
  DebugStateSwapper(State &new_state) : old_state(debug_state) {
    debug_state = &new_state;
  }
  ~DebugStateSwapper() { debug_state = old_state; }
private:
  DebugStateSwapper(const DebugStateSwapper&) = delete;
  DebugStateSwapper &operator=(const DebugStateSwapper&) = delete;

  State *old_state;
};

#define DebugStateSwapper(arg) MISUSE

template <class T, size_t N>
constexpr size_t ArraySize(T(&)[N]) { return N; }

void DumpState(const State &state, FILE *fp);

__attribute__((noreturn))
void CheckFail(int line, const char *func, const char *expr) {
  fprintf(stderr, "[%s:%d] %s(): CHECK(%s) failed\n", __FILE__, line, func, expr);

  // Print stack trace. Typically not very useful, unfortunately, since all
  // functions are declared in the anonymous namespace.
  void *symbols_buffer[100];
  int symbols_size = backtrace(symbols_buffer, ArraySize(symbols_buffer));
  backtrace_symbols_fd(symbols_buffer, symbols_size, 2);

  // Print the global game state, if we have it.
  if (debug_state) DumpState(*debug_state, stderr);

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

const int64_t NANOS_PER_SECOND = 1000000000;

int64_t GetWallTimeNanos() {
  struct timespec tv;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &tv) == 0);
  return NANOS_PER_SECOND*tv.tv_sec + tv.tv_nsec;
}

int64_t GetCpuTimeNanos() {
  struct timespec tv;
  CHECK(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tv) == 0);
  return NANOS_PER_SECOND*tv.tv_sec + tv.tv_nsec;
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

inline bool IsGameOver(const State &state) {
  return state.moves_played >= MAX_MOVES;
}

// Returns the next player to move: 0 for red, 1 for blue.
inline int GetNextPlayer(const State &state) {
  return state.moves_played & 1;
}

bool IsValidMove(const State &state, const Move &move) {
  const int player = GetNextPlayer(state);
  return
      move.field >= 0 && move.field < NUM_FIELDS && !state.occupied[move.field] &&
      move.value >= 1 && move.value <= MAX_VALUE && !state.used[player][move.value];
}

void DoMove(State &state, const Move &move) {
  assert(IsValidMove(state, move));
  state.occupied[move.field] = true;
  const int player = GetNextPlayer(state);
  state.used[player][move.value] = true;
  int v = player == 0 ? move.value : -move.value;
  state.value[move.field] = v;
  const int *ip = neighbours[move.field];
  for (int i; (i = *ip) >= 0; ++ip) state.score[i] += v;
  ++state.moves_played;
}

void UndoMove(State &state, const Move &move) {
  assert(state.moves_played > 0);
  --state.moves_played;
  const int player = GetNextPlayer(state);
  int v = player == 0 ? move.value : -move.value;
  const int *ip = neighbours[move.field];
  for (int i; (i = *ip) >= 0; ++ip) state.score[i] -= v;
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

void Validate(const State &state, const vector<Move> &history) {
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
  return GetNextPlayer(state) == 0 ? score : -score;
}

// Negamax depth-first search with alpha-beta pruning.
//
// If the result is in [lo,hi] (excluding the boundaries), the value is exact.
// If the result is less than or equal to lo, or greater than or equal to hi,
// then it is an upper or lower bound on the true value, respectively.
int Search(State &state, int depth, int lo, int hi, Move *best_move,
    const vector<int> &fields_to_search) {
  assert(lo < hi);  // invariant maintained throughout this function

  // TODO: disable this in non-debug mode?
  ++counter_search.at(depth);

  if (depth == 0) {
    assert(!best_move);
    return Evaluate(state);
  }

  assert(!IsGameOver(state));  // caller should make sure depth is limited

  int best_value = INT_MIN;

  // We always play the highest-value piece next.
  const int player = GetNextPlayer(state);
  Move move = {0, MAX_VALUE};
  while (move.value > 0 && state.used[player][move.value]) --move.value;
  CHECK(move.value > 0);

  const bool debug_print = best_move != nullptr;
  for (int field : fields_to_search) {
    if (state.occupied[field]) continue;
    move.field = field;
    DoMove(state, move);
    int value = -Search(state, depth - 1, -hi, -lo, nullptr, fields_to_search);
    UndoMove(state, move);
    if (debug_print) fprintf(stderr, " %s:%d", FormatMove(move), value);
    if (value > best_value) {
      best_value = value;
      if (best_move) {
        *best_move = move;
      }
      if (best_value > lo) {
        lo = best_value;
        if (lo >= hi) break;  // beta cut-off
      }
    }
  }
  if (debug_print) fputc('\n', stderr);
  return best_value;
}

// Only search fields that are unoccupied, and order them by decreasing number
// of liberties (i.e. number of unoccupied neighbouring fields). This means the
// best fields appear first, which improves the beta-cutoff rate.
vector<int> CalculateFieldsToSearch(const State &state) {
  vector<int> result;
  int liberties[NUM_FIELDS] = {};
  for (int field = 0; field < NUM_FIELDS; ++field) {
    if (!state.occupied[field]) {
      result.push_back(field);
      const int *fp = neighbours[field];
      for (int f; (f = *fp) >= 0; ++fp) {
        liberties[field] += !state.occupied[f];
      }
    }
  }
  if (enable_move_ordering) {
    std::random_shuffle(result.begin(), result.end());
    std::stable_sort(result.begin(), result.end(), [&liberties](int f, int g) {
      return liberties[f] > liberties[g];
    });
  }
  return result;
}

Move SelectMove(State &state) {
  int64_t cpu_time_nanos = GetCpuTimeNanos();
  int64_t wall_time_nanos = GetWallTimeNanos();

  DebugStateSwapper setter(state);

  int search_depth = std::min(MAX_MOVES - state.moves_played, max_search_depth);
  assert(search_depth > 0);
  counter_search.assign(search_depth + 1, 0LL);

  Move best_move;
  int value = Search(state, search_depth, -1000, +1000, &best_move,
      CalculateFieldsToSearch(state));

  fprintf(stderr, "value: %d best_move: %s evals", value, FormatMove(best_move));
  for (int i = 0; i <= search_depth; ++i) fprintf(stderr, " %lld", counter_search[i]);
  fprintf(stderr, " (%lld total)\n",
      std::accumulate(counter_search.begin(), counter_search.end(), 0LL));
  cpu_time_nanos = GetCpuTimeNanos() - cpu_time_nanos;
  wall_time_nanos = GetWallTimeNanos() - wall_time_nanos;
  fprintf(stderr, " time: %.3lfs cpu %.3lfs wall\n",
      1e-9*cpu_time_nanos, 1e-9*wall_time_nanos);

  CHECK(IsValidMove(state, best_move));
  return best_move;
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

unsigned GetSeed(const vector<Move> &history) {
  CHECK(history.size() >= INITIAL_STONES);
  unsigned seed = 0x811c9dc5;
  for (int i = 0; i < INITIAL_STONES; ++i) {
    assert(history[i].value == 0);
    seed ^= history[i].field;
    seed *= 16777619;
  }
  return seed;
}

void RunGame(vector<Move> history) {
  srand(GetSeed(history));
  State state = GetState(history);
  const char *line = ReadNextLine();
  if (line == nullptr) return;
  int my_player = GetNextPlayer(state);
  if (strcmp(line, "Start") == 0) {
    line = nullptr;
  } else {
    my_player = 1 - my_player;
  }
  while (!IsGameOver(state)) {
    Validate(state, history);
    Move move;
    if (GetNextPlayer(state) == my_player) {
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
  fprintf(stderr, "Game is over.\n");
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
      len > 2*(INITIAL_STONES + MAX_MOVES) ||
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

void PrintPlayerId() {
  fprintf(stderr, "Supernova %d (gcc %s glibc++ %d)",
      PLAYER_VERSION, __VERSION__, __GLIBCXX__);
#if __x86_64__
  fputs(" x86_64", stderr);
#endif
#if __OPTIMIZE__
  fputs(" optimized", stderr);
#endif
#ifdef DEBUG
  fputs(" debug", stderr);
#endif
#ifdef NDEBUG
  fputs(" ndebug", stderr);
  assert(0);
#endif
  fputc('\n', stderr);
}

enum class Mode { PLAY, ANALYZE, BENCHMARK };

struct Args {
  Mode mode = Mode::PLAY;
  vector<Move> transcript;
};

// Usage:
//
//   player [<mode>] [<options>] [<base36-game-state>]
//
// Supported modes:
//
//    play       (default) Play a game.
//    analyze    Analyze a single game state.
//    benchmark  Run benchmark on states read from stdin.
//
// Supported options:
//
//  -d<N> / --max_search_depth=<N>  set the maximum search depth to N
//
// base36-game-state: If given, continue from the given game state, instead of
// starting with an empty board. The state must include at least the initial
// brown stones. When playing a game, the first line of input must be "Start" or
// a valid move, which determines which color the AI plays, as usual.
Args ParseArgs(int argc, char *argv[]) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "analyze") == 0) {
      CHECK(args.mode == Mode::PLAY);
      args.mode = Mode::ANALYZE;
      continue;
    }
    if (strcmp(argv[i], "benchmark") == 0) {
      CHECK(args.mode == Mode::PLAY);
      args.mode = Mode::BENCHMARK;
      continue;
    }
    vector<Move> moves = DecodeStateString(argv[i]);
    if (!moves.empty()) {
      CHECK(args.transcript.empty());
      args.transcript = std::move(moves);
      continue;
    }
    int int_arg = 0;
    if (sscanf(argv[i], "--max_search_depth=%d", &int_arg) == 1 ||
        sscanf(argv[i], "-d%d", &int_arg) == 1) {
      CHECK(int_arg > 0);
      max_search_depth = int_arg;
      continue;
    }
    if (strcmp(argv[i], "-o") == 0) {
      enable_move_ordering = true;
      continue;
    }
    fprintf(stderr, "Ignored argument %d: [%s]\n", i, argv[i]);
  }
  return args;
}

int Main(int argc, char *argv[]) {
  Args args = ParseArgs(argc, argv);
  if (args.mode == Mode::PLAY) {
    PrintPlayerId();
    std::vector<Move> history = args.transcript;
    if (history.empty()) {
      history = ReadInitialStones();
      if (history.empty()) {
        fprintf(stderr, "Failed to read initial stones!\n");
        return 1;
      }
    }
    RunGame(std::move(history));
    fprintf(stderr, "Exiting.\n");
  } else if (args.mode == Mode::ANALYZE) {
    CHECK(!args.transcript.empty());
    State state = GetState(args.transcript);
    Move move = SelectMove(state);
    fprintf(stderr, "Best move: %s\n", FormatMove(move));
  } else if (args.mode == Mode::BENCHMARK) {
    char line[1024];
    vector<long long> total_search(MAX_MOVES + 1);
    while (fgets(line, sizeof(line), stdin) != NULL) {
      char *nl = strchr(line, '\n');
      CHECK(nl != NULL);
      *nl = '\0';
      vector<Move> moves = DecodeStateString(line);
      if (moves.empty()) {
        fprintf(stderr, "Invalid state string: [%s]\n", line);
        return 1;
      }
      State state = GetState(moves);
      SelectMove(state);
      for (size_t i = 0; i < counter_search.size(); ++i) total_search[i] += counter_search[i];
    }
    for (int i = 0; i <= MAX_MOVES && total_search[i]; ++i) fprintf(stderr, "%lld ", total_search[i]);
    fprintf(stderr, "(total: %lld)\n", std::accumulate(total_search.begin(), total_search.end(), 0LL));
  }
  return 0;
}

}  // namespace

int main(int argc, char *argv[]) { return Main(argc, argv); }
