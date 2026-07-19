# LoRa Link Security & Provisioning

This document describes the security model implemented to protect data transmitted by sensor nodes and ensure the integrity of the gateway.

The system relies on **AES-GCM-128** authenticated encryption, replay protection, and a secure configuration mechanism via Bluetooth Low Energy (BLE).

---

## 1. Authenticated Encryption: AES-128-GCM

The protocol uses **AES-GCM (Galois/Counter Mode)** with a 128-bit symmetric key.

Unlike classical encryption modes (such as CBC) which only provide confidentiality, GCM provides two cryptographic guarantees:

1.  **Confidentiality:** Sensor readings are encrypted. An attacker listening on the radio frequency cannot read the transmitted values.
2.  **Integrity / Authenticity:** A **8-byte (64-bit)** Message Authentication Code (MAC tag) is computed over the entire message. The gateway immediately rejects any message that has been tampered with in transit.

---

## 2. IV (Initialization Vector) Construction

A fundamental requirement of GCM mode is that **the same (Key, IV) pair must NEVER be reused**, or the algorithm's security breaks (XOR keystream leak).

To guarantee nonce uniqueness on microcontrollers that may restart frequently, the 12-byte (96-bit) IV is assembled dynamically as follows:

```
+-----------+----------------------+--------------------------+---------------------+
| node_id   | sequence_number      | node_random_id           | Padding             |
| (1 byte)  | (4 bytes)            | (4 bytes)                | (3 bytes)           |
+-----------+----------------------+--------------------------+---------------------+
```

*   **Unique node identifier (`node_id`):** Prevents two distinct nodes (even sharing the same key) from ever emitting with the same IV.
*   **Sequence number (`seq`):** Incremented by 1 on each packet. Ensures a unique IV for every packet within the same boot session.
*   **Random session ID (`node_random_id`):** Generated at boot using the ESP32's **hardware True Random Number Generator (TRNG)** via `esp_random()`. Even if the node loses power (which resets `seq` to `0`), the new IV will start with a completely different `node_random_id`, eliminating any risk of IV collision.

---

## 3. Authenticated Associated Data (AAD)

The 9-byte header (`node_id` + `seq` + `random_id`) must be transmitted in plaintext so the gateway can identify the sender and decode the packet.

To prevent an attacker from tampering with these plaintext bytes (e.g., forging the node ID or sequence number), the header is passed as **Authenticated Associated Data (AAD)** to the AES-GCM block:

```cpp
gcm.addAuthData(frame, HDR_SIZE);
```

If even a single bit of the header is modified during transmission, the GCM tag verification will fail on the gateway side and the packet will be immediately discarded.

---

## 4. Replay Protection

An attacker could intercept a valid frame (e.g., "Temperature: 25°C") and retransmit it multiple times to corrupt dashboards or flood the gateway.

To counter this:
1.  The sequence number `seq` is included in every packet.
2.  The gateway tracks the last valid sequence number processed for each node (`nodes[node_id].seq`).
3.  Any packet arriving with a sequence number less than or equal to the last received value is **immediately discarded** as a suspected replay.

---

## 5. AES Key Hardening

To avoid shipping a default hardcoded AES key in the firmware source (a critical vulnerability in case of source code theft or firmware decompilation):

*   **Zero initialization:** On the very first boot of a sensor node, the AES key in NVM (Preferences) is initialized to all zeros (`memset(0)`).
*   **Operational block:** The node refuses to transmit meaningful data until a non-zero AES key has been configured via BLE provisioning.

---

## 6. Secure BLE Provisioning

To deploy and provision a new node:

1.  The installer holds the **BOOT** button during startup to force entry into **BLE Configuration Mode**.
2.  The node starts its BLE service (secured by the `NimBLE-DataPipe` protocol).
3.  The installer connects to the node via the local Web Bluetooth admin interface.
4.  The interface allows securely configuring:
    *   The node's unique AES key.
    *   The node identifier (`node_id`).
    *   The node name (`node_name`).
    *   The LoRa transmission interval.
5.  Parameters are stored in the ESP32's non-volatile memory (NVM). On reboot, the node loads its unique key and begins secured transmissions.
