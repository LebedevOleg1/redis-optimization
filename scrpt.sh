#!/usr/bin/env bash
# load_test.sh: Script to measure performance before and after cache warming using wrk and redis-cli.

# Configuration: adjust endpoint and redis-cli command if needed.
ENDPOINT="http://127.0.0.1:18080/articles"
REDIS_CLI="redis-cli -p 6379"
WARMUP_CURL="curl -s"

# Number of threads, connections, and duration for wrk
WRK_THREADS=4
WRK_CONNECTIONS=100
WRK_DURATION="30s"

echo "=== Load Testing Script ==="
echo "Endpoint: $ENDPOINT"
echo "Redis CLI: $REDIS_CLI"
echo "wrk config: threads=$WRK_THREADS, connections=$WRK_CONNECTIONS, duration=$WRK_DURATION"

# 1. Clear cache
echo
echo ">>> Clearing cache key 'articles_all'..."
$REDIS_CLI DEL articles_all
echo ">>> Cache cleared."

# 2. Initial test (cache miss)
echo
echo ">>> Running initial load test (cache miss)..."
echo "Command: wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $ENDPOINT"
wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $ENDPOINT | tee initial_wrk.txt

# 3. Warm up cache
echo
echo ">>> Warming up cache by a single request..."
$WARMUP_CURL $ENDPOINT > /dev/null
echo ">>> Cache warmed."

# 4. Cached test (cache hit)
echo
echo ">>> Running load test after cache warming (cache hit)..."
echo "Command: wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $ENDPOINT"
wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $ENDPOINT | tee cached_wrk.txt

# 5. Display summary
echo
echo "=== Summary ==="
echo "Initial (cache miss) wrk results:"
cat initial_wrk.txt
echo
echo "Cached (cache hit) wrk results:"
cat cached_wrk.txt

# Optionally, test individual article endpoint:
echo
echo ">>> Testing single article endpoint (/article/1)"
TEST_ENDPOINT_ARTICLE="http://127.0.0.1:18080/article/1"
echo "Clearing cache key 'article:1'..."
$REDIS_CLI DEL article:1
echo "Initial test (miss) for /article/1:"
wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $TEST_ENDPOINT_ARTICLE | tee initial_article_wrk.txt
echo "Warming cache..."
$WARMUP_CURL $TEST_ENDPOINT_ARTICLE > /dev/null
echo "Cached test (hit) for /article/1:"
wrk -t$WRK_THREADS -c$WRK_CONNECTIONS -d$WRK_DURATION $TEST_ENDPOINT_ARTICLE | tee cached_article_wrk.txt
echo
echo "Article endpoint summary:"
echo "Initial /article/1:"
cat initial_article_wrk.txt
echo
echo "Cached /article/1:"
cat cached_article_wrk.txt

echo
echo "Load testing completed. Review the results in the output above or in the generated files:"
echo "  initial_wrk.txt, cached_wrk.txt, initial_article_wrk.txt, cached_article_wrk.txt"
