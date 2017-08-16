#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

namespace {

const int SIZE = 8;
const int NUM_FIELDS = 36;
const int INITIAL_STONES = 5;
const int MAX_VALUE = 15;

enum class Color { NONE = 0, BROWN, RED, BLUE };

struct Field {
  Color color = Color::NONE;
  int value = 0;
};

struct State {
  Field fields[NUM_FIELDS];
  int turns = 0;
  bool used[2][MAX_VALUE] = {};
};

struct Move {
  Color color = Color::NONE;
  int field = 0;
  int value = 0;
};

struct Player {
  const int fd_in;
  const int fd_out;
  const pid_t pid;
};

int CoordsToFieldIndex(int u, int v) {
  return SIZE*u - u*(u - 1)/2 + v;
}

bool AreCoordsValid(int u, int v) {
  return 0 <= u && u < SIZE && 0 <= v && u + v < SIZE;
}

int CalculateScore(const State &state) {
  int score = 0;
  for (int u1 = 0; u1 < SIZE; ++u1) {
    for (int v1 = 0; v1 < SIZE - u1; ++v1) {
      const int i = CoordsToFieldIndex(u1, v1);
      if (state.fields[i].color == Color::NONE) {
        for (int du = -1; du <= 1; ++du) {
          for (int dv = -1; dv <= 1; ++dv) {
            if ((du != 0 || dv != 0) && abs(du + dv) <= 1) {
              const int u2 = u1 + du;
              const int v2 = v1 + dv;
              if (AreCoordsValid(u2, v2)) {
                const int j = CoordsToFieldIndex(u2, v2);
                if (state.fields[j].color == Color::RED) {
                  score += state.fields[j].value;
                } else if (state.fields[j].color == Color::BLUE) {
                  score -= state.fields[j].value;
                }
              }
            }
          }
        }
      }
    }
  }
  return score;
}

std::string ReadLine(Player &player) {
  // In theory, the player is allowed to write less or more than one line at a
  // time. However, we currently don't support that. We expect to read exactly
  // one line at a time; nothing more, nothing less.
  char buf[1024];
  ssize_t n = read(player.fd_out, buf, sizeof(buf));
  std::string line;
  if (n < 0) {
    perror("read()");
    return line;
  }
  if (n == 0) {
    fprintf(stderr, "End of file reached!\n");
    return line;
  }
  char *p = reinterpret_cast<char*>(memchr(buf, '\n', n));
  if (p == NULL) {
    fprintf(stderr, "End of line not found!\n");
    return line;
  }
  if (p != buf + n - 1) {
    fprintf(stderr, "Extra data after end of line found!\n");
    return line;
  }
  line.assign(buf, n - 1);
  return line;
}

// Returns a copy of `s` with all non-ASCII characters escaped, using C-style
// escapes (e.g. "\x09" == tab).
std::string EscapeString(const std::string &s) {
  std::string t;
  t.reserve(s.size() + 2);
  t += '"';
  static const char hexdigits[] = "0123456789abcdef";
  for (char ch : s) {
    if (ch >= 32 && ch <= 126) {
      if (ch == '\\' || ch == '"') {
        t += '\\';
      }
      t += ch;
    } else {
      t += "\\x";
      t += hexdigits[(ch & 0xf0) >> 4];
      t += hexdigits[(ch & 0x0f) >> 0];
    }
  }
  t += '"';
  return t;
}

bool Write(Player &player, std::string s) {
  // Temporarily ignore SIGPIPE to avoid aborting the process if the write
  // fails. This makes this function non-thread-safe!
  void (*old_handler)(int) = signal(SIGPIPE, SIG_IGN);
  ssize_t size_written = write(player.fd_in, s.data(), s.size());
  signal(SIGPIPE, old_handler);
  if (size_written != static_cast<ssize_t>(s.size())) {
    fprintf(stderr, "Write %s failed!\n", EscapeString(s).c_str());
    return false;
  }
  return true;
}

void Quit(Player &player) {
  Write(player, "Quit\n");
  close(player.fd_in);
  int status = 0;
  if (waitpid(player.pid, &status, 0) != player.pid) {
    perror("waitpid");
  } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "Player did not exit normally! status=%d\n", status);
  }
}

