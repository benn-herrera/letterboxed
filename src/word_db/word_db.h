#pragma once
#include "core/core.h"
#include <algorithm>

namespace bng::word_db {
  using namespace core;
  struct Word;

  class DictBuf{
  public:
    BNG_DECL_NO_COPY(DictBuf);

    explicit DictBuf(uint32_t sz = 0) {
      if (sz) {
        _text = new char[sz + 2];
        _capacity = sz;
        _text[0] = _text[sz] = _text[sz + 1] = 0;
      }
    }

    inline Word append(const DictBuf& src, const Word& w);

    static uint32_t header_size_bytes() {
      return offsetof(DictBuf, _text);
    }

    bool in_capacity(const char* p) const {
      return p < (_text + _capacity);
    }

    bool in_size(const char* p) const {
      return p < (_text + _size);
    }

    uint32_t capacity() const { return _capacity; }

    uint32_t size() const { return _size; }

    char* front() { return _text; }

    const char* front() const { return _text; }

    char* back() { return _text + _size; }

    const char* back() const { return _text + _size; }

    void set_size(uint32_t new_size_bytes) {
      BNG_VERIFY(new_size_bytes <= _capacity, "");
      _size = new_size_bytes;
    }

    operator bool() const {
      return !!_size;
    }

    bool operator!() const {
      return !_size;
    }

    ~DictBuf() {
      delete[] _text;
      _text = nullptr;
      _capacity = 0;
    }

    // format example: printf("%.*s", w.length, buf.ptr(w));
    inline const char* ptr(const Word& w) const;

  private:
    uint32_t _capacity = 0;
    uint32_t _size = 0;
    char* _text = nullptr;
  };

  struct Word {
    uint32_t begin : 26 = 0;
    uint32_t length : 6 = 0;
    uint32_t letters : 26 = 0;
    uint32_t letter_count : 5 = 0;
    uint32_t is_dead : 1 = 0;

    Word() = default;

    Word(const Word& rhs, uint32_t new_begin) {
      *this = rhs;
      this->begin = new_begin;
    }

    uint32_t read_str(const char* buf_start, const char* p) {
      begin = uint32_t(p - buf_start);
      const auto b = buf_start + begin;
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
      BNG_VERIFY(letter_count <= 26, "");
      is_dead = (length < 3 || letter_count > 12 || has_double);
      return uint32_t(p - b);
    }

    uint32_t read_str(const DictBuf& buf, const char* p) {
      return read_str(buf.front(), p);
    }

    operator bool() const {
      return !!length;
    }

    bool operator !() const {
      return !length;
    }

    static uint32_t letter_to_idx(char ltr) {
      const auto i = uint32_t(ltr - 'a');
      BNG_VERIFY(i < 26, "");
      return i;
    }

    static bool is_end(const char* c) {
      return !*c || *c == '\n' || *c == '\r';
    }

    static char idx_to_letter(uint32_t i) {
      BNG_VERIFY(i < 26, "");
      return char('a' + i);
    }

    static uint32_t letter_to_bit(char ltr) {
      const auto i = uint32_t(ltr - 'a');
      BNG_VERIFY(i < 26, "");
      return 1u << i;
    }

  };


  inline const char* DictBuf::ptr(const Word& w) const {
    BNG_VERIFY(w.begin < _capacity, "word out of range");
    return _text + w.begin;
  }

  inline Word DictBuf::append(const DictBuf& src, const Word& w) {
    BNG_VERIFY(_size + w.length < _capacity, "");
    auto new_word = Word(w, _size);
    memcpy(_text + _size, src.ptr(w), w.length + 1);
    _size += w.length + 1;
    return new_word;
  }


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


  enum class WordI : uint32_t { kInvalid = ~0u };

  class WordDB {
  public:
    BNG_DECL_NO_COPY(WordDB);

    using SideSet = Word[4];

    WordDB(const char* path = nullptr) {
      memset(words_by_letter, 0xff, sizeof(words_by_letter));
      if (path) {
        load(path);
      }
    }

    ~WordDB() {
      delete words_buf;
      words_buf = nullptr;
    }

    uint32_t size() const {
      BNG_VERIFY(stats.total_count == live_word_count(), "");
      return stats.total_count;
    }

