all: lib/game.js lib/grid.js lib/svg.js mcts/mcts.wasm Makefile lib/chartist-js/.git

clean:
	rm -f lib/*.js mcts/mcts.wasm mcts/mcts.js mcts/mcts.worker.js mcts/mcts.wast mcts/mcts.wasm.map

docker: all
	docker build -t mcts-tictactoe .

lib/game.js lib/grid.js lib/svg.js: lib/game.ts lib/grid.ts lib/svg.ts
	tsc lib/game.ts lib/grid.ts lib/svg.ts

mcts/mcts.wasm: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h
	#
	# USE_PTHREADS and PROC_COUNT must be set to single-threading due to Spectre mitigations!
	# If/When browsers support SharedArrayBuffer in WASM, this can be re-enabled.
	#
	emcc -Os -DPROC_COUNT=1 -lpthread -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s USE_PTHREADS=0 -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap"]' -s EXPORTED_FUNCTIONS='["_get_move", "_transposition_table_size", "_get_value"]' -std=c++17 -o mcts/mcts.js mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp

mcts-debug: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h
	#
	# USE_PTHREADS and PROC_COUNT must be set to single-threading due to Spectre mitigations!
	# If/When browsers support SharedArrayBuffer in WASM, this can be re-enabled.
	#
	# Compiling WASM binary in debug mode - this is much, much slower!
	#
	emcc --source-map-base "" -s DEMANGLE_SUPPORT=1 -s DISABLE_EXCEPTION_CATCHING=2 -s EXCEPTION_DEBUG=1 -s ASSERTIONS=1 -s SAFE_HEAP=1 -g -DPROC_COUNT=1 -lpthread -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s USE_PTHREADS=0 -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap"]' -s EXPORTED_FUNCTIONS='["_get_move", "_transposition_table_size"]' -std=c++17 -o mcts/mcts.js mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp

mcts/libmcts.so: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h
	#
	# This will compile with support for however many cores your system reports!
	#
	echo "Compiling with $$(grep -c ^processor /proc/cpuinfo)-thread support..."
	clang++ -DPROC_COUNT=$$(grep -c ^processor /proc/cpuinfo) -fPIC -lpthread -lm -std=c++17 -Ofast -shared -omcts/libmcts.so mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp

binary-test: mcts/board.cpp mcts/board.h mcts/emcc_interface.cpp mcts/mcts.cpp mcts/mcts.h
	clang++ -DPROC_COUNT=$$(grep -c ^processor /proc/cpuinfo) -lpthread -lm -std=c++17 -O0 -g -o mcts/bin mcts/mcts.cpp mcts/board.cpp mcts/emcc_interface.cpp

lib/chartist-js/.git:
	git submodule update --init --recursive
