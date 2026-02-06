# Contributing to logAnalyzer

First off, thank you for considering contributing to logAnalyzer! It's people like you that make logAnalyzer such a great tool.

## Where do I go from here?

If you've noticed a bug or have a feature request, [make one](https://github.com/your-organization/logAnalyzer/issues/new)! It's generally best if you get confirmation of your bug or approval for your feature request this way before starting to code.

### Fork & create a branch

If this is something you think you can fix, then [fork logAnalyzer](https://github.com/your-organization/logAnalyzer/fork) and create a branch with a descriptive name.

A good branch name would be (where issue #123 is the ticket you're working on):

```sh
git checkout -b 123-add-a-new-feature
```

### Running the Test Suite

To run the test suite, first ensure you have built the project with tests enabled (which is the default). Then, navigate to the `build` directory and execute `ctest`:

```bash
cd build
ctest --verbose
```

### Developer Tools

The following commands can be run from the `build` directory to assist with development and maintenance:

-   **Generate Documentation**:
    Generates HTML documentation using Doxygen. The output will be in `build/docs/html/`.
    ```bash
    cmake --build . --target doc
    ```

-   **Format Code**:
    Automatically formats the C++ source code using `clang-format` according to the project's style guidelines. This is a good step to run before committing your changes.
    ```bash
    cmake --build . --target format
    ```

### Implement your fix or feature

At this point, you're ready to make your changes! Feel free to ask for help; everyone is a beginner at first :smile_cat:

### Make a Pull Request

At this point, you should switch back to your master branch and make sure it's up to date with logAnalyzer's master branch.

```sh
git remote add upstream git@github.com:your-organization/logAnalyzer.git
git checkout master
git pull upstream master
```

Then update your feature branch from your local copy of master, and push it!

```sh
git checkout 123-add-a-new-feature
git rebase master
git push --force-with-lease origin 123-add-a-new-feature
```

Finally, go to GitHub and [make a Pull Request](https://github.com/your-organization/logAnalyzer/compare)

### Keeping your Pull Request updated

If a maintainer asks you to "rebase" your PR, they're saying that a lot of code has changed, and that you need to update your branch so it's easier to merge.

To learn more about rebasing and merging, check out this guide on [merging vs. rebasing](https://www.atlassian.com/git/tutorials/merging-vs-rebasing).

Once you've rebased your branch, you'll have to force push to update the PR.

```sh
git push --force-with-lease origin 123-add-a-new-feature
```
