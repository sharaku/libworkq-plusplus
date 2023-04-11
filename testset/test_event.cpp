#include <gtest/gtest.h>
#include "../include/workq++.hpp"

TEST(test_worqpp_event, event)
{
	RecordProperty("Test",
		"Create libsharaku::workque::event."
	);
	RecordProperty("Expected",
		"- The nice value specified in the constructor can be obtained by casting nice_t\n"
		"- When executing operator(), the callback specified in the constructor is called with the specified arguments."
	);

	int called = 0;
	int arg1 = 0;
	std::function<void(int)> func = [&called, &arg1](int a){ called++; arg1 = a;};
	libsharaku::workque::event ev (libsharaku::workque::nice_t(13), func, 97);
	libsharaku::workque::nice_t nice = libsharaku::workque::nice_t(ev);

	ev();

	EXPECT_EQ(13, nice);
	EXPECT_EQ(1, called);
	EXPECT_EQ(97, arg1);
}
