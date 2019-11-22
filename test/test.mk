TEST_SRCS = $(wildcard test/*.cc)
TESTS = $(patsubst test/%.cc, test/test_%, $(TEST_SRCS))

LDFLAGS += -Wl,-rpath=$(shell pwd)/lib -L$(shell pwd)/lib -lrdc -ldl -libverbs -pthread

test/test_% : test/%.cc $(SLIB)
		$(CXX) $(INCFLAGS) $(CFLAGS) -DLOGGING_IMPLEMENTATION=1  -MM -MT test/$* $< >test/$*.d
			$(CXX) $(INCFLAGS) $(CFLAGS) -DLOGGING_IMPLEMENTATION=1 -o $@ $(filter %.cc %.a, $^) $(LDFLAGS)

-include test/*.d
