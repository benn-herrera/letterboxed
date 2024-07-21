#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>
#include <memory>
#include <algorithm>
#include <chrono>

#pragma warning( disable : 4514 5045 )

#define DECL_NO_COPY(CLASS) \
  CLASS(const CLASS&) = delete; \
  CLASS& operator =(const CLASS&) = delete;\
  CLASS(CLASS&& rhs) noexcept { *this = std::move(rhs); }; \
  CLASS& operator =(CLASS&& rhs) noexcept { \
    memcpy(this, &rhs, sizeof(*this)); \
    memset(&rhs, 0, sizeof(*this)); \
    return *this; \
  }


struct File {
  DECL_NO_COPY(File);

  FILE* fp;

  explicit File(const char* path, const char* mode) : fp(nullptr) {
#if defined(BNG_IS_WINDOWS)
    fopen_s(&fp, path, mode);
#else
    fp = fopen(path, mode);
#endif
  }

  operator FILE* () { return fp; }
  operator bool() const { return !!fp; }
  bool operator!() const { return !fp; }

  uint32_t size_bytes() {
    fseek(fp, 0, SEEK_END);
    auto sz = uint32_t(ftell(fp));
    rewind(fp);
    return sz;
  }

  ~File() {
    if (fp) {
      fclose(fp);
      fp = nullptr;
    }
  }
};


struct DictStats {
  uint32_t word_counts[26] = {};
  uint32_t total_count = 0;
  uint32_t pad0 = 0;

  operator bool() const {
    return !!total_count;
  }

  bool operator !() const {
    return !total_count;
  }
};


struct DictBuf {
  DECL_NO_COPY(DictBuf);

  uint32_t size_bytes = 0;
  uint32_t pad0 = 0;
  char* text = nullptr;

  explicit DictBuf(uint32_t sz=0) {
    if (sz) {
      text = new char[sz + 2];
      size_bytes = sz;
      text[0] = text[sz] = text[sz + 1] = 0;
    }
  }

  operator bool() const {
    return !!text;
  }

  bool operator!() const {
    return !text;
  }

  ~DictBuf() {
    delete[] text;
    text = nullptr;
    size_bytes = 0;
  }
};


inline uint32_t letter_to_idx(char ltr) {
  const auto i = uint32_t(ltr - 'a');
  assert(i < 26);
  return i;
}

inline bool is_end(const char* c) {
  return !*c || *c == '\n' || *c == '\r';
}

inline char idx_to_letter(uint32_t i) {
  assert(i < 26);
  return char('a' + i);
}

inline uint32_t letter_to_bit(char ltr) {
  const auto i = uint32_t(ltr - 'a');
  assert(i < 26);
  return 1u << i;
}

/*
inline uint32_t bit_to_index(uint32_t b) {
  assert(b && !(b & (b - 1)));
  uint32_t i = 0, j;

  j = uint32_t(!!(0xffff0000 & b)) << 4;
  b >>= j;
  i += j;

  j = uint32_t(!!(0xff00 & b)) << 3;
  b >>= j;
  i += j;

  j = uint32_t(!!(0xf0 & b)) << 2;
  b >>= j;
  i += j;

  j = uint32_t(!!(0xc & b)) << 1;
  b >>= j;
  i += j;

  j = uint32_t(!!(0x2 & b));
  b >>= j;
  i += j;

  return i;
}

inline char bit_to_letter(uint32_t b) {
  return char('a' + bit_to_index(b));
}
*/


struct Word {
  uint32_t begin : 26 = 0;
  uint32_t length : 6 = 0;
  uint32_t letters : 26 = 0;
  uint32_t letter_count : 5 = 0;
  uint32_t is_dead : 1 = 0;

  Word() = default;

  // printf with printf("%.*s", word.length, word.str(buf));
  const char* str(const DictBuf& buf) const {
    return buf.text + begin;
  }

