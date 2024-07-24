#include "word_db_std.h"
#include <algorithm>
#include <sstream>
#include <fstream>

namespace bng::word_db_std {
  //
  // Word
  //

  size_t Word::read_str(const std::string& buf, size_t offset) {
    begin = uint32_t(offset);
    bool has_double = false;
    auto i = offset;
    auto e = buf.size();
    for (; i < e && !is_end(buf[i]); ++i) {
      has_double = has_double || (i > offset && (buf[i] == buf[i - 1]));
      const auto lbit = letter_to_bit(buf[i]);
      letter_count += uint32_t(!(letters & lbit));
      letters |= lbit;
    }
    const auto char_count = uint32_t(i - offset);
    length = char_count;
    for (; i < e && is_end(buf[i]); ++i)
      ;
    BNG_VERIFY(letter_count <= 26, "accounting error. can't have %d unique letters", uint32_t(letter_count));
    is_dead = ((char_count > 0x3f) || length < 3 || letter_count > 12 || has_double);
    return i - offset;
  }

  void Word::letters_to_str(uint64_t letter_bits, char* pout) {
    for (uint32_t li = 0; letter_bits && li < 26; ++li) {
      const auto lb = (1ull << li);
      if ((lb & letter_bits)) {
        *pout++ = idx_to_letter(li);
        letter_bits ^= lb;
      }
    }
    *pout = 0;
  }


  //
  // TextBuf
  // 

  Word TextBuf::append(const TextBuf& src, const Word& w) {
    BNG_VERIFY(size() + w.length < capacity(), "");
    auto new_word = Word(w, uint32_t(size()));
    resize(size() + w.length);
    memcpy(data() + size() - w.length, src.ptr(w), w.length);
    return new_word;
  }

  //
  // SolutionSet
  //

  void SolutionSet::sort(const WordDB& wordDB) {
    std::sort(
      begin(),
      end(),
      [&wordDB](auto& lhs, auto& rhs) -> bool {
        return
          (wordDB.word(lhs.a)->length + wordDB.word(lhs.b)->length)
          <
          (wordDB.word(rhs.a)->length + wordDB.word(rhs.b)->length);
      }
    );
  }


  //
  // WordDB Public
  //

  WordDB::WordDB(const std::filesystem::path& path) {
    clear_words_by_letter();
    load(path);
  }

  bool WordDB::load(const std::filesystem::path& path) {
    BNG_VERIFY(!path.empty(), "invalid path");
    BNG_VERIFY(!*this, "already loaded.");

    if (path.extension() == ".stp") {
      load_preproc(path);
    }
    else if (path.extension() == ".txt") {
      load_word_list(path);
    }
    else {
      auto pstr = path.generic_string();
      BNG_VERIFY(false, "%s has unknown extension. must be .txt or .stp", pstr.c_str());
    }
    return *this;
  }

  void WordDB::save(const std::filesystem::path& path) {
    BNG_VERIFY(!path.empty(), "invalid path");
    if (path.extension() == ".stp") {
      return save_preproc(path);
    }
    auto pstr = path.generic_string();
    BNG_VERIFY(false, "path %s has invalid extension, must be .stp", pstr.c_str());
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
        live_stats.word_counts[li] = 0;
        live_stats.size_bytes[li] = 0;
        continue;
      }

      // no words start with this letter.
      if (words_by_letter[li] == WordIdx::kInvalid) {
        continue;
      }

