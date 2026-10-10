# Bomber QA BAT setup

`RunBomberQA.bat` runs `Bomber.Integration.Bomb` through `UnrealEditor-Cmd.exe`, validates the new `index.json`, and sends only verified results to the exact Google Sheet named `Bomber QA 테스트 케이스`.

## One-time Google Apps Script setup

1. Open the target spreadsheet:
   `https://docs.google.com/spreadsheets/d/1UGXPM7sho_VNmtClY1Nrd-pBGjv7YkrBoXlWgwz534g/edit`
2. Open **Extensions > Apps Script**.
3. Replace the editor contents with `GoogleAppsScript.gs` and save.
4. Open `qa-sheet.local.json`. If it does not exist, copy it from `qa-sheet.local.example.json` and replace the example token with a long random value.
5. In **Project Settings > Script Properties**, add:
   - Property: `BOMBER_QA_TOKEN`
   - Value: the `token` from `qa-sheet.local.json`
6. Select **Deploy > New deployment > Web app**.
   - Execute as: **Me**
   - Who has access: **Anyone**
7. Copy the deployed `/exec` URL into `webAppUrl` in `qa-sheet.local.json`.

`qa-sheet.local.json` is excluded from Git. Never commit its URL/token.

## Run

Close Unreal Editor and double-click `RunBomberQA.bat`.

Exit codes:

- `0`: tests passed and Sheet update was verified
- `1`: at least one test failed
- `2`: at least one test was blocked or incomplete
- `3`: tests finished, but Sheet synchronization failed
- `10`: Unreal Editor was already running

Every validated payload is retained under `Saved/Automation/SheetSync` so a Sheet failure does not destroy the test result.
