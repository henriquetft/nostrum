#!/usr/bin/env python3

"""
Nostr Event Bulk Uploader
------------------------------------------------------------------
An asynchronous utility designed to broadcast Nostr events 
(from a JSONL file) to a relay using multiple concurrent workers.

Usage:
    --relay <relay_url> --file <input_file> --workers <count>
"""

import asyncio
import json
import argparse
import time
import websockets

MAX_SIZE = 131072

def load_events(filename):
    events = []
    with open(filename, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if len(line.encode()) >= MAX_SIZE:
                continue
            events.append(json.loads(line))
    return events


async def wait_ok(ws, event_id, timeout=15):
    while True:
        raw = await asyncio.wait_for(ws.recv(), timeout=timeout)
        msg = json.loads(raw)

        # expected: ["OK", event_id, accepted, message]
        if (isinstance(msg, list)
            and len(msg) >= 2
            and msg[0] == "OK"
            and msg[1] == event_id
        ):
            return msg


async def worker(worker_id, relay_url, queue, stats):
    while True:
        try:
            async with websockets.connect(
                relay_url,
                max_size=None,
                ping_interval=None
            ) as ws:

                while True:
                    item = await queue.get()

                    if item is None:
                        queue.task_done()
                        return

                    index, event = item
                    event_id = event["id"]

                    msg = json.dumps(["EVENT", event], separators=(",", ":"))
                    await ws.send(msg)

                    # waits specifically for this event response
                    await wait_ok(ws, event_id)

                    stats["sent"] += 1

                    if stats["sent"] % 1000 == 0:
                        print("sent:", stats["sent"])

                    queue.task_done()

        except Exception as exc:
            print(f"[worker {worker_id}] reconnecting: {exc}")
            await asyncio.sleep(1)


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--relay", required=True)
    parser.add_argument("--file", required=True)
    parser.add_argument("--workers", type=int, default=1)
    args = parser.parse_args()

    events = load_events(args.file)

    q = asyncio.Queue()

    for i, ev in enumerate(events, start=1):
        await q.put((i, ev))

    for _ in range(args.workers):
        await q.put(None)

    stats = {"sent": 0}

    start = time.perf_counter()

    tasks = [
        asyncio.create_task(worker(i + 1, args.relay, q, stats))
        for i in range(args.workers)
    ]

    await q.join()
    await asyncio.gather(*tasks)

    elapsed = time.perf_counter() - start

    print("\nRESULTS")
    print("sent:", stats["sent"], "events")
    print("time:", round(elapsed, 2), "s")
    print("rate:", round(stats["sent"] / elapsed, 2), "events/s")


if __name__ == "__main__":
    asyncio.run(main())