      for (auto wi = size_t(li); words_buf[wi]; ++wi) {
        auto& w = words_buf[wi];
        // check for use of unavailable letters
        if ((w.letters | all_letters) != all_letters) {
          cull_word(w);
          continue;
        }
        bool double_tap = false;
        for (auto sp = str(w), se = str(w) + w.length - 1; !double_tap && sp < se; ++sp) {
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
          cull_word(w);
          continue;
        }
      }
    }

    *this = clone_packed();
  }

  SolutionSet WordDB::solve(const SideSet& sides) const {
    uint32_t all_letters = 0;
    uint32_t all_letter_count = 0;
    char letters_str[27] = {};

    for (const auto& s : sides) {
      if (s.letter_count != 3) {
        auto si = uint32_t(intptr_t(&s - sides.front()));
        s.get_letters_str(letters_str);
        BNG_PRINT("side[%d] %s is not 3 letters.\n",
          si + 1, letters_str);
        return SolutionSet();
      }
      all_letters |= uint32_t(s.letters);
    }
    for (auto lbits = all_letters; lbits; ++all_letter_count, lbits &= (lbits - 1))
      ;
    if (all_letter_count != 12) {
      Word::letters_to_str(all_letters, letters_str);
      BNG_PRINT("puzzle must have 12 unique letters, not %d (%s)\n",
        all_letter_count, letters_str);
      return SolutionSet();
    }

    SolutionSet solutions(size() / 2);

    // run through all letters used in the puzzle
    for (uint32_t ali = 0; ali < 26; ++ali) {
      const auto alb = uint32_t(1u << ali);
      if (!(alb & all_letters)) {
        continue;
      }

      // run through all words starting with this letter - these are candidateA
      for (auto wpa = first_word(ali); wpa && *wpa; ++wpa) {
        // run through all words starting with the last letter of candidateA - these are candidateB
        const auto bli = last_letter_idx(*wpa);
        for (auto wpb = first_word(bli); wpb && *wpb; ++wpb) {
          const auto hit_letters = wpa->letters | wpb->letters;
          if (hit_letters == all_letters) {
            solutions.add(word_i(*wpa), word_i(*wpb));
          }
        }
      }
    }

    return solutions;
  }

  bool WordDB::is_equivalent(const WordDB& rhs) const {
    return
      text_buf.size() == rhs.text_buf.size() &&
      words_buf.size() == rhs.words_buf.size() &&
      !memcmp(&live_stats, &rhs.live_stats, sizeof(live_stats)) &&
      !memcmp(words_by_letter, rhs.words_by_letter, sizeof(words_by_letter)) &&
      !memcmp(words_buf.data(), rhs.words_buf.data(), words_buf.size()) &&
      !memcmp(text_buf.as_string().data(), rhs.text_buf.as_string().data(), text_buf.size());
  }

  //
  // WordDB Private
  //

  void WordDB::load_preproc(const std::filesystem::path& path) {
    BNG_VERIFY(!path.empty() && path.extension() == ".stp", "invalid path");
    std::ifstream fin(path, std::ifstream::binary);
    if (!fin.is_open()) {
      return;
    }
    fin.read((char*)&mem_stats, sizeof(mem_stats));
    fin.read((char*)&words_by_letter, sizeof(words_by_letter));
    size_t tb_size = 0, wb_size = 0;
    fin.read((char*)&tb_size, sizeof(tb_size));
    fin.read((char*)&wb_size, sizeof(wb_size));
    {
      std::string tb;
      tb.resize(tb_size);
      fin.read(tb.data(), std::streamsize(wb_size));
      text_buf = std::move(tb);
    }
    words_buf.resize(wb_size);
    fin.read((char*)words_buf.data(), std::streamsize(wb_size * sizeof(Word)));
    live_stats = mem_stats;
  }

  void WordDB::save_preproc(const std::filesystem::path& path) const {
    BNG_VERIFY(!path.empty() && path.extension() == ".stp", "");
    BNG_VERIFY(text_buf.size() == live_stats.total_size_bytes(), "");
    std::ofstream fout(path, std::ofstream::binary);
    BNG_VERIFY(fout.is_open(), "");
    if (!fout.is_open()) {
      return;
    }
    fout.write((const char*)&mem_stats, sizeof(mem_stats));
    fout.write((const char*)&words_by_letter, sizeof(words_by_letter));
    size_t tb_size = text_buf.size();
    size_t wb_size = words_buf.size();
    fout.write((const char*)&tb_size, sizeof(tb_size));
    fout.write((const char*)&wb_size, sizeof(wb_size));
    fout.write((const char*)text_buf.as_string().data(), std::streamsize(tb_size));
    fout.write((const char*)words_buf.data(), std::streamsize(wb_size * sizeof(Word)));
  }


  void WordDB::load_word_list(const std::filesystem::path& path) {
    BNG_VERIFY(!path.empty() && path.extension() == ".txt", "");

    text_buf = TextBuf();
    {
      std::stringstream readstr;
      {
        std::ifstream dict_file(path, std::ifstream::in);
        if (!dict_file.is_open()) {
          return;
        }
        readstr << dict_file.rdbuf();
      }
      text_buf = readstr.str();
    }

    process_word_list();
  }

  void WordDB::process_word_list() {
    mem_stats = text_buf.collect_stats();
    BNG_VERIFY(mem_stats, "");

    collate_words();
    *this = clone_packed();
  }

  TextStats TextBuf::collect_stats() const {
    BNG_VERIFY(*this, "");
    auto stats = TextStats{};
   
    for (size_t i = 0, e = size(); i < e; ) {
      const auto iw = i;
      auto li = Word::letter_to_idx((*this)[i]);
      ++stats.word_counts[li];
      // catch out of order dictionary
      BNG_VERIFY(li == 25 || !stats.word_counts[li + 1], "");
      for (; !Word::is_end((*this)[i]); ++i)
        ;
      for (; i < e && Word::is_end((*this)[i]); ++i)
        ;
      stats.size_bytes[li] += uint32_t(i - iw);
    }

    return stats;
  }

  void WordDB::collate_words() {
    BNG_VERIFY(words_buf.empty(), "");
    words_buf.reserve(words_count());

    size_t wi_row_start = words_buf.size();
    uint32_t row_live_count = 0;
    uint32_t row_live_size_bytes = 0;

    clear_words_by_letter();

    size_t i = 0;
    const auto& tb = text_buf.as_string();
    const size_t te = tb.size();

    for (; i < te; ) {
      auto li = Word::letter_to_idx(tb[i]);
      if (words_by_letter[li] == WordIdx::kInvalid) {
        if (li) {
          // null terminate
          words_buf.emplace_back();
          const auto row_total_count = uint32_t(words_buf.size() - wi_row_start); (void)row_total_count;
          BNG_VERIFY(row_total_count == mem_stats.word_counts[li - 1] + 1, "");
          live_stats.word_counts[li - 1] = row_live_count;
          live_stats.size_bytes[li - 1] = row_live_size_bytes;
          row_live_count = 0;
          row_live_size_bytes = 0;
          wi_row_start = words_buf.size();
        }
        // cache the start of the word list.
        words_by_letter[li] = WordIdx(uint32_t(wi_row_start));
      }
      i += words_buf.emplace_back().read_str(tb, i);

      if (!words_buf.back().is_dead) {
        row_live_size_bytes += uint32_t(words_buf.back().length);
        ++row_live_count;
      }
    }

    BNG_VERIFY(i == text_buf.size(), "");

    {
      // null terminate
      words_buf.emplace_back();
      const auto row_total_count = uint32_t(words_buf.size() - wi_row_start); (void)row_total_count;
      BNG_VERIFY(row_total_count == mem_stats.word_counts[25] + 1, "");
      live_stats.word_counts[25] = row_live_count;
      live_stats.size_bytes[25] = row_live_size_bytes;
    }

    for (uint32_t li = 0; li < 26; ++li) {
      if (!live_stats.word_counts[li]) {
        words_by_letter[li] = WordIdx::kInvalid;
      }
    }

    BNG_VERIFY(uint32_t(words_buf.size()) == words_count(), "");
    BNG_VERIFY(first_letter_idx(words_buf[words_buf.size() - 2]) == 25, "");
  }

  void WordDB::cull_word(Word& word) {
    auto li = first_letter_idx(word);
    BNG_VERIFY(live_stats.size_bytes[li] >= word.length, "");
    BNG_VERIFY(live_stats.word_counts[li], "");
    live_stats.size_bytes[li] -= uint32_t(word.length);
    --live_stats.word_counts[li];
    word.is_dead = true;
  }

  WordDB WordDB::clone_packed() const {
    const uint32_t live_size = live_stats.total_size_bytes();
    const uint32_t live_count = live_stats.total_count(); (void)live_count;
    BNG_VERIFY(
      *this &&
      live_size < text_buf.capacity() &&
      live_count < mem_stats.total_count(), "");

    WordDB out;

    out.text_buf = TextBuf(live_size);
    out.mem_stats = live_stats;
    out.live_stats = live_stats;
    out.words_buf.reserve(out.words_count());

    uint32_t live_row_count = 0; (void)live_row_count;

    for (uint32_t li = 0; li < 26; ++li) {
      if (!live_stats.word_counts[li]) {
        out.words_by_letter[li] = WordIdx::kInvalid;
        continue;
      }

      out.words_by_letter[li] = WordIdx(uint32_t(out.words_buf.size()));

      for (auto wi = size_t(words_by_letter[li]); words_buf[wi]; ++wi) {
        const auto& w = words_buf[wi];
        if (!w.is_dead) {
          Word wout = out.text_buf.append(text_buf, w);
          out.words_buf.emplace_back(wout);
        }
      }

      const auto row_count = uint32_t(out.words_buf.size() - uint32_t(out.words_by_letter[li])); (void)row_count;
      BNG_VERIFY(row_count == out.live_stats.word_counts[li], "");
      // null terminate
      out.words_buf.emplace_back();
      ++live_row_count;
    }

    const auto copy_count = uint32_t(out.words_buf.size()); (void)copy_count;
    BNG_VERIFY(copy_count == out.live_stats.total_count() + live_row_count, "");

    return out;
  }

  void WordDB::clear_words_by_letter() {
    for (auto& wbl : words_by_letter) {
      wbl = WordIdx::kInvalid;
    }
  }
}
