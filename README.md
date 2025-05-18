# Redis from scratch using the Redis Protocol Parser

## Run tests under `scripts` directory

```bash
➜ ./scripts/test_redis.sh
======================================
Test: PING
======================================
✅ Test passed

======================================
Test: ECHO hello
======================================
✅ Test passed

======================================
Test: SET foo bar
======================================
✅ Test passed

======================================
Test: GET foo
======================================
✅ Test passed

======================================
Test: SET exp PX 100
======================================
✅ Test passed

======================================
Test: GET exp (immediate)
======================================
✅ Test passed

======================================
Test: GET exp (after 200ms, should expire)
======================================
✅ Test passed

======================================
Tests completed: 7 passed, 0 failed
======================================
```
