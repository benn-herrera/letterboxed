#include "core/core.h"
#include "word_db/word_db.h"
#include <direct.h>

int main(int argc, char** argv) {
  using namespace bng::core;
  using namespace bng::word_db;

  if (argc != 5) {
    BNG_PRINT("usage: <side> <side> <side> <side>\n  e.g. letterboxed abc def ghi jkl\n");
    return 1;
  }

  Word sides[4];
  uint32_t all_letters = 0;

  // initialize puzzle conditions.
  {
    for (uint32_t i = 1; i < 5; ++i) {
      const char* side_str = argv[i];
      auto& side = sides[i - 1];
      side.read_str(side_str, side_str);
      if (side.length != 3 || side.letter_count != 3 || bool(all_letters & side.letters)) {
        BNG_PRINT("%s %s %s %s are not 4 sides of 3 unique letters.\n",
          argv[1], argv[2], argv[3], argv[4]);
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
    (void)_chdir(file_path);
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

  SolutionSet solutions(wordDB.size() / 2);

  // run through all letters used in the puzzle
  for (uint32_t ali = 0; ali < 26; ++ali) {
    const auto alb = uint32_t(1u << ali);
    if (!(alb & all_letters)) {
      continue;
    }

    // run through all words starting with this letter - these are candidateA
    for (auto wpa = wordDB.first_word(ali); *wpa; ++wpa) {
      // run through all words starting with the last letter of candidateA - these are candidateB
      const auto bli = wordDB.last_letter_idx(*wpa);
      for (auto wpb = wordDB.first_word(bli); *wpb; ++wpb) {
        const auto hit_letters = wpa->letters | wpb->letters;
        if (hit_letters == all_letters) {
          solutions.add(wordDB.word_i(*wpa), wordDB.word_i(*wpb));
        }
      }
    }

    // sort shortest to longest.
    solutions.sort(wordDB);
  }

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
