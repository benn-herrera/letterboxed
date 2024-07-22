#include "core/core.h"
#include "word_db/word_db.h"

int main(int argc, char** argv) {
  using namespace bng::core;
  using namespace bng::word_db;

  if (argc != 5) {
    BNG_PRINT("usage: <side> <side> <side> <side>\n  e.g. letterboxed vrq wue isl dmo\n");
    return 1;
  }

  uint32_t all_letters = 0;

  WordDB::SideSet sides;

  // initialize puzzle conditions.
  {
    static auto print_err = [&argv]() {
      BNG_PRINT("%s %s %s %s are not 4 sides of 3 unique letters.\n",
        argv[1], argv[2], argv[3], argv[4]);
      };
    auto copy_side = [](const char* s, char* sout) {
      auto sp = s;
      for (; *sp; ++sp, ++sout) {
        const auto si = uint32_t(sp - s);
        if (si >= 3) {
          return false;
        }
        *sout = char(tolower(*sp));
        if (*sout < 'a' || *sout > 'z') {
          return false;
        }
      }
      return uint32_t(sp - s) == 3;
      };
    char side_str[4] = {};
    for (uint32_t i = 1; i < 5; ++i) {
      if (!copy_side(argv[i], side_str)) {
        print_err();
        return 1;
      }
      auto& side = sides[i - 1];
      side = Word(side_str);
      if (side.letter_count != 3 || bool(all_letters & side.letters)) {
        print_err();
        return 1;
      }
      all_letters |= side.letters;
    }
  }

  auto total_timer = ScopedTimer();
  double preload_ms = 0;

  WordDB wordDB;

  // load word database

  {
    char file_path[512];
    strcpy_s(file_path, sizeof(file_path), argv[0]);
    for (char* e = file_path + strlen(file_path) - 1; e > file_path; --e) {
      if (*e == '/' || *e == '\\') {
        *e = 0;
        break;
      }
    }
    (void)chdir(file_path);
  }

  {
    auto txt_name = "words_alpha.txt";
    auto pre_name = "words_alpha.pre";
    {
      auto preload_timer = ScopedTimer();
      wordDB.load(pre_name);
      preload_ms = preload_timer.elapsed_ms();
    }

    if (!wordDB) {
      BNG_PRINT("pre-processing %s to %s.\n", txt_name, pre_name);
      auto preload_timer = ScopedTimer();
      wordDB.load(txt_name);
      wordDB.save(pre_name);
      preload_ms = preload_timer.elapsed_ms();
      BNG_PRINT("done pre-processing to %s.\n", pre_name);
    }
  }

  auto solution_timer = ScopedTimer();

  // eliminate non-candiates.
  wordDB.cull(sides);

  auto solutions = wordDB.solve(sides);

  const auto solution_ms = solution_timer.elapsed_ms();
  const auto total_ms = total_timer.elapsed_ms();

  BNG_PRINT("%d possible solutions\n======================\n", solutions.count);

  // print them out.
  for (auto ps = solutions.buf, pe = solutions.buf + solutions.count; ps < pe; ++ps) {
    auto& a = wordDB.word(ps->a);
    auto& b = wordDB.word(ps->b);
    auto sa = wordDB.str(a);
    auto sb = wordDB.str(b);

    BNG_PRINT("    %.*s -> %.*s\n", uint32_t(a.length), sa, uint32_t(b.length), sb);
  }

  BNG_PRINT("\npreload_time: %lgms  solution time: %lgms  total_time: %lgms\n",
    preload_ms, solution_ms, total_ms);

  return 0;
}
