# Bomber QA BAT setup

The root BAT files run the selected suite through `UnrealEditor-Cmd.exe`, validate the new `index.json`, and send only verified results to the exact Google Sheet named `Bomber QA 테스트 케이스`.

- `RunBomberQA.bat`: `Bomber.Integration.Bomb` -> `통합 테스트 케이스`
- `RunBomberSystemQA.bat`: `Bomber.System` -> `시스템 테스트 케이스`
- `RunBomberJiraFailureDemo.bat`: runs only `SYS-JIRA-FAIL-001`, updates the system sheet, creates or updates a Jira bug, and posts the failure summary to Slack

`Bomber.SystemDemo.Jira.IntentionalFailure` is deliberately excluded from the normal system BAT. It will be invoked separately when validating the Jira failure pipeline.

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

Jira and Slack secrets live only in `qa-integrations.local.json`, copied from `qa-integrations.local.example.json`. This local file is also excluded from Git.

The deployed script discovers test IDs from the target sheet and validates only the suite-specific ID/path prefixes. Adding another `UNIT-`, `INT-`, `SYS-`, `ACC-`, or `UAT-` row does not require another deployment. Deploy a new Apps Script version only when `GoogleAppsScript.gs`, the payload schema, or the supported sheet names change.

## Run

Close Unreal Editor, then double-click the BAT for the suite you want to run.

For video evidence, open Unreal Editor yourself, open **Session Frontend > Automation**, search for `Bomber.SystemVisual.Video`, and run the two visual scenarios. They use the same real PIE interactions as the system tests, but add numbered on-screen messages and longer holds. Visual scenarios do not update Google Sheets, Jira, or Slack.

Exit codes:

- `0`: tests passed and Sheet update was verified
- `1`: at least one test failed
- `2`: at least one test was blocked or incomplete
- `3`: tests finished, but Sheet synchronization failed
- `10`: Unreal Editor was already running
- `4`: Jira or Slack integration failed

Every validated payload is retained under `Saved/Automation/SheetSync` so a Sheet failure does not destroy the test result.
