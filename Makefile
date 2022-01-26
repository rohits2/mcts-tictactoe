WCXX=emcc
WWASM=-s WASM=1 -s ALLOW_MEMORY_GROWTH=1
WPTHREADS=-lpthread -s USE_PTHREADS=1 -s PROXY_TO_PTHREAD

all: lib/game.js lib/grid.js lib/svg.js mcts/mcts.wasm Makefile lib/chartist-js/.git

clean:
	rm -f lib/*.js mcts/mcts.wasm mcts/mcts.js mcts/mcts.worker.js mcts/mcts.wast mcts/mcts.wasm.map

docker: all
	docker build -t mcts-tictactoe .

lib/game.js lib/grid.js lib/svg.js: lib/game.ts lib/grid.ts lib/svg.ts
	tsc lib/game.ts lib/grid.ts lib/svg.ts

mcts/mcts.wasm mcts/mcts.worker.js mcts/mcts.js: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h mcts/worker.h mcts/worker.cpp
	emcc -O3 -DPROC_COUNT=4 $(WPTHREADS) $(WWASM) -s 'EXPORTED_RUNTIME_METHODS=["cppall", "cwrap"]' -s EXPORTED_FUNCTIONS='["_get_move", "_transposition_table_size", "_get_value", "_main"]' -std=c++17 -o mcts/mcts.js mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp mcts/worker.cpp

mcts-debug: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h mcts/worker.h mcts/worker.cpp
	emcc $(WPTHREADS) $(WWASM) --source-map-base "" -s DEMANGLE_SUPPORT=1 -s EXCEPTION_DEBUG=1 -s ASSERTIONS=1 -s SAFE_HEAP=1 -g -DPROC_COUNT=1 -s 'EXPORTED_RUNTIME_METHODS=["cppall", "cwrap"]' -s EXPORTED_FUNCTIONS='["_get_move", "_transposition_table_size", "_get_value", "_main"]' -std=c++17 -o mcts/mcts.js mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp mcts/worker.cpp

mcts/libmcts.so: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h mcts/worker.h mcts/worker.cpp
	#
	# This will compile with support for however many cores your system reports!
	#
	echo "Compiling with $$(grep -c ^processor /proc/cpuinfo)-thread support..."
	clang++ -DPROC_COUNT=$$(grep -c ^processor /proc/cpuinfo) -fPIC -lpthread -lm -std=c++17 -Ofast -shared -omcts/libmcts.so mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp mcts/worker.cpp

binary-test: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h mcts/worker.h mcts/worker.cpp
	clang++ -DPROC_COUNT=$$(grep -c ^processor /proc/cpuinfo) -lpthread -lm -std=c++17 -O0 -g -o mcts/bin mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp mcts/worker.cpp

lib/chartist-js/.git:
	git submodule update --init --recursive
