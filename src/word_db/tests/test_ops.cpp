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
}
BNG_END_TEST()

BNG_BEGIN_TEST(load_list) {
	write_word_list();
	WordDB db("word_list.txt");
	BT_CHECK(db);
}

//
// TODO: test cull, save and load preproc
//

BNG_END_TEST()