  uint32_t read_str(const char* buf, const char* p) {
    begin = uint32_t(p - buf);
    const auto b = buf + begin;
    bool has_double = false;
    for (; !is_end(p); ++p) {
      has_double = has_double || (p > b && (*p == *(p - 1)));
      const auto lbit = letter_to_bit(*p);
      letter_count += uint32_t(!(letters & lbit));
      letters |= lbit;
    }
    length = uint32_t(p - b);
    for (; *p && is_end(p); ++p)
      ;
    assert(letter_count <= 26);
    is_dead = (length < 3 || letter_count > 12 || has_double);
    return uint32_t(p - b);
  }

  uint32_t read_str(const DictBuf& buf, const char* p) {
    return read_str(buf.text, p);
  }

  uint32_t write_str(const DictBuf& buf, char* p) const {
    memcpy(p, buf.text + begin, length + 1);
    return length + 1;
  }

  operator bool() const {
    return !!length;
  }

  bool operator !() const {
    return !length;
  }  
};

enum class WordI : uint32_t { kInvalid = ~0u };

struct WordDB {
  DECL_NO_COPY(WordDB);

  DictStats stats;
  WordI     words_by_letter[26] = {};
  uint32_t  size_bytes_by_letter[26] = {};
  DictBuf   dict_buf;
  Word* words_buf = nullptr;

  uint32_t live_word_count() const {
    uint32_t _live_word_count = 0;
    for (auto c : stats.word_counts) {
      _live_word_count += c;
    }
    return _live_word_count;
  }

  uint32_t live_size_bytes() const {
    uint32_t _live_size_bytes = 0;
    for (auto s : size_bytes_by_letter) {
      _live_size_bytes += s;
    }
    return _live_size_bytes;
  }

  WordDB(const char* path=nullptr) {
    memset(words_by_letter, 0xff, sizeof(words_by_letter));
    if (path) {
      load(path);
    }
  }

  ~WordDB() {
    delete words_buf;
    words_buf = nullptr;
  }

