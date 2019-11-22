PERF_SRCS = $(wildcard perfs/*.cc)
PERFS = $(patsubst perfs/%.cc, perfs/perf_%, $(PERF_SRCS))

LDFLAGS = -Wl,-rpath=$(shell pwd)/lib -L$(shell pwd)/lib -lrdc -ldl -pthread

perfs/perf_% : perfs/%.cc
	$(CXX) $(INCFLAGS) $(CFLAGS) -DLOGGING_IMPLEMENTATION=1  -MM -MT perfs/$* $< >perfs/$*.d
	$(CXX) $(INCFLAGS) $(CFLAGS) -DLOGGING_IMPLEMENTATION=1 -o $@ $(filter %.cc %.a, $^) $(LDFLAGS)

-include perfs/*.d