Player SpawnPlayer(const char *command) {
  int pipe_in[2];
  int pipe_out[2];
  if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0) {
    perror("pipe()");
    exit(1);
  }
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork()");
    exit(1);
  }
  if (pid == 0) {
    // Child process.
    if (dup2(pipe_in[0], 0) != 0 || dup2(pipe_out[1], 1) != 1) {
      perror("dup2()");
      exit(1);
    }
    if (close(pipe_in[0]) != 0 || close(pipe_in[1]) != 0 ||
        close(pipe_out[0]) != 0 || close(pipe_out[1]) != 0) {
      perror("close()");
      exit(1);
    }
    execl("/bin/sh", "/bin/sh", "-c", command, NULL);
    perror("exec");
    exit(1);
  } else {
    // Parent process.
    if (close(pipe_in[0]) != 0 || close(pipe_out[1]) != 0) {
      perror("close()");
      exit(1);
    }
    Player player = { pipe_in[1], pipe_out[0], pid };
    return player;
  }
}

struct PlayerQuitter {
  PlayerQuitter(Player &player) : player(player) {}
  ~PlayerQuitter() { Quit(player); }

private:
  PlayerQuitter(const PlayerQuitter&) = delete;
  void operator=(const PlayerQuitter&) = delete;

  Player &player;
};

uint64_t GetRandomSeed() {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    perror("open(\"/dev/urandom\")");
    exit(1);
  }
  uint64_t seed = 0;
  if (read(fd, &seed, sizeof(seed)) != sizeof(seed)) {
    perror("read()");
    exit(1);
  }
  close(fd);
  return seed;
}

static std::pair<int, int> IndexToCoords(int v) {
  int u = 0;
  int n = SIZE;
  while (v >= n) {
    v -= n;
    u += 1;
    n -= 1;
  }
  return {u, v};
}

std::string FieldToString(int i) {
  assert(i >= 0 && i < NUM_FIELDS);
  std::pair<int, int> coords = IndexToCoords(i);
  std::string s(2, '\0');
  s[0] = 'A' + coords.first;
  s[1] = '1' + coords.second;
  return s;
}

bool ValidateCoords(int u, int v) {
  return 0 <= u && u < SIZE && 0 <= v && u + v < SIZE;
}

int CoordsToIndex(int u, int v) {
  return SIZE*u - u*(u - 1)/2 + v;
}

bool ParseMove(const std::string &s, int *field_out, int *value_out) {
  if (s.size() < 4 || s.size() > 5) return false;
  int u = s[0] - 'A';
  int v = s[1] - '1';
  if (!ValidateCoords(u, v)) return false;
  if (s[2] != '=') return false;
  if (s[3] < '0' || s[3] > '9') return false;
  int value = s[3] - '0';
  if (s.size() > 4) {
    if (s[4] < '0' || s[4] > '9') return false;
    value = 10*value + (s[4] - '0');
  }
  if (value < 1 || value > MAX_VALUE) {
    return false;
  }
  *field_out = CoordsToIndex(u, v);
  *value_out = value;
  return true;
}

Color NextColor(const State &state) {
  if (state.turns < INITIAL_STONES) return Color::BROWN;
  if (state.turns >= INITIAL_STONES + 2*MAX_VALUE) return Color::NONE;
  return ((state.turns - INITIAL_STONES) & 1) == 0 ? Color::RED : Color::BLUE;
}

int ColorToPlayerIndex(Color color) {
  if (color == Color::RED) return 0;
  if (color == Color::BLUE) return 1;
  assert(false);
  return -1;
}

bool ValidateMove(const State &state, const Move &move, std::string *reason = nullptr) {
  if (move.field < 0 || move.field >= NUM_FIELDS) {
    if (reason) *reason = "field index out of range";
    return false;
  }
  if (state.fields[move.field].color != Color::NONE) {
    if (reason) *reason = "field is not empty";
    return false;
  }
  if (move.color != NextColor(state)) {
    if (reason) *reason = "color does not match next color to move";
    return false;
  }
  if (move.color == Color::BROWN) {
    if (move.value != 0) {
      if (reason) *reason = "brown stone cannot have value";
      return false;
    }
    return true;
  }
  if (move.color == Color::RED || move.color == Color::BLUE) {
    if (move.value < 1 || move.value > MAX_VALUE) {
      if (reason) *reason = "stone value out of range";
      return false;
    }
    if (state.used[ColorToPlayerIndex(move.color)][move.value - 1]) {
      if (reason) *reason = "stone value has been used";
      return false;
    }
    return true;
  }
  return false;
}

