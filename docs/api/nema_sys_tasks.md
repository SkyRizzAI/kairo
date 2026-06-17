# nema:sys/tasks

> core (always available)  
> Package: `nema:sys@1.0`  

Async task scheduling + timers. Maps to TaskRunner (task_runner.h:33). @future — not yet exposed in JS v0. Basis for setTimeout/setInterval.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `submit(job: handle, done: handle)` | `void` | — |
| `timeout(ms: u32, callback: handle) → handle` | `handle` | — |
| `interval(ms: u32, callback: handle) → handle` | `handle` | — |
| `cancel(token: handle)` | `void` | — |

### `submit`

Submit a job to run on a worker thread; done() is called on the UI loop. @blocking marks that the job may block (http, wifi scan, ble enable).

**Parameters:**

- `job`: `handle`
- `done`: `handle`

### `timeout`

Schedule a callback after delay milliseconds (one-shot).

**Parameters:**

- `ms`: `u32`
- `callback`: `handle`

**Returns:** `handle`

### `interval`

Schedule a repeating callback every interval milliseconds.

**Parameters:**

- `ms`: `u32`
- `callback`: `handle`

**Returns:** `handle`

### `cancel`

Cancel a pending timeout/interval by handle.

**Parameters:**

- `token`: `handle`
