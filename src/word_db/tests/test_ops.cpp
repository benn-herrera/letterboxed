#include "word_db.h"
#include "test_harness/test_harness.h"

using namespace bng::word_db;

static char dict_text[] =
	"ant\nantonym\n"
	"bean\nbearskin\n"
	"cat\n"
	"dog\n"
	"ear\n"
	"fit\n"
	"gab\n"
	"hah\nheehaw\nhumdinger\n"
	"ion\n"
	"jot\n"
	"kit\n"
	"lag\n"
	"manta\n"
	"nematode\n"
	"octopus\n"
	"penguin\n"
	"quiche\n"
	"ramen\n"
	"s\nsupercalifragilisticexpialidocious\n"
	"tan\n"
	"use\n"
	"vim\n"
	"wit\n"
	"xray\n"
	"yank\n"
	"zebra\nzephyr\nzigzag\n";

// bearskin -> nematode is one possible answer
const char* puzzle_sides[] = {
	"btn", "akd", "oes", "mir"
};

void write_word_list() {
	File word_list("word_list.txt", "w");
	assert(word_list);
	fwrite(dict_text, sizeof(dict_text) - 1, 1, word_list);
}

BNG_BEGIN_TEST(dict_counts) {
	TextBuf db(sizeof(dict_text) - 1);
	memcpy(db.end(), dict_text, sizeof(dict_text) - 1);
	db.set_size(db.capacity());

	TextStats stats = db.collect_stats();

	BT_CHECK(stats.word_counts['a' -'a'] == 2);
	BT_CHECK(stats.word_counts['b' - 'a'] == 2);
	BT_CHECK(stats.word_counts['c' - 'a'] == 1);
	BT_CHECK(stats.word_counts['d' - 'a'] == 1);
	BT_CHECK(stats.word_counts['e' - 'a'] == 1);
	BT_CHECK(stats.word_counts['f' - 'a'] == 1);
	BT_CHECK(stats.word_counts['g' - 'a'] == 1);
	BT_CHECK(stats.word_counts['h' - 'a'] == 3);
	BT_CHECK(stats.word_counts['i' - 'a'] == 1);
	BT_CHECK(stats.word_counts['j' - 'a'] == 1);
	BT_CHECK(stats.word_counts['k' - 'a'] == 1);
	BT_CHECK(stats.word_counts['l' - 'a'] == 1);
	BT_CHECK(stats.word_counts['m' - 'a'] == 1);
	BT_CHECK(stats.word_counts['n' - 'a'] == 1);
	BT_CHECK(stats.word_counts['o' - 'a'] == 1);
	BT_CHECK(stats.word_counts['p' - 'a'] == 1);
	BT_CHECK(stats.word_counts['q' - 'a'] == 1);
	BT_CHECK(stats.word_counts['r' - 'a'] == 1);
	BT_CHECK(stats.word_counts['s' - 'a'] == 2);
	BT_CHECK(stats.word_counts['t' - 'a'] == 1);
	BT_CHECK(stats.word_counts['u' - 'a'] == 1);
	BT_CHECK(stats.word_counts['v' - 'a'] == 1);
	BT_CHECK(stats.word_counts['w' - 'a'] == 1);
	BT_CHECK(stats.word_counts['x' - 'a'] == 1);
	BT_CHECK(stats.word_counts['y' - 'a'] == 1);
	BT_CHECK(stats.word_counts['z' - 'a'] == 3);
	BT_CHECK(stats.total_count() == 33);

	BT_CHECK(stats.size_bytes['a' - 'a'] == 12);
	BT_CHECK(stats.size_bytes['b' - 'a'] == 14);
	BT_CHECK(stats.size_bytes['c' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['d' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['e' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['f' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['g' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['h' - 'a'] == 21);
	BT_CHECK(stats.size_bytes['i' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['j' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['k' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['l' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['m' - 'a'] == 6);
	BT_CHECK(stats.size_bytes['n' - 'a'] == 9);
	BT_CHECK(stats.size_bytes['o' - 'a'] == 8);
	BT_CHECK(stats.size_bytes['p' - 'a'] == 8);
	BT_CHECK(stats.size_bytes['q' - 'a'] == 7);
	BT_CHECK(stats.size_bytes['r' - 'a'] == 6);
	BT_CHECK(stats.size_bytes['s' - 'a'] == 37);
	BT_CHECK(stats.size_bytes['t' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['u' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['v' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['w' - 'a'] == 4);
	BT_CHECK(stats.size_bytes['x' - 'a'] == 5);
	BT_CHECK(stats.size_bytes['y' - 'a'] == 5);
	BT_CHECK(stats.size_bytes['z' - 'a'] == 20);
	BT_CHECK(stats.total_size_bytes() == uint32_t(sizeof(dict_text) - 1));
}
BNG_END_TEST()

BNG_BEGIN_TEST(load_and_solve) {
	write_word_list();
	{
		WordDB db("word_list.txt");
		BT_CHECK(db);
		db.save("words.pre");
		BT_CHECK(File("words.pre", "rb"));
		WordDB db2("words.pre");
		BT_CHECK(db2);
		BT_CHECK(db.is_equivalent(db2));
	}
	{
		WordDB db("words.pre");
		(uint32_t)unlink("words.pre");
		WordDB::SideSet sides = { 
			Word(puzzle_sides[0]), 
			Word(puzzle_sides[1]), 
			Word(puzzle_sides[2]), 
			Word(puzzle_sides[3])
		};

		// ensure the test data is not bogus.
		{
			auto puzzle_letters = uint32_t(sides[0].letters | sides[1].letters | sides[2].letters | sides[3].letters);
			uint32_t letter_count = 0;
			// ensure the test puzzle has 12 unique letters and the dictionary has all letters to solve it.
			for (; puzzle_letters; letter_count += puzzle_letters & 1, puzzle_letters >>= 1)
				;
			assert(letter_count == 12);
			uint32_t dict_letters = 0;
			for (auto c : dict_text) {
				dict_letters |= uint32_t(1u << uint32_t('a' - c));
			}
			assert((dict_letters & puzzle_letters) == puzzle_letters);
		}

		// verify the dictionary preprocessing did not eliminate words it should not have
		// and that they are sufficient to solve the puzzle
		{
			const uint32_t live_count = db.get_text_stats().total_count();
			// 3 less than the 33 in the dictionary. culled:
			// s - too short
			// heehaw - double letter
			// supercalifragilisticexpialidocious - more than 12 unique letters
			BT_CHECK(live_count == 30);

			uint32_t live_letters = 0;
			for (uint32_t i = 0, li = 0; li < live_count; ++i) {
				const auto& w = db.word(WordIdx(i));
				if (!w) {
					continue;
				}
				BT_CHECK(!w.is_dead);
				live_letters |= uint32_t(w.letters);
				++li;
				printf("%.*s\n", uint32_t(w.length), db.get_text_buf().ptr(w));
			}

			const auto puzzle_letters = uint32_t(sides[0].letters | sides[1].letters | sides[2].letters | sides[3].letters);
			BT_CHECK((live_letters & puzzle_letters) == puzzle_letters);
		}

		db.cull(sides);

		SolutionSet solutions = db.solve(sides);

		BT_CHECK(solutions.count == 1);

		auto ps = solutions.buf;
		auto& a = db.word(ps->a);
		auto& b = db.word(ps->b);
		auto sa = db.str(a); (void)sa;
		auto sb = db.str(b); (void)sb;

		BT_CHECK(!strncmp(sa, "bearskin", a.length));
		BT_CHECK(!strncmp(sb, "nematode", a.length));
	}
	unlink("word_list.txt");
}

BNG_END_TEST()

