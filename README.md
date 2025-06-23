### Crypto SPOT/PERP Market Data Feed Handler

## Introduction & Features

Runs on three threads, each pinned to a different CPU core. Coordination between threads is completely lock-free.

- Handler Thread: Handles socket streams asynchronously from multiple venues, unpacks it into a common structure (TickData) and pushes it onto a lock-free tick queue.
  - Uses asio to handle concurrent server connections within a single thread via callback functions.
- Consumer Thread: Polls from the tick queue, aggregates it and does a CAS update for the latest snapshot atomic.
  - Atomic pointer: Used to set the latest tick data for snapshots without using locks.
- Snapshot Thread: Pulls the latest snapshot on a user defined frequency, handles double version buffering & timestamp checks to prevent stale data.
  - Spin waits: Used to immediately handle snapshots based on user frequency.
  - Double version buffer: Prevents read on concurrent partial writes, this is done by making sure version before and after read operation is consistent.

<img width="480" alt="Screenshot 2025-06-17 at 10 02 23 PM" src="https://github.com/user-attachments/assets/3f4e1f8e-ba64-45ec-a41f-2a1ab6652e08" />

## To do:
- Add feeds for Hyperliquid.
- Clean up code to headers + source.
