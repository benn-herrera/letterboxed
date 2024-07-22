#include "core.h"
#include "test_harness/test_harness.h"

using Data = uint32_t;
static Data uninit = 0xf00df00d;
struct Mover {
	BNG_DECL_NO_COPY(Mover);
	explicit Mover(Data* d = &uninit) : pdata(d) {}
	~Mover() { if (pdata && pdata != &uninit) { pdata[0] = 0xdeadbeef; } }
	Data* pdata;
};

BNG_TEST(test_move_copy, {
	{
		Data clean[] = {1};
		Mover src(clean);
		Mover dst;
		BT_CHECK(dst.pdata == &uninit && uninit == 0xf00df00d);
		dst = std::move(src);
		BT_CHECK(clean[0] == 1);
		BT_CHECK(dst.pdata == clean && src.pdata == nullptr);
	}
	{
		Data dead[] = {3};
		Mover src;
		BT_CHECK(src.pdata == &uninit && uninit == 0xf00df00d);
		Mover dst(dead);
		dst = std::move(src);
		BT_CHECK(dead[0] == 0xdeadbeef);
		BT_CHECK(src.pdata == nullptr && dst.pdata == &uninit && uninit == 0xf00df00d);
	}
	{
		Data clean[] = {1};
		Data dead[] = {3};
		Mover src(clean);
		Mover dst(dead);
		dst = std::move(src);
		BT_CHECK(clean[0] == 1);
		BT_CHECK(dead[0] == 0xdeadbeef);
		BT_CHECK(dst.pdata == clean && src.pdata == nullptr);
	}
});

BNG_TEST(test_move_constructor, {
	{
		Data clean[] = {1};
		Mover src(clean);
		Mover dst(std::move(src));
		BT_CHECK(clean[0] == 1);
		BT_CHECK(dst.pdata == clean && src.pdata == nullptr);
	}
	{
		Data clean[] = {1};
		Mover src(clean);
		Mover dst = std::move(src);
		BT_CHECK(clean[0] == 1);
		BT_CHECK(dst.pdata == clean && src.pdata == nullptr);
	}
	{
		Data clean[] = {1};
		auto dst = Mover(Mover(clean));
		BT_CHECK(clean[0] == 1);
		BT_CHECK(dst.pdata == clean);
	}
	});
