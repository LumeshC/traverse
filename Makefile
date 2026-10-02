CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic

.PHONY: all check samples demo clean

all: simulated_test run_realgraph

simulated_test: simulated_test.cpp graph_operations.cpp graph_simulator.cpp graph_operations.h graph_simulator.h instrumentation.h
	$(CXX) $(CXXFLAGS) -o simulated_test simulated_test.cpp graph_operations.cpp graph_simulator.cpp

run_realgraph: run_realgraph.cpp realgraph.cpp graph_operations.cpp graph_operations.h realgraph.h instrumentation.h
	$(CXX) $(CXXFLAGS) -o run_realgraph run_realgraph.cpp realgraph.cpp graph_operations.cpp

check: all
	./simulated_test check
	./run_realgraph --self-test

samples: simulated_test
	./simulated_test samples

demo:
	open demo/index.html

clean:
	rm -f simulated_test run_realgraph
