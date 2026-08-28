# Security Policy

## Project Status

This repository contains an **experimental engineering prototype** and is **not** certified or approved for use in:

- Safety-critical systems
- Medical devices or life-support equipment
- Real-time industrial control systems
- Aerospace or defence applications
- Any application where failure could cause personal injury or property damage

## Supported Versions

No stable release has been published. All code on `main` is experimental.

| Version | Supported |
|---------|-----------|
| `main` (pre-release) | Experimental only — no security guarantee |

## Reporting a Vulnerability

If you discover a security vulnerability in this repository, please **do not** open a public GitHub issue.

Instead, report it privately by emailing the repository owner or by using [GitHub's private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability) if enabled for this repository.

Please include:
- A description of the vulnerability and its potential impact.
- Steps to reproduce the issue.
- Any suggested mitigations.

We will acknowledge your report within 7 days and aim to address confirmed vulnerabilities within 30 days.

## Deployment Guidance

Before deploying any part of this accelerator to hardware:
1. Obtain a full synthesis report and verify timing closure.
2. Have the design reviewed by a qualified hardware engineer.
3. Conduct hardware-in-the-loop testing with representative workloads.
4. Consult applicable safety and regulatory standards for your target application.
5. Obtain legal and engineering sign-off before use in any safety-relevant context.
