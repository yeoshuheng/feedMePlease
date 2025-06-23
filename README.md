### Crypto SPOT/PERP Market Data Feed Handler

## Introduction 

Runs on three threads, each pinned to a different CPU core. Coordination between threads is completely lock-free.

- Handler Thread: Handles socket streams asynchronously from multiple venues, unpacks it into a common structure (TickData) and pushes it onto a lock-free tick queue.
- Consumer Thread: Polls from the tick queue, aggregates it and does a CAS update for the latest snapshot atomic.
- Snapshot Thread: Pulls the latest snapshot on a user defined frequency, handles version buffering & discard logic for stale data / skew delays.

<img width="480" alt="Screenshot 2025-06-17 at 10 02 23 PM" src="https://github.com/user-attachments/assets/3f4e1f8e-ba64-45ec-a41f-2a1ab6652e08" />

## To do:
- Add feeds for Hyperliquid.
- Clean up code to headers + source.
