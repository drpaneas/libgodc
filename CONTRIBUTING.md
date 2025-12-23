# Contributing to libgodc

First off, thank you for considering contributing to libgodc. It's people like you that make libgodc such a great tool.

When contributing to this repository, please first discuss the change you wish to make via issue, email, or any other method with the owners of this repository before making a change.

Please note we have a code of conduct, please follow it in all your interactions with the project.

## Current Focus

The project is currently focused on **making the runtime stable**. Core runtime work (garbage collection, goroutines, channels, scheduler) requires deep systems programming knowledge and careful testing.

## Easy Ways to Contribute

Not everyone wants to dive into runtime internals — and that's fine! An easy and valuable way to contribute is to **port more KOS and KOS-Ports functions to the Go wrapper library**.

This involves:
1. Finding a KOS function you want to use
2. Writing a Go wrapper in the `kos/` package
3. Testing it on real hardware or emulator

The workflow is straightforward and documented in the [KOS Wrappers](https://drpaneas.github.io/libgodc/reference/kos-wrappers.html) reference.

## Pull Request Process

1. Ensure any install or build dependencies are removed before the end of the layer when doing a build.
2. Update the README.md with details of changes to the interface, this includes new environment variables, exposed ports, useful file locations, and container parameters.
3. Increase the version numbers in any examples files and the README.md to the new version that this Pull Request would represent. The versioning scheme we use is [SemVer](http://semver.org/).
4. You may merge the Pull Request in once you have the sign-off of two other developers, or if you do not have permission to do that, you may request the second reviewer to merge it for you.

### Commit Titles

1. **Dependency updates**: Commits that include dependency updates should follow a pattern that includes `(deps)` in the message, such as:
   * `chore(deps): update lodash to version 4.17.21`

2. **New Features**: Commits introducing new features should start with `feat` and optionally include a scope in parentheses. Examples include:
   * `feat: add login feature`
   * `feat(auth): implement OAuth2`

3. **Security updates**: Security-related updates should start with `sec` and can optionally include a scope. Examples:
   * `sec: patch XSS vulnerability`
   * `sec(database): secure connection handling`

4. **Bug fixes**: Commits fixing bugs should start with `fix` or `refactor` and can optionally include a scope. Examples:
   * `fix: resolve memory leak issue`
   * `refactor: improve query performance`

5. **Documentation updates**: For commits that update documentation, the message should start with `doc` or `docs` and can optionally include a scope. Examples:
   * `docs: update README with new installation instructions`
   * `doc(cli): add usage examples`

6. **Build process updates**: Commits related to the build process or continuous integration should start with `build` or `ci` and can optionally include a scope. Examples:
   * `ci: add GitHub Actions workflow for CI`
   * `build: update Dockerfile for optimized image size`

7. **Other work**: This is a catch-all category for commits that do not fit the above patterns.

## Reporting Bugs

* A quick summary and/or background
* Steps to reproduce
  * Be specific!
  * Give sample code if you can.
* What you expected would happen
* What actually happens
* Notes (possibly including why you think this might be happening, or stuff you tried that didn't work)

## Feature Requests

Feature requests are welcome. But take a moment to find out whether your idea fits with the scope and aims of the project. It's up to *you* to make a strong case to convince the project's developers of the merits of this feature. Please provide as much detail and context as possible.

## Code of Conduct

Our [Code of Conduct](CODE_OF_CONDUCT.md) explains our expectations for all those that participate in our community, as well as the consequences for unacceptable behavior.

We invite you to help us make libgodc as good as it can be. We're looking forward to your contributions.

