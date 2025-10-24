# === Robust Makefile for macOS + SDL2 (no fixed src/ path) ===
CC = clang
CFLAGS = -Wall -I. -I./include -I/opt/homebrew/include -D_THREAD_SAFE
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer

# 현재 폴더 이하 모든 .c를 재귀적으로 수집 (build/, .git/ 제외)
SRC := $(shell find . -type f -name '*.c' ! -path './build/*' ! -path './.git/*')

OUT = growing

.PHONY: all run clean files check

all: check
	@echo "Compiling source files..."
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS)
	@echo "✅ Build complete!"

run: all
	./$(OUT)

clean:
	rm -f $(OUT)
	@echo "🧹 Clean complete."

files:
	@echo "Detected C sources:"
	@printf "  %s\n" $(SRC)

# 빌드 전에 main 존재와 소스 유효성 체크
check:
	@if [ -z "$(strip $(SRC))" ]; then \
		echo "❌ No .c files found under current folder."; \
		echo "   Open the correct project folder in VS Code or add sources."; \
		exit 1; \
	fi
	@if ! grep -R -n "int[[:space:]]\+main[[:space:]]*(" $(SRC) >/dev/null 2>&1; then \
		echo "❌ No 'main' function found in any .c file."; \
		echo "   Add an entry point (e.g., main.c) or tell me where it is."; \
		exit 1; \
	fi
