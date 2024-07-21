#include "word_db.h"
#include "test_harness/test_harness.h"

using namespace bng::word_db;

static char dict_text[] =
	"ant\nantonym\n"
	"bean\nbearskin\n"
	"c\n"
	"d\n"
	"e\n"
	"f\n"
	"g\n"
	"hah\nheehaw\nhumdinger\n"
	"i\n"
	"j\n"
	"k\n"
	"l\n"
	"m\n"
	"n\n"
	"o\n"
	"p\n"
	"q\n"
	"r\n"
	"s\nsupercalifragilisticexpialidocious\n"
	"t\n"
	"u\n"
	"v\n"
	"w\n"
	"x\n"
	"y\n"
	"zebra\nzigzag\n";

void write_word_list() {
	File word_list("word_list.txt", "w");
	assert(word_list);
	fwrite(dict_text, sizeof(dict_text) - 1, 1, word_list);
}

BNG_TEST(test_load_list, {
	write_word_list();
	WordDB db("word_list.txt");
	BT_CHECK(db);
	});

BNG_TEST(test_save_load_preproc, {
	BT_CHECK(true);
	});

BNG_TEST(test_cull, {
	BT_CHECK(true);
	});
