// A simple random Black Hole player for testing purposes.

#include <assert.h>
#include <unistd.h>

#include <algorithm>
#include <random>
#include <iostream>
#include <string>
#include <vector>

namespace {

using std::vector;
using std::string;

std::random_device random_device;
std::default_random_engine random_engine(random_device());

vector<string> fields = {
  "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8",
  "B1", "B2", "B3", "B4", "B5", "B6", "B7",
  "C1", "C2", "C3", "C4", "C5", "C6",
  "D1", "D2", "D3", "D4", "D5",
  "E1", "E2", "E3", "E4",
  "F1", "F2", "F3",
  "G1", "G2",
  "H1"};

vector<string> values[2] = {
  {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"},
  {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}};

int my_player = 0;

string ParseField(string move) {
  return move.substr(0, move.find('='));
}

string ParseValue(string move) {
  auto i = move.find('=');
  assert(i < move.size());
  return move.substr(i + 1);
}

void Remove(vector<string> *v, const string &elem) {
  auto it = std::find(v->begin(), v->end(), elem);
  if (it == v->end()) {
    std::cerr << "Element not found: [" << elem << "]" << std::endl;
    exit(1);
  }
  v->erase(it);
}

void OtherMove(const string &move) {
   Remove(&fields, ParseField(move));
   Remove(&values[1 - my_player], ParseValue(move));
}

string RemoveRandomElement(vector<string> *v) {
  assert(!v->empty());
  std::uniform_int_distribution<int> distribution(0, (int)v->size() - 1);
  auto it = v->begin() + distribution(random_engine);
  string result = std::move(*it);
  v->erase(it);
  return result;
}

string RandomMove() {
  string field = RemoveRandomElement(&fields);
  string value = RemoveRandomElement(&values[my_player]);
  return field + '=' + value;
}

string ReadLine() {
  string line;
  getline(std::cin, line);
  if (line == "Quit") exit(0);
  return line;
}

void Main() {
  for (int i = 0; i < 5; ++i) {
    Remove(&fields, ReadLine());
  }
  string line = ReadLine();
  if (line != "Start") {
    my_player = 1;
    OtherMove(line);
  }
  while (fields.size() > 1) {
    std::cout << RandomMove() << std::endl;
    if (fields.size() > 1) {
      OtherMove(ReadLine());
    }
  }
  line = ReadLine();
  std::cerr << "Unexpected line: [" << line << "] (expected Quit)" << std::endl;
}

}  // namespace

int main() {
  Main();
}
