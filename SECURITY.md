# Security Policy

## Reporting a vulnerability

If you discover a security vulnerability in Palanu, please report it
**privately** — do not open a public issue, pull request, or discussion.

Preferred channels:

- Use GitHub's [private vulnerability reporting](https://github.com/SkyRizzAI/kairo/security/advisories/new)
  ("Report a vulnerability" on the Security tab), or
- Email **viandwicyber@gmail.com** with the details.

Please include:

- A description of the issue and its impact.
- Steps to reproduce (proof-of-concept, affected board/target, firmware version
  or commit).
- Any suggested mitigation, if you have one.

## What to expect

- We aim to acknowledge your report within **5 business days**.
- We'll keep you updated on our assessment and a fix timeline.
- Once a fix is released, we're happy to credit you in the advisory unless you
  prefer to remain anonymous.

Please give us a reasonable window to ship a fix before any public disclosure.

## Scope

Palanu is firmware for embedded multi-tool devices. Reports about the runtime
core, drivers, the link/transport layer, the JS app sandbox, and the simulator
are all in scope. Issues in third-party vendored dependencies should be reported
upstream, but feel free to flag them here so we can pull in the fix.
