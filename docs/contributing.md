---
title: Contributing
description: Guidelines for contributing to Incogine.
sidebar_position: 15
tags: [contributing, community]
---

# Contributing to Incogine

Thank you for your interest in contributing to **Incogine**! This document outlines the guidelines and best practices for contributing to the project.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](../CODE_OF_CONDUCT.md).

## License Agreement

By contributing to **Incogine**, you agree that your contributions will be licensed under the **Mozilla Public License 2.0 (MPL-2.0)**.

### Important notes on attribution and license

1. **Attribution**: All contributions must retain `leafstudiosDot` and `Incogine`. These must be prominently displayed in the source code, documentation, and compiled binaries where applicable.
2. **Non-removal clause**: `leafstudiosDot`, trademark, and the term `Incogine` cannot be removed from any derivative works or compiled versions.
3. **Same license**: Any modifications to the base code (the core engine) must be shared under the same MPL-2.0 license. Game-specific code that uses the engine can be licensed differently and does not need to be shared.

## How to contribute

### 1. Reporting issues

If you find a bug or have a feature request, please open an issue on GitHub. When submitting an issue, provide as much detail as possible to help us understand and replicate the problem.

### 2. Making changes

1. **Fork the repository**: Start by forking the repository on GitHub — this creates a copy of the project where you can make your changes.
2. **Create a branch**: Create a new branch for your work with a descriptive name, e.g. `feature/amazing-features`, `bug/fix-issue-[no of issue]`.
3. **Write clear, concise code**: Follow the project's coding standards and guidelines. Ensure your code is well-documented, with comments where necessary.
4. **Test your changes**: Before submitting, make sure your code is properly tested and does not break any existing functionality.

### 3. Submitting changes

1. **Commit your changes**: Write clear and concise commit messages. Ensure each commit is focused on a single issue or improvement.
2. **Push to your fork**: Push your changes to your forked repository.
3. **Open a pull request**: Submit a pull request to the main repository. In the description, explain what you changed and why, and reference any issues the PR addresses.

### 4. Code review

Your pull request will be reviewed by project maintainers. They may suggest changes or ask questions to clarify your contribution. Please be responsive to feedback and make the requested changes.

### 5. Merging changes

Once your pull request is reviewed and approved by the maintainers, it will be merged into `main` or any requested branch. Thank you for your contribution!

## Acknowledgments

We deeply appreciate your contributions to **Incogine**. By following these guidelines, you help us maintain a high-quality codebase and a welcoming community for everyone that uses Incogine.

## Contact

If you have any questions about contributing, reach out to any project maintainer (@TuxPenguin09) or open an issue on GitHub.

## Customization notes

- If you are going to start an Incogine project, replace **Incogine** with the name of your project in `src/project.xml` at the key `name` (single token, no spaces — see [Project XML](./project-xml.md)).
- The key `incogine_version` at `src/project.xml` should not be modified by you — updating the base's source code will update that key's value.
- If you have a separate Code of Conduct, link to it.