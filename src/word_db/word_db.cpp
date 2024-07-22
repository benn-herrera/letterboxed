#include "word_db.h"
#include <algorithm>

namespace bng::word_db {
  //
  // DictBuf
  // 

  TextBuf::TextBuf(uint32_t sz) {
    if (sz) {
      _text = new char[sz + 2];
      _capacity = sz;
      _text[0] = _text[sz] = _text[sz + 1] = 0;
    }
  }

  Word TextBuf::append(const TextBuf& src, const Word& w) {
    BNG_VERIFY(_size + w.length < _capacity, "");
    auto new_word = Word(w, _size);
    memcpy(_text + _size, src.ptr(w), w.length + 1);
    _size += uint32_t(w.length + 1);
    return new_word;
  }


  //
  // Word
  //

  uint32_t Word::read_str(const char* buf_start, const char* p) {
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


  //
  // SolutionSet
  //

  void SolutionSet::sort(const WordDB& wordDB) {
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


  //
  // WordDB Public
  //

  bool WordDB::load(const char* path) {
    BNG_VERIFY(path && *path, "");
    auto p = path + (strlen(path) - 4);
    const auto ext =
      (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    switch (ext) {
    case '.pre': load_preproc(path); break;
    case '.txt': load_word_list(path); break;
    default:
      BNG_VERIFY(false, "unknown extension. must be .txt or .pre");
      return false;
    }

    for (uint32_t i = 0; i < 26; ++i) {
      if (live_counts.word_counts[i]) {
        BNG_VERIFY(first_letter_idx(*first_word(i)) == i, "inconsistent data");
        BNG_VERIFY(first_letter_idx(*last_word(i)) == i, "inconsistent data");
      }
      else {
        BNG_VERIFY(words_by_letter[i] == WordIdx::kInvalid, "inconsitent metadata");
      }
    }

    return true;
  }

  void WordDB::save(const char* path) {
    BNG_VERIFY(path && *path, "");
    auto p = path + (strlen(path) - 4);
    const auto ext =
      (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    switch (ext) {
    case '.pre': return save_preproc(path);
    }
    BNG_VERIFY(false, "extension must be .pre");
  }

  void WordDB::cull(const SideSet& sides) {
    uint32_t all_letters = 0;
    for (auto s : sides) {
      all_letters |= s.letters;
    }

    for (uint32_t li = 0; li < 26; ++li) {
      const auto lb = uint32_t(1u << li);

      // letter not in puzzle
      if (!(lb & all_letters)) {
        words_by_letter[li] = WordIdx::kInvalid;
        live_counts.word_counts[li] = 0;
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

  //
  // WordDB Private
  //

  bool WordDB::load_preproc(const char* path) {
    BNG_VERIFY(path && *path && !strcmp(path + strlen(path) - 4, ".pre"), "");
    *this = WordDB();

    if (auto fin = File(path, "rb")) {
      if (fread(this, header_size_bytes(), 1, fin) != 1) {
        *this = WordDB();
        return false;
      }
      live_counts = mem_counts;

      words_buf = new Word[words_count()];
      if (fread(words_buf, words_size_bytes(), 1, fin) != 1) {
        BNG_VERIFY(false, "");
      }

      dict_buf = TextBuf(dict_buf.capacity());
      dict_buf.set_size(dict_buf.capacity());
      if (fread(dict_buf.begin(), dict_buf.capacity(), 1, fin) != 1) {
        BNG_VERIFY(false, "");
      }
    }
    return *this;
  }

  void WordDB::save_preproc(const char* path) const {
    BNG_VERIFY(path && *path && !strcmp(path + strlen(path) - 4, ".pre"), "");
    BNG_VERIFY(dict_buf.size() == live_counts.total_size_bytes() && dict_buf.size() == dict_buf.capacity(), "");
    auto fout = File(path, "wb");
    BNG_VERIFY(fout, "");
    if (fwrite(this, header_size_bytes(), 1, fout) != 1) {
      BNG_VERIFY(false, "");
    }
    if (fwrite(words_buf, words_size_bytes(), 1, fout) != 1) {
      BNG_VERIFY(false, "");
    }
    if (fwrite(dict_buf.begin(), dict_buf.size(), 1, fout) != 1) {
      BNG_VERIFY(false, "");
    }
  }


  bool WordDB::load_word_list(const char* path) {
    dict_buf = TextBuf();

    if (auto dict_file = File(path, "r")) {
      dict_buf = TextBuf(dict_file.size_bytes());
      if (size_t read_count = fread(dict_buf.begin(), 1, dict_buf.capacity(), dict_file.fp)) {
        if (read_count < dict_buf.capacity()) {
          memset(dict_buf.begin() + read_count, 0, dict_buf.capacity() - read_count);
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

  void WordDB::process_word_list() {
    mem_counts = dict_buf.collect_stats();
    BNG_VERIFY(mem_counts, "");

    collate_words();
    *this = clone_packed();
  }

  TextStats TextBuf::collect_stats() const {
    BNG_VERIFY(*this, "");
    auto stats = TextStats{};
   
    for (const char* p = begin(); *p; ) {
      const auto pw = p;
      auto li = Word::letter_to_idx(*p);
      ++stats.word_counts[li];
      // catch out of order dictionary
      BNG_VERIFY(li == 25 || !stats.word_counts[li + 1], "");
      for (; !Word::is_end(p); ++p)
        ;
      for (; *p && Word::is_end(p); ++p)
        ;
      stats.size_bytes[li] += uint32_t(p - pw);
    }

    return stats;
  }

  void WordDB::collate_words() {
    BNG_VERIFY(!words_buf, "");
    words_buf = new Word[words_count()];

    auto wp = words_buf;
    const Word* wp_row_start = wp;
    uint32_t row_live_count = 0;
    uint32_t row_live_size_bytes = 0;

    for (auto& wbl : words_by_letter) {
      wbl = WordIdx::kInvalid;
    }

    const char* p = dict_buf.begin();
    for (; *p; ++wp) {
      BNG_VERIFY(dict_buf.in_capacity(p), "");
      auto li = Word::letter_to_idx(*p);
      if (words_by_letter[li] == WordIdx::kInvalid) {
        if (li) {
          // null terminate
          *wp++ = Word();
          const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
          BNG_VERIFY(row_total_count == mem_counts.word_counts[li - 1] + 1, "");
          live_counts.word_counts[li - 1] = row_live_count;
          live_counts.size_bytes[li - 1] = row_live_size_bytes;
          row_live_count = 0;
          row_live_size_bytes = 0;
          wp_row_start = wp;
        }
        // cache the start of the word list.
        words_by_letter[li] = word_i(*wp);
      }
      p += wp->read_str(dict_buf, p);
      if (!wp->is_dead) {
        row_live_size_bytes += uint32_t(wp->length + 1);
        ++row_live_count;
      }
    }

    BNG_VERIFY(p == dict_buf.end(), "");

    {
      // null terminate
      *wp++ = Word();
      const auto row_total_count = uint32_t(wp - wp_row_start); (void)row_total_count;
      BNG_VERIFY(row_total_count == mem_counts.word_counts[25] + 1, "");
      live_counts.word_counts[25] = row_live_count;
      live_counts.size_bytes[25] = row_live_size_bytes;
    }

    for (uint32_t i = 0; i < 26; ++i) {
      if (!live_counts.word_counts[i]) {
        words_by_letter[i] = WordIdx::kInvalid;
      }
    }

    BNG_VERIFY(uint32_t(wp - words_buf) == words_count(), "");
    BNG_VERIFY(first_letter_idx(*(wp - 2)) == 25, "");
  }

  void WordDB::cull_word(Word& word) {
    auto li = first_letter_idx(word);
    BNG_VERIFY(live_counts.size_bytes[li] > word.length, "");
    live_counts.size_bytes[li] -= uint32_t(word.length + 1);
    BNG_VERIFY(live_counts.word_counts[li], "");
    --live_counts.word_counts[li];
    word.is_dead = true;
  }

  WordDB WordDB::clone_packed() const {
    const uint32_t live_size = live_counts.total_size_bytes();
    const uint32_t live_count = live_counts.total_count();
    BNG_VERIFY(
      *this &&
      live_size < dict_buf.capacity() &&
      live_count < mem_counts.total_count(), "");

    WordDB out;

    out.dict_buf = TextBuf(live_size);
    out.mem_counts = out.live_counts = live_counts;
    out.words_buf = new Word[out.words_count()];

    Word* wpo = out.words_buf;
    uint32_t live_row_count = 0; (void)live_row_count;

    for (uint32_t li = 0; li < 26; ++li) {
      if (!live_counts.word_counts[li]) {
        out.words_by_letter[li] = WordIdx::kInvalid;
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
      BNG_VERIFY(row_count == out.live_counts.word_counts[li], "");
      // null terminate
      *wpo++ = Word();
      ++live_row_count;
    }

    const auto copy_count = uint32_t(wpo - out.words_buf); (void)copy_count;
    BNG_VERIFY(copy_count == out.live_counts.total_count() + live_row_count, "");

    return out;
  }
}
