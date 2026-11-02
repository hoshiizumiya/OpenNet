## Contribute Your Code

### OpenNet Project

1. Download and install [Visual Studio 2026 Community](https://visualstudio.microsoft.com/downloads/).
   - No need to select workloads; Visual Studio will handle it automatically.
   - Close Visual Studio Installer to ensure a smooth installation experience for workloads.
2. Fork this repository and clone the project to your local device. Make sure with a good network. Or by this.<img width="400" alt="image" src="https://github.com/user-attachments/assets/6d861b6b-d3b7-48d0-8241-257c9b7ead54" />

3. Open the slnx file with your `OpenNet.slnx` Visual Studio will prompt you to install the necessary workloads, closing and reopening automatically.

### Start Pull Request

- All code-related changes from authors' own branches will directly to be merged to `master` branch to ensure a clear and high efficient branch management.
- Please use [keywords](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/using-keywords-in-issues-and-pull-requests) to link your PR or commits with issues, so issues can be automatically closed once commits are merged into `master` branch.

### Test Binary Package

After push your own git to your github, the github action will be triggerd and you can check the final .msix build file and install it. It's a release build version by msbuild. You can also build it by yourself with change build cofigreation to release and build the project.

## Start New Issue

To help users solve problems faster and increase developers' efficiency in solving problems, OpenNet provides detailed documentation to explain common problems and issue templates to guide users to report program problems by submitting issues.

Before submitting a new issue, you should check the following pages:

- [Current Opened BUG Report Issues](https://github.com/hoshiizumiya/OpenNet/issues?q=is%3Aissue+is%3Aopen+label%3ABUG)

When starting a new issue, please use the issue templates:

- Describe your issue in details to help developers to reproduce the issue
- Your description of reproduction should be a step-by-step story
- If your issue is about program crash
  - Remember to provide your Device ID
  - Check Windows Event Viewer, and attach associated `OpenNet` details in the issue body
  - Check your dump file location `%LOCALAPPDATA%\CrashDumps` and compress it to a zip file and upload to the github issue if your crash didn't get captured by the app unhandled exception report window

## Document Modification

OpenNet Document site is stored in repository [Millennium-Science-Technology-R-D-Inst/OpenNet.Docs](https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Docs), you can process the following steps to test the site in your local device:

1. Download and install [NodeJS 26](https://nodejs.org/en/download/)
2. Clone the repository
3. Run `pnpm install` in the root directory of the document project
4. Run `pnpm run docs:dev` to start test on 5173 port

### Localization

OpenNet.Docs project structure is designed as multiple languages site. Each language has its independent folder under `docs` directory.

**If you wish to add another language document, you can [start an issue in document repository](https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Docs/issues/new/choose) to ask developer to setup an environment for you, or you can process the following steps by yourself:** 

1. make a copy of `zh` folder, rename the new folder as the new language's code
2. 
