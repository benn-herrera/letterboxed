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

    Word append(const DictBuf& src, const Word& w);

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

    uint32_t read_str(const char* buf_start, const char* p);

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

    operator bool() const {
      return !!words_buf;
    }

    bool operator !() const {
      return !words_buf;
    }

    bool load(const char* path);

    void save(const char* path);

    void cull(const SideSet& sides);

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

    const Word* last_word(uint32_t letter_i) const {
      return first_word(letter_i) + stats.word_counts[letter_i] - 1;
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

    bool load_preproc(const char* path);

    void save_preproc(const char* path) const;

    bool load_word_list(const char* path);

    void process_word_list();

    void collect_dict_stats();

    void collate_words();

    WordDB clone_packed() const;

    void cull_word(Word& word);

    static uint32_t header_size_bytes() {
      return offsetof(WordDB, dict_buf) + DictBuf::header_size_bytes();
    }

    uint32_t words_count() const {
      return uint32_t(stats.total_count + 26);
    }

    uint32_t words_size_bytes() const {
      return uint32_t(sizeof(Word) * words_count());
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
