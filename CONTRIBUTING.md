# Contributing to Incogine

Thank you for your interest in contributing to **Incogine**! This document outlines the guidelines and best practices for contributing to the project.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## License Agreement

By contributing to **Incogine**, you agree that your contributions will be licensed under the **Incogine License**. 

### Important Notes on Attribution and License:

1. **Attribution**: All contributions must retain `leafstudiosDot` and `Incogine`. These must be prominently displayed in the source code, documentation, and compiled binaries where applicable.
   
2. **Non-Removal Clause**: `leafstudiosDot`, trademark, and the term `Incogine` name cannot be removed from any derivative works or compiled versions.

3. **Same License**: Any modifications to the base code (the core engine) must be shared under the same MPL-2.0 license. However, game-specific code that uses the engine can be licensed differently and does not need to be shared.

## How to Contribute for the Incogine source code

### 1. Reporting Issues

If you find a bug or have a feature request, please open an issue on GitHub. When submitting an issue, please provide as much detail as possible to help us understand and replicate the problem.

### 2. Making Changes

1. **Fork the Repository**: Start by forking the repository on GitHub. This creates a copy of the project where you can make your changes.
   
2. **Create a Branch**: Create a new branch for your work. Use a descriptive name for the branch like `feature/amazing-features`, `bug/fix-issue-[no of issue]`.

3. **Write Clear, Concise Code**: Follow the project’s coding standards and guidelines. Ensure your code is well-documented, and include comments where necessary.

4. **Test Your Changes**: Before submitting, make sure your code is properly tested and that it does not break any existing functionality.

### 3. Submitting Changes

1. **Commit Your Changes**: Write clear and concise commit messages. Ensure each commit is focused on a single issue or improvement.

2. **Push to Your Fork**: Push your changes to your forked repository.

3. **Open a Pull Request**: Submit a pull request to the main repository. In your pull request description, explain what changes you made and why. Reference any issues your PR addresses.

### 4. Code Review

Your pull request will be reviewed by project maintainers. They may suggest changes or ask questions to clarify your contribution. Please be responsive to feedback and make the requested changes.

### 5. Merging Changes

Once your pull request was reviewed by project maintainers and is approved, it will be merged into the `main` or any requested branch. Thank you for your contribution!

## Acknowledgments

We deeply appreciate your contributions to **Incogine**. By following these guidelines, you help us maintain a high-quality codebase and a welcoming community for everyone that uses Incogine.

## Contact

If you have any questions about contributing, please reach out to any project maintainers, @TuxPenguin09 or open an issue on GitHub.

---

### Customization Notes:
- If you are going to start an Incogine project, Replace **Incogine** with the name of your project in `src/project.xml` at the key `name`.
- The key `incogine_version` at `src/project.xml` should not be modified by you, updating the base's source code will update that key's value.
- If you have a separate Code of Conduct, link to it.