  bool load(const char* path) {
    assert(path && *path);
    auto p = path + (strlen(path) - 4);
    const auto ext = 
      (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    switch (ext) {
    case '.txt': return load_word_list(path);
    case '.pre': return load_preproc(path);
    }
    assert(false);
    return false;
  }

  void save(const char* path) {
    assert(path && *path);
    auto p = path + (strlen(path) - 4);
    const auto ext =
      (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    switch (ext) {
    case '.pre': return save_preproc(path);
    }
    assert(false);
  }

  operator bool() const {
    return !!words_buf;
  }

  bool operator !() const {
    return !words_buf;
  }

  using SideSet = Word[4];
  void cull(const SideSet& sides) {
    uint32_t all_letters = 0;
    for (auto s : sides) {
      all_letters |= s.letters;
    }

    for (uint32_t li = 0; li < 26; ++li) {
      const auto lb = uint32_t(1u << li);

      // letter not in puzzle
      if (!(lb & all_letters)) {
        words_by_letter[li] = WordI::kInvalid;
        stats.word_counts[li] = 0;
        continue;
      }

      for (auto wp = words_buf + uint32_t(words_by_letter[li]); *wp; ++wp) {
        // check for use of unavailable letters
        if ((wp->letters | all_letters) != all_letters) {
          cull_word(*wp);
          continue;
        }
        bool double_tap = false;
        for (auto sp = str(*wp), se = str(*wp) + wp->length - 1; !double_tap && sp < se; ++sp) {
          auto letter_pair = letter_to_bit(*sp) | letter_to_bit(*(sp + 1));
          assert(bool(letter_pair & (letter_pair - 1)));
          for (auto s : sides) {
            auto overlap = s.letters & letter_pair;
            // hits same side with 2 sequential letters.
            if (bool(overlap & (overlap - 1))) {
              double_tap = true;
              break;
            }
          }
        }
        if (double_tap) {
          cull_word(*wp);
          continue;
        }
      }
    }

    *this = clone_packed();
  }

  const char* str(const Word& w) const {
    return w.str(dict_buf);
  }

  uint32_t first_letter_idx(const Word& w) const {
    return letter_to_idx(w.str(dict_buf)[0]);
  }

  uint32_t last_letter_idx(const Word& w) const {
    return letter_to_idx(w.str(dict_buf)[(w.length - 1)]);
  }

  uint32_t first_letter_bit(const Word& w) const {
    return letter_to_bit(w.str(dict_buf)[0]);
  }

  uint32_t last_letter_bit(const Word& w) const {
    return letter_to_bit(w.str(dict_buf)[(w.length - 1)]);
  }

  WordI word_i(const Word& w) const {
    auto wi = uint32_t(&w - words_buf);
    assert(wi < stats.total_count);
    return WordI(wi);
  }

  const Word& word(WordI i) const {
    assert(i != WordI::kInvalid);
    return words_buf[uint32_t(i)];
  }

  const Word* first_word(uint32_t letter_i) const {
    assert(letter_i < 26);
    return &word(words_by_letter[letter_i]);
  }

private:
  void cull_word(Word& word) {
    auto li = first_letter_idx(word);
    assert(size_bytes_by_letter[li] > word.length);
    size_bytes_by_letter[li] -= (word.length + 1);
    assert(stats.word_counts[li]);
    --stats.word_counts[li];
    word.is_dead = true;
  }

  void collect_dict_stats() {
    assert(dict_buf);
    stats = DictStats{};
    for (char* p = dict_buf.text; *p; ) {
      auto li = letter_to_idx(*p);
      ++stats.word_counts[li];
      // catch out of order dictionary
      assert(!stats.word_counts[li + 1]);
      for (; !is_end(p); ++p)
        ;
      for (; *p && is_end(p); ++p)
        ;
    }
    for (uint32_t i = 0; i < 26; ++i) {
      // verify all letters populated
      assert(stats.word_counts[i]);
      stats.total_count += stats.word_counts[i];
    }
  }

  void collate_words() {
    assert(!words_buf);
    words_buf = new Word[words_count()];

    auto wp = words_buf;
    const Word* wp_row_start = wp;
    uint32_t row_live_count = 0;
    uint32_t row_live_size_bytes = 0;

    memset(words_by_letter, 0xff, sizeof(words_by_letter));
    auto p = dict_buf.text;
    for (; *p; ++wp) {
      assert(p < (dict_buf.text + dict_buf.size_bytes));
      auto li = letter_to_idx(*p);
      if (words_by_letter[li] == WordI::kInvalid) {
        if (li) {
          // null terminate
          *wp++ = Word();
          const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
          assert(row_total_count == stats.word_counts[li - 1] + 1);
          stats.word_counts[li - 1] = row_live_count;
          size_bytes_by_letter[li - 1] = row_live_size_bytes;
          row_live_count = 0;
          row_live_size_bytes = 0;
          wp_row_start = wp;
        }
        // cache the start of the word list.
        words_by_letter[li] = word_i(*wp);
      }
      p += wp->read_str(dict_buf, p);
      if (!wp->is_dead) {
        row_live_size_bytes += (wp->length + 1);
        ++row_live_count;
      }
    }

    assert(uint32_t(p - dict_buf.text) == dict_buf.size_bytes);

    {
      // null terminate
      *wp++ = Word();
      const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
      assert(row_total_count == stats.word_counts[25] + 1);
      stats.word_counts[25] = row_live_count;
      size_bytes_by_letter[25] = row_live_size_bytes;
    }

    for (uint32_t i = 0; i < 26; ++i) {
      if (stats.word_counts[i]) {
        assert(first_letter_idx(words_buf[uint32_t(words_by_letter[i])]) == i);
      }
      else {
        words_by_letter[i] = WordI::kInvalid;
      }
    }

    assert(uint32_t(wp - words_buf) == words_count());
    assert(first_letter_idx(*(wp - 2)) == 25); 
  }

  void process_word_list() {
    collect_dict_stats();
    assert(dict_buf && stats);

    collate_words();
    *this = clone_packed();
  }

  bool load_word_list(const char* path) {
    dict_buf = DictBuf();

    if (auto dict_file = File(path, "r")) {
      dict_buf = DictBuf(dict_file.size_bytes());
      if (size_t read_count = fread(dict_buf.text, 1, dict_buf.size_bytes, dict_file.fp)) {
        if (read_count < dict_buf.size_bytes) {
          memset(dict_buf.text + read_count, 0, dict_buf.size_bytes - read_count);
          dict_buf.size_bytes = uint32_t(read_count);
        }
      }
      else {
        assert(false);
      }
    }

    process_word_list();

    return true;
  }

  uint32_t header_size_bytes() const {
    return uint32_t((const uint8_t*)&dict_buf.text - (const uint8_t*)this);
  }

  uint32_t words_count() const {
    return uint32_t(stats.total_count + 26);
  }

  uint32_t words_size_bytes() const {
    return uint32_t(sizeof(Word) * words_count());
  }

  bool load_preproc(const char* path) {
    assert(path && *path && !strcmp(path + strlen(path) - 4, ".pre"));
    *this = WordDB();
    if (auto fin = File(path, "rb")) {
      if (fread(this, header_size_bytes(), 1, fin) != 1) {
        *this = WordDB();
        return false;
      }
      words_buf = new Word[words_count()];
      dict_buf = DictBuf(dict_buf.size_bytes);
      if (fread(words_buf, words_size_bytes(), 1, fin) != 1) {
        assert(false);
      }
      if (fread(dict_buf.text, dict_buf.size_bytes, 1, fin) != 1) {
        assert(false);
      }
    }
    return *this;
  }

  void save_preproc(const char* path) const {
    assert(path && *path && !strcmp(path + strlen(path) - 4, ".pre"));
    assert(dict_buf.size_bytes == live_size_bytes());
    auto fout = File(path, "wb");
    assert(fout);
    if (fwrite(this, header_size_bytes(), 1, fout) != 1) {
      assert(false);
    }
    if (fwrite(words_buf, words_size_bytes(), 1, fout) != 1) {
      assert(false);
    }
    if (fwrite(dict_buf.text, dict_buf.size_bytes, 1, fout) != 1) {
      assert(false);
    }
  }

  WordDB clone_packed() const {
    const uint32_t live_size = live_size_bytes();
    const uint32_t live_count = live_word_count();
    assert(*this && live_size < dict_buf.size_bytes && live_count < stats.total_count);

    WordDB out;

    printf("packing down from %u words, %u bytes -> %u words, %u bytes\n",
      stats.total_count, dict_buf.size_bytes, live_count, live_size);

    out.dict_buf = DictBuf(live_size);
    out.stats = stats;
    out.stats.total_count = live_count;
    out.words_buf = new Word[out.words_count()];

    memcpy(out.size_bytes_by_letter, size_bytes_by_letter, sizeof(size_bytes_by_letter));

    auto otxt = out.dict_buf.text;
    const auto ote = otxt + out.dict_buf.size_bytes; (void)ote;

    Word* wpo = out.words_buf;
    uint32_t live_row_count = 0;

    for (uint32_t li = 0; li < 26; ++li) {      
      if (!stats.word_counts[li]) {
        out.words_by_letter[li] = WordI::kInvalid;
        continue;
      }

      out.words_by_letter[li] = WordI(uint32_t(wpo - out.words_buf));
      const auto wpo_row_start = wpo;

      for (auto wp = words_buf + uint32_t(words_by_letter[li]); *wp; wp++) {
        if (!wp->is_dead) {
          // length and letter bits are good.
          *wpo = *wp;
          // fix up the buffer offset
          wpo->begin = uint32_t(otxt - out.dict_buf.text);
          assert(otxt + wpo->length <= ote);
          otxt += wp->write_str(dict_buf, otxt);
          ++wpo;
        }
      }
      const auto row_count = uint32_t(wpo - wpo_row_start); (void)row_count;
      assert(row_count == out.stats.word_counts[li]);
      // null terminate
      *wpo++ = Word();
      ++live_row_count;
    }
    const auto copy_count = uint32_t(wpo - out.words_buf); (void)copy_count;
    assert(out.live_word_count() == out.stats.total_count);
    assert(copy_count == out.stats.total_count + live_row_count);
    return out;
  }
};

struct Solution {
  WordI a = WordI::kInvalid;
  WordI b = WordI::kInvalid;
};

struct SolutionBuf {
  Solution* buf = nullptr;
  size_t count = 0;

  SolutionBuf(size_t c = 0) {
    if (c) {
      count = c;
      buf = new Solution[c];
    }
  }

  ~SolutionBuf() {
    delete[] buf;
    buf = nullptr;
  }
};

int main(int argc, const char** argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: <side> <side> <side> <side>\n  e.g. letterboxed abc def ghi jkl\n");
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
        fprintf(stderr, "%s %s %s %s are not 4 sides of 3 unique letters.\n",
          argv[1], argv[2], argv[3], argv[4]);
        return 1;
      }
      all_letters |= side.letters;
    }
  }


  auto start_time = std::chrono::steady_clock::now();
  int64_t preload_us = 0;

  WordDB wordDB;

  // load word database
  {
    auto txt_name = "words_alpha.txt";
    auto pre_name = "words_alpha.pre";
    char file_path[512];
    char* e;
    {
      strcpy_s(file_path, sizeof(file_path), argv[0]);
      e = file_path + strlen(file_path) - 1;
      for (; e > file_path && *e != '/' && *e != '\\'; --e)
        ;
      e[1] = 0;
      e++;
    }
    strcpy_s(e, 512 - rsize_t(e - file_path), pre_name);
    {
      auto preload_start_time = std::chrono::steady_clock::now();
      wordDB.load(file_path);
      preload_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - preload_start_time).count();
    }

    if (!wordDB) {
      printf("pre-processing %s to %s.\n", txt_name, pre_name);
      auto preload_start_time = std::chrono::steady_clock::now();
      strcpy_s(e, 512 - rsize_t(e - file_path), txt_name);
      wordDB.load(file_path);
      strcpy_s(e, 512 - rsize_t(e - file_path), pre_name);
      wordDB.save(file_path);
      preload_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - preload_start_time).count();
      printf("done pre-processing to %s.\n", pre_name);
    }
  }

  auto solution_start_time = std::chrono::steady_clock::now();

  // eliminate non-candiates.
  wordDB.cull(sides);

  SolutionBuf solutions(wordDB.stats.total_count/2);
  auto psol = solutions.buf;

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
          // we have a possible solution.
          *psol++ = Solution{ wordDB.word_i(*wpa), wordDB.word_i(*wpb) };
        }
      }
    }

    // sort shortest to longest.
    std::sort(
      solutions.buf,
      psol,
      [&wordDB](auto& lhs, auto& rhs) -> bool{
        return 
          wordDB.word(lhs.a).length + wordDB.word(lhs.b).length < wordDB.word(rhs.a).length + wordDB.word(rhs.b).length;
      }
    );
  }

  auto end_time = std::chrono::steady_clock::now();
  const auto solution_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - solution_start_time).count();
  const auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

  printf("%d possible solutions\n================\n", uint32_t(psol - solutions.buf));

  // print them out.
  for (auto ps = solutions.buf; ps < psol; ++ps) {
    auto& a = wordDB.word(ps->a);
    auto& b = wordDB.word(ps->b);
    auto sa = wordDB.str(a);
    auto sb = wordDB.str(b);

    printf("    %.*s -> %.*s\n", a.length, sa, b.length, sb);
  }

  printf("\npreload_time: %lgms  solution time: %lgms  total_time: %lgms\n", 
    double(preload_us)/1000.0, double(solution_us)/1000.0, double(total_us)/1000.0);

  return 0;
}