void ExecuteMove(State &state, const Move &move) {
  assert(move.field >= 0 && move.field < NUM_FIELDS);
  Field &field = state.fields[move.field];
  assert(field.color == Color::NONE);
  if (state.turns < INITIAL_STONES) {
    assert(move.color == Color::BROWN);
    assert(move.value == 0);
    field.color = move.color;
  } else {
    int player = (state.turns - INITIAL_STONES) & 1;
    assert(move.color == (player == 0 ? Color::RED : Color::BLUE));
    field.color = move.color;
    assert(move.value > 0 && move.value <= MAX_VALUE);
    field.value = move.value;
    assert(!state.used[player][move.value - 1]);
    state.used[player][move.value - 1] = true;
  }
  state.turns++;
}

std::string FormatMove(const Move &move) {
  if (move.color == Color::BROWN) {
    return FieldToString(move.field);
  }
  assert(move.color == Color::RED || move.color == Color::BLUE);
  std::ostringstream oss;
  oss << FieldToString(move.field) << '=' << move.value;
  return oss.str();
}

std::string EncodeHistory(const std::vector<Move> &moves) {
  static const char BASE36DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string s;
  s.reserve(moves.size()*2);
  for (const Move &move : moves) {
    int f = move.field;
    int v = move.color == Color::RED ? move.value :
        move.color == Color::BLUE ? move.value + MAX_VALUE : 0;
    assert(0 <= f && f < 36);
    assert(0 <= v && v < 36);
    s += BASE36DIGITS[f];
    s += BASE36DIGITS[v];
  }
  return s;
}

void Main(const char *command_player1, const char *command_player2) {
  Player players[2] = {
    SpawnPlayer(command_player1),
    SpawnPlayer(command_player2)};
  PlayerQuitter quit1(players[0]);
  PlayerQuitter quit2(players[1]);

  State state;
  std::vector<Move> history;

  // Randomly draw initial brown stones.
  {
    int fields[NUM_FIELDS];
    for (int i = 0; i < NUM_FIELDS; ++i) {
      fields[i] = i;
    }
    uint64_t seed = GetRandomSeed();
    for (int i = 0; NextColor(state) == Color::BROWN; ++i) {
      int n = NUM_FIELDS - i;
      std::swap(fields[i], fields[i + seed%n]);
      seed /= n;
      Move move;
      move.color = Color::BROWN;
      move.field = fields[i];
      ExecuteMove(state, move);
      history.push_back(move);
      std::string line = FormatMove(move) + "\n";
      Write(players[0], line);
      Write(players[1], line);
    }
  }
  Write(players[0], "Start\n");
  Color next_color = NextColor(state);
  while (next_color != Color::NONE) {
    Move move;
    move.color = next_color;
    int next_player = ColorToPlayerIndex(next_color);
    std::string line = ReadLine(players[next_player]);
    if (!ParseMove(line, &move.field, &move.value)) {
      fprintf(stderr, "Could not parse move from player %d %s!\n",
          next_player, EscapeString(line).c_str());
      break;
    }
    std::string reason;
    if (!ValidateMove(state, move, &reason)) {
      fprintf(stderr, "Invalid move from player %d %s: %s!\n",
          next_player, EscapeString(line).c_str(), reason.c_str());
      break;
    }
    ExecuteMove(state, move);
    history.push_back(move);
    next_color = NextColor(state);
    if (next_color != Color::NONE) {
      // Send player's move to other player.
      std::string line = FormatMove(move) + '\n';
      Write(players[1 - next_player], line);
    }
  }
  int score = 0;
  if (next_color == Color::NONE) {
    // Regular game end.
    score = CalculateScore(state);
  } else if (next_color == Color::RED) {
    // Red made an illegal move. Blue wins.
    score = -99;
  } else if (next_color == Color::BLUE) {
    // Blue made an illegal move. Red wins.
    score = +99;
  } else {
    assert(false);
  }
  printf("%s %s%d\n",
      EncodeHistory(history).c_str(), (score > 0 ? "+" : ""), score);
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: arbiter <player1> <player2>\n");
    return 1;
  }
  Main(argv[1], argv[2]);
  return 0;
}
