#pragma once
#include "core/core.h"

namespace bng::word_db {
  using namespace core;


  class TextBuf;
  class WordDB;


  struct TextStats {
    uint32_t word_counts[26] = {};
    uint32_t size_bytes[26] = {};

    operator bool() const {
      for (auto wc : word_counts) {
        if (wc) {
          return true;
        }
      }
      return false;
    }

    bool operator!() const {
      return bool(*this) == false;
    }

    uint32_t total_count(bool null_terminated=false) const {
      uint32_t tc = 0;
      for (auto wc : word_counts) {
        tc += wc + uint32_t(wc && null_terminated);
      }
      return tc;
    }

    uint32_t total_size_bytes() const {
      uint32_t tsb = 0;
      for (auto sb : size_bytes) {
        tsb += sb;
      }
      return tsb;
    }
  };


  struct Word {
    uint64_t begin : 26 = 0;
    uint64_t length : 6 = 0;
    uint64_t letters : 26 = 0;
    uint64_t letter_count : 5 = 0;
    uint64_t is_dead : 1 = 0;

    Word() = default;

    explicit Word(const char* str) {
      read_str(str, str);
    }

    Word(const Word& rhs, uint32_t new_begin) {
      *this = rhs;
      this->begin = new_begin;
    }

    uint32_t read_str(const char* buf_start, const char* p);
    inline uint32_t read_str(const TextBuf& buf, const char* p);

    void get_letters_str(char* pout) const {
      letters_to_str(letters, pout);
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

    static bool is_end(char c) {
      return !c || c == '\n' || c == '\r';
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

    static void letters_to_str(uint64_t letter_bits, char* pout);
  };


  class TextBuf {
  public:
    BNG_DECL_NO_COPY_IMPL_MOVE(TextBuf);

    explicit TextBuf(uint32_t sz = 0);

    Word append(const TextBuf& src, const Word& w);

    uint32_t capacity() const { return _capacity; }
    uint32_t size() const { return _size; }
    char* begin() { return _text; }
    char* front() { return _text; }
    const char* begin() const { return _text; }
    const char* front() const { return _text; }
    char* end() { return _text + _size; }
    const char* end() const { return _text + _size; }

    void set_size(uint32_t new_size_bytes) {
      BNG_VERIFY(new_size_bytes <= _capacity, "");
      _size = new_size_bytes;
    }

    bool in_size(const char* p) const {
      return p < (_text + _size);
    }

    bool in_capacity(const char* p) const {
      return p < (_text + _capacity);
    }

    // format example: printf("%.*s", w.length, buf.ptr(w));
    const char* ptr(const Word& w) const {
      BNG_VERIFY(w.begin < _size, "word out of range");
      return _text + w.begin;
    }

    operator bool() const {
      return !!_size;
    }

    bool operator!() const {
      return !_size;
    }

    TextStats collect_stats() const;

    ~TextBuf() {
      delete[] _text;
      _text = nullptr;
      _capacity = 0;
    }

    static uint32_t header_size_bytes() {
      return offsetof(TextBuf, _text);
    }

  private:
    uint32_t _capacity = 0;
    uint32_t _size = 0;
    char* _text = nullptr;
  };


  inline uint32_t Word::read_str(const TextBuf& buf, const char* p) {
    return read_str(buf.begin(), p);
  }


  enum class WordIdx : uint32_t { kInvalid = ~0u };


  struct Solution {
    WordIdx a = WordIdx::kInvalid;
    WordIdx b = WordIdx::kInvalid;
  };


  struct SolutionSet {
    BNG_DECL_NO_COPY_IMPL_MOVE(SolutionSet);

    explicit SolutionSet(uint32_t c = 0) {
      if (c) {
        _capacity = c;
        buf = new Solution[c];
      }
    }

    ~SolutionSet() {
      delete[] buf;
      buf = nullptr;
      _size = _capacity = 0;
    }

    void add(WordIdx a, WordIdx b) {
      BNG_VERIFY(_size < _capacity, "out of space");
      buf[_size++] = Solution{ a, b };
    }

    const Solution* begin() const { return buf; }
    const Solution* end() const { return buf + _size; }

    Solution* begin() { return buf; }
    Solution* end() { return buf + _size; }

    const Solution& front() const { return *buf; }
    const Solution& back() const { return *(buf + _size); }

    size_t capacity() const {
      return _capacity;
    }

    size_t size() const {
      return _size;
    }

    void sort(const WordDB& wordDB);

  private:
    Solution* buf = nullptr;
    uint32_t _size = 0;
    uint32_t _capacity = 0;
  };


  class WordDB {
  public:
    BNG_DECL_NO_COPY_IMPL_MOVE(WordDB);

    using SideSet = std::array<Word, 4>;

    explicit WordDB(const char* path = nullptr);

    ~WordDB();

    uint32_t size() const {
      BNG_VERIFY(mem_stats.total_count() == live_stats.total_count(), "");
      return live_stats.total_count();
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

    SolutionSet solve(const SideSet& sides) const;

    bool is_equivalent(const WordDB& rhs) const;

    const TextStats& get_text_stats() const {
      return live_stats;
    }

    const TextBuf& get_text_buf() {
      return text_buf;
    }

    // format example: printf("%.*s", w.length, word_db.str(w));
    const char* str(const Word& w) const {
      return text_buf.ptr(w);
    }

    uint32_t first_letter_idx(const Word& w) const {
      return Word::letter_to_idx(text_buf.ptr(w)[0]);
    }

    uint32_t last_letter_idx(const Word& w) const {
      return Word::letter_to_idx(text_buf.ptr(w)[(w.length - 1)]);
    }

    WordIdx word_i(const Word& w) const {
      auto wi = uint32_t(&w - words_buf);
      BNG_VERIFY(wi < words_count(), "word not in buffer!");
      return WordIdx(wi);
    }

    const Word* word(WordIdx i) const {
      return (i != WordIdx::kInvalid) ? (words_buf + uint32_t(i)) : nullptr;
    }

    const Word* first_word(uint32_t letter_i) const {
      BNG_VERIFY(letter_i < 26, "invalid letter index");
      return word(words_by_letter[letter_i]);
    }

    const Word* last_word(uint32_t letter_i) const {
      auto pword = first_word(letter_i) + mem_stats.word_counts[letter_i] - 1;
      BNG_VERIFY(!*(pword + 1), "word list for letter not null terminated.");
      return pword;
    }

  private:
    void load_preproc(const char* path);

    void save_preproc(const char* path) const;

    void load_word_list(const char* path);

    void process_word_list();

    void collate_words();

    WordDB clone_packed() const;

    void cull_word(Word& word);

    static uint32_t header_size_bytes() {
      return offsetof(WordDB, text_buf) + TextBuf::header_size_bytes();
    }

    uint32_t words_count() const {
      return uint32_t(mem_stats.total_count(/*null_terminated*/true));
    }

    uint32_t words_size_bytes() const {
      return uint32_t(sizeof(Word) * words_count());
    }

    void clear_words_by_letter();

    Word& word_rw(WordIdx i) {
      BNG_VERIFY(i != WordIdx::kInvalid, "");
      return words_buf[uint32_t(i)];
    }

    Word* first_word_rw(uint32_t letter_i) {
      BNG_VERIFY(letter_i < 26, "");
      return &word_rw(words_by_letter[letter_i]);
    }

  private:
    TextStats mem_stats;
    WordIdx words_by_letter[26] = {};
    // members here and before serialized in .pre files
    TextBuf text_buf;
    Word* words_buf = nullptr;
    // members here and below do not get serialized.
    TextStats live_stats;
  };
} // namespace bng::word_db
