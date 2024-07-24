#pragma once
#include "core/core.h"

namespace bng::word_db_std {
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

    uint32_t total_count() const {
      uint32_t tc = 0;
      for (auto wc : word_counts) {
        tc += wc;
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

    explicit Word(const std::string& buf) {
      read_str(buf, 0);
    }

    Word(const Word& rhs, uint32_t new_begin) {
      *this = rhs;
      this->begin = new_begin;
    }

    size_t read_str(const std::string& buf, size_t offset);

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


  class TextBuf : private std::string {
  public:
    using super = std::string;

  public:
    BNG_DECL_NO_COPY(TextBuf);

    explicit TextBuf(std::string&& rhs) noexcept 
      : super(std::move(rhs)) 
    {
    }

    explicit TextBuf(uint32_t sz = 0) {
      reserve(sz);
    }

    TextBuf& operator=(std::string&& rhs) {
      super::operator=(std::move(rhs));
      return *this;
    }

    using super::size;
    using super::capacity;

    operator const super& () const { return *this; }

    const super& as_string() const { return *this; }

    Word append(const TextBuf& src, const Word& w);

    void set_size(uint32_t new_size_bytes) {
      BNG_VERIFY(new_size_bytes <= capacity(), "");
      resize(new_size_bytes);
    }

    bool in_size(const char* p) const {
      return size_t(p - &front()) < size();
    }

    bool in_capacity(const char* p) const {
      return size_t(p - &front()) < capacity();
    }

    // format example: printf("%.*s", w.length, buf.ptr(w));
    const char* ptr(const Word& w) const {
      BNG_VERIFY(w.begin < size(), "word out of range");
      return data() + w.begin;
    }

    operator bool() const {
      return !empty();
    }

    bool operator!() const {
      return empty();
    }

    TextStats collect_stats() const;
  };


  enum class WordIdx : uint32_t { kInvalid = ~0u };


  struct Solution {
    WordIdx a = WordIdx::kInvalid;
    WordIdx b = WordIdx::kInvalid;
  };


  class SolutionSet : private std::vector<Solution> {
  public:
    using super = std::vector<Solution>;

    BNG_DECL_NO_COPY(SolutionSet);

    SolutionSet() = default;

    SolutionSet(size_t sz) {
      reserve(sz);
    }

    void add(WordIdx a, WordIdx b) {
      emplace_back(Solution{ a, b });
    }

    operator const super& () const { return *this; }

    using super::size;
    using super::capacity;
    using super::begin;
    using super::end;
    using super::front;
    using super::back;

    void sort(const WordDB& wordDB);
  };


  class WordDB {
  public:
    BNG_DECL_NO_COPY(WordDB);

    using SideSet = std::array<Word, 4>;

    WordDB() = default;

    explicit WordDB(const std::filesystem::path& path);


    uint32_t size() const {
      BNG_VERIFY(mem_stats.total_count() == live_stats.total_count(), "");
      return live_stats.total_count();
    }

    operator bool() const {
      return !words_buf.empty();
    }

    bool operator !() const {
      return words_buf.empty();
    }

    bool load(const std::filesystem::path& path);

    void save(const std::filesystem::path& path);

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
      auto wi = uint32_t(&w - &words_buf.front());
      BNG_VERIFY(wi < words_count(), "word not in buffer!");
      return WordIdx(wi);
    }

    const Word* word(WordIdx i) const {
      return (i != WordIdx::kInvalid) ? (&words_buf.front() + uint32_t(i)) : nullptr;
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
    void load_preproc(const std::filesystem::path& path);

    void save_preproc(const std::filesystem::path& path) const;

    void load_word_list(const std::filesystem::path& path);

    void process_word_list();

    void collate_words();

    WordDB clone_packed() const;

    void cull_word(Word& word);

    static uint32_t header_size_bytes() {
      return offsetof(WordDB, text_buf);
    }

    uint32_t words_count() const {
      return uint32_t(mem_stats.total_count() + 26);
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
    TextStats mem_stats = {};
    WordIdx words_by_letter[26] = {};
    // members here and before serialized in .pre files
    TextBuf text_buf;
    std::vector<Word> words_buf;
    // members here and below do not get serialized.
    TextStats live_stats = {};
  };
} // namespace bng::word_db
