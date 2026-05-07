# Contributing

TelosZero Core is private during bootstrap. Contributions should go through pull
requests against `main` or a documented release branch.

## Development Rules

- Keep changes narrowly scoped.
- Preserve upstream attribution when cherry-picking.
- Do not make consensus or storage-format changes without a design note.
- Keep `nodeos`, `cleos`, and `keosd` compatibility unless a release plan says
  otherwise.
- Update packaging and operator docs when behavior affects installation,
  upgrades, or runtime configuration.

## Commit And PR Expectations

Pull requests should include:

- a short summary
- compatibility impact
- tests run
- upstream or Community Edition references, if any

For cherry-picks, include the source commit or PR URL in the PR description.

## Local Verification

At minimum, run syntax checks before opening a PR:

```bash
bash -n tools/tweak-deb.sh
sh -n scripts/postinst
sh -n scripts/prerm
perl -c tools/pinned.pl
python3 -m py_compile tests/TestHarness/Node.py tests/TestHarness/queries.py tests/TestHarness/testUtils.py tests/cli_test.py
```
