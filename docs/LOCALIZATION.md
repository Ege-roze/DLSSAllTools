# UI languages — v0.6

Supported: Turkish (`tr`, default), English (`en`), Simplified Chinese (`zh-CN`). Language choices always display their native names. Traditional Chinese is not included.

The top-right selector updates captions, game-list columns, package labels, status text and the UI font immediately. The selection is saved before applying; a write failure leaves the active language unchanged. The selector is disabled during operations and modal dialogs. Reopening loads the saved choice; missing, malformed, oversized and unknown preferences fall back to Turkish without modifying the preference file.

UI settings live in `ui-preferences.json` alongside `preferences.json` (mod release) and `library.json` (game identities). A language change neither rewrites game data nor installs packages. Manual-store labels are translated only for display; filenames, profile identities, hashes and configuration keys stay stable.

## Translation maintenance

Edit `resources/translations.json`, then run `python tools/generate_translations.py`. Commit the generated `src/translations.inc` too. Visual Studio builds and end users do not need Python or the JSON catalog; translations are compiled into the EXE.

`Tr()` uses exact Turkish source keys for wide captions. `TrUtf8()` is used for application error strings. Do not translate arbitrary game names, paths, binary markers, configuration tokens or persisted identifiers. Resource dialog captions are translated before inserting game data. Dialog font handles live until their dialog closes; Chinese uses Microsoft YaHei UI. New strings need both English and Chinese entries.

Windows-owned captions, file picker controls and third-party error details may follow the system or upstream language. Mod menus inside games are outside this app's language setting.

Run `DLSSAllTools.exe --language-tests <isolated-output-folder>` for preference, Unicode, failure and identity-preservation checks. Installer regression suites remain available separately.
