# Security Policy

## Reporting a vulnerability

**Do not open a public issue for security problems.**

Report privately via GitHub's "Report a vulnerability" feature on this
repository (Security tab), or contact the maintainer directly if you have
another verified channel. Include: affected version/commit, platform, and
reproduction steps. You'll get an acknowledgement; fixes land before public
disclosure whenever possible.

## Scope notes for Otoha specifically

* Otoha makes **no network connections** — anything transmitting data would
  be a critical bug by definition.
* FFmpeg is invoked as an external process with quoted, validated arguments;
  escapes from that quoting (e.g. quote characters in paths) are treated as
  security issues.
* Project files (`.otoha` JSON) are parsed from untrusted sources — parser
  crashes or memory-safety bugs there are in scope.
* Signing keys / certificates must never be committed; reports of leaked
  material in history are critical.

## Supported versions

Security fixes target the latest release tag only.
