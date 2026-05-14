# Governance

TelosZero Core is maintained under the TelosNetwork GitHub organization.

## Project Stewardship

Maintainers should prioritize:

- preserving upstream history and attribution
- keeping runtime behavior compatible unless a change is explicitly reviewed as
  a protocol change
- documenting licensing, patch, build, and release provenance
- replacing inherited CI, release, and signing infrastructure with Telos-owned
  equivalents
- publishing clear operator guidance for every production release

## Maintainer Authority

Maintainers are responsible for reviewing code, release artifacts, security
updates, and public communications. No single maintainer should be the only
person able to merge, release, or administer the repository.

The initial code owner team is `@telosnetwork/telos-core-devs`.

## Branch Policy

`main` is the development branch and should not be used directly in production.

Production release branches use the `release/*-teloszero` naming pattern.
Release branches require:

- a documented release plan
- passing CI for build and test targets selected for that release
- maintainer approval
- documented artifact integrity checks
- upgrade and rollback notes for node operators

## Protocol Changes

Consensus, serialization, database format, protocol feature, and network
compatibility changes require an explicit design note before implementation.
The design note should state expected compatibility, activation requirements,
test coverage, and operator impact.

## Upstream And Community Edition Changes

`AntelopeIO/spring` is tracked as `upstream`. Spring Community Edition may be
tracked as `ce` as a reference source for legal/process cues and useful
patches. Changes from either source should be reviewed and merged
intentionally; bulk imports should be avoided unless they are part of a
documented release sync.
