# nema:sys/log

> core (always available)  
> Package: `nema:sys@1.0`  

System logging. Maps to Logger (log/logger.h:18–26). Apps log via a single function that takes a level string; the host Logger fans the call out to all registered sinks (ConsoleSink + MemorySink).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `log(level: string, tag: string, msg: string)` | `void` | — |

### `log`

Log a message at the given level. level ∈ {"trace", "debug", "info", "warn", "error", "fatal"}

**Parameters:**

- `level`: `string`
- `tag`: `string`
- `msg`: `string`