    bool load(const char* path) {
      BNG_VERIFY(path && *path, "");
      auto p = path + (strlen(path) - 4);
      const auto ext =
        (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
      switch (ext) {
      case '.txt': return load_word_list(path);
      case '.pre': return load_preproc(path);
      }
      BNG_VERIFY(false, "unknown extension. must be .txt or .pre");
      return false;
    }

    void save(const char* path) {
      BNG_VERIFY(path && *path, "");
      auto p = path + (strlen(path) - 4);
      const auto ext =
        (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
      switch (ext) {
      case '.pre': return save_preproc(path);
      }
      BNG_VERIFY(false, "extension must be .pre");
    }

    operator bool() const {
      return !!words_buf;
    }

    bool operator !() const {
      return !words_buf;
    }

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
            auto letter_pair = Word::letter_to_bit(*sp) | Word::letter_to_bit(*(sp + 1));
            BNG_VERIFY(bool(letter_pair & (letter_pair - 1)), "");
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
      return dict_buf.ptr(w);
    }

    uint32_t first_letter_idx(const Word& w) const {
      return Word::letter_to_idx(dict_buf.ptr(w)[0]);
    }

    uint32_t last_letter_idx(const Word& w) const {
      return Word::letter_to_idx(dict_buf.ptr(w)[(w.length - 1)]);
    }

    WordI word_i(const Word& w) const {
      auto wi = uint32_t(&w - words_buf);
      BNG_VERIFY(wi < stats.total_count, "");
      return WordI(wi);
    }

    const Word& word(WordI i) const {
      BNG_VERIFY(i != WordI::kInvalid, "");
      return words_buf[uint32_t(i)];
    }

    const Word* first_word(uint32_t letter_i) const {
      BNG_VERIFY(letter_i < 26, "");
      return &word(words_by_letter[letter_i]);
    }

  private:
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


    void cull_word(Word& word) {
      auto li = first_letter_idx(word);
      BNG_VERIFY(size_bytes_by_letter[li] > word.length, "");
      size_bytes_by_letter[li] -= (word.length + 1);
      BNG_VERIFY(stats.word_counts[li], "");
      --stats.word_counts[li];
      word.is_dead = true;
    }

    void collect_dict_stats() {
      BNG_VERIFY(dict_buf, "");
      stats = DictStats{};
      for (const char* p = dict_buf.front(); *p; ) {
        auto li = Word::letter_to_idx(*p);
        ++stats.word_counts[li];
        // catch out of order dictionary
        BNG_VERIFY(!stats.word_counts[li + 1], "");
        for (; !Word::is_end(p); ++p)
          ;
        for (; *p && Word::is_end(p); ++p)
          ;
      }
      for (uint32_t i = 0; i < 26; ++i) {
        // verify all letters populated
        BNG_VERIFY(stats.word_counts[i], "");
        stats.total_count += stats.word_counts[i];
      }
    }

    void collate_words() {
      BNG_VERIFY(!words_buf, "");
      words_buf = new Word[words_count()];

      auto wp = words_buf;
      const Word* wp_row_start = wp;
      uint32_t row_live_count = 0;
      uint32_t row_live_size_bytes = 0;

      memset(words_by_letter, 0xff, sizeof(words_by_letter));
      const char* p = dict_buf.front();
      for (; *p; ++wp) {
        BNG_VERIFY(dict_buf.in_capacity(p), "");
        auto li = Word::letter_to_idx(*p);
        if (words_by_letter[li] == WordI::kInvalid) {
          if (li) {
            // null terminate
            *wp++ = Word();
            const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
            BNG_VERIFY(row_total_count == stats.word_counts[li - 1] + 1, "");
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

      BNG_VERIFY(p == dict_buf.back(), "");

      {
        // null terminate
        *wp++ = Word();
        const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
        BNG_VERIFY(row_total_count == stats.word_counts[25] + 1, "");
        stats.word_counts[25] = row_live_count;
        size_bytes_by_letter[25] = row_live_size_bytes;
      }

      for (uint32_t i = 0; i < 26; ++i) {
        if (stats.word_counts[i]) {
          BNG_VERIFY(first_letter_idx(words_buf[uint32_t(words_by_letter[i])]) == i, "");
        }
        else {
          words_by_letter[i] = WordI::kInvalid;
        }
      }

      BNG_VERIFY(uint32_t(wp - words_buf) == words_count(), "");
      BNG_VERIFY(first_letter_idx(*(wp - 2)) == 25, "");
    }

    void process_word_list() {
      collect_dict_stats();
      BNG_VERIFY(dict_buf && stats, "");

      collate_words();
      *this = clone_packed();
    }

    bool load_word_list(const char* path) {
      dict_buf = DictBuf();

      if (auto dict_file = File(path, "r")) {
        dict_buf = DictBuf(dict_file.size_bytes());
        if (size_t read_count = fread(dict_buf.front(), 1, dict_buf.capacity(), dict_file.fp)) {
          if (read_count < dict_buf.capacity()) {
            memset(dict_buf.front() + read_count, 0, dict_buf.capacity() - read_count);
            dict_buf.set_size(uint32_t(read_count));
          }
        }
        else {
          BNG_VERIFY(false, "");
        }
      }

      process_word_list();

      return true;
    }

    static uint32_t header_size_bytes() {
      return offsetof(WordDB, dict_buf) + DictBuf::header_size_bytes();
    }

    uint32_t words_count() const {
      return uint32_t(stats.total_count + 26);
    }

    uint32_t words_size_bytes() const {
      return uint32_t(sizeof(Word) * words_count());
    }

    bool load_preproc(const char* path) {
      BNG_VERIFY(path && *path && !strcmp(path + strlen(path) - 4, ".pre"), "");
      *this = WordDB();
      if (auto fin = File(path, "rb")) {
        if (fread(this, header_size_bytes(), 1, fin) != 1) {
          *this = WordDB();
          return false;
        }
        words_buf = new Word[words_count()];
        dict_buf = DictBuf(dict_buf.size());
        if (fread(words_buf, words_size_bytes(), 1, fin) != 1) {
          BNG_VERIFY(false, "");
        }
        if (fread(dict_buf.front(), dict_buf.capacity(), 1, fin) != 1) {
          BNG_VERIFY(false, "");
        }
      }
      return *this;
    }

    void save_preproc(const char* path) const {
      BNG_VERIFY(path && *path && !strcmp(path + strlen(path) - 4, ".pre"), "");
      BNG_VERIFY(dict_buf.size() == live_size_bytes(), "");
      auto fout = File(path, "wb");
      BNG_VERIFY(fout, "");
      if (fwrite(this, header_size_bytes(), 1, fout) != 1) {
        BNG_VERIFY(false, "");
      }
      if (fwrite(words_buf, words_size_bytes(), 1, fout) != 1) {
        BNG_VERIFY(false, "");
      }
      if (fwrite(dict_buf.front(), dict_buf.size(), 1, fout) != 1) {
        BNG_VERIFY(false, "");
      }
    }

    WordDB clone_packed() const {
      const uint32_t live_size = live_size_bytes();
      const uint32_t live_count = live_word_count();
      BNG_VERIFY(
        *this && 
        live_size < dict_buf.capacity() && 
        live_count < stats.total_count, "");

      WordDB out;

      out.dict_buf = DictBuf(live_size);
      out.stats = stats;
      out.stats.total_count = live_count;
      memcpy(out.size_bytes_by_letter, size_bytes_by_letter, sizeof(size_bytes_by_letter));
      out.words_buf = new Word[out.words_count()];

      Word* wpo = out.words_buf;
      uint32_t live_row_count = 0; (void)live_row_count;

      for (uint32_t li = 0; li < 26; ++li) {
        if (!stats.word_counts[li]) {
          out.words_by_letter[li] = WordI::kInvalid;
          continue;
        }

        out.words_by_letter[li] = out.word_i(*wpo);
        const auto wpo_row_start = wpo;

        for (auto wp = first_word(li); *wp; wp++) {
          if (!wp->is_dead) {
            *wpo++ = out.dict_buf.append(dict_buf, *wp);
          }
        }
        const auto row_count = uint32_t(wpo - wpo_row_start); (void)row_count;
        BNG_VERIFY(row_count == out.stats.word_counts[li], "");
        // null terminate
        *wpo++ = Word();
        ++live_row_count;
      }

      const auto copy_count = uint32_t(wpo - out.words_buf); (void)copy_count;
      BNG_VERIFY(out.live_word_count() == out.stats.total_count, "");
      BNG_VERIFY(copy_count == out.stats.total_count + live_row_count, "");

      return out;
    }

  private:
    DictStats stats;
    WordI     words_by_letter[26] = {};
    uint32_t  size_bytes_by_letter[26] = {};
    // members here and before serialized in .pre files
    DictBuf   dict_buf;
    Word* words_buf = nullptr;
    // members here and below do not get serialized.
  };

  struct Solution {
    WordI a = WordI::kInvalid;
    WordI b = WordI::kInvalid;
  };

  struct SolutionSet {
    Solution* buf = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;

    SolutionSet(uint32_t c = 0) {
      if (c) {
        capacity = c;
        buf = new Solution[c];
      }
    }

    void add(WordI a, WordI b) {
      BNG_VERIFY(count < capacity, "out of space");
      buf[count++] = Solution{a, b};
    }

    void sort(const WordDB& wordDB) {
      std::sort(
        buf,
        buf + count,
        [&wordDB](auto& lhs, auto& rhs) -> bool {
          return
            (wordDB.word(lhs.a).length + wordDB.word(lhs.b).length)
            < 
            (wordDB.word(rhs.a).length + wordDB.word(rhs.b).length);
        }
      );
    }

    ~SolutionSet() {
      delete[] buf;
      buf = nullptr;
      count = capacity = 0;
    }
  };
} // namespace bng::word_db
