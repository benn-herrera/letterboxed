#include "word_db.h"
#include "test_harness/test_harness.h"

using namespace bng::word_db;

BNG_BEGIN_TEST(test_letters) {
	BT_CHECK(Word::letter_to_idx('a') == 0);
	BT_CHECK(Word::letter_to_idx('n') == 13);
	BT_CHECK(Word::letter_to_idx('z') == 25);

	BT_CHECK(Word::letter_to_bit('a') == 1);
	BT_CHECK(Word::letter_to_bit('n') == 0x2000);
	BT_CHECK(Word::letter_to_bit('z') == 0x2000000);

	BT_CHECK(Word::idx_to_letter(0)  == 'a');
	BT_CHECK(Word::idx_to_letter(13) == 'n');
	BT_CHECK(Word::idx_to_letter(25) == 'z');
}
BNG_END_TEST()


BNG_BEGIN_TEST(test_word) {
	{
		const char* txt = "xxxacefc";
		Word word;
		BT_CHECK(!word);
		word.read_str(txt, txt + 3);
		BT_CHECK(word);
		BT_CHECK(word.begin == 3);
		BT_CHECK(word.length == 5);
		BT_CHECK(word.letter_count == 4);
		BT_CHECK(word.letters == uint32_t(1u | (1u << 2) | (1u << 4) | (1u << 5)));
		BT_CHECK(word.is_dead == 0);

		auto word2 = Word(word, 7);
		BT_CHECK(word2.begin == 7);
		BT_CHECK(word2.length == word.length);
		BT_CHECK(word2.letter_count == word.letter_count);
		BT_CHECK(word2.letters == word.letters);
		BT_CHECK(word2.is_dead == word.is_dead);
	}
	{
		const char* txt = "aba";
		Word word;
		word.read_str(txt, txt);
		BT_CHECK(word.length == 3);
		// just long enough
		BT_CHECK(word.is_dead == 0);
	}
	{
	  const char* txt = "aceffc";
		Word word;
		word.read_str(txt, txt);
		BT_CHECK(word.begin == 0);
		BT_CHECK(word.length == 6);
		BT_CHECK(word.letter_count == 4);
		BT_CHECK(word.letters == uint32_t(1u | (1u << 2) | (1u << 4) | (1u << 5)));
		// doubled letter
		BT_CHECK(word.is_dead == 1);
  }
	{
		const char* txt = "ab";
		Word word;
		word.read_str(txt, txt);
		BT_CHECK(word.length == 2);
		// too short
		BT_CHECK(word.is_dead == 1);
	}
	{
		const char* txt = "abcdefghijklm";
		Word word;
		word.read_str(txt, txt);
		BT_CHECK(word.length == 13);
		// too many unique letters
		BT_CHECK(word.is_dead == 1);
	}
	{
		const char* txt = "abababababababababababababababababababababababababababababababab";
		Word word;
		word.read_str(txt, txt);
		// too long
		BT_CHECK(word.is_dead == 1);
	}
}
BNG_END_TEST()

