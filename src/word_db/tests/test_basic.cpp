#include "word_db.h"
#include "test_harness/test_harness.h"

BNG_TEST(test_1, {
	BT_CHECK(true);
});

BNG_TEST(test_2, {
	BT_CHECK(!false);
});
