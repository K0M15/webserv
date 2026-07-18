import sys
import time
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor, as_completed

def send_request(url):
    """Sends a single synchronous request to be executed in a thread pool."""
    try:
        # Short timeout keeps threads from getting stuck on a dying server
        with urllib.request.urlopen(url, timeout=2) as response:
            return response.getcode()
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return None  # Connection failed entirely

def stress_test(url, duration):
    print(f"Starting stress test on {url}")
    print(f"Targeting ~100 requests/sec for {duration} seconds...\n")
    
    start_time = time.time()
    total_requests = 0
    results = {200: 0, "errors": 0, "failed_conn": 0}
    
    # A pool of 50 threads handles high concurrency efficiently via OS multiplexing
    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = []
        
        while time.time() - start_time < duration:
            loop_start = time.time()
            
            # Fire a batch of 10 requests rapidly
            for _ in range(10):
                futures.append(executor.submit(send_request, url))
                total_requests += 1
                
            # Clean up finished futures periodically to save memory
            if len(futures) > 200:
                for fut in [f for f in futures if f.done()]:
                    code = fut.result()
                    if code == 200:
                        results[200] += 1
                    elif code is None:
                        results["failed_conn"] += 1
                    else:
                        results["errors"] += 1
                    futures.remove(fut)
            
            # Throttle slightly: 10 requests every 0.1 seconds = ~100 req/sec
            elapsed = time.time() - loop_start
            time.sleep(max(0.0, 0.1 - elapsed))

        # Gather remaining results
        print("Finishing remaining requests...")
        for fut in as_completed(futures):
            code = fut.result()
            if code == 200:
                results[200] += 1
            elif code is None:
                results["failed_conn"] += 1
            else:
                results["errors"] += 1

    # Print Final Stats
    actual_duration = time.time() - start_time
    print("\n" + "="*30)
    print("      TEST RESULTS")
    print("="*30)
    print(f"Total Duration:     {actual_duration:.2f} seconds")
    print(f"Total Sent:         {total_requests} requests")
    print(f"Avg Rate:           {total_requests / actual_duration:.1f} req/sec")
    print(f"Successful (200):   {results[200]}")
    print(f"Server Errors (5xx): {results['errors']}")
    print(f"Connection Drops:   {results['failed_conn']}")

if __name__ == "__main__":
    # Check for the seconds argument
    if len(sys.argv) < 2:
        print("Error: Missing duration.")
        print("Usage: python stress_test.py [seconds]")
        print("Example: python stress_test.py 10")
        sys.exit(1)
        
    try:
        test_seconds = int(sys.argv[1])
    except ValueError:
        print("Error: Duration must be an integer (number of seconds).")
        sys.exit(1)

    # Change this to your server's target endpoint
    TARGET_URL = "http://localhost:8080"
    
    stress_test(TARGET_URL, test_seconds)