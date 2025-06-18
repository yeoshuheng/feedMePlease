### Crypto SPOT/PERP Market Data Feed Handler

## Introduction 

Runs on three threads, each pinned to a different CPU core. Coordination between threads is completely lock-free: transfer between handler to consumer via boost lock-free queue and CAS to coordinate latest snapshot.

<img width="480" alt="Screenshot 2025-06-17 at 10 02 23 PM" src="https://github.com/user-attachments/assets/3f4e1f8e-ba64-45ec-a41f-2a1ab6652e08" />

## To do:
- Add feeds for Hyperliquid.
- Clean up code to headers + source.
