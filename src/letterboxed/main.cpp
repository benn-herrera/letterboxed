#include "core/core.h"
#include "word_db/word_db.h"

using namespace bng::core;
using namespace bng::word_db;

void set_wd(const char* argv0) {
  char file_path[512];
  strcpy_s(file_path, sizeof(file_path), argv0);
  for (char* e = file_path + strlen(file_path) - 1; e > file_path; --e) {
    if (*e == '/' || *e == '\\') {
      *e = 0;
      break;
    }
  }
  (void)chdir(file_path);
}

WordDB load_word_db() {
  WordDB wordDB;

  auto txt_name = "words_alpha.txt";
  auto pre_name = "words_alpha.pre";

  if (!wordDB.load(pre_name)) {
    auto _ = BNG_SCOPED_TIMER("proccessed words_alpha.txt -> words_alpha.pre");
    wordDB.load(txt_name);
    wordDB.save(pre_name);
  }

  return wordDB;
}

WordDB::SideSet init_sides(const char** side_strs) {
  auto print_err = [&side_strs]() {
    BNG_PRINT("%s %s %s %s are not 4 sides of 3 unique letters.\n",
      side_strs[0], side_strs[1], side_strs[2], side_strs[3]);
    };

  WordDB::SideSet sides;

  for (uint32_t si = 0, all_letters = 0; si < 4; ++si) {
    auto& s = sides[si];
    auto side_str = side_strs[si];
    char side_lc[4] = {};
    if (strlen(side_str) != 3) {
      print_err();
      return {};
    }
    for (uint32_t i = 0; i < 3; ++i) {
      side_lc[i] = char(tolower(side_str[i]));
      // side has non alpha characters
      if (side_lc[i] < 'a' || side_lc[i] > 'z') {
        print_err();
        return {};
      }
    }
    s = Word(side_lc);
    if ((all_letters & uint32_t(s.letters))) {
      // sides have overlapping letters
      print_err();
      return {};
    }
    all_letters |= uint32_t(s.letters);
  }

  return sides;
}

int main(int argc, const char** argv) {
  if (argc != 5) {
    BNG_PRINT("usage: <side> <side> <side> <side>\n  e.g. letterboxed vrq wue isl dmo\n");
    return 1;
  }

  WordDB wordDB;
  WordDB::SideSet sides;
  SolutionSet solutions;

  double total_ms = FLT_MAX;
  double preload_ms = FLT_MAX;
  double solve_ms = FLT_MAX;

  {
    auto _tt = ScopedTimer(&total_ms);

    sides = init_sides(argv + 1);
    if (!sides[0]) {
      return 1;
    }

    set_wd(argv[0]);
    {
      auto _pt = ScopedTimer(&preload_ms);
      wordDB = load_word_db();
    }

    {
      auto _st = ScopedTimer(&solve_ms);
      // eliminate non-candidates and solve
      wordDB.cull(sides);
      solutions = wordDB.solve(sides);
    }
  }

  // show results
  solutions.sort(wordDB);
  BNG_PRINT("%d solutions\n=============\n", solutions.count);
  for (auto ps = solutions.buf, pe = solutions.buf + solutions.count; ps < pe; ++ps) {
    auto& a = *wordDB.word(ps->a);
    auto& b = *wordDB.word(ps->b);
    if (a.letter_count == 12 || b.letter_count == 12) {
      auto& c = (a.letter_count == 12) ? a : b;
      BNG_PRINT("    %.*s\n", uint32_t(c.length), wordDB.str(c));
    }
    else {
      BNG_PRINT("    %.*s -> %.*s\n", uint32_t(a.length), wordDB.str(a), uint32_t(b.length), wordDB.str(b));
    }
  }

  // some timing stats.
  BNG_PRINT("\npreload_time: %lgms  solve time: %lgms  total_time: %lgms\n",
    preload_ms, solve_ms, total_ms);

  return 0;
}
