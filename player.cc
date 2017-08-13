#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace {

using std::string;
using std::vector;

const int SIZE = 8;
const int NUM_FIELDS = 36;
const int INITIAL_STONES = 5;
const int MAX_VALUE = 15;

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
  int player = 0;
  bool used[2][MAX_VALUE + 1] = {};
  bool occupied[NUM_FIELDS] = {};
  int value[NUM_FIELDS] = {};  // is this even used for anything?
  int score[NUM_FIELDS] = {};
};

struct Move {
  int field;
  int value;
};

void CheckFail(int line, const char *func, const char *expr) {
  fprintf(stderr, "[%s:%d] %s(): CHECK(%s) failed\n", __FILE__, line, func, expr);
  // TODO: print a stack trace
  abort();
}

#define CHECK(expr) \
  do { if (!(expr)) CheckFail(__LINE__, __func__, #expr); } while(0)

#ifdef DEBUG
  #define DCHECK(expr) CHECK(expr)
#else
  #define DCHECK(expr)
#endif

void DoMove(State &state, const Move &move) {
  DCHECK(!state.occupied[move.field]);
  DCHECK(!state.used[state.player][move.value]);
  state.occupied[move.field] = true;
  state.used[state.player][move.value] = true;
  int v = state.player == 0 ? move.value : -move.value;
  state.value[move.field] = v;
  for (int i : neighbours[move.field]) {
    state.score[i] += v;
  }
  state.player = 1 - state.player;
}

void UndoMove(State &state, const Move &move) {
  DCHECK(state.occupied[move.field]);
  DCHECK(state.used[state.player][move.value]);
  state.player = 1 - state.player;
  int v = state.player == 0 ? move.value : -move.value;
  for (int i : neighbours[move.field]) {
    state.score[i] -= v;
  }
  state.value[move.field] = v;
  state.used[state.player][move.value] = false;
  state.occupied[move.field] = false;
}

/*
char EncodeBase36Char(int i) {
  if (i >= 0 && i < 10) return '0' + i;
  if (i >= 10 && i < 36) return 'a' + i - 10;
  return '?';
}

int DecodeBase36Char(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
  return -1;
}
*/

const char *ReadNextLine() {
  static char buf[100];
  if (fgets(buf, sizeof(buf), stdin) == NULL) {
    fprintf(stderr, "EOF reached!\n");
    return NULL;
  }
  char *eol = strchr(buf, '\n');
  if (eol == NULL) {
    fprintf(stderr, "EOL not found!\n");
    return NULL;
  }
  CHECK(eol[1] == '\0');
  *eol = '\0';
  if (strcmp(buf, "Quit") == 0) {
    fprintf(stderr, "Quit received.\n");
    return NULL;
  }
  return buf;
}

Move SelectMove(State &state) {
  int value = MAX_VALUE;
  while (value > 0 && state.used[state.player][value]) --value;
  CHECK(value > 0);
  int field = 0;
  while (field < NUM_FIELDS && state.occupied[field]) ++field;
  CHECK(field < NUM_FIELDS);
  return Move{field, value};
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
  int u = 0, v = move.field;
  while (v >= SIZE - u) {
    v -= SIZE - u;
    ++u;
    CHECK(u < SIZE);
  }
  fprintf(stdout, "%c%d=%d\n", 'A' + u, v + 1, move.value);
  fflush(stdout);
}

void Main() {
  State state;
  const char *line;
  for (int i = 0; i < INITIAL_STONES; ++i) {
    if ((line = ReadNextLine()) == NULL) return;
    int fieldIndex = ParseField(line);
    CHECK(!state.occupied[fieldIndex]);
    state.occupied[fieldIndex] = true;
  }
  if ((line = ReadNextLine()) == NULL) return;
  int my_player;
  if (strcmp(line, "Start") == 0) {
    my_player = 0;
  } else {
    my_player = 1;
    DoMove(state, ParseMove(line));
  }
  for (;;) {
    Move move;
    if (state.player == my_player) {
      move = SelectMove(state);
      WriteMove(move);
    } else {
      if ((line = ReadNextLine()) == NULL) return;
      move = ParseMove(line);
    }
    DoMove(state, move);
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    fprintf(stderr, "Ignored argument %d: [%s]\n", i, argv[i]);
  }
  Main();
  fprintf(stderr, "Exiting.\n");
  return 0;
}
