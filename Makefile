.PHONY: build run test clean

build:
	mkdir -p build && cd build && cmake .. && make

run: build
	./build/redis-cli

test: build
	@echo "Starting redis-cli server..."

	@./build/redis-cli & \
	REDIS_PID=$$!; \
	sleep 1; \
	bash scripts/test_redis.sh; \
	kill $$REDIS_PID || true
