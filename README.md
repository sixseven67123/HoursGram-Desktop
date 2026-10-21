# HoursGram Desktop

HoursGram Desktop is a native C++/Qt Telegram client fork based on AyuGram Desktop 7.0.9 and Telegram Desktop. The application and its Windows package are branded as HoursGram.

## Windows build

The repository includes a GitHub Actions workflow that builds the x64 Windows client and uploads the build output as a workflow artifact. Open the **Actions** tab, select a completed `HoursGram Desktop Windows Build` run, and download the `HoursGram-Windows-x64` artifact. Builds are not published to GitHub Releases automatically.

For a local build, follow [the Windows build guide](docs/building-win.md).

```powershell
git clone --recursive https://github.com/sixseven67123/HoursGram-Desktop.git hoursgram
```

## Source and attribution

HoursGram is an unofficial Telegram client. This repository preserves the original license and copyright notices for AyuGram Desktop and Telegram Desktop. See [LICENSE](LICENSE) and the upstream projects: [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) and [Telegram Desktop](https://github.com/telegramdesktop/tdesktop).
