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

bool Write(Player &player, std::string s) {
  if (write(player.fd_in, s.data(), s.size()) != static_cast<ssize_t>(s.size())) {
    fprintf(stderr, "Write [%s] failed!\n", s.c_str());
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

bool ValidateMove(const State &state, const Move &move) {
  if (move.color != (state.turns%2 == 0 ? Color::RED : Color::BLUE) ||
      move.field < 0 || move.field >= NUM_FIELDS ||
      move.value < 1 || move.value > MAX_VALUE) {
    return false;
  }
  // TODO: check that field is free
  // TODO: check that player has not used stone of this value yet
  return true;
}

void ExecuteMove(State &state, const Move &move) {
  assert(move.field >= 0 && move.field < NUM_FIELDS);
  Field &field = state.fields[move.field];
  if (move.color == Color::BROWN) {
    assert(state.turns == 0);
    field.color = move.color;
    return;
  }
  assert(move.color == (state.turns%2 == 0 ? Color::RED : Color::BLUE));
  field.color = move.color;
  assert(move.value > 0 && move.value <= MAX_VALUE);
  field.value = move.value;
  // TODO: take piece away from player!
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
    for (int i = 0; i < INITIAL_STONES; ++i) {
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
  int turn = 0;
  for (; turn < MAX_VALUE*2; ++turn) {
    int player = turn%2;
    Move move;
    move.color = {player == 0 ? Color::RED : Color::BLUE};
    std::string line = ReadLine(players[player]);
    if (!ParseMove(line, &move.field, &move.value)) {
      // TODO: would be nice to print this string with escapes, to detect whitespace errors etc.
      fprintf(stderr, "Could not parse move from player %d [%s]!\n", player, line.c_str());
      break;
    }
    // TODO: this is not yet implemented!
    if (!ValidateMove(state, move)) {
      fprintf(stderr, "Invalid move from player %d [%s]!\n", player, line.c_str());
      break;
    }
    ExecuteMove(state, move);
    history.push_back(move);
    if (turn + 1 < MAX_VALUE*2) {
      // Send player's move to other player.
      std::string line = FormatMove(move) + '\n';
      Write(players[1 - player], line);
    }
  }
  // TODO: if turn < MAX_VALUE*2, then player turn%2 got disqualified and should receive a very negative score (e.g. +99/-99?)
  // TODO: evaluate & print final score, and print/log transcript
  printf("%s\n", EncodeHistory(history).c_str());
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: arbiter <player1> <player2>\n");
    return 1;
  }
  fprintf(stderr, "TODO: finish TODOs in this file!\n");  // TODO: remove this
  Main(argv[1], argv[2]);
  return 0;
}
